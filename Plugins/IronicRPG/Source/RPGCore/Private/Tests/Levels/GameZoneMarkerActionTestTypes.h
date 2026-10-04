// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Levels/GameZoneMarkerAction.h"
#include "GameZoneMarkerActionTestTypes.generated.h"

UCLASS()
class UGameZoneMarkerTestAction : public UGameZoneMarkerAction
{
    GENERATED_BODY()

public:
    void CompleteForTest(const FGameZoneMarkerActionResult& Result)
    {
        CompleteAction(Result);
    }

protected:
    virtual void ExecuteAction_Implementation(const FGameZoneMarkerActionContext& Context) override
    {
    }
};
