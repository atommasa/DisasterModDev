// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Components/InteractionControlComponent.h"

#include "Components/BoxComponent.h"
#include "Components/InteractableComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "NativeGameplayTags.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/InteractionTestTypes.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Interaction_Test_Use, "Interaction.Test.Use");

namespace
{
	struct FInteractionTestTarget
	{
		AActor* Actor = nullptr;
		UBoxComponent* Detection = nullptr;
		UInteractableComponent* Interactable = nullptr;
	};

	AActor* SpawnInteractor(UWorld& World, const FVector& Location = FVector::ZeroVector)
	{
		AActor* Actor = World.SpawnActor<AActor>();
		UBoxComponent* Root = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Root);
		Actor->AddInstanceComponent(Root);
		Root->RegisterComponent();
		Actor->SetActorLocation(Location);
		Actor->SetActorRotation(FRotator::ZeroRotator);
		return Actor;
	}

	UInteractionControlComponent* SpawnInteractionControl(UWorld& World)
	{
		AActor* Owner = World.SpawnActor<AActor>();
		UInteractionControlComponent* Control = NewObject<UInteractionControlComponent>(Owner);
		Owner->AddInstanceComponent(Control);
		Control->RegisterComponent();
		return Control;
	}

	FInteractionTestTarget SpawnTarget(UWorld& World, const FVector& Location)
	{
		FInteractionTestTarget Result;
		Result.Actor = World.SpawnActor<AActor>();
		Result.Detection = NewObject<UBoxComponent>(Result.Actor);
		Result.Actor->SetRootComponent(Result.Detection);
		Result.Actor->AddInstanceComponent(Result.Detection);
		Result.Detection->RegisterComponent();
		Result.Actor->SetActorLocation(Location);

		Result.Interactable = NewObject<UInteractableComponent>(Result.Actor);
		Result.Interactable->Interaction.ActionTag = TAG_Interaction_Test_Use;
		Result.Interactable->Interaction.bRequireLineOfSight = false;
		Result.Actor->AddInstanceComponent(Result.Interactable);
		Result.Interactable->RegisterComponent();
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionFocusNearestFacingTest,
	"IronicRPG.Interaction.Focus.NearestFacingTargetWins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteractionFocusNearestFacingTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UInteractionControlComponent* Control = SpawnInteractionControl(*World);
	AActor* Interactor = SpawnInteractor(*World);
	UBoxComponent* InteractorShape = CastChecked<UBoxComponent>(Interactor->GetRootComponent());
	const FInteractionTestTarget Near = SpawnTarget(*World, FVector(100.0f, 20.0f, 0.0f));
	const FInteractionTestTarget Far = SpawnTarget(*World, FVector(200.0f, 0.0f, 0.0f));
	const FInteractionTestTarget Behind = SpawnTarget(*World, FVector(-50.0f, 0.0f, 0.0f));

	Control->NotifyInteractionOverlap(Far.Interactable, Far.Detection, Interactor, InteractorShape, true);
	Control->NotifyInteractionOverlap(Behind.Interactable, Behind.Detection, Interactor, InteractorShape, true);
	Control->NotifyInteractionOverlap(Near.Interactable, Near.Detection, Interactor, InteractorShape, true);

	const FInteractionViewSnapshot& View = Control->GetInteractionView();
	TestTrue(TEXT("A facing target is focused"), View.bHasFocusedInteraction);
	TestEqual(TEXT("The nearest target inside the facing cone wins"), View.FocusedInteraction.TargetId, Near.Interactable->GetTargetId());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionOverlapReferenceCountTest,
	"IronicRPG.Interaction.Focus.AllOverlapsMustEndBeforeFocusClears",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteractionOverlapReferenceCountTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UInteractionControlComponent* Control = SpawnInteractionControl(*World);
	AActor* Interactor = SpawnInteractor(*World);
	UBoxComponent* FirstInteractorShape = CastChecked<UBoxComponent>(Interactor->GetRootComponent());
	UBoxComponent* SecondInteractorShape = NewObject<UBoxComponent>(Interactor);
	Interactor->AddInstanceComponent(SecondInteractorShape);
	SecondInteractorShape->RegisterComponent();
	const FInteractionTestTarget Target = SpawnTarget(*World, FVector(100.0f, 0.0f, 0.0f));

	Control->NotifyInteractionOverlap(Target.Interactable, Target.Detection, Interactor, FirstInteractorShape, true);
	Control->NotifyInteractionOverlap(Target.Interactable, Target.Detection, Interactor, SecondInteractorShape, true);
	TestTrue(TEXT("Two overlap pairs produce one focused interaction"), Control->GetInteractionView().bHasFocusedInteraction);

	Control->NotifyInteractionOverlap(Target.Interactable, Target.Detection, Interactor, FirstInteractorShape, false);
	TestTrue(TEXT("Ending one pair preserves focus"), Control->GetInteractionView().bHasFocusedInteraction);

	Control->NotifyInteractionOverlap(Target.Interactable, Target.Detection, Interactor, SecondInteractorShape, false);
	TestFalse(TEXT("Focus clears after every pair ends"), Control->GetInteractionView().bHasFocusedInteraction);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionFacingLossTest,
	"IronicRPG.Interaction.Focus.FacingLossClearsFocus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteractionFacingLossTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UInteractionControlComponent* Control = SpawnInteractionControl(*World);
	AActor* Interactor = SpawnInteractor(*World);
	UBoxComponent* InteractorShape = CastChecked<UBoxComponent>(Interactor->GetRootComponent());
	const FInteractionTestTarget Target = SpawnTarget(*World, FVector(100.0f, 0.0f, 0.0f));
	Control->NotifyInteractionOverlap(Target.Interactable, Target.Detection, Interactor, InteractorShape, true);
	TestTrue(TEXT("The target starts focused"), Control->GetInteractionView().bHasFocusedInteraction);

	Interactor->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
	Control->NotifyInteractableChanged(Target.Interactable);
	TestFalse(TEXT("Turning outside the facing cone clears focus"), Control->GetInteractionView().bHasFocusedInteraction);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionFocusSwitchBiasTest,
	"IronicRPG.Interaction.Focus.DistanceBiasPreventsJitter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteractionFocusSwitchBiasTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UInteractionControlComponent* Control = SpawnInteractionControl(*World);
	AActor* Interactor = SpawnInteractor(*World);
	UBoxComponent* InteractorShape = CastChecked<UBoxComponent>(Interactor->GetRootComponent());
	const FInteractionTestTarget Current = SpawnTarget(*World, FVector(100.0f, 0.0f, 0.0f));
	const FInteractionTestTarget Challenger = SpawnTarget(*World, FVector(90.0f, 10.0f, 0.0f));

	Control->NotifyInteractionOverlap(Current.Interactable, Current.Detection, Interactor, InteractorShape, true);
	Control->NotifyInteractionOverlap(Challenger.Interactable, Challenger.Detection, Interactor, InteractorShape, true);
	TestEqual(
		TEXT("A slightly closer target does not steal focus"),
		Control->GetInteractionView().FocusedInteraction.TargetId,
		Current.Interactable->GetTargetId());

	Challenger.Actor->SetActorLocation(FVector(50.0f, 10.0f, 0.0f));
	Control->NotifyInteractableChanged(Challenger.Interactable);
	TestEqual(
		TEXT("A clearly closer target receives focus"),
		Control->GetInteractionView().FocusedInteraction.TargetId,
		Challenger.Interactable->GetTargetId());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionUnavailablePresentationTest,
	"IronicRPG.Interaction.Presentation.DisabledRequirementRemainsFocused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteractionUnavailablePresentationTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UInteractionControlComponent* Control = SpawnInteractionControl(*World);
	AActor* Interactor = SpawnInteractor(*World);
	UBoxComponent* InteractorShape = CastChecked<UBoxComponent>(Interactor->GetRootComponent());
	const FInteractionTestTarget Target = SpawnTarget(*World, FVector(100.0f, 0.0f, 0.0f));
	Target.Interactable->Interaction.UnavailablePresentation = EInteractionUnavailablePresentation::Disabled;
	Target.Interactable->SetInteractionEnabled(false, {}, FText::FromString(TEXT("Locked")));
	Control->NotifyInteractionOverlap(Target.Interactable, Target.Detection, Interactor, InteractorShape, true);

	const FInteractionViewSnapshot& View = Control->GetInteractionView();
	TestTrue(TEXT("A disabled-presented interaction remains focused"), View.bHasFocusedInteraction);
	TestFalse(TEXT("The focused interaction is unavailable"), View.FocusedInteraction.bEnabled);
	TestEqual(TEXT("The requirement reason is published"), View.FocusedInteraction.UnavailableReasonText.ToString(), FString(TEXT("Locked")));
	TestFalse(TEXT("An unavailable focus rejects activation"), Control->BeginFocusedInteraction());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionPressRequestLifecycleTest,
	"IronicRPG.Interaction.Execution.PressCompletesExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteractionPressRequestLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UInteractionControlComponent* Control = SpawnInteractionControl(*World);
	UInteractionTestReceiver* Receiver = NewObject<UInteractionTestReceiver>();
	AActor* Interactor = SpawnInteractor(*World);
	UBoxComponent* InteractorShape = CastChecked<UBoxComponent>(Interactor->GetRootComponent());
	const FInteractionTestTarget Target = SpawnTarget(*World, FVector(100.0f, 0.0f, 0.0f));
	Target.Interactable->OnInteractionRequested.AddDynamic(Receiver, &UInteractionTestReceiver::HandleRequested);
	Control->OnInteractionCompleted.AddDynamic(Receiver, &UInteractionTestReceiver::HandleCompleted);
	Control->NotifyInteractionOverlap(Target.Interactable, Target.Detection, Interactor, InteractorShape, true);

	TestTrue(TEXT("Press starts the focused interaction"), Control->BeginFocusedInteraction());
	TestEqual(TEXT("The target receives one request"), Receiver->RequestCount, 1);
	TestTrue(TEXT("The view exposes the active request"), Control->GetInteractionView().ActiveRequestId.IsValid());

	FInteractionResult Success;
	Success.Code = EInteractionResultCode::Succeeded;
	TestTrue(TEXT("The target completes its active request"), Target.Interactable->CompleteInteraction(Receiver->LastRequestId, Success));
	TestFalse(TEXT("A duplicate completion is rejected"), Target.Interactable->CompleteInteraction(Receiver->LastRequestId, Success));
	TestEqual(TEXT("Completion is published exactly once"), Receiver->CompletionCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionHoldActivationTest,
	"IronicRPG.Interaction.Activation.HoldReleaseAndThreshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteractionHoldActivationTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UInteractionControlComponent* Control = SpawnInteractionControl(*World);
	UInteractionTestReceiver* Receiver = NewObject<UInteractionTestReceiver>();
	AActor* Interactor = SpawnInteractor(*World);
	UBoxComponent* InteractorShape = CastChecked<UBoxComponent>(Interactor->GetRootComponent());
	const FInteractionTestTarget Target = SpawnTarget(*World, FVector(100.0f, 0.0f, 0.0f));
	Target.Interactable->Interaction.ActivationPolicy.Mode = EInteractionActivationMode::Hold;
	Target.Interactable->Interaction.ActivationPolicy.HoldDuration = 1.0f;
	Target.Interactable->OnInteractionRequested.AddDynamic(Receiver, &UInteractionTestReceiver::HandleRequested);
	Control->NotifyInteractionOverlap(Target.Interactable, Target.Detection, Interactor, InteractorShape, true);

	TestTrue(TEXT("Hold begins charging"), Control->BeginFocusedInteraction());
	Control->TickComponent(0.4f, LEVELTICK_All, nullptr);
	Control->EndFocusedInteractionInput();
	TestEqual(TEXT("An early release does not dispatch"), Receiver->RequestCount, 0);
	TestEqual(TEXT("An early release clears progress"), Control->GetInteractionView().FocusedInteraction.ActivationProgress, 0.0f);

	TestTrue(TEXT("Hold restarts after an early release"), Control->BeginFocusedInteraction());
	Control->TickComponent(1.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("The hold threshold dispatches once"), Receiver->RequestCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTimeoutTest,
	"IronicRPG.Interaction.Execution.TimeoutCompletesRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteractionTimeoutTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UInteractionControlComponent* Control = SpawnInteractionControl(*World);
	UInteractionTestReceiver* Receiver = NewObject<UInteractionTestReceiver>();
	AActor* Interactor = SpawnInteractor(*World);
	UBoxComponent* InteractorShape = CastChecked<UBoxComponent>(Interactor->GetRootComponent());
	const FInteractionTestTarget Target = SpawnTarget(*World, FVector(100.0f, 0.0f, 0.0f));
	Target.Interactable->Interaction.ExecutionTimeoutSeconds = 0.5f;
	Target.Interactable->OnInteractionRequested.AddDynamic(Receiver, &UInteractionTestReceiver::HandleRequested);
	Control->OnInteractionCompleted.AddDynamic(Receiver, &UInteractionTestReceiver::HandleCompleted);
	Control->NotifyInteractionOverlap(Target.Interactable, Target.Detection, Interactor, InteractorShape, true);

	Control->BeginFocusedInteraction();
	Control->TickComponent(0.5f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Timeout completes the request once"), Receiver->CompletionCount, 1);
	TestEqual(TEXT("Timeout reports its standard result"), Receiver->LastResult.Code, EInteractionResultCode::TimedOut);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionLineOfSightTest,
	"IronicRPG.Interaction.Visibility.OccludedFocusIsHidden",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteractionLineOfSightTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AActor* ControlOwner = World->SpawnActor<AActor>();
	UInteractionControlComponent* Control = NewObject<UInteractionControlComponent>(ControlOwner);
	ControlOwner->AddInstanceComponent(Control);
	Control->RegisterComponent();
	AActor* Interactor = SpawnInteractor(*World);
	UBoxComponent* InteractorShape = CastChecked<UBoxComponent>(Interactor->GetRootComponent());
	const FInteractionTestTarget Target = SpawnTarget(*World, FVector(200.0f, 0.0f, 0.0f));
	Target.Interactable->Interaction.bRequireLineOfSight = true;
	Target.Detection->SetBoxExtent(FVector(20.0f));
	Target.Detection->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Target.Detection->SetCollisionResponseToAllChannels(ECR_Ignore);
	Target.Detection->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);

	AActor* ObstacleActor = World->SpawnActor<AActor>();
	UBoxComponent* Obstacle = NewObject<UBoxComponent>(ObstacleActor);
	Obstacle->SetBoxExtent(FVector(20.0f));
	Obstacle->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Obstacle->SetCollisionResponseToAllChannels(ECR_Ignore);
	Obstacle->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
	ObstacleActor->SetRootComponent(Obstacle);
	ObstacleActor->AddInstanceComponent(Obstacle);
	Obstacle->RegisterComponent();
	ObstacleActor->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));

	Control->NotifyInteractionOverlap(Target.Interactable, Target.Detection, Interactor, InteractorShape, true);
	TestFalse(TEXT("An occluded in-range target is not focused"), Control->GetInteractionView().bHasFocusedInteraction);

	Obstacle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Control->NotifyInteractableChanged(Target.Interactable);
	TestTrue(TEXT("Removing the obstacle restores focus"), Control->GetInteractionView().bHasFocusedInteraction);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionDefinitionValidationTest,
	"IronicRPG.Interaction.Validation.MissingActionAndDetectionFail",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteractionDefinitionValidationTest::RunTest(const FString& Parameters)
{
	UInteractableComponent* Interactable = NewObject<UInteractableComponent>();
	FDataValidationContext Context;
	TestEqual(TEXT("An incomplete interaction definition fails validation"), Interactable->IsDataValid(Context), EDataValidationResult::Invalid);
	TestTrue(TEXT("Validation reports the missing action and primitive"), Context.GetNumErrors() >= 2u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionBlueprintTemplateValidationTest,
	"IronicRPG.Interaction.Validation.BlueprintTemplateDetectionReferenceResolves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteractionBlueprintTemplateValidationTest::RunTest(const FString& Parameters)
{
	const FName BlueprintName = MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("InteractionTemplateValidation"));
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), GetTransientPackage(), BlueprintName, BPTYPE_Normal);
	USimpleConstructionScript* ConstructionScript = Blueprint->SimpleConstructionScript;
	USCS_Node* DetectionNode = ConstructionScript->CreateNode(UBoxComponent::StaticClass(), TEXT("Sphere"));
	ConstructionScript->AddNode(DetectionNode);
	UBoxComponent* Detection = CastChecked<UBoxComponent>(DetectionNode->ComponentTemplate);
	Detection->SetGenerateOverlapEvents(true);
	Detection->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	USCS_Node* InteractableNode = ConstructionScript->CreateNode(UInteractableComponent::StaticClass(), TEXT("Interactable"));
	ConstructionScript->AddNode(InteractableNode);
	UInteractableComponent* Interactable = CastChecked<UInteractableComponent>(InteractableNode->ComponentTemplate);
	Interactable->Interaction.ActionTag = TAG_Interaction_Test_Use;
	FComponentReference& DetectionReference = Interactable->Interaction.DetectionComponents.AddDefaulted_GetRef();
	DetectionReference.ComponentProperty = DetectionNode->GetVariableName();

	FDataValidationContext Context;
	TestNotEqual(
		TEXT("A valid Blueprint template detection reference passes validation"),
		Interactable->IsDataValid(Context),
		EDataValidationResult::Invalid);
	TestEqual(TEXT("Valid Blueprint authoring produces no validation errors"), Context.GetNumErrors(), 0u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionUnhandledActionTest,
	"IronicRPG.Interaction.Execution.UnhandledActionFailsImmediately",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteractionUnhandledActionTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UInteractionControlComponent* Control = SpawnInteractionControl(*World);
	UInteractionTestReceiver* Receiver = NewObject<UInteractionTestReceiver>();
	AActor* Interactor = SpawnInteractor(*World);
	UBoxComponent* InteractorShape = CastChecked<UBoxComponent>(Interactor->GetRootComponent());
	const FInteractionTestTarget Target = SpawnTarget(*World, FVector(100.0f, 0.0f, 0.0f));
	Control->OnInteractionCompleted.AddDynamic(Receiver, &UInteractionTestReceiver::HandleCompleted);
	Control->NotifyInteractionOverlap(Target.Interactable, Target.Detection, Interactor, InteractorShape, true);

	TestTrue(TEXT("The focused interaction accepts the input attempt"), Control->BeginFocusedInteraction());
	TestEqual(TEXT("An unhandled action completes immediately"), Receiver->CompletionCount, 1);
	TestEqual(TEXT("An unhandled action reports failure"), Receiver->LastResult.Code, EInteractionResultCode::Failed);
	TestFalse(TEXT("An unhandled action cannot leave a pending request"), Control->GetInteractionView().ActiveRequestId.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteractionTargetInvalidationTest,
	"IronicRPG.Interaction.Execution.TargetInvalidationCompletesExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteractionTargetInvalidationTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UInteractionControlComponent* Control = SpawnInteractionControl(*World);
	UInteractionTestReceiver* Receiver = NewObject<UInteractionTestReceiver>();
	AActor* Interactor = SpawnInteractor(*World);
	UBoxComponent* InteractorShape = CastChecked<UBoxComponent>(Interactor->GetRootComponent());
	const FInteractionTestTarget Target = SpawnTarget(*World, FVector(100.0f, 0.0f, 0.0f));
	Target.Interactable->OnInteractionRequested.AddDynamic(Receiver, &UInteractionTestReceiver::HandleRequested);
	Control->OnInteractionCompleted.AddDynamic(Receiver, &UInteractionTestReceiver::HandleCompleted);
	Control->NotifyInteractionOverlap(Target.Interactable, Target.Detection, Interactor, InteractorShape, true);
	Control->BeginFocusedInteraction();

	const FGuid RequestId = Receiver->LastRequestId;
	Control->InvalidateTarget(Target.Interactable);
	TestEqual(TEXT("Target invalidation completes once"), Receiver->CompletionCount, 1);
	TestEqual(TEXT("Target invalidation reports its standard result"), Receiver->LastResult.Code, EInteractionResultCode::TargetInvalidated);
	FInteractionResult LateSuccess;
	LateSuccess.Code = EInteractionResultCode::Succeeded;
	TestFalse(TEXT("A late target completion is ignored"), Target.Interactable->CompleteInteraction(RequestId, LateSuccess));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
