// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameZoneSubsystem.h"
#include "Levels/GameZoneMarkerAction.h"

#include "GameZoneSubsystemTestTypes.generated.h"

UCLASS()
class UGameZoneActionNameDefaultTestAction : public UGameZoneMarkerAction
{
    GENERATED_BODY()
};

UCLASS()
class UGameZoneActionNameDynamicTestAction : public UGameZoneMarkerAction
{
    GENERATED_BODY()

public:
    virtual FText GetDisplayName_Implementation(const FGameZoneMarkerActionContext& Context, const FText& DefaultDisplayName) const override
    {
        if (Context.PresentationMode == EMapMarkerDisplayMode::MiniMap || !Context.PlayerController || !Context.WorldContextObject)
        {
            return FText::FromString(TEXT("   "));
        }
        const FText StateLabel = Context.Point.MarkerState == EGameZonePointState::Activated
            ? NSLOCTEXT("MarkerActionNameTest", "Deactivate", "Deactivate")
            : NSLOCTEXT("MarkerActionNameTest", "Activate", "Activate");
        return FText::Format(NSLOCTEXT("MarkerActionNameTest", "NameFormat", "{0}: {1}"), DefaultDisplayName, StateLabel);
    }
};

UCLASS()
class UGameZoneMapTextureTestReceiver : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION()
    void HandlePresentationReady(const FGameZonePresentationSnapshot& Snapshot)
    {
        ++PresentationCompletionCount;
        LastSnapshot = Snapshot;
    }

    UFUNCTION()
    void HandleTextureReady(const FGameZoneMapTextureResult& Result)
    {
        ++CompletionCount;
        LastResult = Result;
    }

public:
    int32 PresentationCompletionCount = 0;
    int32 CompletionCount = 0;
    FGameZonePresentationSnapshot LastSnapshot;
    FGameZoneMapTextureResult LastResult;
};
