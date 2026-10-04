// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RPGWorldMapWidget.h"
#include "RPGWorldMapWidgetTestTypes.generated.h"

UCLASS()
class URPGWorldMapWidgetTest : public URPGWorldMapWidget
{
    GENERATED_BODY()

public:
    void ConstructForTest() { NativeConstruct(); }
    void DestructForTest() { NativeDestruct(); }
    bool IsReadyForTest() const { return bPresentationReady; }
    void ConfigureCanvasesForTest(UCanvasPanel* Content, UCanvasPanel* Markers, UCanvasPanel* Edge,
        TSubclassOf<URPGMapMarkerWidget> MarkerClass)
    {
        MapContentRoot = Content;
        MarkerCanvas = Markers;
        EdgeMarkerCanvas = Edge;
        DefaultMarkerWidgetClass = MarkerClass;
        EdgeMarkerWidgetClass = MarkerClass;
    }

    void ConfigureViewInput(float InCanvasUnitsPerWorldMeter, float InMinimumZoom, float InMaximumZoom, float InZoomStep)
    {
        CanvasUnitsPerWorldMeter = InCanvasUnitsPerWorldMeter;
        MinimumViewZoom = InMinimumZoom;
        MaximumViewZoom = InMaximumZoom;
        ViewZoomStep = InZoomStep;
    }

    void ConfigureViewBounds(const FVector2D& MinimumWorldXY, const FVector2D& MaximumWorldXY, float PaddingMeters)
    {
        CurrentMapWorldBounds.MinimumWorldXY = MinimumWorldXY;
        CurrentMapWorldBounds.MaximumWorldXY = MaximumWorldXY;
        CurrentMapWorldBounds.bIsValid = true;
        ViewPanBoundsPaddingMeters = PaddingMeters;
    }

    void ClearViewBounds()
    {
        CurrentMapWorldBounds = {};
    }

    float GetViewZoomForTest() const { return ViewZoom; }
    FVector GetViewCenterForTest() const { return ViewCenterWorldLocation; }
    const FResolvedGameZoneMapSheet& GetSheetForTest() const { return CurrentSheet; }
};

UCLASS()
class URPGMapMarkerWidgetTest : public URPGMapMarkerWidget
{
    GENERATED_BODY()
};

UCLASS()
class URPGMapEdgeMarkerWidgetTest : public URPGMapMarkerWidget
{
    GENERATED_BODY()
};

UCLASS()
class URPGMapTrackingObserverTest : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION()
    void HandleTrackingChanged(FGuid PreviousPointId, FGuid CurrentPointId)
    {
        ++ChangeCount;
        Previous = PreviousPointId;
        Current = CurrentPointId;
    }

public:
    int32 ChangeCount = 0;
    FGuid Previous;
    FGuid Current;
};
