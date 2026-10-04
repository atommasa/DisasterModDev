// Copyright Ironic Studio. All Rights Reserved.

#include "Levels/GameZoneMarkerAction.h"

FText UGameZoneMarkerAction::GetDisplayName_Implementation(const FGameZoneMarkerActionContext& Context, const FText& DefaultDisplayName) const
{
    return DefaultDisplayName;
}

FGameZoneMarkerActionAvailabilityResult UGameZoneMarkerAction::GetAvailability_Implementation(const FGameZoneMarkerActionContext& Context) const
{
    return {};
}

void UGameZoneMarkerAction::StartAction(const FGameZoneMarkerActionContext& Context, TFunction<void(const FGameZoneMarkerActionResult&)> Completion)
{
    check(!bIsRunning);
    bIsRunning = true;
    CompletionCallback = MoveTemp(Completion);
    ExecuteAction(Context);
}

void UGameZoneMarkerAction::CancelAction()
{
    if (bIsRunning)
    {
        CancelActionExecution();
    }
}

void UGameZoneMarkerAction::ExecuteAction_Implementation(const FGameZoneMarkerActionContext& Context)
{
    FGameZoneMarkerActionResult Result;
    Result.Completion = EGameZoneMarkerActionCompletion::Failed;
    Result.Message = NSLOCTEXT("GameZoneMarkerAction", "NotImplemented", "This marker action is not implemented.");
    CompleteAction(Result);
}

void UGameZoneMarkerAction::CancelActionExecution_Implementation()
{
    FGameZoneMarkerActionResult Result;
    Result.Completion = EGameZoneMarkerActionCompletion::Cancelled;
    CompleteAction(Result);
}

void UGameZoneMarkerAction::CompleteAction(const FGameZoneMarkerActionResult& Result)
{
    if (!ensureMsgf(bIsRunning, TEXT("A marker action attempted to complete more than once.")))
    {
        return;
    }

    bIsRunning = false;
    TFunction<void(const FGameZoneMarkerActionResult&)> Callback = MoveTemp(CompletionCallback);
    CompletionCallback = nullptr;
    if (Callback)
    {
        Callback(Result);
    }
}
