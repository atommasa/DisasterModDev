// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Attributes/RPGAttributeSet.h"
#include "LevelAttributeSet.generated.h"

/**
 * 
 */
UCLASS()
class RPGCORE_API ULevelAttributeSet : public URPGAttributeSet
{
	GENERATED_BODY()

protected:
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

public: // Level Attributes
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Level", meta=(SaveGame))
	FGameplayAttributeData Level = 1.0f;
	ATTRIBUTE_ACCESSORS(ULevelAttributeSet, Level)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Level", meta=(SaveGame, AttributeClampMax = "ExpToNextLevel"))
	FGameplayAttributeData CurrentExp = 1.0f;
	ATTRIBUTE_ACCESSORS(ULevelAttributeSet, CurrentExp)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Level", meta=(SaveGame))
	FGameplayAttributeData ExpToNextLevel = 1.0f;
	ATTRIBUTE_ACCESSORS(ULevelAttributeSet, ExpToNextLevel)

};
