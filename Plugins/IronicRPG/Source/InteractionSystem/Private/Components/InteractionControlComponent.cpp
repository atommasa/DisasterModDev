// Copyright Ironic Studio. All Rights Reserved.

#include "Components/InteractionControlComponent.h"

#include "Characters/BaseCharacter.h"
#include "Components/InteractableComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Controllers/RPGPlayerController.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputActionValue.h"

DEFINE_LOG_CATEGORY_STATIC(LogInteractionControl, Log, All);

namespace
{
	bool AreFocusViewsEquivalent(const FInteractionFocusView& Left, const FInteractionFocusView& Right)
	{
		return Left.TargetId == Right.TargetId
			&& Left.ActionTag == Right.ActionTag
			&& Left.DisplayName.EqualTo(Right.DisplayName)
			&& Left.Icon == Right.Icon
			&& Left.bEnabled == Right.bEnabled
			&& Left.UnavailableReasonTag == Right.UnavailableReasonTag
			&& Left.UnavailableReasonText.EqualTo(Right.UnavailableReasonText)
			&& Left.ActivationPolicy.Mode == Right.ActivationPolicy.Mode
			&& FMath::IsNearlyEqual(Left.ActivationPolicy.HoldDuration, Right.ActivationPolicy.HoldDuration)
			&& Left.ActivationState == Right.ActivationState
			&& FMath::IsNearlyEqual(Left.ActivationProgress, Right.ActivationProgress);
	}

	bool AreSnapshotsEquivalent(const FInteractionViewSnapshot& Left, const FInteractionViewSnapshot& Right)
	{
		return Left.bHasFocusedInteraction == Right.bHasFocusedInteraction
			&& AreFocusViewsEquivalent(Left.FocusedInteraction, Right.FocusedInteraction)
			&& Left.ActiveRequestId == Right.ActiveRequestId;
	}
}

UInteractionControlComponent::UInteractionControlComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UInteractionControlComponent::BeginPlay()
{
	Super::BeginPlay();
	Controller = Cast<ARPGPlayerController>(GetOwner());
	if (Controller)
	{
		Controller->OnControlledCharacterChanged.AddUniqueDynamic(this, &UInteractionControlComponent::HandleControlledCharacterChanged);
		Controller->OnControlModeChanged.AddUniqueDynamic(this, &UInteractionControlComponent::HandleControlModeChanged);
		bGameplayControlActive = EnumHasAnyFlags(Controller->GetControlMode(), ERPGControlMode::Gameplay);

		if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(Controller->InputComponent))
		{
			if (InteractAction)
			{
				EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &UInteractionControlComponent::HandleInteractStarted);
				EnhancedInput->BindAction(InteractAction, ETriggerEvent::Completed, this, &UInteractionControlComponent::HandleInteractCompleted);
				EnhancedInput->BindAction(InteractAction, ETriggerEvent::Canceled, this, &UInteractionControlComponent::HandleInteractCompleted);
			}
			if (CancelInteractionAction)
			{
				EnhancedInput->BindAction(CancelInteractionAction, ETriggerEvent::Started, this, &UInteractionControlComponent::HandleCancelInteraction);
			}
		}
	}
	ReconcileControlledInteractor();
	RefreshFocusView();
}

void UInteractionControlComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Controller)
	{
		Controller->OnControlledCharacterChanged.RemoveDynamic(this, &UInteractionControlComponent::HandleControlledCharacterChanged);
		Controller->OnControlModeChanged.RemoveDynamic(this, &UInteractionControlComponent::HandleControlModeChanged);
	}
	if (ActiveRequest.IsSet() && ActiveRequest->Interactable.IsValid())
	{
		ActiveRequest->Interactable->AbandonInteraction(ActiveRequest->RequestId);
	}
	Controller = nullptr;
	OverlapRecords.Reset();
	ActiveRequest.Reset();
	ClearCharge();
	CurrentView = {};
	LastPublishedView = {};
	Super::EndPlay(EndPlayReason);
}

void UInteractionControlComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	FocusRefreshElapsedSeconds += DeltaTime;
	if (!OverlapRecords.IsEmpty() && FocusRefreshElapsedSeconds >= FocusRefreshInterval)
	{
		FocusRefreshElapsedSeconds = 0.0f;
		RefreshFocusView();
	}

	if (ChargingTargetId.IsValid())
	{
		if (!CurrentView.bHasFocusedInteraction || CurrentView.FocusedInteraction.TargetId != ChargingTargetId || !CurrentView.FocusedInteraction.bEnabled)
		{
			ClearCharge();
			PublishView();
		}
		else
		{
			ChargeElapsedSeconds += DeltaTime;
			const float RequiredSeconds = FMath::Max(CurrentView.FocusedInteraction.ActivationPolicy.HoldDuration, KINDA_SMALL_NUMBER);
			CurrentView.FocusedInteraction.ActivationState = EInteractionActivationState::Charging;
			CurrentView.FocusedInteraction.ActivationProgress = FMath::Clamp(ChargeElapsedSeconds / RequiredSeconds, 0.0f, 1.0f);
			PublishView();
			if (ChargeElapsedSeconds >= RequiredSeconds)
			{
				ClearCharge();
				DispatchFocusedInteraction();
			}
		}
	}

	if (ActiveRequest.IsSet())
	{
		FActiveRequest& Request = ActiveRequest.GetValue();
		Request.ElapsedSeconds += DeltaTime;
		if (Request.TimeoutSeconds > 0.0f && Request.ElapsedSeconds >= Request.TimeoutSeconds)
		{
			FInteractionResult Result;
			Result.Code = EInteractionResultCode::TimedOut;
			FinishActiveRequest(Result);
		}
		else if (Request.bCancelWhenEligibilityLost)
		{
			AActor* Interactor = GetControlledInteractor();
			FResolvedFocus Focus;
			const bool bInRange = OverlapRecords.ContainsByPredicate([&Request](const FOverlapRecord& Record)
			{
				return Record.Interactable == Request.Interactable && Record.OtherComponent.IsValid();
			});
			if (!bInRange || !Interactor || !Request.Interactable.IsValid()
				|| !ResolveFocusForTarget(*Request.Interactable, *Interactor, Focus)
				|| !Focus.View.bEnabled)
			{
				FInteractionResult Result;
				Result.Code = EInteractionResultCode::Canceled;
				FinishActiveRequest(Result);
			}
		}
	}
}

bool UInteractionControlComponent::BeginFocusedInteraction()
{
	if (!bGameplayControlActive || ActiveRequest.IsSet() || ChargingTargetId.IsValid())
	{
		return false;
	}
	RefreshFocusView();
	if (!CurrentView.bHasFocusedInteraction || !CurrentView.FocusedInteraction.bEnabled)
	{
		return false;
	}
	if (CurrentView.FocusedInteraction.ActivationPolicy.Mode == EInteractionActivationMode::Hold)
	{
		ChargingTargetId = CurrentView.FocusedInteraction.TargetId;
		ChargeElapsedSeconds = 0.0f;
		CurrentView.FocusedInteraction.ActivationState = EInteractionActivationState::Charging;
		PublishView();
		UpdateComponentTickEnabled();
		return true;
	}
	return DispatchFocusedInteraction();
}

void UInteractionControlComponent::EndFocusedInteractionInput()
{
	if (ChargingTargetId.IsValid())
	{
		ClearCharge();
		PublishView();
		UpdateComponentTickEnabled();
	}
}

bool UInteractionControlComponent::CancelActiveInteraction()
{
	if (!ActiveRequest.IsSet() || !ActiveRequest->bPlayerCancelable)
	{
		return false;
	}
	FInteractionResult Result;
	Result.Code = EInteractionResultCode::Canceled;
	FinishActiveRequest(Result);
	return true;
}

void UInteractionControlComponent::NotifyInteractionOverlap(
	UInteractableComponent* Interactable,
	UPrimitiveComponent* DetectionComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	bool bBegan)
{
	if (!IsValid(Interactable) || !IsValid(OtherActor) || !IsValid(OtherComponent))
	{
		return;
	}
	AActor* ControlledInteractor = GetControlledInteractor();
	if (ControlledInteractor && ControlledInteractor != OtherActor)
	{
		return;
	}
	Interactable->ObservingControls.Add(this);
	auto Matches = [Interactable, DetectionComponent, OtherComponent](const FOverlapRecord& Record)
	{
		return Record.Interactable == Interactable && Record.DetectionComponent == DetectionComponent && Record.OtherComponent == OtherComponent;
	};
	if (bBegan && !OverlapRecords.ContainsByPredicate(Matches))
	{
		FOverlapRecord& Record = OverlapRecords.AddDefaulted_GetRef();
		Record.Interactable = Interactable;
		Record.DetectionComponent = DetectionComponent;
		Record.OtherComponent = OtherComponent;
	}
	else if (!bBegan)
	{
		OverlapRecords.RemoveAll(Matches);
	}
	RefreshFocusView();
	UpdateComponentTickEnabled();
}

void UInteractionControlComponent::NotifyInteractableChanged(UInteractableComponent* Interactable)
{
	if ((ActiveRequest.IsSet() && ActiveRequest->Interactable == Interactable)
		|| OverlapRecords.ContainsByPredicate([Interactable](const FOverlapRecord& Record) { return Record.Interactable == Interactable; }))
	{
		RefreshFocusView();
	}
}

bool UInteractionControlComponent::CompleteInteractionFromTarget(
	UInteractableComponent* Interactable,
	FGuid RequestId,
	const FInteractionResult& Result)
{
	if (!ActiveRequest.IsSet() || ActiveRequest->RequestId != RequestId || ActiveRequest->Interactable != Interactable)
	{
		UE_LOG(LogInteractionControl, Verbose, TEXT("Ignored stale interaction result %s."), *RequestId.ToString());
		return false;
	}
	FinishActiveRequest(Result);
	return true;
}

void UInteractionControlComponent::InvalidateTarget(UInteractableComponent* Interactable)
{
	OverlapRecords.RemoveAll([Interactable](const FOverlapRecord& Record) { return Record.Interactable == Interactable; });
	if (ActiveRequest.IsSet() && ActiveRequest->Interactable == Interactable)
	{
		FInteractionResult Result;
		Result.Code = EInteractionResultCode::TargetInvalidated;
		FinishActiveRequest(Result);
	}
	if (Interactable && ChargingTargetId == Interactable->GetTargetId())
	{
		ClearCharge();
	}
	RefreshFocusView();
	UpdateComponentTickEnabled();
}

void UInteractionControlComponent::RefreshFocusView()
{
	if (!bGameplayControlActive)
	{
		return;
	}
	if (ActiveRequest.IsSet() && ActiveRequest->Interactable.IsValid())
	{
		FInteractionFocusView PendingView;
		if (!ActiveRequest->Interactable->BuildFocusView(PendingView) && CurrentView.FocusedInteraction.TargetId == ActiveRequest->TargetId)
		{
			PendingView = CurrentView.FocusedInteraction;
		}
		PendingView.ActivationState = EInteractionActivationState::Pending;
		PendingView.ActivationProgress = 0.0f;
		CurrentView.bHasFocusedInteraction = PendingView.TargetId.IsValid();
		CurrentView.FocusedInteraction = MoveTemp(PendingView);
		PublishView();
		return;
	}

	AActor* Interactor = GetControlledInteractor();
	TMap<FGuid, FResolvedFocus> Resolved;
	for (auto It = OverlapRecords.CreateIterator(); It; ++It)
	{
		UInteractableComponent* Interactable = It->Interactable.Get();
		if (!IsValid(Interactable) || !It->OtherComponent.IsValid())
		{
			It.RemoveCurrent();
			continue;
		}
		if (!Interactor)
		{
			Interactor = It->OtherComponent->GetOwner();
		}
		if (!Interactor || Resolved.Contains(Interactable->GetTargetId()))
		{
			continue;
		}
		FResolvedFocus Focus;
		if (ResolveFocusForTarget(*Interactable, *Interactor, Focus))
		{
			Resolved.Add(Focus.View.TargetId, MoveTemp(Focus));
		}
	}

	TArray<FResolvedFocus> Focuses;
	Resolved.GenerateValueArray(Focuses);
	Focuses.Sort([](const FResolvedFocus& Left, const FResolvedFocus& Right)
	{
		if (!FMath::IsNearlyEqual(Left.Distance, Right.Distance))
		{
			return Left.Distance < Right.Distance;
		}
		if (!FMath::IsNearlyEqual(Left.Alignment, Right.Alignment))
		{
			return Left.Alignment > Right.Alignment;
		}
		return Left.View.TargetId.ToString() < Right.View.TargetId.ToString();
	});

	const FResolvedFocus* Chosen = Focuses.IsEmpty() ? nullptr : &Focuses[0];
	if (Chosen && CurrentView.bHasFocusedInteraction && Chosen->View.TargetId != CurrentView.FocusedInteraction.TargetId)
	{
		const FResolvedFocus* Current = Focuses.FindByPredicate([this](const FResolvedFocus& Focus)
		{
			return Focus.View.TargetId == CurrentView.FocusedInteraction.TargetId;
		});
		if (Current && Chosen->Distance + FocusSwitchDistanceBias >= Current->Distance)
		{
			Chosen = Current;
		}
	}

	if (!Chosen)
	{
		ClearCharge();
		CurrentView.bHasFocusedInteraction = false;
		CurrentView.FocusedInteraction = {};
		PublishView();
		return;
	}

	FInteractionFocusView NewView = Chosen->View;
	if (CurrentView.bHasFocusedInteraction && CurrentView.FocusedInteraction.TargetId == NewView.TargetId)
	{
		NewView.ActivationState = CurrentView.FocusedInteraction.ActivationState;
		NewView.ActivationProgress = CurrentView.FocusedInteraction.ActivationProgress;
	}
	else
	{
		ClearCharge();
	}
	CurrentView.bHasFocusedInteraction = true;
	CurrentView.FocusedInteraction = MoveTemp(NewView);
	PublishView();
}

void UInteractionControlComponent::PublishView()
{
	CurrentView.ActiveRequestId = ActiveRequest.IsSet() ? ActiveRequest->RequestId : FGuid();
	if (AreSnapshotsEquivalent(CurrentView, LastPublishedView))
	{
		return;
	}
	++CurrentView.Revision;
	LastPublishedView = CurrentView;
	OnInteractionViewChanged.Broadcast(CurrentView);
}

void UInteractionControlComponent::UpdateComponentTickEnabled()
{
	SetComponentTickEnabled(!OverlapRecords.IsEmpty() || ChargingTargetId.IsValid() || ActiveRequest.IsSet());
}

void UInteractionControlComponent::ReconcileControlledInteractor()
{
	AActor* Interactor = GetControlledInteractor();
	if (!Interactor)
	{
		return;
	}
	TArray<UPrimitiveComponent*> InteractorPrimitives;
	Interactor->GetComponents(InteractorPrimitives);
	for (UPrimitiveComponent* InteractorPrimitive : InteractorPrimitives)
	{
		TArray<UPrimitiveComponent*> Overlapping;
		InteractorPrimitive->GetOverlappingComponents(Overlapping);
		for (UPrimitiveComponent* DetectionPrimitive : Overlapping)
		{
			UInteractableComponent* Interactable = DetectionPrimitive && DetectionPrimitive->GetOwner()
				? DetectionPrimitive->GetOwner()->FindComponentByClass<UInteractableComponent>() : nullptr;
			if (Interactable && Interactable->IsDetectionComponent(DetectionPrimitive))
			{
				NotifyInteractionOverlap(Interactable, DetectionPrimitive, Interactor, InteractorPrimitive, true);
			}
		}
	}
}

void UInteractionControlComponent::ClearCharge()
{
	ChargingTargetId.Invalidate();
	ChargeElapsedSeconds = 0.0f;
	if (CurrentView.FocusedInteraction.ActivationState == EInteractionActivationState::Charging)
	{
		CurrentView.FocusedInteraction.ActivationState = EInteractionActivationState::Idle;
		CurrentView.FocusedInteraction.ActivationProgress = 0.0f;
	}
}

bool UInteractionControlComponent::DispatchFocusedInteraction()
{
	UInteractableComponent* Target = ResolveFocusedTarget();
	if (!CurrentView.bHasFocusedInteraction || !CurrentView.FocusedInteraction.bEnabled || !Target || ActiveRequest.IsSet())
	{
		return false;
	}
	const FInteractionDefinition& Definition = Target->Interaction;
	FActiveRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.TargetId = Target->GetTargetId();
	Request.Interactable = Target;
	Request.TimeoutSeconds = Definition.ExecutionTimeoutSeconds;
	Request.bPlayerCancelable = Definition.bPlayerCancelable;
	Request.bCancelWhenEligibilityLost = Definition.bCancelWhenEligibilityLost;
	Request.bCancelOnGameplayControlLost = Definition.bCancelOnGameplayControlLost;
	ActiveRequest = Request;
	UpdateComponentTickEnabled();
	CurrentView.FocusedInteraction.ActivationState = EInteractionActivationState::Pending;
	PublishView();
	Target->DispatchInteraction(this, GetControlledInteractor(), Request.RequestId);
	return true;
}

bool UInteractionControlComponent::ResolveFocusForTarget(UInteractableComponent& Interactable, AActor& Interactor, FResolvedFocus& OutFocus) const
{
	FInteractionFocusView View;
	if (!Interactable.BuildFocusView(View))
	{
		return false;
	}
	const FVector Anchor = Interactable.ResolveInteractionAnchor();
	const FVector Delta = Anchor - Interactor.GetActorLocation();
	const FVector PlanarDelta(Delta.X, Delta.Y, 0.0f);
	const FVector Forward3D = Interactor.GetActorForwardVector();
	const FVector PlanarForward(Forward3D.X, Forward3D.Y, 0.0f);
	const float Alignment = PlanarDelta.IsNearlyZero() ? 1.0f : FVector::DotProduct(PlanarForward.GetSafeNormal(), PlanarDelta.GetSafeNormal());
	const float MinimumAlignment = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(FocusHalfAngleDegrees, 0.0f, 180.0f)));
	if (Alignment < MinimumAlignment || (Interactable.Interaction.bRequireLineOfSight && !HasLineOfSight(Interactable)))
	{
		return false;
	}
	OutFocus.Interactable = &Interactable;
	OutFocus.View = MoveTemp(View);
	OutFocus.Distance = Delta.Size();
	OutFocus.Alignment = Alignment;
	return true;
}

bool UInteractionControlComponent::HasLineOfSight(const UInteractableComponent& Interactable) const
{
	AActor* Interactor = GetControlledInteractor();
	UWorld* World = GetWorld();
	if (!Interactor || !World)
	{
		return true;
	}
	FVector EyeLocation;
	FRotator EyeRotation;
	Interactor->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InteractionLineOfSight), false);
	QueryParams.AddIgnoredActor(Interactor);
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, EyeLocation, Interactable.ResolveInteractionAnchor(), LineOfSightTraceChannel, QueryParams);
	return !bHit || Hit.GetActor() == Interactable.GetOwner();
}

AActor* UInteractionControlComponent::GetControlledInteractor() const
{
	if (Controller)
	{
		return Controller->GetPawn();
	}
	for (const FOverlapRecord& Record : OverlapRecords)
	{
		if (Record.OtherComponent.IsValid() && Record.OtherComponent->GetOwner())
		{
			return Record.OtherComponent->GetOwner();
		}
	}
	return nullptr;
}

UInteractableComponent* UInteractionControlComponent::ResolveFocusedTarget() const
{
	if (!CurrentView.bHasFocusedInteraction)
	{
		return nullptr;
	}
	for (const FOverlapRecord& Record : OverlapRecords)
	{
		if (Record.Interactable.IsValid() && Record.Interactable->GetTargetId() == CurrentView.FocusedInteraction.TargetId)
		{
			return Record.Interactable.Get();
		}
	}
	return nullptr;
}

void UInteractionControlComponent::FinishActiveRequest(const FInteractionResult& Result)
{
	if (!ActiveRequest.IsSet())
	{
		return;
	}
	const FActiveRequest FinishedRequest = ActiveRequest.GetValue();
	ActiveRequest.Reset();
	if (FinishedRequest.Interactable.IsValid())
	{
		if (Result.Code == EInteractionResultCode::TargetInvalidated)
		{
			FinishedRequest.Interactable->AbandonInteraction(FinishedRequest.RequestId);
		}
		else
		{
			FinishedRequest.Interactable->ReleaseInteraction(FinishedRequest.RequestId, Result);
		}
	}
	RefreshFocusView();
	UpdateComponentTickEnabled();
	OnInteractionCompleted.Broadcast(FinishedRequest.RequestId, Result);
}

void UInteractionControlComponent::HandleInteractStarted(const FInputActionValue& Value)
{
	BeginFocusedInteraction();
}

void UInteractionControlComponent::HandleInteractCompleted(const FInputActionValue& Value)
{
	EndFocusedInteractionInput();
}

void UInteractionControlComponent::HandleCancelInteraction(const FInputActionValue& Value)
{
	CancelActiveInteraction();
}

void UInteractionControlComponent::HandleControlledCharacterChanged(const ABaseCharacter* NewCharacter, const ABaseCharacter* OldCharacter)
{
	OverlapRecords.Reset();
	ClearCharge();
	ReconcileControlledInteractor();
	RefreshFocusView();
	UpdateComponentTickEnabled();
}

void UInteractionControlComponent::HandleControlModeChanged(const int32& NewControlMode)
{
	bGameplayControlActive = (NewControlMode & static_cast<int32>(ERPGControlMode::Gameplay)) != 0;
	if (!bGameplayControlActive)
	{
		ClearCharge();
		if (ActiveRequest.IsSet() && ActiveRequest->bCancelOnGameplayControlLost)
		{
			FInteractionResult Result;
			Result.Code = EInteractionResultCode::Canceled;
			FinishActiveRequest(Result);
		}
		CurrentView.bHasFocusedInteraction = false;
		CurrentView.FocusedInteraction = {};
		PublishView();
	}
	else
	{
		ReconcileControlledInteractor();
		RefreshFocusView();
	}
	UpdateComponentTickEnabled();
}
