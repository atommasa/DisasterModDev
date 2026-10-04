// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "Levels/GameZoneMapData.h"
#include "GameZoneMapRegionVolume.generated.h"

/** Editor-only convex selection volume for one Zone Map Sheet. */
UCLASS(NotBlueprintable, HideCategories = (Collision, Cooking, HLOD, Input, LOD, Networking, Physics, Replication))
class RPGCORE_API AGameZoneMapRegionVolume : public AVolume
{
    GENERATED_BODY()

public:
    AGameZoneMapRegionVolume(const FObjectInitializer& ObjectInitializer);

public:
    virtual bool IsEditorOnly() const override { return true; }
    virtual bool NeedsLoadForClient() const override { return false; }
    virtual bool NeedsLoadForServer() const override { return false; }
    virtual bool IsLevelBoundsRelevant() const override { return false; }
#if WITH_EDITOR
    virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
    virtual bool ActorTypeSupportsDataLayer() const override { return false; }
    virtual bool ActorTypeSupportsExternalDataLayer() const override { return false; }
#endif

public:
    UPROPERTY(EditAnywhere, Category = "Zone Map")
    FGameZoneMapRegionId RegionId;

    UPROPERTY(EditAnywhere, Category = "Zone Map", meta=(ZoneMapSheetReference))
    FGameZoneMapSheetId SheetId;

    UPROPERTY(EditAnywhere, Category = "Zone Map")
    int32 Priority = 0;
};
