// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameZonePointRegistryProvider.generated.h"

class UGameZonePointComponent;

UENUM()
enum class EGameZonePointRegistrationResult : uint8
{
    Registered,
    AlreadyRegistered,
    InvalidPointId,
    DuplicatePointId,
    WrongWorld,
};

UINTERFACE(MinimalAPI)
class UGameZonePointRegistryProvider : public UInterface
{
    GENERATED_BODY()
};

class RPGCORE_API IGameZonePointRegistryProvider
{
    GENERATED_BODY()

public:
    virtual EGameZonePointRegistrationResult RegisterPoint(
        UGameZonePointComponent& Component) = 0;
    virtual void UnregisterPoint(UGameZonePointComponent& Component) = 0;
    virtual void NotifyPointChanged(UGameZonePointComponent& Component) = 0;
};
