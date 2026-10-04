// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/NarrativeNodeInfo.h"
#include "NarrativeBranchNodeInfo.generated.h"

UENUM(BlueprintType)
enum class EConditionImplementationType : uint8
{
	// Use UNarractiveCondition as condition source
	CIT_ConditionObject		UMETA(DisplayName = "Use Narractive Condition"),

	// Use custom blueprint function as condition source
	CIT_ConditionFunction	UMETA(DisplayName = "Use Custom BP Function"),
};

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew)
class NARRATIVESYSTEMRUNTIME_API UNarractiveCondition : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent)
	bool ExecuteCondition() const;
	virtual bool ExecuteCondition_Implementation() const { return false; }

	UFUNCTION(BlueprintNativeEvent)
	FText GetBranchNodeText(bool bInverse) const;
	virtual FText GetBranchNodeText_Implementation(bool bInverse) const { return FText(); }

};

/**
 * 
 */
UCLASS()
class NARRATIVESYSTEMRUNTIME_API UNarrativeBranchNodeInfo : public UNarrativeNodeInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Branch")
	EConditionImplementationType ImplementationType = EConditionImplementationType::CIT_ConditionObject;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Branch", meta=(EditCondition = "ImplementationType == EConditionImplementationType::CIT_ConditionObject", EditConditionHides))
	TObjectPtr<UNarractiveCondition> Condition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Branch", meta=(EditCondition = "ImplementationType == EConditionImplementationType::CIT_ConditionObject", EditConditionHides))
	bool bInverse = false;

public:
	UFUNCTION(BlueprintImplementableEvent)
	bool IsConditionMet() const;

};
