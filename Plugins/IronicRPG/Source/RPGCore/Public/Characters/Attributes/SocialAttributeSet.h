// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Attributes/RPGAttributeSet.h"
#include "SocialAttributeSet.generated.h"

/**
 *
 */
UCLASS()
class RPGCORE_API USocialAttributeSet : public URPGAttributeSet
{
	GENERATED_BODY()

protected:
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

public:
	UPROPERTY(BlueprintReadOnly, Category = "Attributes | Favorability", meta = (SaveGame))
	FGameplayAttributeData Favorability = 0.0f;
	ATTRIBUTE_ACCESSORS(USocialAttributeSet, Favorability)

};

