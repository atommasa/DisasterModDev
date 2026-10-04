// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionTypes.h"
#include "InteractionControlComponent.generated.h"

class ABaseCharacter;
class ARPGPlayerController;
class UInputAction;
class UInteractableComponent;
class UPrimitiveComponent;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionViewChanged, const FInteractionViewSnapshot&, Snapshot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionCompleted, FGuid, RequestId, const FInteractionResult&, Result);

UCLASS(ClassGroup=(ControlComponents), meta=(BlueprintSpawnableComponent))
class INTERACTIONSYSTEM_API UInteractionControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionControlComponent(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	UFUNCTION(BlueprintCallable, Category="RPG|Interaction")
	bool BeginFocusedInteraction();

	UFUNCTION(BlueprintCallable, Category="RPG|Interaction")
	void EndFocusedInteractionInput();

	UFUNCTION(BlueprintCallable, Category="RPG|Interaction")
	bool CancelActiveInteraction();

	UFUNCTION(BlueprintPure, Category="RPG|Interaction")
	const FInteractionViewSnapshot& GetInteractionView() const { return CurrentView; }

public:
	void NotifyInteractionOverlap(
		UInteractableComponent* Interactable,
		UPrimitiveComponent* DetectionComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		bool bBegan);
	void NotifyInteractableChanged(UInteractableComponent* Interactable);
	bool CompleteInteractionFromTarget(UInteractableComponent* Interactable, FGuid RequestId, const FInteractionResult& Result);
	void InvalidateTarget(UInteractableComponent* Interactable);

public:
	UPROPERTY(BlueprintAssignable, Category="RPG|Interaction")
	FOnInteractionViewChanged OnInteractionViewChanged;

	UPROPERTY(BlueprintAssignable, Category="RPG|Interaction")
	FOnInteractionCompleted OnInteractionCompleted;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> CancelInteractionAction;

	UPROPERTY(EditDefaultsOnly, Category="Interaction|Focus", meta=(ClampMin="0.0", ClampMax="180.0", Units="deg"))
	float FocusHalfAngleDegrees = 60.0f;

	UPROPERTY(EditDefaultsOnly, Category="Interaction|Focus", meta=(ClampMin="0.0", Units="cm"))
	float FocusSwitchDistanceBias = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category="Interaction|Focus", meta=(ClampMin="0.01", Units="s"))
	float FocusRefreshInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category="Interaction|Detection")
	TEnumAsByte<ECollisionChannel> LineOfSightTraceChannel = ECC_GameTraceChannel1;

private:
	struct FOverlapRecord
	{
		TWeakObjectPtr<UInteractableComponent> Interactable;
		TWeakObjectPtr<UPrimitiveComponent> DetectionComponent;
		TWeakObjectPtr<UPrimitiveComponent> OtherComponent;
	};

	struct FResolvedFocus
	{
		TWeakObjectPtr<UInteractableComponent> Interactable;
		FInteractionFocusView View;
		float Distance = 0.0f;
		float Alignment = -1.0f;
	};

	struct FActiveRequest
	{
		FGuid RequestId;
		FGuid TargetId;
		TWeakObjectPtr<UInteractableComponent> Interactable;
		float ElapsedSeconds = 0.0f;
		float TimeoutSeconds = 0.0f;
		bool bPlayerCancelable = false;
		bool bCancelWhenEligibilityLost = false;
		bool bCancelOnGameplayControlLost = false;
	};

	void RefreshFocusView();
	void PublishView();
	void UpdateComponentTickEnabled();
	void ReconcileControlledInteractor();
	void ClearCharge();
	bool DispatchFocusedInteraction();
	bool ResolveFocusForTarget(UInteractableComponent& Interactable, AActor& Interactor, FResolvedFocus& OutFocus) const;
	bool HasLineOfSight(const UInteractableComponent& Interactable) const;
	AActor* GetControlledInteractor() const;
	UInteractableComponent* ResolveFocusedTarget() const;
	void FinishActiveRequest(const FInteractionResult& Result);

	UFUNCTION()
	void HandleInteractStarted(const FInputActionValue& Value);

	UFUNCTION()
	void HandleInteractCompleted(const FInputActionValue& Value);

	UFUNCTION()
	void HandleCancelInteraction(const FInputActionValue& Value);

	UFUNCTION()
	void HandleControlledCharacterChanged(const ABaseCharacter* NewCharacter, const ABaseCharacter* OldCharacter);

	UFUNCTION()
	void HandleControlModeChanged(const int32& NewControlMode);

private:
	UPROPERTY(Transient)
	TObjectPtr<ARPGPlayerController> Controller;

	UPROPERTY(Transient)
	FInteractionViewSnapshot CurrentView;

	FInteractionViewSnapshot LastPublishedView;
	TArray<FOverlapRecord> OverlapRecords;
	TOptional<FActiveRequest> ActiveRequest;
	FGuid ChargingTargetId;
	float ChargeElapsedSeconds = 0.0f;
	float FocusRefreshElapsedSeconds = 0.0f;
	bool bGameplayControlActive = true;
};
