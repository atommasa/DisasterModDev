// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/NarrativeNodeInfo.h"
#include "StructUtils/PropertyBag.h"
#include "NarrativeSetVariablesNodeInfo.generated.h"

class UDialogueBlueprint;

USTRUCT(BlueprintType)
struct NARRATIVESYSTEMRUNTIME_API FNarrativeVariableAssignment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Variable", meta = (GetOptions = "GetBlueprintVariableOptions"))
	FName VariableName;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid VariableGuid;
#endif

	UPROPERTY(EditAnywhere, Category = "Variable", meta = (FixedLayout, ShowOnlyInnerProperties))
	mutable FInstancedPropertyBag Value;

	FString ToString() const;
};

class FNarrativeSetVariablesNodeInfoHelper
{
public:
	static constexpr const TCHAR* ValuePropertyName = TEXT("Value");

	static bool CopyBagValueToObjectProperty(
		const FInstancedPropertyBag& Bag,
		FName BagPropertyName,
		UObject* TargetObject,
		FName TargetPropertyName
	);

#if WITH_EDITOR
	static bool RebuildBagFromProperty(
		FInstancedPropertyBag& Bag,
		const FProperty* SourceProperty
	);

	static bool RebuildBagFromPropertyIfNeeded(
		FInstancedPropertyBag& Bag,
		const FProperty* SourceProperty
	);

	static FProperty* FindDialogueVariableProperty(
		const UDialogueBlueprint* DialogueBP,
		FName VariableName
	);

	static const FBPVariableDescription* FindBPVariableByName(
		const UDialogueBlueprint* DialogueBP,
		FName VariableName
	);

	static const FBPVariableDescription* FindBPVariableByGuid(
		const UDialogueBlueprint* DialogueBP,
		const FGuid& VariableGuid
	);
#endif
};

UCLASS()
class NARRATIVESYSTEMRUNTIME_API UNarrativeSetVariablesNodeInfo : public UNarrativeNodeInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Variables")
	TArray<FNarrativeVariableAssignment> VariableAssignments;

public:
	bool ApplyVariablesToObject(UObject* TargetObject) const;

#if WITH_EDITOR
public:
	bool RefreshVariableAssignmentsFromBlueprint(bool bModifyIfChanged, bool bPeferName);

protected:
	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent) override;

	UFUNCTION(CallInEditor)
	TArray<FString> GetBlueprintVariableOptions() const;

	void OnDialogueBlueprintChanged(UBlueprint* Blueprint);

#endif // WITH_EDITOR

protected:
	bool TryGetDialogueBlueprint() const;

protected:
	mutable TWeakObjectPtr<UDialogueBlueprint> DialogueBP;
};