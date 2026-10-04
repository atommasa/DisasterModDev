// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Levels/GameZoneMarkerActionTestTypes.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerActionCompletionTest,
    "IronicRPG.GameZone.MarkerAction.ExecutionCompletesWithResult",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerActionCompletionTest::RunTest(const FString& Parameters)
{
    UGameZoneMarkerTestAction* Action = NewObject<UGameZoneMarkerTestAction>();
    int32 CompletionCount = 0;
    FGameZoneMarkerActionResult ObservedResult;

    Action->StartAction({}, [&CompletionCount, &ObservedResult](const FGameZoneMarkerActionResult& Result)
        {
            ++CompletionCount;
            ObservedResult = Result;
        });

    TestEqual(TEXT("A pending action has not completed"), CompletionCount, 0);

    FGameZoneMarkerActionResult Success;
    Success.Completion = EGameZoneMarkerActionCompletion::Succeeded;
    Action->CompleteForTest(Success);

    TestEqual(TEXT("Completion is delivered once"), CompletionCount, 1);
    TestEqual(TEXT("Completion preserves the action result"), ObservedResult.Completion, EGameZoneMarkerActionCompletion::Succeeded);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerActionCancellationTest,
    "IronicRPG.GameZone.MarkerAction.ExecutionCancellationCompletes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerActionCancellationTest::RunTest(const FString& Parameters)
{
    UGameZoneMarkerTestAction* Action = NewObject<UGameZoneMarkerTestAction>();
    FGameZoneMarkerActionResult ObservedResult;
    bool bCompleted = false;

    Action->StartAction({}, [&bCompleted, &ObservedResult](const FGameZoneMarkerActionResult& Result)
        {
            bCompleted = true;
            ObservedResult = Result;
        });
    Action->CancelAction();

    TestTrue(TEXT("Cancellation completes a running action"), bCompleted);
    TestEqual(TEXT("Cancellation reports the cancelled result"), ObservedResult.Completion, EGameZoneMarkerActionCompletion::Cancelled);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
