// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Levels/GameZoneMapData.h"
#include "GameZoneMapSheetBounds.generated.h"

class UBoxComponent;

/** Editor-only rectangular world mapping for one Zone Map Sheet. */
UCLASS(NotBlueprintable, HideCategories = (Collision, Cooking, HLOD, Input, LOD, Networking, Physics, Replication))
class RPGCORE_API AGameZoneMapSheetBounds : public AActor
{
    GENERATED_BODY()

public:
    AGameZoneMapSheetBounds();

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
    UBoxComponent* GetBoundsComponent() const { return BoundsComponent; }

public:
    UPROPERTY(EditAnywhere, Category = "Zone Map", meta=(ZoneMapSheetReference))
    FGameZoneMapSheetId SheetId;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone Map")
    TObjectPtr<UBoxComponent> BoundsComponent;
};
