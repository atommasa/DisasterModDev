// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Characters/Components/CharacterAbilitySystemComponent.h"
#include "AbilityTask_WaitComboInput.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitComboInputDelegate, FGameplayTag, InputTag);

/**
 * 
 */
UCLASS()
class RPGCORE_API UAbilityTask_WaitComboInput : public UAbilityTask
{
	GENERATED_BODY()
	
public:
    UPROPERTY(BlueprintAssignable)
    FWaitComboInputDelegate OnInputReceived;

    UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta=(HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
    static UAbilityTask_WaitComboInput* WaitComboInput(
        UGameplayAbility* OwningAbility,
        FGameplayTagContainer AllowedInputTags,
        bool bBlockOtherInputs = true,
        bool bEndTaskOnInputReceived = true,
        int32 Priority = 0
    );

    virtual void Activate() override;

    virtual void OnDestroy(bool AbilityEnded) override;

private:
    EInputRouteResult HandleInputTag(const FGameplayTag& InputTag);

private:
    FGameplayTagContainer AllowedInputTags;

    bool bBlockOtherInputs = true;

    bool bEndTaskOnInputReceived = true;

    int32 Priority = 100;

    bool bFinished = false;

    FDelegateHandle InputListenerHandle;
};
