// Copyright Ironic Studio. All Rights Reserved.


#include "Abilities/AbilityTask_WaitComboInput.h"

UAbilityTask_WaitComboInput* UAbilityTask_WaitComboInput::WaitComboInput(UGameplayAbility* OwningAbility, FGameplayTagContainer InAllowedInputTags, bool bInBlockOtherInputs, bool bInEndTaskOnInputReceived, int32 InPriority)
{
    UAbilityTask_WaitComboInput* Task = NewAbilityTask<UAbilityTask_WaitComboInput>(OwningAbility);

    Task->AllowedInputTags = MoveTemp(InAllowedInputTags);
    Task->bBlockOtherInputs = bInBlockOtherInputs;
    Task->bEndTaskOnInputReceived = bInEndTaskOnInputReceived;
    Task->Priority = InPriority;

    return Task;
}

void UAbilityTask_WaitComboInput::Activate()
{
    Super::Activate();

    URPGAbilitySystemComponent* RPGASC =
        Cast<URPGAbilitySystemComponent>(AbilitySystemComponent.Get());

    if (!RPGASC)
    {
        EndTask();
        return;
    }

    InputListenerHandle = RPGASC->RegisterInputTagListener(
        this,
        FInputTagListener::CreateUObject(
            this,
            &UAbilityTask_WaitComboInput::HandleInputTag
        ),
        Priority
    );
}

void UAbilityTask_WaitComboInput::OnDestroy(bool AbilityEnded)
{
    URPGAbilitySystemComponent* RPGASC = Cast<URPGAbilitySystemComponent>(AbilitySystemComponent.Get());

    if (RPGASC && InputListenerHandle.IsValid())
    {
        RPGASC->UnregisterInputTagListener(InputListenerHandle);
        InputListenerHandle.Reset();
    }

    Super::OnDestroy(AbilityEnded);
}

EInputRouteResult UAbilityTask_WaitComboInput::HandleInputTag(const FGameplayTag & InputTag)
{
    if (bFinished)
    {
        return EInputRouteResult::PassThrough;
    }

    if (!AllowedInputTags.HasTagExact(InputTag))
    {
        return bBlockOtherInputs
            ? EInputRouteResult::Blocked
            : EInputRouteResult::PassThrough;
    }

    bFinished = true;

    if (ShouldBroadcastAbilityTaskDelegates())
    {
        OnInputReceived.Broadcast(InputTag);
    }

    if (bEndTaskOnInputReceived)
    {
        EndTask();
    }

    return EInputRouteResult::Consumed;
}
