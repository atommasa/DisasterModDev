// Copyright Ironic Studio. All Rights Reserved.

#include "Nodes/NarrativeSetVariablesNodeInfo.h"
#include "NarrativeAsset.h"

#include "UObject/UnrealType.h"

#define NVA_TYPE_NAME_TEXT_CHECKED(Type) \
	((void)sizeof(Type), TEXT(#Type))

FString FNarrativeVariableAssignment::ToString() const
{
	if (!Value.IsValid())
	{
		return TEXT("Invalid PropertyBag");
	}

	const FPropertyBagPropertyDesc* Desc = Value.FindPropertyDescByName(TEXT("Value"));
	
	if (!Desc || !Desc->CachedProperty)
	{
		return FString::Printf(
			TEXT("Property '%s' not found"),
			TEXT("Value")
		);
	}

	const uint8* BagMemory = Value.GetValue().GetMemory();
	if (!BagMemory)
	{
		return TEXT("No Bag Memory");
	}

	const FProperty* Property = Desc->CachedProperty;
	const void* ValuePtr = BagMemory + Property->GetOffset_ForInternal();

	FString ValueString;
	Property->ExportTextItem_Direct(
		ValueString,
		ValuePtr,
		nullptr,
		nullptr,
		PPF_None
	);

	const FString TypeStr = Property->GetCPPType();
	if (TypeStr == NVA_TYPE_NAME_TEXT_CHECKED(FString) || TypeStr == NVA_TYPE_NAME_TEXT_CHECKED(FName))
	{
		if (!TypeStr.IsEmpty())
		{
			ValueString = FString::Printf(TEXT("\"%s\""), *ValueString);
		}
	}
	else if (TypeStr == NVA_TYPE_NAME_TEXT_CHECKED(FText))
	{
		auto ExtractLastQuotedString = [](const FString& In) -> FString
			{
				int32 EndQuoteIndex = INDEX_NONE;
				if (!In.FindLastChar(TEXT('"'), EndQuoteIndex))
				{
					return FString();
				}

				FString BeforeEndQuote = In.Left(EndQuoteIndex);

				int32 StartQuoteIndex = INDEX_NONE;
				if (!BeforeEndQuote.FindLastChar(TEXT('"'), StartQuoteIndex))
				{
					return FString();
				}

				return In.Mid(StartQuoteIndex + 1, EndQuoteIndex - StartQuoteIndex - 1);
			};

		ValueString = FString::Printf(TEXT("\"%s\""), *ExtractLastQuotedString(ValueString));
	}

	return ValueString;
}

bool FNarrativeSetVariablesNodeInfoHelper::CopyBagValueToObjectProperty(
	const FInstancedPropertyBag& Bag,
	FName BagPropertyName,
	UObject* TargetObject,
	FName TargetPropertyName
)
{
	if (!TargetObject || !Bag.IsValid())
	{
		return false;
	}

	FProperty* TargetProperty = FindFProperty<FProperty>(
		TargetObject->GetClass(),
		TargetPropertyName
	);

	if (!TargetProperty)
	{
		return false;
	}

	const FPropertyBagPropertyDesc* Desc = Bag.FindPropertyDescByName(BagPropertyName);
	if (!Desc || !Desc->CachedProperty)
	{
		return false;
	}

	const FProperty* SourceProperty = Desc->CachedProperty;
	if (!SourceProperty->SameType(TargetProperty))
	{
		return false;
	}

	const uint8* SourcePtr =
		Bag.GetValue().GetMemory() + SourceProperty->GetOffset_ForInternal();

	void* TargetPtr = TargetProperty->ContainerPtrToValuePtr<void>(TargetObject);

	TargetProperty->CopyCompleteValue(TargetPtr, SourcePtr);
	return true;
}

#if WITH_EDITOR

bool FNarrativeSetVariablesNodeInfoHelper::RebuildBagFromProperty(
	FInstancedPropertyBag& Bag,
	const FProperty* SourceProperty
)
{
	if (!SourceProperty)
	{
		const bool bWasValid = Bag.IsValid();
		Bag.Reset();
		return bWasValid;
	}

	Bag.Reset();

	Bag.AddProperty(ValuePropertyName, SourceProperty);

	return true;
}

bool FNarrativeSetVariablesNodeInfoHelper::RebuildBagFromPropertyIfNeeded(
	FInstancedPropertyBag& Bag,
	const FProperty* SourceProperty
)
{
	if (!SourceProperty)
	{
		const bool bWasValid = Bag.IsValid();
		Bag.Reset();
		return bWasValid;
	}

	const FPropertyBagPropertyDesc* Desc =
		Bag.FindPropertyDescByName(ValuePropertyName);

	const FProperty* CachedProperty =
		Desc ? Desc->CachedProperty : nullptr;

	if (CachedProperty && CachedProperty->SameType(SourceProperty))
	{
		return false;
	}

	Bag.Reset();
	Bag.AddProperty(ValuePropertyName, SourceProperty);

	return true;
}

FProperty* FNarrativeSetVariablesNodeInfoHelper::FindDialogueVariableProperty(
	const UDialogueBlueprint* DialogueBP,
	FName VariableName
)
{
	if (!DialogueBP || VariableName.IsNone())
	{
		return nullptr;
	}

	// Editor 查詢優先用 SkeletonGeneratedClass，因為剛改 Blueprint 變數時通常它較即時
	if (UClass* SkeletonClass = DialogueBP->SkeletonGeneratedClass)
	{
		if (FProperty* Property = FindFProperty<FProperty>(SkeletonClass, VariableName))
		{
			return Property;
		}
	}

	if (UClass* GeneratedClass = DialogueBP->GeneratedClass)
	{
		if (FProperty* Property = FindFProperty<FProperty>(GeneratedClass, VariableName))
		{
			return Property;
		}
	}

	return nullptr;
}

const FBPVariableDescription* FNarrativeSetVariablesNodeInfoHelper::FindBPVariableByName(
	const UDialogueBlueprint* DialogueBP,
	FName VariableName
)
{
	if (!DialogueBP || VariableName.IsNone())
	{
		return nullptr;
	}

	for (const FBPVariableDescription& VarDesc : DialogueBP->NewVariables)
	{
		if (VarDesc.VarName == VariableName)
		{
			return &VarDesc;
		}
	}

	return nullptr;
}

const FBPVariableDescription* FNarrativeSetVariablesNodeInfoHelper::FindBPVariableByGuid(
	const UDialogueBlueprint* DialogueBP,
	const FGuid& VariableGuid
)
{
	if (!DialogueBP || !VariableGuid.IsValid())
	{
		return nullptr;
	}

	for (const FBPVariableDescription& VarDesc : DialogueBP->NewVariables)
	{
		if (VarDesc.VarGuid == VariableGuid)
		{
			return &VarDesc;
		}
	}

	return nullptr;
}

#endif // WITH_EDITOR

bool UNarrativeSetVariablesNodeInfo::ApplyVariablesToObject(UObject* TargetObject) const
{
	if (!TargetObject)
	{
		return false;
	}

	bool bAppliedAll = true;

	for (const FNarrativeVariableAssignment& Assignment : VariableAssignments)
	{
		if (Assignment.VariableName.IsNone())
		{
			continue;
		}

		const bool bApplied =
			FNarrativeSetVariablesNodeInfoHelper::CopyBagValueToObjectProperty(
				Assignment.Value,
				FName(FNarrativeSetVariablesNodeInfoHelper::ValuePropertyName),
				TargetObject,
				Assignment.VariableName
			);

		bAppliedAll &= bApplied;
	}

	return bAppliedAll;
}

#if WITH_EDITOR
void UNarrativeSetVariablesNodeInfo::PostInitProperties()
{
	Super::PostInitProperties();

	if (TryGetDialogueBlueprint() && !DialogueBP->OnChanged().IsBoundToObject(this))
	{
		DialogueBP->OnChanged().AddUObject(this, &UNarrativeSetVariablesNodeInfo::OnDialogueBlueprintChanged);
	}
}

void UNarrativeSetVariablesNodeInfo::BeginDestroy()
{
	if (TryGetDialogueBlueprint())
	{
		DialogueBP->OnChanged().RemoveAll(this);
	}

	Super::BeginDestroy();
}

void UNarrativeSetVariablesNodeInfo::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent)
{
	const FName MemberPropertyName =
		PropertyChangedChainEvent.MemberProperty
		? PropertyChangedChainEvent.MemberProperty->GetFName()
		: NAME_None;

	const FName ChangedPropertyName =
		PropertyChangedChainEvent.Property
		? PropertyChangedChainEvent.Property->GetFName()
		: NAME_None;

	const bool bAssignmentsChanged =
		MemberPropertyName == GET_MEMBER_NAME_CHECKED(
			UNarrativeSetVariablesNodeInfo,
			VariableAssignments
		);

	const bool bVariableNameChanged =
		ChangedPropertyName == GET_MEMBER_NAME_CHECKED(
			FNarrativeVariableAssignment,
			VariableName
		);

	if (!bAssignmentsChanged && !bVariableNameChanged)
	{
		Super::PostEditChangeChainProperty(PropertyChangedChainEvent);
		return;
	}

	RefreshVariableAssignmentsFromBlueprint(/*bModifyIfChanged=*/true, /*bPeferName=*/bVariableNameChanged);

	Super::PostEditChangeChainProperty(PropertyChangedChainEvent);
}

TArray<FString> UNarrativeSetVariablesNodeInfo::GetBlueprintVariableOptions() const
{
	TArray<FString> Options;
	Options.Add(TEXT("None"));

	if (!TryGetDialogueBlueprint())
	{
		return Options;
	}

	TSet<FName> UsedNames;
	for (const FNarrativeVariableAssignment& Assignment : VariableAssignments)
	{
		if (!Assignment.VariableName.IsNone())
		{
			UsedNames.Add(Assignment.VariableName);
		}
	}

	for (const FBPVariableDescription& VarDesc : DialogueBP->NewVariables)
	{
		if (UsedNames.Contains(VarDesc.VarName))
		{
			continue;
		}

		// TODO: 目前只支援非 Container 變數，因為 Container 變數的 Value Bag 還沒做好
		if (VarDesc.VarType.ContainerType != EPinContainerType::None)
		{
			continue;
		}

		Options.Add(VarDesc.VarName.ToString());
	}

	return Options;
}

bool UNarrativeSetVariablesNodeInfo::RefreshVariableAssignmentsFromBlueprint(bool bModifyIfChanged, bool bReferName)
{
	if (!TryGetDialogueBlueprint())
	{
		return false;
	}

	bool bChanged = false;

	auto ModifyOnce = [&]()
		{
			if (bModifyIfChanged && !bChanged)
			{
				Modify();
			}

			bChanged = true;
		};

	for (FNarrativeVariableAssignment& Assignment : VariableAssignments)
	{
		if (Assignment.VariableName.IsNone())
		{
			if (Assignment.Value.IsValid())
			{
				ModifyOnce();
				Assignment.Value.Reset();
			}

#if WITH_EDITORONLY_DATA
			if (Assignment.VariableGuid.IsValid())
			{
				ModifyOnce();
				Assignment.VariableGuid.Invalidate();
			}
#endif

			continue;
		}

		const FBPVariableDescription* VarDesc = nullptr;

		if (bReferName)
		{
			VarDesc = FNarrativeSetVariablesNodeInfoHelper::FindBPVariableByName(
				DialogueBP.Get(),
				Assignment.VariableName
			);

#if WITH_EDITORONLY_DATA
			if (VarDesc && VarDesc->VarGuid.IsValid() && Assignment.VariableGuid != VarDesc->VarGuid)
			{
				ModifyOnce();
				Assignment.VariableGuid = VarDesc->VarGuid;
			}
#endif
		}
		else
		{
#if WITH_EDITORONLY_DATA
			if (Assignment.VariableGuid.IsValid())
			{
				VarDesc = FNarrativeSetVariablesNodeInfoHelper::FindBPVariableByGuid(
					DialogueBP.Get(),
					Assignment.VariableGuid
				);
			}
#endif

			if (!VarDesc)
			{
				VarDesc = FNarrativeSetVariablesNodeInfoHelper::FindBPVariableByName(
					DialogueBP.Get(),
					Assignment.VariableName
				);

#if WITH_EDITORONLY_DATA
				if (VarDesc && VarDesc->VarGuid.IsValid() && Assignment.VariableGuid != VarDesc->VarGuid)
				{
					ModifyOnce();
					Assignment.VariableGuid = VarDesc->VarGuid;
				}
#endif
			}
		}

		if (!VarDesc)
		{
			if (Assignment.Value.IsValid())
			{
				ModifyOnce();
				Assignment.Value.Reset();
			}

			continue;
		}

		if (Assignment.VariableName != VarDesc->VarName)
		{
			ModifyOnce();
			Assignment.VariableName = VarDesc->VarName;
		}

		FProperty* FoundProperty =
			FNarrativeSetVariablesNodeInfoHelper::FindDialogueVariableProperty(
				DialogueBP.Get(),
				VarDesc->VarName
			);

		if (!FoundProperty)
		{
			if (Assignment.Value.IsValid())
			{
				ModifyOnce();
				Assignment.Value.Reset();
			}

			continue;
		}

		if (FNarrativeSetVariablesNodeInfoHelper::RebuildBagFromPropertyIfNeeded(Assignment.Value, FoundProperty))
		{
			ModifyOnce();
		}
	}

	return bChanged;
}

void UNarrativeSetVariablesNodeInfo::OnDialogueBlueprintChanged(UBlueprint* Blueprint)
{
	if (!TryGetDialogueBlueprint())
	{
		return;
	}

	if (Blueprint != DialogueBP.Get())
	{
		return;
	}

	const bool bChanged = RefreshVariableAssignmentsFromBlueprint(/*bModifyIfChanged=*/true, /*bReferName=*/false);

	if (!bChanged)
	{
		return;
	}

	Modify();
}

#endif // WITH_EDITOR

bool UNarrativeSetVariablesNodeInfo::TryGetDialogueBlueprint() const
{
	if (DialogueBP.IsValid())
	{
		return true;
	}

#if WITH_EDITOR
	DialogueBP = Cast<UDialogueBlueprint>(GetOutermostObject());
#endif

	return DialogueBP.IsValid();
}
