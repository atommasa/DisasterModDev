// Copyright Ironic Studio. All Rights Reserved.

#include "RPGIdReferenceMigrationWorkspace.h"

#include "RPGIdReferenceAudit.h"
#include "RPGIdReferenceMigrationConfigUndo.h"
#include "RPGIdReferenceMigrationHandoff.h"
#include "Assets/RPGAssetManager.h"
#include "Assets/RPGPrimaryAsset.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Settings/CharacterSystemSettings.h"
#include "Settings/GameZoneSystemSettings.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "RPGIdReferenceMigrationWorkspace"

namespace
{
	const TCHAR* BlueprintPrefix = TEXT("Blueprint:");
	const TCHAR* TypedCharacterConfig = TEXT("Config:/Script/RPGCore.CharacterSystemSettings");
	const TCHAR* TypedGameZoneConfig = TEXT("Config:/Script/RPGCore.GameZoneSystemSettings");
	const TCHAR* LegacyConfig = TEXT("LegacyConfig:/Script/RPGCore.RPGSettings");
	const TCHAR* LegacySection = TEXT("/Script/RPGCore.RPGSettings");

	struct FContainerRehash
	{
		FProperty* Property = nullptr;
		void* Value = nullptr;
		int32 PhysicalIndex = INDEX_NONE;
		bool bIdentityElement = false;

		void Rehash() const
		{
			if (const FSetProperty* Set = CastField<FSetProperty>(Property))
			{
				FScriptSetHelper(Set, Value).Rehash();
			}
			else if (const FMapProperty* Map = CastField<FMapProperty>(Property))
			{
				FScriptMapHelper(Map, Value).Rehash();
			}
		}

		bool HasCollision() const
		{
			if (!bIdentityElement)
			{
				return false;
			}
			if (const FSetProperty* Set = CastField<FSetProperty>(Property))
			{
				FScriptSetHelper Helper(Set, Value);
				const void* Current = Helper.GetElementPtr(PhysicalIndex);
				for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
				{
					if (Index != PhysicalIndex && Helper.IsValidIndex(Index)
						&& Set->ElementProp->Identical(Current, Helper.GetElementPtr(Index)))
					{
						return true;
					}
				}
			}
			else if (const FMapProperty* Map = CastField<FMapProperty>(Property))
			{
				FScriptMapHelper Helper(Map, Value);
				const void* Current = Helper.GetKeyPtr(PhysicalIndex);
				for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
				{
					if (Index != PhysicalIndex && Helper.IsValidIndex(Index)
						&& Map->KeyProp->Identical(Current, Helper.GetKeyPtr(Index)))
					{
						return true;
					}
				}
			}
			return false;
		}
	};

	struct FResolvedRPGId
	{
		FRPGId* Value = nullptr;
		TArray<FContainerRehash> Containers;
	};

	struct FPreparedEdit
	{
		FRPGIdReferenceMigrationEdit Edit;
		TWeakObjectPtr<UObject> Object;
		TWeakObjectPtr<UBlueprint> Blueprint;
		FResolvedRPGId Resolved;
		UEdGraphPin* Pin = nullptr;
		TWeakObjectPtr<UEdGraphNode> PinNode;
		FString OriginalPinValue;
		FName LegacyKey;
		int32 LegacyIndex = INDEX_NONE;
		bool bLegacyZoneContext = false;
		bool bObjectWasTransactional = false;
		bool bApplied = false;
	};

	int32 PhysicalIndexForLogical(const FScriptSetHelper& Helper, const int32 LogicalIndex)
	{
		int32 Logical = 0;
		for (int32 Physical = 0; Physical < Helper.GetMaxIndex(); ++Physical)
		{
			if (Helper.IsValidIndex(Physical) && Logical++ == LogicalIndex)
			{
				return Physical;
			}
		}
		return INDEX_NONE;
	}

	int32 PhysicalIndexForLogical(const FScriptMapHelper& Helper, const int32 LogicalIndex)
	{
		int32 Logical = 0;
		for (int32 Physical = 0; Physical < Helper.GetMaxIndex(); ++Physical)
		{
			if (Helper.IsValidIndex(Physical) && Logical++ == LogicalIndex)
			{
				return Physical;
			}
		}
		return INDEX_NONE;
	}

	bool ParseIndexedSegment(const FString& Segment, TCHAR Open, TCHAR Close, FString& OutName, int32& OutIndex)
	{
		const int32 OpenIndex = Segment.Find(FString::Chr(Open), ESearchCase::CaseSensitive, ESearchDir::FromStart);
		if (OpenIndex == INDEX_NONE || !Segment.EndsWith(FString::Chr(Close)))
		{
			return false;
		}
		OutName = Segment.Left(OpenIndex);
		const FString IndexText = Segment.Mid(OpenIndex + 1, Segment.Len() - OpenIndex - 2);
		if (IndexText.IsEmpty() || !IndexText.IsNumeric())
		{
			return false;
		}
		OutIndex = FCString::Atoi(*IndexText);
		return OutIndex >= 0;
	}

	bool ResolveValue(void* Value, FProperty& Property, const FString& Remaining, FResolvedRPGId& Out, FString& OutError);

	bool ResolveStructPath(void* Container, UStruct& Struct, const FString& Path, FResolvedRPGId& Out, FString& OutError)
	{
		FString Segment;
		FString Remaining;
		if (!Path.Split(TEXT("."), &Segment, &Remaining))
		{
			Segment = Path;
		}

		FString PropertyName = Segment;
		int32 Index = INDEX_NONE;
		const bool bArray = ParseIndexedSegment(Segment, TCHAR('['), TCHAR(']'), PropertyName, Index);
		const bool bContainer = !bArray && ParseIndexedSegment(Segment, TCHAR('{'), TCHAR('}'), PropertyName, Index);
		FProperty* Property = FindFProperty<FProperty>(&Struct, *PropertyName);
		if (!Property)
		{
			OutError = FString::Printf(TEXT("Property %s was not found while resolving %s."), *PropertyName, *Path);
			return false;
		}

		void* PropertyValue = Property->ContainerPtrToValuePtr<void>(Container);
		if (bArray)
		{
			FArrayProperty* Array = CastField<FArrayProperty>(Property);
			if (!Array)
			{
				OutError = FString::Printf(TEXT("%s is not an array."), *PropertyName);
				return false;
			}
			FScriptArrayHelper Helper(Array, PropertyValue);
			if (!Helper.IsValidIndex(Index))
			{
				OutError = FString::Printf(TEXT("Array index %d is stale for %s."), Index, *PropertyName);
				return false;
			}
			return ResolveValue(Helper.GetRawPtr(Index), *Array->Inner, Remaining, Out, OutError);
		}
		if (bContainer)
		{
			if (FSetProperty* Set = CastField<FSetProperty>(Property))
			{
				FScriptSetHelper Helper(Set, PropertyValue);
				const int32 Physical = PhysicalIndexForLogical(Helper, Index);
				if (Physical == INDEX_NONE)
				{
					OutError = FString::Printf(TEXT("Set index %d is stale for %s."), Index, *PropertyName);
					return false;
				}
				Out.Containers.Add({Set, PropertyValue, Physical, true});
				return ResolveValue(Helper.GetElementPtr(Physical), *Set->ElementProp, Remaining, Out, OutError);
			}
			if (FMapProperty* Map = CastField<FMapProperty>(Property))
			{
				FScriptMapHelper Helper(Map, PropertyValue);
				const int32 Physical = PhysicalIndexForLogical(Helper, Index);
				FString Side;
				FString Nested;
				if (Physical == INDEX_NONE || !Remaining.Split(TEXT("."), &Side, &Nested))
				{
					Side = Remaining;
				}
				const bool bKey = Side == TEXT("Key");
				if (Physical == INDEX_NONE || (!bKey && Side != TEXT("Value")))
				{
					OutError = FString::Printf(TEXT("Map path is stale for %s."), *PropertyName);
					return false;
				}
				Out.Containers.Add({Map, PropertyValue, Physical, bKey});
				return ResolveValue(bKey ? Helper.GetKeyPtr(Physical) : Helper.GetValuePtr(Physical),
					bKey ? *Map->KeyProp : *Map->ValueProp, Nested, Out, OutError);
			}
			OutError = FString::Printf(TEXT("%s is not a set or map."), *PropertyName);
			return false;
		}
		return ResolveValue(PropertyValue, *Property, Remaining, Out, OutError);
	}

	bool ResolveValue(void* Value, FProperty& Property, const FString& Remaining, FResolvedRPGId& Out, FString& OutError)
	{
		FStructProperty* StructProperty = CastField<FStructProperty>(&Property);
		if (Remaining.IsEmpty() && StructProperty && StructProperty->Struct == FRPGId::StaticStruct())
		{
			Out.Value = static_cast<FRPGId*>(Value);
			return true;
		}
		if (Remaining.IsEmpty() || !StructProperty)
		{
			OutError = TEXT("The exact property path does not end at an FRPGId value.");
			return false;
		}
		return ResolveStructPath(Value, *StructProperty->Struct, Remaining, Out, OutError);
	}

	UObject* ResolveObject(const FString& ObjectPath)
	{
		const FSoftObjectPath Path(ObjectPath);
		return Path.ResolveObject() ? Path.ResolveObject() : Path.TryLoad();
	}

	bool ParseBracketValue(const FString& Text, const FString& Prefix, FString& OutValue, FString& OutRemaining)
	{
		if (!Text.StartsWith(Prefix))
		{
			return false;
		}
		const int32 Close = Text.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Prefix.Len());
		if (Close == INDEX_NONE)
		{
			return false;
		}
		OutValue = Text.Mid(Prefix.Len(), Close - Prefix.Len());
		OutRemaining = Text.Mid(Close + 1);
		if (OutRemaining.StartsWith(TEXT(".")))
		{
			OutRemaining.RightChopInline(1);
		}
		return !OutValue.IsEmpty();
	}

	bool ResolveGraphPin(UBlueprint& Blueprint, const FString& Path, UEdGraphPin*& OutPin, FString& OutError)
	{
		FString GraphName;
		FString Remaining;
		FString NodeName;
		FString PinName;
		if (!ParseBracketValue(Path, TEXT("Graph["), GraphName, Remaining)
			|| !ParseBracketValue(Remaining, TEXT("Node["), NodeName, Remaining)
			|| !ParseBracketValue(Remaining, TEXT("Pin["), PinName, Remaining) || !Remaining.IsEmpty())
		{
			OutError = TEXT("The Blueprint graph evidence path is malformed.");
			return false;
		}

		TArray<UEdGraph*> Graphs;
		Blueprint.GetAllGraphs(Graphs);
		UEdGraph** GraphEntry = Graphs.FindByPredicate([&GraphName](const UEdGraph* Candidate)
		{
			return IsValid(Candidate) && Candidate->GetName() == GraphName;
		});
		UEdGraph* Graph = GraphEntry ? *GraphEntry : nullptr;
		if (!Graph)
		{
			OutError = TEXT("The referenced Blueprint graph no longer exists.");
			return false;
		}
		const TObjectPtr<UEdGraphNode>* NodeEntry = Graph->Nodes.FindByPredicate([&NodeName](const UEdGraphNode* Candidate)
		{
			return IsValid(Candidate) && Candidate->GetName() == NodeName;
		});
		UEdGraphNode* Node = NodeEntry ? NodeEntry->Get() : nullptr;
		if (!Node)
		{
			OutError = TEXT("The referenced Blueprint node no longer exists.");
			return false;
		}

		FString RootPinName = PinName;
		const bool bSplit = RootPinName.RemoveFromEnd(TEXT(".Id"));
		UEdGraphPin** PinEntry = Node->Pins.FindByPredicate([&RootPinName](const UEdGraphPin* Candidate)
		{
			return Candidate && Candidate->PinName.ToString() == RootPinName;
		});
		UEdGraphPin* RootPin = PinEntry ? *PinEntry : nullptr;
		if (!RootPin)
		{
			OutError = TEXT("The referenced Blueprint pin no longer exists.");
			return false;
		}
		if (!bSplit)
		{
			OutPin = RootPin;
			return true;
		}

		TArray<UEdGraphPin*> Pending = RootPin->SubPins;
		while (!Pending.IsEmpty())
		{
			UEdGraphPin* Candidate = Pending.Pop(EAllowShrinking::No);
			if (!Candidate)
			{
				continue;
			}
			FString Leaf = Candidate->PinName.ToString();
			int32 Separator = INDEX_NONE;
			if (Leaf.FindLastChar(TEXT('_'), Separator))
			{
				Leaf.RightChopInline(Separator + 1);
			}
			if (Leaf == GET_MEMBER_NAME_STRING_CHECKED(FRPGId, Id))
			{
				OutPin = Candidate;
				return true;
			}
			Pending.Append(Candidate->SubPins);
		}
		OutError = TEXT("The referenced split RPGId pin no longer has an Id child.");
		return false;
	}

	bool ImportPinValue(const UEdGraphPin& Pin, FRPGId& OutValue)
	{
		if (Pin.PinType.PinCategory == UEdGraphSchema_K2::PC_Struct
			&& Pin.PinType.PinSubCategoryObject == FRPGId::StaticStruct())
		{
			const TCHAR* End = FRPGId::StaticStruct()->ImportText(*Pin.GetDefaultAsString(), &OutValue, nullptr, PPF_None, nullptr,
				FRPGId::StaticStruct()->GetName());
			return End && FString(End).TrimStartAndEnd().IsEmpty();
		}
		FName Name;
		const FProperty* IdProperty = FRPGId::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FRPGId, Id));
		const TCHAR* End = IdProperty ? IdProperty->ImportText_Direct(*Pin.GetDefaultAsString(), &Name, nullptr, PPF_None, nullptr) : nullptr;
		OutValue = FRPGId(Name);
		return End && FString(End).TrimStartAndEnd().IsEmpty();
	}

	FString ExportPinValue(const UEdGraphPin& Pin, const FRPGId& Value)
	{
		FString Text;
		if (Pin.PinType.PinCategory == UEdGraphSchema_K2::PC_Struct
			&& Pin.PinType.PinSubCategoryObject == FRPGId::StaticStruct())
		{
			FRPGId::StaticStruct()->ExportText(Text, &Value, nullptr, nullptr, PPF_None, nullptr);
		}
		else
		{
			const FProperty* IdProperty = FRPGId::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FRPGId, Id));
			IdProperty->ExportText_Direct(Text, &Value.Id, nullptr, nullptr, PPF_None);
		}
		return Text;
	}

	FString EditKey(const FRPGIdReferenceMigrationEdit& Edit)
	{
		return FString::Printf(TEXT("%d\n%s\n%s"), static_cast<int32>(Edit.Writer), *Edit.Source, *Edit.PropertyPath);
	}

	class FRPGIdReferenceMigrationProductionWorkspace final : public IRPGIdReferenceMigrationApplyWorkspace
	{
	public:
		explicit FRPGIdReferenceMigrationProductionWorkspace(FString InConfigFilenameOverride)
			: ConfigFilenameOverride(MoveTemp(InConfigFilenameOverride))
		{
		}

		virtual bool Begin(const FRPGIdReferenceMigrationPlan& Plan, FText& OutError) override;
		virtual bool ApplyEdit(const FRPGIdReferenceMigrationEdit& Edit, FText& OutError) override;
		virtual bool ApplyOwner(const FRPGIdReferenceMigrationRequest& Request, FText& OutError) override;
		virtual bool VerifyOldIdAbsent(const FRPGId& OldId, FText& OutError) override;
		virtual bool StageRedirect(const FRPGIdRedirect& Redirect, FText& OutError) override;
		virtual bool Commit(FText& OutError) override;
		virtual bool Rollback(FText& OutError) override;

	private:
		bool PrepareEdit(const FRPGIdReferenceMigrationEdit& Edit, FPreparedEdit& Out, FString& OutError);
		bool PrepareReflectionEdit(const FRPGIdReferenceMigrationEdit& Edit, UObject& Object, const FString& Path,
			UBlueprint* Blueprint, FPreparedEdit& Out, FString& OutError);
		bool PrepareLegacyEdit(const FRPGIdReferenceMigrationEdit& Edit, FPreparedEdit& Out, FString& OutError);
		bool ReplaceLegacy(FPreparedEdit& Prepared, const FRPGId& Value, FString& OutError);
		void RememberDirty(UPackage& Package);
		void RestoreValues();

		FRPGIdReferenceMigrationPlan ActivePlan;
		FString ConfigFilenameOverride;
		TMap<FString, FPreparedEdit> PreparedEdits;
		TMap<TWeakObjectPtr<UPackage>, bool> OriginalDirty;
		TSet<TWeakObjectPtr<UObject>> ModifiedObjects;
		TSet<TWeakObjectPtr<UBlueprint>> ModifiedBlueprints;
		TWeakObjectPtr<URPGPrimaryAsset> Owner;
		FRPGId OriginalOwnerId;
		FConfigFile ConfigBackup;
		FString ConfigFilename;
		FString ConfigFileBackup;
		bool bHasConfigBackup = false;
		bool bOwnerApplied = false;
		bool bConfigChanged = false;
		bool bHandoffStaged = false;
		bool bHandoffAdded = false;
		FRPGIdRedirect StagedRedirect;
		TStrongObjectPtr<URPGIdReferenceMigrationConfigUndo> ConfigUndo;
		TUniquePtr<FScopedTransaction> Transaction;
	};

	void FRPGIdReferenceMigrationProductionWorkspace::RememberDirty(UPackage& Package)
	{
		if (!OriginalDirty.Contains(&Package))
		{
			OriginalDirty.Add(&Package, Package.IsDirty());
		}
	}

	bool FRPGIdReferenceMigrationProductionWorkspace::PrepareReflectionEdit(const FRPGIdReferenceMigrationEdit& Edit,
		UObject& Object, const FString& Path, UBlueprint* Blueprint, FPreparedEdit& Out, FString& OutError)
	{
		Out.Object = &Object;
		Out.Blueprint = Blueprint;
		Out.bObjectWasTransactional = Object.HasAnyFlags(RF_Transactional);
		if (!ResolveStructPath(&Object, *Object.GetClass(), Path, Out.Resolved, OutError) || !Out.Resolved.Value)
		{
			return false;
		}
		if (*Out.Resolved.Value != Edit.ExpectedValue)
		{
			OutError = TEXT("The exact property no longer contains the expected old Id.");
			return false;
		}
		RememberDirty(*Object.GetOutermost());
		return true;
	}

	bool FRPGIdReferenceMigrationProductionWorkspace::PrepareLegacyEdit(const FRPGIdReferenceMigrationEdit& Edit,
		FPreparedEdit& Out, FString& OutError)
	{
		FString KeyText = Edit.PropertyPath;
		Out.bLegacyZoneContext = KeyText.StartsWith(TEXT("DefaultGameZoneContext"));
		int32 Open = INDEX_NONE;
		int32 Close = INDEX_NONE;
		if (KeyText.FindChar(TEXT('['), Open) && KeyText.FindChar(TEXT(']'), Close))
		{
			Out.LegacyIndex = FCString::Atoi(*KeyText.Mid(Open + 1, Close - Open - 1));
			KeyText.LeftInline(Open);
		}
		else
		{
			Out.LegacyIndex = Out.bLegacyZoneContext ? 0 : INDEX_NONE;
			KeyText.RemoveFromEnd(TEXT(".ZoneId"));
		}
		Out.LegacyKey = *KeyText;
		if (Out.LegacyIndex == INDEX_NONE)
		{
			OutError = TEXT("The legacy config evidence path has no exact index.");
			return false;
		}

		FConfigFile* Config = GConfig ? GConfig->FindConfigFile(ConfigFilename) : nullptr;
		const FConfigSection* Section = Config ? Config->FindSection(LegacySection) : nullptr;
		TArray<FString> Values;
		if (Section)
		{
			Section->MultiFind(Out.LegacyKey, Values, true);
		}
		if (!Values.IsValidIndex(Out.LegacyIndex) || !Values[Out.LegacyIndex].Contains(Edit.ExpectedValue.ToString()))
		{
			OutError = TEXT("The legacy config entry no longer contains the expected old Id.");
			return false;
		}
		return true;
	}

	bool FRPGIdReferenceMigrationProductionWorkspace::PrepareEdit(const FRPGIdReferenceMigrationEdit& Edit,
		FPreparedEdit& Out, FString& OutError)
	{
		Out.Edit = Edit;
		if (Edit.Writer == ERPGIdReferenceMigrationWriter::LegacyConfigProperty)
		{
			return PrepareLegacyEdit(Edit, Out, OutError);
		}
		if (Edit.Writer == ERPGIdReferenceMigrationWriter::TypedConfigProperty)
		{
			UObject* Settings = Edit.Source == TypedCharacterConfig ? static_cast<UObject*>(GetMutableDefault<UCharacterSystemSettings>())
				: Edit.Source == TypedGameZoneConfig ? static_cast<UObject*>(GetMutableDefault<UGameZoneSystemSettings>()) : nullptr;
			return Settings && PrepareReflectionEdit(Edit, *Settings, Edit.PropertyPath, nullptr, Out, OutError);
		}
		if (Edit.Source.StartsWith(BlueprintPrefix))
		{
			UBlueprint* Blueprint = Cast<UBlueprint>(ResolveObject(Edit.Source.RightChop(FCString::Strlen(BlueprintPrefix))));
			if (!Blueprint)
			{
				OutError = TEXT("The referenced Blueprint could not be loaded.");
				return false;
			}
			RememberDirty(*Blueprint->GetOutermost());
			if (Edit.Writer == ERPGIdReferenceMigrationWriter::BlueprintGraphLiteral)
			{
				UEdGraphPin* Pin = nullptr;
				FRPGId Current;
				if (!ResolveGraphPin(*Blueprint, Edit.PropertyPath, Pin, OutError) || !Pin || !ImportPinValue(*Pin, Current)
					|| Current != Edit.ExpectedValue)
				{
					OutError = OutError.IsEmpty() ? TEXT("The graph pin no longer contains the expected old Id.") : OutError;
					return false;
				}
				Out.Blueprint = Blueprint;
				Out.Pin = Pin;
				Out.PinNode = Pin->GetOwningNode();
				Out.OriginalPinValue = Pin->GetDefaultAsString();
				return true;
			}

			UObject* Target = nullptr;
			FString Path = Edit.PropertyPath;
			if (Edit.Writer == ERPGIdReferenceMigrationWriter::BlueprintDefaultProperty)
			{
				Path.RemoveFromStart(TEXT("ClassDefaultObject."));
				Target = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
			}
			else if (Edit.Writer == ERPGIdReferenceMigrationWriter::BlueprintTemplateProperty)
			{
				FString RelativePath;
				if (!ParseBracketValue(Path, TEXT("Template["), RelativePath, Path))
				{
					OutError = TEXT("The Blueprint template evidence path is malformed.");
					return false;
				}
				TArray<UObject*> NestedObjects;
				GetObjectsWithOuter(Blueprint, NestedObjects, EGetObjectsFlags::IncludeNestedObjects);
				if (Blueprint->GeneratedClass)
				{
					GetObjectsWithOuter(Blueprint->GeneratedClass, NestedObjects, EGetObjectsFlags::IncludeNestedObjects);
				}
				UObject** Match = NestedObjects.FindByPredicate([Blueprint, &RelativePath](const UObject* Candidate)
				{
					return IsValid(Candidate) && Candidate->GetPathName(Blueprint) == RelativePath;
				});
				Target = Match ? *Match : nullptr;
			}
			if (!Target)
			{
				OutError = TEXT("The referenced Blueprint default or template object no longer exists.");
				return false;
			}
			return PrepareReflectionEdit(Edit, *Target, Path, Blueprint, Out, OutError);
		}

		UObject* Object = ResolveObject(Edit.Source);
		if (!Object)
		{
			OutError = TEXT("The referenced native object could not be loaded.");
			return false;
		}
		return PrepareReflectionEdit(Edit, *Object, Edit.PropertyPath, nullptr, Out, OutError);
	}

	bool FRPGIdReferenceMigrationProductionWorkspace::Begin(const FRPGIdReferenceMigrationPlan& Plan, FText& OutError)
	{
		ActivePlan = Plan;
		ConfigFilename = ConfigFilenameOverride.IsEmpty()
			? FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / TEXT("DefaultIronicRPG.ini"))
			: FPaths::ConvertRelativePathToFull(ConfigFilenameOverride);
		if (Plan.RequiredArtifacts.ContainsByPredicate([](const FRPGIdReferenceMigrationArtifact& Artifact)
		{
			return Artifact.Kind == ERPGIdReferenceMigrationArtifactKind::ConfigFile;
		}))
		{
			FConfigFile* Config = GConfig ? GConfig->FindConfigFile(ConfigFilename) : nullptr;
			if (!Config || !FFileHelper::LoadFileToString(ConfigFileBackup, *ConfigFilename))
			{
				OutError = LOCTEXT("ConfigPreflight", "The project config could not be snapshotted before migration.");
				return false;
			}
			ConfigBackup = *Config;
			bHasConfigBackup = true;
			ConfigUndo.Reset(NewObject<URPGIdReferenceMigrationConfigUndo>(GetTransientPackage(), NAME_None, RF_Transactional));
			ConfigUndo->Initialize(ConfigFilename, ConfigFileBackup);
		}

		FString Error;
		for (const FRPGIdReferenceMigrationEdit& Edit : Plan.Edits)
		{
			FPreparedEdit Prepared;
			if (!PrepareEdit(Edit, Prepared, Error) || PreparedEdits.Contains(EditKey(Edit)))
			{
				OutError = FText::FromString(Error.IsEmpty() ? TEXT("The migration plan contains a duplicate writer target.") : Error);
				return false;
			}
			PreparedEdits.Add(EditKey(Edit), MoveTemp(Prepared));
		}

		Owner = Cast<URPGPrimaryAsset>(ResolveObject(Plan.ExpectedOwner.ToString()));
		if (!Owner.IsValid() || Owner->GetId() != Plan.OldId)
		{
			OutError = LOCTEXT("OwnerPreflight", "The expected owner no longer contains the old Id.");
			return false;
		}
		OriginalOwnerId = Owner->GetId();
		RememberDirty(*Owner->GetOutermost());
		Transaction = MakeUnique<FScopedTransaction>(LOCTEXT("Transaction", "Migrate RPG Id References"));
		return true;
	}

	bool FRPGIdReferenceMigrationProductionWorkspace::ReplaceLegacy(FPreparedEdit& Prepared, const FRPGId& Value,
		FString& OutError)
	{
		FConfigFile* Config = GConfig ? GConfig->FindConfigFile(ConfigFilename) : nullptr;
		const FConfigSection* Section = Config ? Config->FindSection(LegacySection) : nullptr;
		TArray<FString> Values;
		if (Section)
		{
			Section->MultiFind(Prepared.LegacyKey, Values, true);
		}
		if (!Section || !Values.IsValidIndex(Prepared.LegacyIndex))
		{
			OutError = TEXT("The legacy config entry changed during Apply.");
			return false;
		}

		FString Exported;
		FRPGId::StaticStruct()->ExportText(Exported, &Value, nullptr, nullptr, PPF_None, nullptr);
		if (Prepared.bLegacyZoneContext)
		{
			const FString OldText = Prepared.Edit.ExpectedValue.ToString();
			if (!Values[Prepared.LegacyIndex].ReplaceInline(*OldText, *Value.ToString(), ESearchCase::CaseSensitive))
			{
				OutError = TEXT("The legacy Zone context changed during Apply.");
				return false;
			}
		}
		else
		{
			Values[Prepared.LegacyIndex] = Exported;
		}
		Config->SetArray(LegacySection, *Prepared.LegacyKey.ToString(), Values);
		bConfigChanged = true;
		return true;
	}

	bool FRPGIdReferenceMigrationProductionWorkspace::ApplyEdit(const FRPGIdReferenceMigrationEdit& Edit, FText& OutError)
	{
		FPreparedEdit* Prepared = PreparedEdits.Find(EditKey(Edit));
		if (!Prepared)
		{
			OutError = LOCTEXT("MissingPrepared", "A certified writer was not prepared; migration stopped.");
			return false;
		}
		FString Error;
		if (Prepared->Edit.Writer == ERPGIdReferenceMigrationWriter::LegacyConfigProperty)
		{
			if (!ReplaceLegacy(*Prepared, Edit.ReplacementValue, Error))
			{
				OutError = FText::FromString(Error);
				return false;
			}
		}
		else if (Prepared->Pin && Prepared->PinNode.IsValid())
		{
			UBlueprint* Blueprint = Prepared->Blueprint.Get();
			UEdGraphPin* Pin = Prepared->Pin;
			Blueprint->Modify();
			Prepared->PinNode->SetFlags(RF_Transactional);
			Pin->Modify();
			if (!Pin->GetSchema())
			{
				OutError = LOCTEXT("PinWrite", "The Blueprint schema rejected the certified pin replacement.");
				return false;
			}
			Pin->GetSchema()->TrySetDefaultValue(*Pin, ExportPinValue(*Pin, Edit.ReplacementValue));
			FRPGId Written;
			if (!ImportPinValue(*Pin, Written) || Written != Edit.ReplacementValue)
			{
				OutError = LOCTEXT("PinWrite", "The Blueprint schema rejected the certified pin replacement.");
				Prepared->bApplied = true;
				return false;
			}
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
			ModifiedBlueprints.Add(Blueprint);
		}
		else if (Prepared->Object.IsValid() && Prepared->Resolved.Value)
		{
			UObject* Object = Prepared->Object.Get();
			Object->SetFlags(RF_Transactional);
			Object->Modify();
			*Prepared->Resolved.Value = Edit.ReplacementValue;
			for (const FContainerRehash& Container : Prepared->Resolved.Containers)
			{
				if (Container.HasCollision())
				{
					OutError = LOCTEXT("ContainerCollision", "The replacement creates a set element or map key collision.");
					Prepared->bApplied = true;
					return false;
				}
			}
			for (const FContainerRehash& Container : ReverseIterate(Prepared->Resolved.Containers))
			{
				Container.Rehash();
			}
			Object->MarkPackageDirty();
			ModifiedObjects.Add(Object);
			if (Prepared->Blueprint.IsValid())
			{
				FBlueprintEditorUtils::MarkBlueprintAsModified(Prepared->Blueprint.Get());
				ModifiedBlueprints.Add(Prepared->Blueprint);
			}
			if (Edit.Writer == ERPGIdReferenceMigrationWriter::TypedConfigProperty)
			{
				bConfigChanged = true;
			}
		}
		Prepared->bApplied = true;
		return true;
	}

	bool FRPGIdReferenceMigrationProductionWorkspace::ApplyOwner(const FRPGIdReferenceMigrationRequest& Request, FText& OutError)
	{
		if (!Owner.IsValid() || Owner->GetId() != Request.OldId)
		{
			OutError = LOCTEXT("OwnerChanged", "The owner changed during Apply.");
			return false;
		}
		URPGPrimaryAsset* Asset = Owner.Get();
		Asset->Modify();
		FProperty* IdProperty = FindFProperty<FProperty>(URPGPrimaryAsset::StaticClass(), TEXT("Id"));
		*IdProperty->ContainerPtrToValuePtr<FRPGId>(Asset) = Request.NewId;
		URPGAssetManager& Manager = URPGAssetManager::Get();
		Manager.RefreshAssetData(Asset);
		if (Manager.GetPrimaryAssetPath(Asset->GetPrimaryAssetId()) != FSoftObjectPath(Asset))
		{
			OutError = LOCTEXT("OwnerMapping", "AssetManager could not register the migrated owner Id.");
			bOwnerApplied = true;
			return false;
		}
		Asset->MarkPackageDirty();
		Asset->OnRPGAssetModified.Broadcast(Request.NewId);
		bOwnerApplied = true;
		return true;
	}

	bool FRPGIdReferenceMigrationProductionWorkspace::VerifyOldIdAbsent(const FRPGId& OldId, FText& OutError)
	{
		const FRPGIdReferenceAuditResult Audit = RPGIdReferencePrivate::AuditDevelopmentReferences(
			OldId, RPGIdReferencePrivate::ERPGIdAuditMode::Direct);
		if (!Audit.Hits.IsEmpty())
		{
			OutError = FText::Format(LOCTEXT("ReverseHits", "Reverse audit still found {0} old-Id reference(s)."),
				FText::AsNumber(Audit.Hits.Num()));
			return false;
		}
		for (const FText& Gap : Audit.CoverageGaps)
		{
			const FString Text = Gap.ToString();
			bool bOwnDirtyBlueprint = false;
			for (const TWeakObjectPtr<UBlueprint>& Blueprint : ModifiedBlueprints)
			{
				bOwnDirtyBlueprint |= Blueprint.IsValid() && Text.Contains(FSoftObjectPath(Blueprint.Get()).ToString())
					&& Text.Contains(TEXT("dirty"));
			}
			if (!bOwnDirtyBlueprint)
			{
				OutError = FText::Format(LOCTEXT("ReverseGap", "Reverse audit is incomplete: {0}"), Gap);
				return false;
			}
		}
		return true;
	}

	bool FRPGIdReferenceMigrationProductionWorkspace::StageRedirect(const FRPGIdRedirect& Redirect, FText& OutError)
	{
		if (!FRPGIdReferenceMigrationHandoff::Stage(Redirect, bHandoffAdded, OutError))
		{
			return false;
		}
		StagedRedirect = Redirect;
		bHandoffStaged = true;
		return true;
	}

	bool FRPGIdReferenceMigrationProductionWorkspace::Commit(FText& OutError)
	{
		if (bConfigChanged)
		{
			bool bSaved = true;
			if (ModifiedObjects.Contains(GetMutableDefault<UCharacterSystemSettings>()))
			{
				bSaved &= GetMutableDefault<UCharacterSystemSettings>()->TryUpdateDefaultConfigFile(ConfigFilename, false);
			}
			if (ModifiedObjects.Contains(GetMutableDefault<UGameZoneSystemSettings>()))
			{
				bSaved &= GetMutableDefault<UGameZoneSystemSettings>()->TryUpdateDefaultConfigFile(ConfigFilename, false);
			}
			if (GConfig)
			{
				GConfig->Flush(false, ConfigFilename);
			}
			if (!bSaved)
			{
				OutError = LOCTEXT("ConfigCommit", "The project config could not be committed.");
				return false;
			}
			FString CommittedContents;
			if (!ConfigUndo.IsValid() || !FFileHelper::LoadFileToString(CommittedContents, *ConfigFilename))
			{
				OutError = LOCTEXT("ConfigTransaction", "The committed config could not be attached to the Undo transaction.");
				return false;
			}
			ConfigUndo->SetCommittedContents(CommittedContents);
		}
		Transaction.Reset();
		return true;
	}

	void FRPGIdReferenceMigrationProductionWorkspace::RestoreValues()
	{
		for (TPair<FString, FPreparedEdit>& Pair : PreparedEdits)
		{
			FPreparedEdit& Prepared = Pair.Value;
			if (!Prepared.bApplied || Prepared.Edit.Writer == ERPGIdReferenceMigrationWriter::LegacyConfigProperty)
			{
				continue;
			}
			if (Prepared.Pin)
			{
				Prepared.Pin->DefaultValue = Prepared.OriginalPinValue;
			}
			else if (Prepared.Resolved.Value)
			{
				*Prepared.Resolved.Value = Prepared.Edit.ExpectedValue;
				for (const FContainerRehash& Container : ReverseIterate(Prepared.Resolved.Containers))
				{
					Container.Rehash();
				}
				if (!Prepared.bObjectWasTransactional && Prepared.Object.IsValid())
				{
					Prepared.Object->ClearFlags(RF_Transactional);
				}
			}
		}
		if (bOwnerApplied && Owner.IsValid())
		{
			FProperty* IdProperty = FindFProperty<FProperty>(URPGPrimaryAsset::StaticClass(), TEXT("Id"));
			*IdProperty->ContainerPtrToValuePtr<FRPGId>(Owner.Get()) = OriginalOwnerId;
			URPGAssetManager::Get().RefreshAssetData(Owner.Get());
			Owner->OnRPGAssetModified.Broadcast(OriginalOwnerId);
		}
	}

	bool FRPGIdReferenceMigrationProductionWorkspace::Rollback(FText& OutError)
	{
		FText HandoffError;
		const bool bHandoffRestored = !bHandoffStaged
			|| FRPGIdReferenceMigrationHandoff::RollbackStage(StagedRedirect, bHandoffAdded, HandoffError);
		RestoreValues();
		bool bConfigRestored = true;
		if (bHasConfigBackup && GConfig)
		{
			FConfigFile* Config = GConfig->FindConfigFile(ConfigFilename);
			if (Config)
			{
				*Config = ConfigBackup;
			}
			if (!FFileHelper::SaveStringToFile(ConfigFileBackup, *ConfigFilename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				bConfigRestored = false;
			}
		}
		for (const TPair<TWeakObjectPtr<UPackage>, bool>& Pair : OriginalDirty)
		{
			if (Pair.Key.IsValid())
			{
				Pair.Key->SetDirtyFlag(Pair.Value);
			}
		}
		if (Transaction)
		{
			Transaction->Cancel();
			Transaction.Reset();
		}
		if (!bHandoffRestored)
		{
			OutError = HandoffError;
		}
		else if (!bConfigRestored)
		{
			OutError = LOCTEXT("ConfigRollback", "The migration values were restored, but the config file backup could not be restored.");
		}
		return bHandoffRestored && bConfigRestored;
	}
}

TUniquePtr<IRPGIdReferenceMigrationApplyWorkspace> RPGIdReferenceMigrationPrivate::CreateProductionWorkspace(
	const FString& ConfigFilenameOverride)
{
	return MakeUnique<FRPGIdReferenceMigrationProductionWorkspace>(ConfigFilenameOverride);
}

#undef LOCTEXT_NAMESPACE
