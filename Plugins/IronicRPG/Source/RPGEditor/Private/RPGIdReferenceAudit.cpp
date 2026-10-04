// Copyright Ironic Studio. All Rights Reserved.

#include "RPGIdReferenceAudit.h"

#include "RPGIdBlueprintReferenceIndex.h"
#include "HAL/IConsoleManager.h"

#include "Assets/RPGPrimaryAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Settings/CharacterSystemSettings.h"
#include "Settings/GameZoneSystemSettings.h"
#include "Components/ActorComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "Engine/WorldInitializationValues.h"
#include "EngineUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/DataLayer/ExternalDataLayerHelper.h"
#include "WorldPartition/WorldPartitionHandle.h"

#define LOCTEXT_NAMESPACE "RPGIdReferenceAudit"

DEFINE_LOG_CATEGORY_STATIC(LogRPGIdReferenceAudit, Log, All);

namespace
{
	TAutoConsoleVariable<int32> CVarReferenceAuditMode(TEXT("rpg.IdReferenceAudit.Mode"), 0,
		TEXT("0: interactive index; 1: direct diagnostic; 2: shadow diagnostic. Commandlets always use direct."));

	const FString LegacySettingsSection = TEXT("/Script/RPGCore.RPGSettings");
	const FString LegacyConfigSource = TEXT("LegacyConfig:/Script/RPGCore.RPGSettings");

	bool IsWorkspaceAsset(const URPGPrimaryAsset* Asset)
	{
		return IsValid(Asset) && Asset->IsAsset() && !Asset->IsTemplate() && !Asset->HasAnyFlags(RF_Transient)
			&& Asset->GetOutermost() != GetTransientPackage() && !Asset->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor);
	}

	bool IsWorkspaceWorld(const UWorld* World)
	{
		return IsValid(World) && World->IsAsset() && !World->HasAnyFlags(RF_Transient)
			&& World->GetOutermost() != GetTransientPackage() && !World->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor)
			&& World->GetOutermost()->GetName().StartsWith(TEXT("/Game/"));
	}

	class FScopedReferenceAuditWorldInitialization
	{
	public:
		explicit FScopedReferenceAuditWorldInitialization(UWorld& InWorld)
			: World(InWorld)
		{
			if (World.bIsWorldInitialized)
			{
				return;
			}

			FWorldInitializationValues Values;
			Values.AllowAudioPlayback(false);
			Values.RequiresHitProxies(false);
			Values.ShouldSimulatePhysics(false);
			Values.EnableTraceCollision(true);
			Values.SetTransactional(false);
			Values.CreateWorldPartition(true);
			Values.CreateAISystem(false);
			Values.CreateNavigation(false);
			World.InitWorld(Values);
			World.UpdateWorldComponents(true, false);
			bOwnsInitialization = true;
		}

		~FScopedReferenceAuditWorldInitialization()
		{
			if (bOwnsInitialization)
			{
				World.ClearWorldComponents();
				World.CleanupWorld();
				World.SetPhysicsScene(nullptr);
			}
		}

	private:
		UWorld& World;
		bool bOwnsInitialization = false;
	};

	FString JoinPath(const FString& Prefix, const FString& Segment)
	{
		return Prefix.IsEmpty() ? Segment : Prefix + TEXT(".") + Segment;
	}

	void CollectStructReferences(const void* Container, const UStruct& Struct, const FString& Source, const FString& Prefix,
		TArray<RPGIdReferencePrivate::FRPGIdReferenceRecord>& OutReferences);

	void CollectValueReferences(const FProperty& Property, const void* Value, const FString& Source, const FString& Path,
		TArray<RPGIdReferencePrivate::FRPGIdReferenceRecord>& OutReferences)
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
		{
			if (StructProperty->Struct == FRPGId::StaticStruct())
			{
				OutReferences.Add({ *static_cast<const FRPGId*>(Value), Source, Path });
				return;
			}
			CollectStructReferences(Value, *StructProperty->Struct, Source, Path, OutReferences);
			return;
		}
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(&Property))
		{
			FScriptArrayHelper Helper(ArrayProperty, Value);
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				CollectValueReferences(*ArrayProperty->Inner, Helper.GetRawPtr(Index), Source,
					FString::Printf(TEXT("%s[%d]"), *Path, Index), OutReferences);
			}
			return;
		}
		if (const FSetProperty* SetProperty = CastField<FSetProperty>(&Property))
		{
			FScriptSetHelper Helper(SetProperty, Value);
			int32 LogicalIndex = 0;
			for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
			{
				if (Helper.IsValidIndex(Index))
				{
					CollectValueReferences(*SetProperty->ElementProp, Helper.GetElementPtr(Index), Source,
						FString::Printf(TEXT("%s{%d}"), *Path, LogicalIndex++), OutReferences);
				}
			}
			return;
		}
		if (const FMapProperty* MapProperty = CastField<FMapProperty>(&Property))
		{
			FScriptMapHelper Helper(MapProperty, Value);
			int32 LogicalIndex = 0;
			for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
			{
				if (Helper.IsValidIndex(Index))
				{
					const FString EntryPath = FString::Printf(TEXT("%s{%d}"), *Path, LogicalIndex++);
					CollectValueReferences(*MapProperty->KeyProp, Helper.GetKeyPtr(Index), Source, EntryPath + TEXT(".Key"), OutReferences);
					CollectValueReferences(*MapProperty->ValueProp, Helper.GetValuePtr(Index), Source, EntryPath + TEXT(".Value"), OutReferences);
				}
			}
		}
	}

	void CollectStructReferences(const void* Container, const UStruct& Struct, const FString& Source, const FString& Prefix,
		TArray<RPGIdReferencePrivate::FRPGIdReferenceRecord>& OutReferences)
	{
		for (TFieldIterator<FProperty> It(&Struct); It; ++It)
		{
			const FProperty& Property = **It;
			if (Property.HasMetaData(TEXT("IdClaim")))
			{
				continue;
			}
			const FString Path = JoinPath(Prefix, Property.GetName());
			CollectValueReferences(Property, Property.ContainerPtrToValuePtr<void>(Container), Source, Path, OutReferences);
		}
	}

	void ScanObject(const UObject& Object, const FString& Source, const FRPGId& Target, TArray<FRPGIdReferenceHit>& OutHits)
	{
		RPGIdReferencePrivate::ScanStruct(&Object, *Object.GetClass(), Source, FString(), Target, OutHits);
	}

	bool IsWorkspaceBlueprint(const UBlueprint* Blueprint)
	{
		if (!IsValid(Blueprint) || (!Blueprint->IsAsset() && !Blueprint->IsA<ULevelScriptBlueprint>()) || Blueprint->HasAnyFlags(RF_Transient)
			|| Blueprint->GetOutermost() == GetTransientPackage() || Blueprint->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
		{
			return false;
		}
		const FString PackageName = Blueprint->GetOutermost()->GetName();
		return PackageName.StartsWith(TEXT("/Game/")) || PackageName.StartsWith(TEXT("/IronicRPG/"));
	}

	bool ImportRPGId(const FString& Text, FRPGId& OutId)
	{
		OutId = FRPGId();
		const TCHAR* End = FRPGId::StaticStruct()->ImportText(*Text, &OutId, nullptr, PPF_None, nullptr,
			FRPGId::StaticStruct()->GetName());
		return End && FString(End).TrimStartAndEnd().IsEmpty();
	}

	bool IsRPGIdStructPin(const UEdGraphPin& Pin)
	{
		return Pin.PinType.PinCategory == UEdGraphSchema_K2::PC_Struct
			&& Pin.PinType.PinSubCategoryObject == FRPGId::StaticStruct() && !Pin.PinType.IsContainer();
	}

	FString GraphPinPath(const UEdGraph& Graph, const UEdGraphNode& Node, const FString& PinName)
	{
		return FString::Printf(TEXT("Graph[%s].Node[%s].Pin[%s]"), *Graph.GetName(), *Node.GetName(), *PinName);
	}

	const UEdGraphPin* FindSplitIdPin(const UEdGraphPin& ParentPin)
	{
		for (const UEdGraphPin* SubPin : ParentPin.SubPins)
		{
			if (!SubPin)
			{
				continue;
			}
			FString LeafName = SubPin->PinName.ToString();
			int32 Separator = INDEX_NONE;
			if (LeafName.FindLastChar(TEXT('_'), Separator))
			{
				LeafName.RightChopInline(Separator + 1);
			}
			if (LeafName == GET_MEMBER_NAME_STRING_CHECKED(FRPGId, Id))
			{
				return SubPin;
			}
			if (const UEdGraphPin* Nested = FindSplitIdPin(*SubPin))
			{
				return Nested;
			}
		}
		return nullptr;
	}

	void ExtractBlueprintGraphPinEvidence(UBlueprint& Blueprint, const FString& Source,
		RPGIdReferencePrivate::FRPGIdBlueprintEvidence& OutEvidence)
	{
		TArray<UEdGraph*> Graphs;
		Blueprint.GetAllGraphs(Graphs);
		for (const UEdGraph* Graph : Graphs)
		{
			if (!IsValid(Graph))
			{
				continue;
			}
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				if (!IsValid(Node))
				{
					continue;
				}
				for (const UEdGraphPin* Pin : Node->Pins)
				{
					if (!Pin || Pin->Direction != EGPD_Input || Pin->bOrphanedPin || Pin->bDefaultValueIsIgnored
						|| !IsRPGIdStructPin(*Pin) || Pin->HasAnyConnections())
					{
						continue;
					}

					FRPGId Value;
					FString DisplayPinName = Pin->PinName.ToString();
					if (Pin->SubPins.IsEmpty())
					{
						const FString DefaultValue = Pin->GetDefaultAsString();
						if (DefaultValue.IsEmpty())
						{
							continue;
						}
						if (!ImportRPGId(DefaultValue, Value))
						{
							OutEvidence.Issues.Add({ RPGIdReferencePrivate::ERPGIdReferenceIssueCode::MalformedBlueprintGraphPin,
								FSoftObjectPath(&Blueprint).ToString(), GraphPinPath(*Graph, *Node, DisplayPinName) });
							continue;
						}
					}
					else
					{
						const UEdGraphPin* IdPin = FindSplitIdPin(*Pin);
						DisplayPinName += TEXT(".Id");
						if (!IdPin)
						{
							OutEvidence.Issues.Add({ RPGIdReferencePrivate::ERPGIdReferenceIssueCode::MalformedBlueprintGraphPin,
								FSoftObjectPath(&Blueprint).ToString(), GraphPinPath(*Graph, *Node, DisplayPinName) });
							continue;
						}
						if (IdPin->HasAnyConnections())
						{
							continue;
						}
						const FString DefaultValue = IdPin->GetDefaultAsString();
						if (DefaultValue.IsEmpty())
						{
							continue;
						}
						const FProperty* IdProperty = FRPGId::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FRPGId, Id));
						FName ParsedId;
						const TCHAR* End = IdProperty ? IdProperty->ImportText_Direct(*DefaultValue, &ParsedId, nullptr, PPF_None, nullptr) : nullptr;
						if (!End || !FString(End).TrimStartAndEnd().IsEmpty())
						{
							OutEvidence.Issues.Add({ RPGIdReferencePrivate::ERPGIdReferenceIssueCode::MalformedBlueprintGraphPin,
								FSoftObjectPath(&Blueprint).ToString(), GraphPinPath(*Graph, *Node, DisplayPinName) });
							continue;
						}
						Value = FRPGId(ParsedId);
					}
					OutEvidence.References.Add({ Value, Source, GraphPinPath(*Graph, *Node, DisplayPinName) });
				}
			}
		}
	}

	bool ExtractLegacyStructMember(const FString& Text, const FString& MemberName, FString& OutValue)
	{
		const FString Marker = MemberName + TEXT("=");
		int32 ValueStart = Text.Find(Marker, ESearchCase::CaseSensitive);
		if (ValueStart == INDEX_NONE)
		{
			return false;
		}
		ValueStart += Marker.Len();
		while (ValueStart < Text.Len() && FChar::IsWhitespace(Text[ValueStart]))
		{
			++ValueStart;
		}
		if (ValueStart >= Text.Len() || Text[ValueStart] != TCHAR('('))
		{
			return false;
		}

		int32 Depth = 0;
		bool bQuoted = false;
		bool bEscaped = false;
		for (int32 Index = ValueStart; Index < Text.Len(); ++Index)
		{
			const TCHAR Character = Text[Index];
			if (bQuoted)
			{
				if (bEscaped)
				{
					bEscaped = false;
				}
				else if (Character == TCHAR('\\'))
				{
					bEscaped = true;
				}
				else if (Character == TCHAR('"'))
				{
					bQuoted = false;
				}
				continue;
			}
			if (Character == TCHAR('"'))
			{
				bQuoted = true;
			}
			else if (Character == TCHAR('('))
			{
				++Depth;
			}
			else if (Character == TCHAR(')') && --Depth == 0)
			{
				OutValue = Text.Mid(ValueStart, Index - ValueStart + 1);
				return true;
			}
		}
		return false;
	}

	void AppendMalformedLegacyConfigGap(const FName Key, const int32 Index, TArray<FText>& OutCoverageGaps)
	{
		OutCoverageGaps.Add(FText::Format(LOCTEXT("MalformedLegacyConfig",
			"Legacy config entry {0}.{1}[{2}] could not be parsed and was not fully inspected."), FText::FromString(LegacySettingsSection),
			FText::FromName(Key), FText::AsNumber(Index)));
	}

	void ScanLegacyIdArray(const FConfigSection& Section, const FName Key, const FRPGId& Target, TArray<FRPGIdReferenceHit>& OutHits,
		TArray<FText>& OutCoverageGaps)
	{
		TArray<FString> Values;
		Section.MultiFind(Key, Values, true);
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			FRPGId Value;
			if (!ImportRPGId(Values[Index], Value))
			{
				AppendMalformedLegacyConfigGap(Key, Index, OutCoverageGaps);
			}
			else if (Value == Target)
			{
				OutHits.Add({ LegacyConfigSource, FString::Printf(TEXT("%s[%d]"), *Key.ToString(), Index) });
			}
		}
	}

	void ScanWorldImpl(UWorld& World, const FRPGId& Target, TArray<FRPGIdReferenceHit>& OutHits, TArray<FText>& OutCoverageGaps,
		TSet<FName>* OutScannedExternalDataLayerPackages)
	{
		const FString WorldSource = FSoftObjectPath(&World).ToString();
		FScopedReferenceAuditWorldInitialization Initialization(World);
		if (!World.bIsWorldInitialized)
		{
			OutCoverageGaps.Add(FText::Format(LOCTEXT("WorldInit", "Could not initialize Level {0} for reference inspection."),
				FText::FromString(WorldSource)));
			return;
		}

		TArray<FWorldPartitionReference> LoadedActorReferences;
		if (UWorldPartition* WorldPartition = World.GetWorldPartition())
		{
			if (!WorldPartition->IsInitialized())
			{
				OutCoverageGaps.Add(FText::Format(LOCTEXT("WorldPartitionInit", "World Partition is not initialized for Level {0}."),
					FText::FromString(WorldSource)));
			}
			else
			{
				WorldPartition->LoadAllActors(LoadedActorReferences);
				for (const FWorldPartitionReference& Reference : LoadedActorReferences)
				{
					if (!Reference.IsLoaded() || !IsValid(Reference.GetActor()))
					{
						OutCoverageGaps.Add(FText::Format(LOCTEXT("WorldPartitionActorLoad", "At least one World Partition actor could not be loaded for Level {0}."),
							FText::FromString(WorldSource)));
						break;
					}
				}
			}
		}

		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			AActor* Actor = *It;
			if (!IsValid(Actor))
			{
				continue;
			}
			if (OutScannedExternalDataLayerPackages && Actor->IsPackageExternal())
			{
				if (const UPackage* ExternalPackage = Actor->GetExternalPackage();
					ExternalPackage && FExternalDataLayerHelper::IsExternalDataLayerPath(ExternalPackage->GetName()))
				{
					OutScannedExternalDataLayerPackages->Add(ExternalPackage->GetFName());
				}
			}
			ScanObject(*Actor, FSoftObjectPath(Actor).ToString(), Target, OutHits);
			TInlineComponentArray<UActorComponent*> Components(Actor);
			for (const UActorComponent* Component : Components)
			{
				if (IsValid(Component))
				{
					ScanObject(*Component, FSoftObjectPath(Component).ToString(), Target, OutHits);
				}
			}
		}
	}
}

void RPGIdReferencePrivate::ScanStruct(const void* Container, const UStruct& Struct, const FString& Source, const FString& Prefix,
	const FRPGId& Target, TArray<FRPGIdReferenceHit>& OutHits)
{
	TArray<FRPGIdReferenceRecord> References;
	CollectStructReferences(Container, Struct, Source, Prefix, References);
	for (const FRPGIdReferenceRecord& Reference : References)
	{
		if (Reference.Id == Target)
		{
			OutHits.Add({ Reference.Source, Reference.PropertyPath });
		}
	}
}

void RPGIdReferencePrivate::ScanWorld(UWorld& World, const FRPGId& Target, TArray<FRPGIdReferenceHit>& OutHits,
	TArray<FText>& OutCoverageGaps)
{
	ScanWorldImpl(World, Target, OutHits, OutCoverageGaps, nullptr);
}

RPGIdReferencePrivate::FRPGIdBlueprintEvidence RPGIdReferencePrivate::ExtractBlueprintEvidence(UBlueprint& Blueprint)
{
	FRPGIdBlueprintEvidence Evidence;
	const FString BlueprintPath = FSoftObjectPath(&Blueprint).ToString();
	const FString Source = TEXT("Blueprint:") + FSoftObjectPath(&Blueprint).ToString();
	if (!Blueprint.IsUpToDate())
	{
		Evidence.Issues.Add({ ERPGIdReferenceIssueCode::BlueprintCompileState, BlueprintPath, FString() });
	}

	UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(Blueprint.GeneratedClass);
	if (!GeneratedClass)
	{
		Evidence.Issues.Add({ ERPGIdReferenceIssueCode::BlueprintGeneratedClass, BlueprintPath, FString() });
		return Evidence;
	}

	UObject* ClassDefaultObject = GeneratedClass->GetDefaultObject();
	if (!IsValid(ClassDefaultObject))
	{
		Evidence.Issues.Add({ ERPGIdReferenceIssueCode::BlueprintClassDefaultObject, BlueprintPath, FString() });
	}
	else
	{
		CollectStructReferences(ClassDefaultObject, *GeneratedClass, Source, TEXT("ClassDefaultObject"), Evidence.References);
	}

	TArray<UObject*> NestedObjects;
	GetObjectsWithOuter(&Blueprint, NestedObjects, EGetObjectsFlags::IncludeNestedObjects);
	GetObjectsWithOuter(GeneratedClass, NestedObjects, EGetObjectsFlags::IncludeNestedObjects);
	TSet<const UObject*> ScannedTemplates;
	for (const UObject* Object : NestedObjects)
	{
		if (!IsValid(Object) || Object == ClassDefaultObject || Object->IsA<UClass>() || !Object->IsTemplate()
			|| ScannedTemplates.Contains(Object))
		{
			continue;
		}
		ScannedTemplates.Add(Object);
		const FString TemplatePath = FString::Printf(TEXT("Template[%s]"), *Object->GetPathName(&Blueprint));
		CollectStructReferences(Object, *Object->GetClass(), Source, TemplatePath, Evidence.References);
	}

	ExtractBlueprintGraphPinEvidence(Blueprint, Source, Evidence);
	return Evidence;
}

void RPGIdReferencePrivate::AppendBlueprintEvidence(const FRPGIdBlueprintEvidence& Evidence, const FRPGId& Target,
	TArray<FRPGIdReferenceHit>& OutHits, TArray<FText>& OutCoverageGaps)
{
	for (const FRPGIdReferenceRecord& Reference : Evidence.References)
	{
		if (Reference.Id == Target)
		{
			OutHits.Add({ Reference.Source, Reference.PropertyPath });
		}
	}

	for (const FRPGIdReferenceIssue& Issue : Evidence.Issues)
	{
		switch (Issue.Code)
		{
		case ERPGIdReferenceIssueCode::BlueprintCompileState:
			OutCoverageGaps.Add(FText::Format(LOCTEXT("BlueprintCompileState",
				"Blueprint {0} is dirty, unknown, compiling or contains compile errors; its effective defaults could not be proven current."),
				FText::FromString(Issue.Source)));
			break;
		case ERPGIdReferenceIssueCode::BlueprintGeneratedClass:
			OutCoverageGaps.Add(FText::Format(LOCTEXT("BlueprintGeneratedClass",
				"Blueprint {0} has no inspectable generated class."), FText::FromString(Issue.Source)));
			break;
		case ERPGIdReferenceIssueCode::BlueprintClassDefaultObject:
			OutCoverageGaps.Add(FText::Format(LOCTEXT("BlueprintClassDefaultObject",
				"Blueprint {0} has no inspectable class default object."), FText::FromString(Issue.Source)));
			break;
		case ERPGIdReferenceIssueCode::MalformedBlueprintGraphPin:
			OutCoverageGaps.Add(FText::Format(LOCTEXT("MalformedBlueprintGraphPin",
				"Blueprint {0} graph literal {1} could not be parsed as an RPG Id and was not fully inspected."),
				FText::FromString(Issue.Source), FText::FromString(Issue.Detail)));
			break;
		default:
			checkNoEntry();
			break;
		}
	}
}

void RPGIdReferencePrivate::ScanBlueprint(UBlueprint& Blueprint, const FRPGId& Target, TArray<FRPGIdReferenceHit>& OutHits,
	TArray<FText>& OutCoverageGaps)
{
	AppendBlueprintEvidence(ExtractBlueprintEvidence(Blueprint), Target, OutHits, OutCoverageGaps);
}

void RPGIdReferencePrivate::AppendExternalDataLayerCoverageGaps(const TSet<FName>& OnDiskPackages, const TSet<FName>& ScannedPackages,
	TArray<FText>& OutCoverageGaps)
{
	TArray<FName> UnscannedPackages = OnDiskPackages.Difference(ScannedPackages).Array();
	UnscannedPackages.Sort([](const FName& A, const FName& B)
	{
		return A.LexicalLess(B);
	});
	for (const FName PackageName : UnscannedPackages)
	{
		OutCoverageGaps.Add(FText::Format(LOCTEXT("ExternalDataLayerPackageGap",
			"External Data Layer actor package {0} was not registered with an inspected Level and could not be scanned."),
			FText::FromName(PackageName)));
	}
}

void RPGIdReferencePrivate::ScanLegacyConfig(const FConfigFile& Config, const FRPGId& Target, TArray<FRPGIdReferenceHit>& OutHits,
	TArray<FText>& OutCoverageGaps)
{
	const FConfigSection* Section = Config.FindSection(LegacySettingsSection);
	if (!Section)
	{
		return;
	}

	ScanLegacyIdArray(*Section, TEXT("PlayableCharacters"), Target, OutHits, OutCoverageGaps);
	ScanLegacyIdArray(*Section, TEXT("DefaultPartyMembers"), Target, OutHits, OutCoverageGaps);

	TArray<FString> Contexts;
	Section->MultiFind(TEXT("DefaultGameZoneContext"), Contexts, true);
	for (int32 Index = 0; Index < Contexts.Num(); ++Index)
	{
		FString ZoneIdText;
		FRPGId ZoneId;
		if (!ExtractLegacyStructMember(Contexts[Index], TEXT("ZoneId"), ZoneIdText) || !ImportRPGId(ZoneIdText, ZoneId))
		{
			AppendMalformedLegacyConfigGap(TEXT("DefaultGameZoneContext"), Index, OutCoverageGaps);
		}
		else if (ZoneId == Target)
		{
			const FString Path = Contexts.Num() == 1 ? TEXT("DefaultGameZoneContext.ZoneId")
				: FString::Printf(TEXT("DefaultGameZoneContext[%d].ZoneId"), Index);
			OutHits.Add({ LegacyConfigSource, Path });
		}
	}
}

FRPGIdReferenceAuditResult RPGIdReferencePrivate::AuditDevelopmentReferences(const FRPGId& Target, ERPGIdAuditMode Mode)
{
	const double AuditStartSeconds = FPlatformTime::Seconds();
	double RegistryDiscoveryMilliseconds = 0.0;
	double NativeAssetMilliseconds = 0.0;
	double BlueprintMilliseconds = 0.0;
	double LevelMilliseconds = 0.0;
	double OverlayAndConfigMilliseconds = 0.0;
	int32 NativeAssetCount = 0;
	int32 BlueprintCount = 0;
	int32 LevelCount = 0;
	int32 SynchronousLoadCount = 0;
	FRPGIdReferenceAuditResult Result;
	RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexCache ShadowIndexCache;
	FString ShadowQueryReason;
	if (IsRunningCommandlet()) { Mode = ERPGIdAuditMode::Direct; }
	else if (Mode == ERPGIdAuditMode::Automatic)
	{
		const int32 DiagnosticMode = CVarReferenceAuditMode.GetValueOnGameThread();
		Mode = DiagnosticMode == 1 ? ERPGIdAuditMode::Direct : DiagnosticMode == 2 ? ERPGIdAuditMode::Shadow : ERPGIdAuditMode::Indexed;
	}
	const bool bIndexed = Mode == ERPGIdAuditMode::Indexed;
	const bool bShadowQueryReady = (bIndexed || Mode == ERPGIdAuditMode::Shadow)
		&& FRPGIdBlueprintReferenceIndex::QueryExactSnapshot(ShadowIndexCache, ShadowQueryReason);
	if (bIndexed)
	{
		if (!bShadowQueryReady)
		{
			Result.CoverageGaps.Add(FText::FromString(TEXT("Blueprint reference index is not ready. ") + ShadowQueryReason));
			Result.Summary = FText::FromString(TEXT("Reference audit incomplete. Wait for the index to finish rebuilding, then Check again. Apply is disabled."));
			UE_LOG(LogRPGIdReferenceAudit, Display, TEXT("Indexed Check for %s: NotReady; total=%.1fms Blueprint sync loads=0; %s"),
				*Target.ToString(), (FPlatformTime::Seconds() - AuditStartSeconds) * 1000.0, *ShadowQueryReason);
			return Result;
		}
		Result.IndexGeneration = FRPGIdBlueprintReferenceIndex::GetGeneration();
	}
	TArray<RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexEntry> DirectBlueprintEntries;
	TSet<FSoftObjectPath> ScannedAssets;
	TSet<FSoftObjectPath> ScannedBlueprints;
	TSet<FSoftObjectPath> ScannedWorlds;
	TSet<FName> OnDiskExternalDataLayerPackages;
	TSet<FName> ScannedExternalDataLayerPackages;
	bool bExternalDataLayerDiscoveryComplete = false;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	if (Registry.IsLoadingAssets() || !Registry.IsSearchAllAssets())
	{
		Result.CoverageGaps.Add(LOCTEXT("RegistryLoading",
			"Mounted asset discovery is incomplete; native asset, Blueprint, Level and External Data Layer references were not fully inspected."));
	}
	else
	{
		const double RegistryDiscoveryStartSeconds = FPlatformTime::Seconds();
		TArray<FAssetData> AllAssets;
		if (!Registry.GetAllAssets(AllAssets, true))
		{
			Result.CoverageGaps.Add(LOCTEXT("ExternalDataLayerQuery", "The External Data Layer actor package query failed."));
		}
		else
		{
			bExternalDataLayerDiscoveryComplete = true;
			for (const FAssetData& Data : AllAssets)
			{
				if (FExternalDataLayerHelper::IsExternalDataLayerPath(Data.PackageName.ToString()))
				{
					OnDiskExternalDataLayerPackages.Add(Data.PackageName);
				}
			}
		}
		RegistryDiscoveryMilliseconds = (FPlatformTime::Seconds() - RegistryDiscoveryStartSeconds) * 1000.0;

		const double NativeAssetStartSeconds = FPlatformTime::Seconds();
		FARFilter Filter;
		Filter.ClassPaths.Add(URPGPrimaryAsset::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;
		Filter.bIncludeOnlyOnDiskAssets = true;
		TArray<FAssetData> Assets;
		if (!Registry.GetAssets(Filter, Assets, false))
		{
			Result.CoverageGaps.Add(LOCTEXT("RegistryQuery", "The native RPGPrimaryAsset reference query failed."));
		}
		else
		{
			for (const FAssetData& Data : Assets)
			{
				SynchronousLoadCount += Data.IsAssetLoaded() ? 0 : 1;
				URPGPrimaryAsset* Asset = Cast<URPGPrimaryAsset>(Data.GetAsset());
				if (!IsWorkspaceAsset(Asset))
				{
					Result.CoverageGaps.Add(FText::Format(LOCTEXT("AssetLoad", "Could not inspect native asset {0}."),
						FText::FromString(Data.GetSoftObjectPath().ToString())));
					continue;
				}
				const FSoftObjectPath Path(Asset);
				ScannedAssets.Add(Path);
				ScanObject(*Asset, Path.ToString(), Target, Result.Hits);
				++NativeAssetCount;
			}
		}
		NativeAssetMilliseconds = (FPlatformTime::Seconds() - NativeAssetStartSeconds) * 1000.0;

		const double BlueprintStartSeconds = FPlatformTime::Seconds();
		if (!bIndexed)
		{
			FARFilter BlueprintFilter;
			BlueprintFilter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
			BlueprintFilter.PackagePaths.Add(FName(TEXT("/Game")));
			BlueprintFilter.PackagePaths.Add(FName(TEXT("/IronicRPG")));
			BlueprintFilter.bRecursiveClasses = true;
			BlueprintFilter.bRecursivePaths = true;
			BlueprintFilter.bIncludeOnlyOnDiskAssets = true;
			TArray<FAssetData> Blueprints;
			if (!Registry.GetAssets(BlueprintFilter, Blueprints, false))
			{
				Result.CoverageGaps.Add(LOCTEXT("BlueprintQuery", "The Blueprint default and template reference query failed."));
			}
			else
			{
				Blueprints.Sort([](const FAssetData& A, const FAssetData& B)
				{
					return A.PackageName.LexicalLess(B.PackageName);
				});
				for (const FAssetData& Data : Blueprints)
				{
					Result.BlueprintSynchronousLoads += Data.IsAssetLoaded() ? 0 : 1;
					SynchronousLoadCount += Data.IsAssetLoaded() ? 0 : 1;
					UBlueprint* Blueprint = Cast<UBlueprint>(Data.GetAsset());
					if (!IsWorkspaceBlueprint(Blueprint))
					{
						Result.CoverageGaps.Add(FText::Format(LOCTEXT("BlueprintLoad", "Could not inspect Blueprint {0}."),
							FText::FromString(Data.GetSoftObjectPath().ToString())));
						continue;
					}
					const FSoftObjectPath Path(Blueprint);
					ScannedBlueprints.Add(Path);
					FRPGIdBlueprintEvidence Evidence = ExtractBlueprintEvidence(*Blueprint);
					DirectBlueprintEntries.Add({ Data.PackageName, Path.ToString(), Evidence });
					AppendBlueprintEvidence(Evidence, Target, Result.Hits, Result.CoverageGaps);
					++BlueprintCount;
				}
			}
			if (bShadowQueryReady)
			{
				const RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexShadowComparison ShadowComparison =
					RPGIdReferenceIndexPrivate::CompareBlueprintEvidence(ShadowIndexCache.Entries, DirectBlueprintEntries);
				const bool bShadowResultAccepted = FRPGIdBlueprintReferenceIndex::RecordShadowComparison(Target,
					ShadowIndexCache.WorkspaceFingerprint, ShadowComparison);
				if (ShadowComparison.bExact && bShadowResultAccepted)
				{
					UE_LOG(LogRPGIdReferenceAudit, Display, TEXT("Blueprint index shadow comparison for %s is exact across %d packages."),
						*Target.ToString(), DirectBlueprintEntries.Num());
				}
				else if (bShadowResultAccepted)
				{
					UE_LOG(LogRPGIdReferenceAudit, Warning, TEXT("Blueprint index shadow comparison for %s failed: %s"),
						*Target.ToString(), *ShadowComparison.Difference);
				}
				else
				{
					UE_LOG(LogRPGIdReferenceAudit, Verbose,
						TEXT("Blueprint index shadow comparison for %s was discarded because the index generation changed."),
						*Target.ToString());
				}
			}
			else if (!IsRunningCommandlet())
			{
				UE_LOG(LogRPGIdReferenceAudit, Verbose, TEXT("Blueprint index shadow comparison for %s was skipped: %s"),
					*Target.ToString(), *ShadowQueryReason);
			}
		}
		BlueprintMilliseconds = (FPlatformTime::Seconds() - BlueprintStartSeconds) * 1000.0;

		const double LevelStartSeconds = FPlatformTime::Seconds();
		FARFilter WorldFilter;
		WorldFilter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
		WorldFilter.PackagePaths.Add(FName(TEXT("/Game")));
		WorldFilter.bRecursivePaths = true;
		WorldFilter.bIncludeOnlyOnDiskAssets = true;
		TArray<FAssetData> Worlds;
		if (!Registry.GetAssets(WorldFilter, Worlds, false))
		{
			Result.CoverageGaps.Add(LOCTEXT("WorldQuery", "The Level reference query failed."));
		}
		else
		{
			for (const FAssetData& Data : Worlds)
			{
				SynchronousLoadCount += Data.IsAssetLoaded() ? 0 : 1;
				UWorld* World = Cast<UWorld>(Data.GetAsset());
				if (!IsWorkspaceWorld(World))
				{
					Result.CoverageGaps.Add(FText::Format(LOCTEXT("WorldLoad", "Could not inspect Level {0}."),
						FText::FromString(Data.GetSoftObjectPath().ToString())));
					continue;
				}
				const FSoftObjectPath Path(World);
				ScannedWorlds.Add(Path);
				ScanWorldImpl(*World, Target, Result.Hits, Result.CoverageGaps, &ScannedExternalDataLayerPackages);
				++LevelCount;
			}
		}
		LevelMilliseconds = (FPlatformTime::Seconds() - LevelStartSeconds) * 1000.0;
	}

	const double OverlayAndConfigStartSeconds = FPlatformTime::Seconds();
	for (TObjectIterator<URPGPrimaryAsset> It; It; ++It)
	{
		if (IsWorkspaceAsset(*It) && !ScannedAssets.Contains(FSoftObjectPath(*It)))
		{
			ScanObject(**It, FSoftObjectPath(*It).ToString(), Target, Result.Hits);
		}
	}
	for (TObjectIterator<UWorld> It; It; ++It)
	{
		if (IsWorkspaceWorld(*It) && !ScannedWorlds.Contains(FSoftObjectPath(*It)))
		{
			ScanWorldImpl(**It, Target, Result.Hits, Result.CoverageGaps, &ScannedExternalDataLayerPackages);
		}
	}

	ScanObject(*GetDefault<UCharacterSystemSettings>(), TEXT("Config:/Script/RPGCore.CharacterSystemSettings"), Target, Result.Hits);
	ScanObject(*GetDefault<UGameZoneSystemSettings>(), TEXT("Config:/Script/RPGCore.GameZoneSystemSettings"), Target, Result.Hits);
	if (!GConfig)
	{
		Result.CoverageGaps.Add(LOCTEXT("LegacyConfigUnavailable", "The IronicRPG config cache is unavailable; legacy Id entries were not inspected."));
	}
	else
	{
		const FString ConfigFilename = GConfig->GetConfigFilename(TEXT("IronicRPG"));
		if (const FConfigFile* Config = GConfig->FindConfigFile(ConfigFilename))
		{
			ScanLegacyConfig(*Config, Target, Result.Hits, Result.CoverageGaps);
		}
		else
		{
			Result.CoverageGaps.Add(LOCTEXT("LegacyConfigMissing",
				"The IronicRPG config hierarchy could not be loaded; legacy Id entries were not inspected."));
		}
	}
	for (TObjectIterator<UBlueprint> It; It; ++It)
	{
		if (IsWorkspaceBlueprint(*It) && !ScannedBlueprints.Contains(FSoftObjectPath(*It)))
		{
			ScanBlueprint(**It, Target, Result.Hits, Result.CoverageGaps);
			ScannedBlueprints.Add(FSoftObjectPath(*It));
			++Result.BlueprintOverlayCount;
			if (bIndexed && It->GetOutermost()->IsDirty())
			{
				Result.CoverageGaps.Add(FText::FromString(TEXT("Loaded Blueprint package has unsaved changes: ") + It->GetOutermost()->GetName()));
			}
		}
	}
	if (bIndexed)
	{
		for (const auto& Entry : ShadowIndexCache.Entries)
		{
			if (!ScannedBlueprints.Contains(FSoftObjectPath(Entry.AssetPath)))
			{
				AppendBlueprintEvidence(Entry.Evidence, Target, Result.Hits, Result.CoverageGaps);
				++Result.BlueprintCachedCount;
			}
		}
		BlueprintCount = ShadowIndexCache.Entries.Num();
		if (!FRPGIdBlueprintReferenceIndex::IsGenerationCurrent(Result.IndexGeneration))
		{
			Result.CoverageGaps.Add(FText::FromString(TEXT("The reference index changed during inspection. Check again.")));
		}
	}
	if (bExternalDataLayerDiscoveryComplete)
	{
		AppendExternalDataLayerCoverageGaps(OnDiskExternalDataLayerPackages, ScannedExternalDataLayerPackages, Result.CoverageGaps);
	}
	OverlayAndConfigMilliseconds = (FPlatformTime::Seconds() - OverlayAndConfigStartSeconds) * 1000.0;
	Result.Hits.Sort([](const FRPGIdReferenceHit& A, const FRPGIdReferenceHit& B)
	{
		return A.Source == B.Source ? A.PropertyPath < B.PropertyPath : A.Source < B.Source;
	});
	Result.Summary = FText::Format(LOCTEXT("Summary", "Read-only development audit found {0} reference(s). Covered: native RPGPrimaryAsset "
		"properties, Blueprint class defaults/templates and unconnected graph literals, typed project settings, known legacy RPGSettings Id entries, "
		"Level actors/components and registered World Partition actor descriptors. Apply remains disabled while {1} coverage gap(s) remain; connected "
		"runtime values and old player saves are outside this development guarantee."),
		FText::AsNumber(Result.Hits.Num()), FText::AsNumber(Result.CoverageGaps.Num()));
	if (bIndexed)
	{
		Result.Summary = FText::FromString(Result.Summary.ToString() + FString::Printf(
			TEXT(" Index generation %llu; %d cached Blueprint(s), %d live overlay(s). Apply repeats verification."),
			Result.IndexGeneration, Result.BlueprintCachedCount, Result.BlueprintOverlayCount));
	}
	UE_LOG(LogRPGIdReferenceAudit, Display, TEXT("Blueprint audit route=%s generation=%llu saved=%d cached=%d overlays=%d hits=%d gaps=%d Blueprint sync loads=%d"),
		bIndexed ? TEXT("Indexed") : TEXT("Direct"), Result.IndexGeneration, BlueprintCount, Result.BlueprintCachedCount,
		Result.BlueprintOverlayCount, Result.Hits.Num(), Result.CoverageGaps.Num(), Result.BlueprintSynchronousLoads);
	const double TotalMilliseconds = (FPlatformTime::Seconds() - AuditStartSeconds) * 1000.0;
	UE_LOG(LogRPGIdReferenceAudit, Display,
		TEXT("Check timing for %s: total=%.1fms registry=%.1fms native=%.1fms blueprints=%.1fms levels=%.1fms overlay+config=%.1fms; "
			"scanned native=%d blueprints=%d levels=%d synchronous package loads=%d"), *Target.ToString(), TotalMilliseconds,
		RegistryDiscoveryMilliseconds, NativeAssetMilliseconds, BlueprintMilliseconds, LevelMilliseconds, OverlayAndConfigMilliseconds,
		NativeAssetCount, BlueprintCount, LevelCount, SynchronousLoadCount);
	return Result;
}

#undef LOCTEXT_NAMESPACE
