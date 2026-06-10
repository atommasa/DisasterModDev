#include "Abilities/AbilityTask_PlayPhaseAndWait.h"
#include "Abilities/RPGGameplayAbility.h"
#include "Characters/BaseCharacter.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"

UAbilityTask_PlayPhaseAndWait* UAbilityTask_PlayPhaseAndWait::CreatePlayPhaseAndWaitProxy(
	UGameplayAbility* OwningAbility, FName TaskInstanceName, int32 Index)
{
	UAbilityTask_PlayPhaseAndWait* NewTask = NewAbilityTask<UAbilityTask_PlayPhaseAndWait>(OwningAbility, TaskInstanceName);
	NewTask->PhaseIndex = Index;

	return NewTask;
}

void UAbilityTask_PlayPhaseAndWait::Activate()
{
	Super::Activate();

	URPGGameplayAbility* RPGAbility = Cast<URPGGameplayAbility>(Ability);
	if (!RPGAbility || PhaseIndex < 0 || PhaseIndex >= RPGAbility->GetRequiredMontageCount())
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast();
		}

		EndTask();
		return;
	}

	const UAbilityAsset* AbilityAsset = RPGAbility->GetAbilityAsset();
	if (!AbilityAsset)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast();
		}

		EndTask();
		return;
	}

	ABaseCharacter* Avatar = Cast<ABaseCharacter>(Ability->GetAvatarActorFromActorInfo());
	if (!Avatar)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast();
		}

		EndTask();
		return;
	}

	FCharacterAnimInput InputStruct;
	InputStruct.AnimTagContainer = AbilityAsset->GetRequireAnimTagContainers().IsValidIndex(PhaseIndex)
		? AbilityAsset->GetRequireAnimTagContainers()[PhaseIndex]
		: FGameplayTagContainer::EmptyContainer;

	FCharacterAnimEntry OutputStruct;
	Avatar->GetCharacterAnimEntry(InputStruct, OutputStruct);

	if (!OutputStruct.IsValid() || !OutputStruct.Montage.IsValid())
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast();
		}

		EndTask();
		return;
	}

	PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		Ability,
		FName(FString::Printf(TEXT("PlayPhaseAndWait_MontageTask_%d"), PhaseIndex)),
		OutputStruct.Montage.Get(),
		OutputStruct.PlayRate,
		OutputStruct.StartSection,
		OutputStruct.bStopWhenAbilityEnds,
		OutputStruct.AnimRootMotionTranslationScale,
		OutputStruct.StartTimeSeconds,
		OutputStruct.bAllowInterruptAfterBlendOut
	);

	if (!PlayMontageTask)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast();
		}

		EndTask();
		return;
	}

	// Bind inner task events to our handlers
	PlayMontageTask->OnCompleted.AddDynamic(this, &UAbilityTask_PlayPhaseAndWait::HandleCompleted);
	PlayMontageTask->OnBlendOut.AddDynamic(this, &UAbilityTask_PlayPhaseAndWait::HandleBlendOut);
	PlayMontageTask->OnInterrupted.AddDynamic(this, &UAbilityTask_PlayPhaseAndWait::HandleInterrupted);
	PlayMontageTask->OnCancelled.AddDynamic(this, &UAbilityTask_PlayPhaseAndWait::HandleCancelled);

	PlayMontageTask->ReadyForActivation();
}

void UAbilityTask_PlayPhaseAndWait::ExternalCancel()
{
	StopPlayingPhase();

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnCancelled.Broadcast();
	}

	Super::ExternalCancel();

	EndTask();
}

FString UAbilityTask_PlayPhaseAndWait::GetDebugString() const
{
	return PlayMontageTask ? PlayMontageTask->GetDebugString() : TEXT("No PlayMontageTask");
}

void UAbilityTask_PlayPhaseAndWait::OnDestroy(bool AbilityEnded)
{
	StopPlayingPhase();

	PlayMontageTask = nullptr;

	Super::OnDestroy(AbilityEnded);
}

bool UAbilityTask_PlayPhaseAndWait::StopPlayingPhase()
{
	// Stop the inner montage task if it's active
	if (PlayMontageTask)
	{
		PlayMontageTask->ExternalCancel();
		return true;
	}

	return Ability != nullptr;
}

// ---- inner task event forwarding ----

void UAbilityTask_PlayPhaseAndWait::HandleCompleted()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnCompleted.Broadcast();
	}
	EndTask();
}

void UAbilityTask_PlayPhaseAndWait::HandleBlendOut()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnBlendOut.Broadcast();
	}
}

void UAbilityTask_PlayPhaseAndWait::HandleInterrupted()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnInterrupted.Broadcast();
	}

	EndTask();
}

void UAbilityTask_PlayPhaseAndWait::HandleCancelled()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnCancelled.Broadcast();
	}

	EndTask();
}
