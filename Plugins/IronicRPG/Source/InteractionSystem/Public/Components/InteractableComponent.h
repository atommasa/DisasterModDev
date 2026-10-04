// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionTypes.h"
#include "InteractableComponent.generated.h"

class UInteractionControlComponent;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnInteractionRequested,
	AActor*, Interactor,
	UInteractableComponent*, Interactable,
	FGameplayTag, ActionTag,
	FGuid, RequestId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTargetInteractionEnded, FGuid, RequestId, const FInteractionResult&, Result);

UCLASS(ClassGroup=(Interaction), Blueprintable, meta=(BlueprintSpawnableComponent))
class INTERACTIONSYSTEM_API UInteractableComponent : public UActorComponent
{
	GENERATED_BODY()
	friend class UInteractionControlComponent;

public:
	UInteractableComponent(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
public:
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

public:
	UFUNCTION(BlueprintPure, Category="RPG|Interaction")
	FGuid GetTargetId() const;

	UFUNCTION(BlueprintPure, Category="RPG|Interaction")
	bool IsInteractionEnabled() const;

	UFUNCTION(BlueprintCallable, Category="RPG|Interaction", meta=(AutoCreateRefTerm="ReasonText"))
	void SetInteractionEnabled(bool bEnabled, FGameplayTag ReasonTag, const FText& ReasonText);

	UFUNCTION(BlueprintCallable, Category="RPG|Interaction")
	bool CompleteInteraction(FGuid RequestId, const FInteractionResult& Result);

public:
	bool BuildFocusView(FInteractionFocusView& OutView) const;
	FVector ResolveInteractionAnchor() const;
	bool IsDetectionComponent(const UPrimitiveComponent* DetectionComponent) const;
	void DispatchInteraction(UInteractionControlComponent* Control, AActor* Interactor, FGuid RequestId);
	void ReleaseInteraction(FGuid RequestId, const FInteractionResult& Result);
	void NotifyTargetInvalidated();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	FInteractionDefinition Interaction;

	UPROPERTY(BlueprintAssignable, Category="RPG|Interaction")
	FOnInteractionRequested OnInteractionRequested;

	UPROPERTY(BlueprintAssignable, Category="RPG|Interaction")
	FOnTargetInteractionEnded OnInteractionEnded;

private:
	UFUNCTION()
	void OnDetectionBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnDetectionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

private:
	void EnsureTargetId() const;
	void InitializeRuntimeState();
	void BindDetectionComponents();
	void UnbindDetectionComponents();
	void NotifyOverlappingControlsChanged();
	void AbandonInteraction(FGuid RequestId);
	UActorComponent* ResolveComponentReference(const FComponentReference& Reference) const;
	TArray<UPrimitiveComponent*> ResolveDetectionComponents() const;

private:
	UPROPERTY(Transient)
	bool bInteractionEnabled = true;

	UPROPERTY(Transient)
	bool bRuntimeStateInitialized = false;

	UPROPERTY(Transient)
	FGameplayTag UnavailableReasonTag;

	UPROPERTY(Transient)
	FText UnavailableReasonText;

	UPROPERTY(Transient)
	mutable FGuid RuntimeTargetId;

	TSet<TWeakObjectPtr<UPrimitiveComponent>> BoundDetectionComponents;
	TSet<TWeakObjectPtr<UInteractionControlComponent>> ObservingControls;
	TMap<FGuid, TWeakObjectPtr<UInteractionControlComponent>> RequestOwners;
};
