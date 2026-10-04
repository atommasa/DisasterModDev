// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayTagContainer.h"
#include "AbilityTask_PlayPhaseAndWait.generated.h"

// Kept forward-declared here to avoid making every includer of this task pull in the custom ASC header.
enum class EInputRouteResult : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPhaseWaitSimpleDelegate, FGameplayTag, BufferedInputTag);

/**
 * Per-phase combo-input behavior.
 *
 * The three event tags are expected to be sent by montage notifies:
 * - ComboWindowOpenEventTag: start accepting combo input.
 * - ComboWindowCloseEventTag: stop accepting combo input.
 * - ComboTransitionEventTag: ask the owning ability to resolve the buffered input.
 *
 * If no event tags are supplied, this phase behaves exactly like a normal
 * PlayMontageAndWait wrapper and never installs an input listener.
 */
USTRUCT(BlueprintType)
struct RPGCORE_API FPhaseComboInputSettings
{
	GENERATED_BODY()

	/** Gameplay event fired by the montage when this phase starts accepting combo input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo")
	FGameplayTag ComboWindowOpenEventTag;

	/** Gameplay event fired by the montage when this phase stops accepting combo input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo")
	FGameplayTag ComboWindowCloseEventTag;

	/** Gameplay event fired by the montage at the phase's hand-off / branch point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo")
	FGameplayTag ComboTransitionEventTag;

	/** Inputs this phase is allowed to buffer while its combo window is open. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo", meta=(Categories = "Ability.Input"))
	FGameplayTagContainer AllowedInputTags;

	/**
	 * When true, inputs not present in AllowedInputTags are blocked while the
	 * combo window is open instead of activating their normally equipped ability.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo")
	bool bBlockOtherInputsDuringComboWindow = true;

	/**
	 * Higher values are consulted first by the ASC input router. Keep combo
	 * windows above ordinary listeners; 100 is a sensible default.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo")
	int32 InputListenerPriority = 100;

	bool IsEnabled() const
	{
		return ComboWindowOpenEventTag.IsValid()
			|| ComboWindowCloseEventTag.IsValid()
			|| ComboTransitionEventTag.IsValid();
	}
};

UCLASS()
class RPGCORE_API UAbilityTask_PlayPhaseAndWait : public UAbilityTask
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FPhaseWaitSimpleDelegate OnCompleted;

	UPROPERTY(BlueprintAssignable)
	FPhaseWaitSimpleDelegate OnBlendOut;

	UPROPERTY(BlueprintAssignable)
	FPhaseWaitSimpleDelegate OnInterrupted;

	UPROPERTY(BlueprintAssignable)
	FPhaseWaitSimpleDelegate OnCancelled;

	/**
	 * Fired only when ComboTransitionEventTag is received.
	 *
	 * BufferedInputTag is invalid when the player did not provide a valid input
	 * during this phase's combo window.
	 */
	UPROPERTY(BlueprintAssignable)
	FPhaseWaitSimpleDelegate OnComboTransition;

	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta=(HidePin = "OwningAbility", DefaultToSelf = "OwningAbility",BlueprintInternalUseOnly = "true"))
	static UAbilityTask_PlayPhaseAndWait* PlayPhaseAndWaitProxy(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		int32 Index = 0,
		FPhaseComboInputSettings ComboInputSettings = FPhaseComboInputSettings()
	);

	virtual void Activate() override;
	virtual void ExternalCancel() override;
	virtual FString GetDebugString() const override;

protected:
	virtual void OnDestroy(bool AbilityEnded) override;

private:
	enum class EPhaseTaskResult : uint8
	{
		Completed,
		Interrupted,
		Cancelled
	};

	void FinishTask(EPhaseTaskResult Result, bool bStopInnerTask);

	void BindInnerTaskDelegates();
	void UnbindInnerTaskDelegates();

	void RegisterGameplayEventCallbacks();
	void UnregisterGameplayEventCallbacks();

	void RegisterComboInputListener();
	void UnregisterComboInputListener();

	bool StopPlayingPhase();

	/** Called by URPGAbilitySystemComponent's input router. */
	EInputRouteResult HandleComboInputTag(const FGameplayTag& InputTag);

	void HandleComboWindowOpened(const FGameplayEventData* Payload);
	void HandleComboWindowClosed(const FGameplayEventData* Payload);
	void HandleComboTransition(const FGameplayEventData* Payload);

	UFUNCTION()
	void HandleCompleted();

	UFUNCTION()
	void HandleBlendOut();

	UFUNCTION()
	void HandleInterrupted();

	UFUNCTION()
	void HandleCancelled();

private:
	UPROPERTY()
	TObjectPtr<class UAbilityTask_PlayMontageAndWait> PlayMontageTask;

	UPROPERTY()
	int32 PhaseIndex = INDEX_NONE;

	UPROPERTY()
	FPhaseComboInputSettings ComboSettings;

	/** Becomes true only between the configured Combo Window Open / Close events. */
	bool bComboWindowOpen = false;

	/** First valid input wins for one combo window. */
	bool bHasBufferedComboInput = false;

	UPROPERTY()
	FGameplayTag BufferedComboInputTag;

	/** Prevent terminal callbacks from re-entering while EndTask() destroys this task. */
	bool bIsFinishing = false;

	/** Prevent duplicate OnBlendOut broadcasts if the inner task reports it more than once. */
	bool bHasBroadcastBlendOut = false;

	/** Tracks whether our callbacks are still registered on the inner montage task. */
	bool bInnerTaskDelegatesBound = false;

	/** Handles for GenericGameplayEventCallbacks on the owning ASC. */
	FDelegateHandle ComboWindowOpenEventHandle;
	FDelegateHandle ComboWindowCloseEventHandle;
	FDelegateHandle ComboTransitionEventHandle;

	/** Handle returned by URPGAbilitySystemComponent::RegisterInputTagListener. */
	FDelegateHandle ComboInputListenerHandle;
};
