// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_PlayPhaseAndWait.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPhaseWaitSimpleDelegate);

/**
 * 
 */
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

	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName = "PlayPhaseAndWait",
		HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_PlayPhaseAndWait* CreatePlayPhaseAndWaitProxy(
		UGameplayAbility* OwningAbility, FName TaskInstanceName, int32 Index = 0);

	virtual void Activate() override;

	virtual void ExternalCancel() override;

	virtual FString GetDebugString() const override;

protected:
	virtual void OnDestroy(bool AbilityEnded) override;

private:
	UFUNCTION()
	void HandleCompleted();

	UFUNCTION()
	void HandleBlendOut();

	UFUNCTION()
	void HandleInterrupted();

	UFUNCTION()
	void HandleCancelled();

	bool StopPlayingPhase();

private:
	FDelegateHandle BlendedInHandle;

	UPROPERTY()
	TObjectPtr<class UAbilityTask_PlayMontageAndWait> PlayMontageTask;

	UPROPERTY()
	int32 PhaseIndex;
	
};
