// Copyright Ironic Studio. All Rights Reserved.

#include "Levels/GameZoneMapRegionVolume.h"

#include "Components/BrushComponent.h"

AGameZoneMapRegionVolume::AGameZoneMapRegionVolume(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = false;
    bIsEditorOnlyActor = true;
    bNotForClientOrServer = true;
#if WITH_EDITORONLY_DATA
    bIsSpatiallyLoaded = false;
#endif
    bColored = true;
    BrushColor = FColor(128, 96, 255);

    if (UBrushComponent* RegionBrush = GetBrushComponent())
    {
        RegionBrush->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        RegionBrush->SetGenerateOverlapEvents(false);
        RegionBrush->SetCanEverAffectNavigation(false);
    }
}
