// Copyright Ironic Studio. All Rights Reserved.

#include "Components/InteractableComponent.h"

#include "Components/InteractionControlComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "InteractionNativeTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogInteractionTarget, Log, All);

UInteractableComponent::UInteractableComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInteractableComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureTargetId();
	InitializeRuntimeState();
	BindDetectionComponents();
}

void UInteractableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	NotifyTargetInvalidated();
	UnbindDetectionComponents();
	ObservingControls.Reset();
	RequestOwners.Reset();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
EDataValidationResult UInteractableComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!Interaction.ActionTag.IsValid())
	{
		Context.AddError(FText::FromString(TEXT("Interaction requires a valid ActionTag.")));
		Result = EDataValidationResult::Invalid;
	}

	const TArray<UPrimitiveComponent*> DetectionComponents = ResolveDetectionComponents();
	if (DetectionComponents.IsEmpty())
	{
		Context.AddError(FText::FromString(TEXT("Interaction has no valid detection primitive.")));
		Result = EDataValidationResult::Invalid;
	}
	for (const UPrimitiveComponent* Primitive : DetectionComponents)
	{
		if (!Primitive->GetGenerateOverlapEvents() || Primitive->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Overlap)
		{
			Context.AddError(FText::Format(
				FText::FromString(TEXT("Interaction detection primitive '{0}' must generate overlap events and overlap Pawn.")),
				FText::FromString(GetNameSafe(Primitive))));
			Result = EDataValidationResult::Invalid;
		}
	}

	if (const AActor* Owner = GetOwner())
	{
		TArray<UInteractableComponent*> Components;
		Owner->GetComponents(Components);
		if (Components.Num() > 1)
		{
			Context.AddError(FText::FromString(TEXT("An Actor may own only one InteractableComponent.")));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}

#endif

FGuid UInteractableComponent::GetTargetId() const
{
	EnsureTargetId();
	return RuntimeTargetId;
}

bool UInteractableComponent::IsInteractionEnabled() const
{
	return bRuntimeStateInitialized ? bInteractionEnabled : Interaction.bInitiallyEnabled;
}

void UInteractableComponent::SetInteractionEnabled(bool bEnabled, FGameplayTag ReasonTag, const FText& ReasonText)
{
	if (!bRuntimeStateInitialized)
	{
		InitializeRuntimeState();
	}
	if (bInteractionEnabled == bEnabled && UnavailableReasonTag == ReasonTag && UnavailableReasonText.EqualTo(ReasonText))
	{
		return;
	}
	bInteractionEnabled = bEnabled;
	UnavailableReasonTag = ReasonTag;
	UnavailableReasonText = ReasonText;
	NotifyOverlappingControlsChanged();
}

bool UInteractableComponent::CompleteInteraction(FGuid RequestId, const FInteractionResult& Result)
{
	TWeakObjectPtr<UInteractionControlComponent>* Owner = RequestOwners.Find(RequestId);
	if (!Owner || !Owner->IsValid())
	{
		UE_LOG(LogInteractionTarget, Verbose, TEXT("Ignored stale interaction completion %s on %s."), *RequestId.ToString(), *GetNameSafe(GetOwner()));
		return false;
	}
	return Owner->Get()->CompleteInteractionFromTarget(this, RequestId, Result);
}

bool UInteractableComponent::BuildFocusView(FInteractionFocusView& OutView) const
{
	if (!Interaction.ActionTag.IsValid())
	{
		return false;
	}
	const bool bEnabled = IsInteractionEnabled();
	if (!bEnabled && Interaction.UnavailablePresentation == EInteractionUnavailablePresentation::Hidden)
	{
		return false;
	}

	OutView = {};
	OutView.TargetId = GetTargetId();
	OutView.ActionTag = Interaction.ActionTag;
	OutView.DisplayName = Interaction.DisplayName;
	OutView.Icon = Interaction.Icon;
	OutView.bEnabled = bEnabled;
	OutView.UnavailableReasonTag = bRuntimeStateInitialized ? UnavailableReasonTag : Interaction.InitialUnavailableReasonTag;
	OutView.UnavailableReasonText = bRuntimeStateInitialized ? UnavailableReasonText : Interaction.InitialUnavailableReasonText;
	OutView.ActivationPolicy = Interaction.ActivationPolicy;
	return true;
}

FVector UInteractableComponent::ResolveInteractionAnchor() const
{
	if (const USceneComponent* Anchor = Cast<USceneComponent>(ResolveComponentReference(Interaction.InteractionAnchor)))
	{
		return Anchor->GetComponentLocation();
	}
	if (const AActor* Owner = GetOwner())
	{
		const FBox Bounds = Owner->GetComponentsBoundingBox(true);
		return Bounds.IsValid ? Bounds.GetCenter() : Owner->GetActorLocation();
	}
	return FVector::ZeroVector;
}

bool UInteractableComponent::IsDetectionComponent(const UPrimitiveComponent* DetectionComponent) const
{
	return ResolveDetectionComponents().Contains(DetectionComponent);
}

void UInteractableComponent::DispatchInteraction(UInteractionControlComponent* Control, AActor* Interactor, FGuid RequestId)
{
	if (!IsValid(Control) || !RequestId.IsValid())
	{
		return;
	}
	if (!RequestOwners.IsEmpty() || !OnInteractionRequested.IsBound())
	{
		FInteractionResult Failure;
		Failure.Code = EInteractionResultCode::Failed;
		Failure.ReasonTag = !OnInteractionRequested.IsBound() ? InteractionNativeTags::ErrorUnhandledAction : InteractionNativeTags::ErrorTargetBusy;
		Failure.ReasonText = !OnInteractionRequested.IsBound()
			? FText::FromString(TEXT("No interaction handler is bound."))
			: FText::FromString(TEXT("The target is already handling another interaction."));
		Control->CompleteInteractionFromTarget(this, RequestId, Failure);
		return;
	}

	RequestOwners.Add(RequestId, Control);
	OnInteractionRequested.Broadcast(Interactor, this, Interaction.ActionTag, RequestId);
}

void UInteractableComponent::ReleaseInteraction(FGuid RequestId, const FInteractionResult& Result)
{
	if (RequestOwners.Remove(RequestId) > 0)
	{
		OnInteractionEnded.Broadcast(RequestId, Result);
	}
}

void UInteractableComponent::NotifyTargetInvalidated()
{
	TArray<TWeakObjectPtr<UInteractionControlComponent>> Controls;
	RequestOwners.GenerateValueArray(Controls);
	for (const TWeakObjectPtr<UInteractionControlComponent>& Control : Controls)
	{
		if (Control.IsValid())
		{
			Control->InvalidateTarget(this);
		}
	}
	for (const TWeakObjectPtr<UInteractionControlComponent>& Control : ObservingControls)
	{
		if (Control.IsValid())
		{
			Control->InvalidateTarget(this);
		}
	}
}

void UInteractableComponent::OnDetectionBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	AController* PawnController = Pawn ? Pawn->GetController() : nullptr;
	UInteractionControlComponent* Control = PawnController ? PawnController->FindComponentByClass<UInteractionControlComponent>() : nullptr;
	if (!Control || !IsDetectionComponent(OverlappedComponent))
	{
		return;
	}
	ObservingControls.Add(Control);
	Control->NotifyInteractionOverlap(this, OverlappedComponent, OtherActor, OtherComponent, true);
}

void UInteractableComponent::OnDetectionEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	AController* PawnController = Pawn ? Pawn->GetController() : nullptr;
	UInteractionControlComponent* Control = PawnController ? PawnController->FindComponentByClass<UInteractionControlComponent>() : nullptr;
	if (Control && IsDetectionComponent(OverlappedComponent))
	{
		Control->NotifyInteractionOverlap(this, OverlappedComponent, OtherActor, OtherComponent, false);
	}
}

void UInteractableComponent::EnsureTargetId() const
{
	if (!RuntimeTargetId.IsValid() || HasAnyFlags(RF_ClassDefaultObject))
	{
		RuntimeTargetId = HasAnyFlags(RF_ClassDefaultObject) ? FGuid() : FGuid::NewGuid();
	}
}

void UInteractableComponent::InitializeRuntimeState()
{
	bInteractionEnabled = Interaction.bInitiallyEnabled;
	UnavailableReasonTag = Interaction.InitialUnavailableReasonTag;
	UnavailableReasonText = Interaction.InitialUnavailableReasonText;
	bRuntimeStateInitialized = true;
}

void UInteractableComponent::BindDetectionComponents()
{
	for (UPrimitiveComponent* Primitive : ResolveDetectionComponents())
	{
		if (!Primitive || BoundDetectionComponents.Contains(Primitive))
		{
			continue;
		}
		if (!Primitive->GetGenerateOverlapEvents() || Primitive->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Overlap)
		{
			UE_LOG(
				LogInteractionTarget,
				Warning,
				TEXT("Interaction detection primitive %s must generate overlap events and overlap Pawn."),
				*GetNameSafe(Primitive));
			continue;
		}
		Primitive->OnComponentBeginOverlap.AddUniqueDynamic(this, &UInteractableComponent::OnDetectionBeginOverlap);
		Primitive->OnComponentEndOverlap.AddUniqueDynamic(this, &UInteractableComponent::OnDetectionEndOverlap);
		BoundDetectionComponents.Add(Primitive);
	}
}

void UInteractableComponent::UnbindDetectionComponents()
{
	for (const TWeakObjectPtr<UPrimitiveComponent>& Primitive : BoundDetectionComponents)
	{
		if (Primitive.IsValid())
		{
			Primitive->OnComponentBeginOverlap.RemoveDynamic(this, &UInteractableComponent::OnDetectionBeginOverlap);
			Primitive->OnComponentEndOverlap.RemoveDynamic(this, &UInteractableComponent::OnDetectionEndOverlap);
		}
	}
	BoundDetectionComponents.Reset();
}

void UInteractableComponent::NotifyOverlappingControlsChanged()
{
	for (auto It = ObservingControls.CreateIterator(); It; ++It)
	{
		if (It->IsValid())
		{
			It->Get()->NotifyInteractableChanged(this);
		}
		else
		{
			It.RemoveCurrent();
		}
	}
}

void UInteractableComponent::AbandonInteraction(FGuid RequestId)
{
	RequestOwners.Remove(RequestId);
}

UActorComponent* UInteractableComponent::ResolveComponentReference(const FComponentReference& Reference) const
{
	if (UActorComponent* Component = Reference.GetComponent(GetOwner()))
	{
		return Component;
	}

#if WITH_EDITOR
	if (GetOwner() || Reference.OtherActor.IsValid() || Reference.ComponentProperty.IsNone())
	{
		return nullptr;
	}
	UBlueprintGeneratedClass* ActualClass = GetTypedOuter<UBlueprintGeneratedClass>();
	for (UBlueprintGeneratedClass* SearchClass = ActualClass; SearchClass; SearchClass = Cast<UBlueprintGeneratedClass>(SearchClass->GetSuperClass()))
	{
		if (!SearchClass->SimpleConstructionScript)
		{
			continue;
		}
		for (USCS_Node* Node : SearchClass->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->GetVariableName() == Reference.ComponentProperty)
			{
				return Node->GetActualComponentTemplate(ActualClass);
			}
		}
	}
#endif
	return nullptr;
}

TArray<UPrimitiveComponent*> UInteractableComponent::ResolveDetectionComponents() const
{
	TArray<UPrimitiveComponent*> Result;
	for (const FComponentReference& Reference : Interaction.DetectionComponents)
	{
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(ResolveComponentReference(Reference)))
		{
			Result.AddUnique(Primitive);
		}
	}
	return Result;
}
