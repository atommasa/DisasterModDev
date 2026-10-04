// Copyright Ironic Studio. All Rights Reserved.

#include "Levels/GameZoneMapSheetBounds.h"

#include "Components/BoxComponent.h"

AGameZoneMapSheetBounds::AGameZoneMapSheetBounds()
{
    PrimaryActorTick.bCanEverTick = false;
    bIsEditorOnlyActor = true;
#if WITH_EDITORONLY_DATA
    bIsSpatiallyLoaded = false;
#endif

    BoundsComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
    SetRootComponent(BoundsComponent);
    BoundsComponent->SetBoxExtent(FVector(500.0, 500.0, 100.0));
    BoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BoundsComponent->SetGenerateOverlapEvents(false);
    BoundsComponent->SetCanEverAffectNavigation(false);
    BoundsComponent->ShapeColor = FColor(64, 192, 255);
    BoundsComponent->SetLineThickness(4.0f);
}
