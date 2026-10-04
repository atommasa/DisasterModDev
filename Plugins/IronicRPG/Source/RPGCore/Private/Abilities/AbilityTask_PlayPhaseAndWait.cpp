// Copyright Ironic Studio. All Rights Reserved.


#include "Abilities/AbilityTask_PlayPhaseAndWait.h"

#include "Characters/Components/CharacterAbilitySystemComponent.h"
#include "Abilities/RPGGameplayAbility.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Characters/BaseCharacter.h"

UAbilityTask_PlayPhaseAndWait* UAbilityTask_PlayPhaseAndWait::PlayPhaseAndWaitProxy(
	UGameplayAbility* OwningAbility,
	FName TaskInstanceName,
	int32 Index,
	FPhaseComboInputSettings InComboInputSettings)
{
	UAbilityTask_PlayPhaseAndWait* NewTask = NewAbilityTask<UAbilityTask_PlayPhaseAndWait>(
		OwningAbility,
		TaskInstanceName);

	NewTask->PhaseIndex = Index;
	NewTask->ComboSettings = MoveTemp(InComboInputSettings);

	return NewTask;
}

void UAbilityTask_PlayPhaseAndWait::Activate()
{
	Super::Activate();

	URPGGameplayAbility* RPGAbility = Cast<URPGGameplayAbility>(Ability);
	if (!RPGAbility || PhaseIndex < 0 || PhaseIndex >= RPGAbility->GetRequiredMontageCount())
	{
		FinishTask(EPhaseTaskResult::Cancelled, false);
		return;
	}

	const UAbilityAsset* AbilityAsset = RPGAbility->GetAbilityAsset();
	if (!AbilityAsset)
	{
		FinishTask(EPhaseTaskResult::Cancelled, false);
		return;
	}

	ABaseCharacter* Avatar = Cast<ABaseCharacter>(Ability->GetAvatarActorFromActorInfo());
	if (!Avatar)
	{
		FinishTask(EPhaseTaskResult::Cancelled, false);
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
		FinishTask(EPhaseTaskResult::Cancelled, false);
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
		OutputStruct.bAllowInterruptAfterBlendOut);

	if (!PlayMontageTask)
	{
		FinishTask(EPhaseTaskResult::Cancelled, false);
		return;
	}

	BindInnerTaskDelegates();

	// Register before the montage is activated. A notify near time zero must not
	// be missed because its Gameplay Event callback was registered too late.
	RegisterGameplayEventCallbacks();

	PlayMontageTask->ReadyForActivation();
}

void UAbilityTask_PlayPhaseAndWait::ExternalCancel()
{
	// Do not call Super::ExternalCancel(). The base implementation may EndTask()
	// before this task has disconnected all callbacks.
	FinishTask(EPhaseTaskResult::Cancelled, true);
}

FString UAbilityTask_PlayPhaseAndWait::GetDebugString() const
{
	FString Result = PlayMontageTask
		? PlayMontageTask->GetDebugString()
		: TEXT("No PlayMontageTask");

	if (ComboSettings.IsEnabled())
	{
		Result += FString::Printf(
			TEXT("\nComboWindow=%s BufferedInput=%s"),
			bComboWindowOpen ? TEXT("true") : TEXT("false"),
			*BufferedComboInputTag.ToString());
	}

	return Result;
}

void UAbilityTask_PlayPhaseAndWait::OnDestroy(bool AbilityEnded)
{
	// All callbacks/listeners must be disconnected first. In particular, an
	// input listener must never survive a phase that was interrupted or replaced.
	UnregisterComboInputListener();
	UnregisterGameplayEventCallbacks();
	UnbindInnerTaskDelegates();

	if (!bIsFinishing)
	{
		StopPlayingPhase();
	}

	PlayMontageTask = nullptr;

	Super::OnDestroy(AbilityEnded);
}

void UAbilityTask_PlayPhaseAndWait::FinishTask(EPhaseTaskResult Result, bool bStopInnerTask)
{
	if (bIsFinishing)
	{
		return;
	}

	bIsFinishing = true;

	// Must happen before ExternalCancel() on the inner montage task, otherwise
	// OnCancelled can re-enter this outer task and recurse into EndTask().
	UnregisterComboInputListener();
	UnregisterGameplayEventCallbacks();
	UnbindInnerTaskDelegates();

	if (bStopInnerTask)
	{
		StopPlayingPhase();
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		switch (Result)
		{
		case EPhaseTaskResult::Completed:
			OnCompleted.Broadcast({});
			break;

		case EPhaseTaskResult::Interrupted:
			OnInterrupted.Broadcast({});
			break;

		case EPhaseTaskResult::Cancelled:
			OnCancelled.Broadcast({});
			break;
		}
	}

	EndTask();
}

void UAbilityTask_PlayPhaseAndWait::BindInnerTaskDelegates()
{
	if (!PlayMontageTask || bInnerTaskDelegatesBound)
	{
		return;
	}

	PlayMontageTask->OnCompleted.AddDynamic(this, &UAbilityTask_PlayPhaseAndWait::HandleCompleted);
	PlayMontageTask->OnBlendOut.AddDynamic(this, &UAbilityTask_PlayPhaseAndWait::HandleBlendOut);
	PlayMontageTask->OnInterrupted.AddDynamic(this, &UAbilityTask_PlayPhaseAndWait::HandleInterrupted);
	PlayMontageTask->OnCancelled.AddDynamic(this, &UAbilityTask_PlayPhaseAndWait::HandleCancelled);

	bInnerTaskDelegatesBound = true;
}

void UAbilityTask_PlayPhaseAndWait::UnbindInnerTaskDelegates()
{
	if (!PlayMontageTask || !bInnerTaskDelegatesBound)
	{
		return;
	}

	PlayMontageTask->OnCompleted.RemoveDynamic(this, &UAbilityTask_PlayPhaseAndWait::HandleCompleted);
	PlayMontageTask->OnBlendOut.RemoveDynamic(this, &UAbilityTask_PlayPhaseAndWait::HandleBlendOut);
	PlayMontageTask->OnInterrupted.RemoveDynamic(this, &UAbilityTask_PlayPhaseAndWait::HandleInterrupted);
	PlayMontageTask->OnCancelled.RemoveDynamic(this, &UAbilityTask_PlayPhaseAndWait::HandleCancelled);

	bInnerTaskDelegatesBound = false;
}

void UAbilityTask_PlayPhaseAndWait::RegisterGameplayEventCallbacks()
{
	if (!ComboSettings.IsEnabled())
	{
		return;
	}

	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC)
	{
		return;
	}

	if (ComboSettings.ComboWindowOpenEventTag.IsValid())
	{
		FGameplayEventMulticastDelegate& Delegate =
			ASC->GenericGameplayEventCallbacks.FindOrAdd(ComboSettings.ComboWindowOpenEventTag);

		ComboWindowOpenEventHandle = Delegate.AddUObject(
			this,
			&UAbilityTask_PlayPhaseAndWait::HandleComboWindowOpened);
	}

	if (ComboSettings.ComboWindowCloseEventTag.IsValid())
	{
		FGameplayEventMulticastDelegate& Delegate =
			ASC->GenericGameplayEventCallbacks.FindOrAdd(ComboSettings.ComboWindowCloseEventTag);

		ComboWindowCloseEventHandle = Delegate.AddUObject(
			this,
			&UAbilityTask_PlayPhaseAndWait::HandleComboWindowClosed);
	}

	if (ComboSettings.ComboTransitionEventTag.IsValid())
	{
		FGameplayEventMulticastDelegate& Delegate =
			ASC->GenericGameplayEventCallbacks.FindOrAdd(ComboSettings.ComboTransitionEventTag);

		ComboTransitionEventHandle = Delegate.AddUObject(
			this,
			&UAbilityTask_PlayPhaseAndWait::HandleComboTransition);
	}
}

void UAbilityTask_PlayPhaseAndWait::UnregisterGameplayEventCallbacks()
{
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC)
	{
		return;
	}

	auto RemoveCallback = [ASC](const FGameplayTag& EventTag, FDelegateHandle& Handle)
	{
		if (!EventTag.IsValid() || !Handle.IsValid())
		{
			return;
		}

		if (FGameplayEventMulticastDelegate* Delegate =
			ASC->GenericGameplayEventCallbacks.Find(EventTag))
		{
			Delegate->Remove(Handle);
		}

		Handle.Reset();
	};

	RemoveCallback(ComboSettings.ComboWindowOpenEventTag, ComboWindowOpenEventHandle);
	RemoveCallback(ComboSettings.ComboWindowCloseEventTag, ComboWindowCloseEventHandle);
	RemoveCallback(ComboSettings.ComboTransitionEventTag, ComboTransitionEventHandle);
}

void UAbilityTask_PlayPhaseAndWait::RegisterComboInputListener()
{
	if (ComboInputListenerHandle.IsValid() || ComboSettings.AllowedInputTags.IsEmpty())
	{
		return;
	}

	URPGAbilitySystemComponent* RPGASC =
		Cast<URPGAbilitySystemComponent>(AbilitySystemComponent.Get());

	if (!RPGASC)
	{
		return;
	}

	ComboInputListenerHandle = RPGASC->RegisterInputTagListener(
		this,
		FInputTagListener::CreateUObject(
			this,
			&UAbilityTask_PlayPhaseAndWait::HandleComboInputTag),
		ComboSettings.InputListenerPriority);
}

void UAbilityTask_PlayPhaseAndWait::UnregisterComboInputListener()
{
	if (!ComboInputListenerHandle.IsValid())
	{
		return;
	}

	if (URPGAbilitySystemComponent* RPGASC =
		Cast<URPGAbilitySystemComponent>(AbilitySystemComponent.Get()))
	{
		RPGASC->UnregisterInputTagListener(ComboInputListenerHandle);
	}

	ComboInputListenerHandle.Reset();
}

bool UAbilityTask_PlayPhaseAndWait::StopPlayingPhase()
{
	if (!PlayMontageTask)
	{
		return false;
	}

	PlayMontageTask->ExternalCancel();
	return true;
}

EInputRouteResult UAbilityTask_PlayPhaseAndWait::HandleComboInputTag(
	const FGameplayTag& InputTag)
{
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Combo] Input Task=%p Phase=%d Tag=%s Window=%d Buffered=%d"),
		this,
		PhaseIndex,
		*InputTag.ToString(),
		bComboWindowOpen,
		bHasBufferedComboInput
	);

	if (bIsFinishing || !bComboWindowOpen)
	{
		return EInputRouteResult::PassThrough;
	}

	if (!ComboSettings.AllowedInputTags.HasTagExact(InputTag))
	{
		return ComboSettings.bBlockOtherInputsDuringComboWindow
			? EInputRouteResult::Blocked
			: EInputRouteResult::PassThrough;
	}

	// First valid input wins. Once it is buffered, this phase must no longer
	// intercept more input, otherwise later presses could overwrite intent.
	if (!bHasBufferedComboInput)
	{
		bHasBufferedComboInput = true;
		BufferedComboInputTag = InputTag;
		UnregisterComboInputListener();
	}

	return EInputRouteResult::Consumed;
}

void UAbilityTask_PlayPhaseAndWait::HandleComboWindowOpened(
	const FGameplayEventData* Payload)
{
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Combo] WindowOpen Task=%p Phase=%d"),
		this,
		PhaseIndex
	);

	if (bIsFinishing || bComboWindowOpen)
	{
		return;
	}

	bComboWindowOpen = true;
	bHasBufferedComboInput = false;
	BufferedComboInputTag = FGameplayTag();

	RegisterComboInputListener();
}

void UAbilityTask_PlayPhaseAndWait::HandleComboWindowClosed(
	const FGameplayEventData* Payload)
{
	if (bIsFinishing)
	{
		return;
	}

	bComboWindowOpen = false;
	UnregisterComboInputListener();
}

void UAbilityTask_PlayPhaseAndWait::HandleComboTransition(const FGameplayEventData* Payload)
{
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Combo] Transition Task=%p Phase=%d Buffered=%s"),
		this,
		PhaseIndex,
		*BufferedComboInputTag.ToString()
	);

	if (bIsFinishing)
	{
		return;
	}

	bComboWindowOpen = false;
	UnregisterComboInputListener();

	if (ShouldBroadcastAbilityTaskDelegates() && BufferedComboInputTag.IsValid())
	{
		// Intentionally broadcasts an invalid tag when no valid input was
		// buffered. This gives Blueprint one uniform branch point.
		OnComboTransition.Broadcast(BufferedComboInputTag);
	}
}

void UAbilityTask_PlayPhaseAndWait::HandleCompleted()
{
	FinishTask(EPhaseTaskResult::Completed, false);
}

void UAbilityTask_PlayPhaseAndWait::HandleBlendOut()
{
	if (bIsFinishing || bHasBroadcastBlendOut)
	{
		return;
	}

	bHasBroadcastBlendOut = true;

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnBlendOut.Broadcast({});
	}
}

void UAbilityTask_PlayPhaseAndWait::HandleInterrupted()
{
	FinishTask(EPhaseTaskResult::Interrupted, false);
}

void UAbilityTask_PlayPhaseAndWait::HandleCancelled()
{
	FinishTask(EPhaseTaskResult::Cancelled, false);
}
