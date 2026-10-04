// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RPGMiniMapWidget.h"
#include "UI/RPGMapMarkerWidget.h"
#include "RPGMiniMapWidgetTestTypes.generated.h"

UCLASS()
class URPGMiniMapMarkerWidgetTest : public URPGMapMarkerWidget
{
    GENERATED_BODY()
};

UCLASS()
class URPGMiniMapEdgeMarkerWidgetTest : public URPGMapMarkerWidget
{
    GENERATED_BODY()
};

UCLASS()
class URPGMiniMapWidgetTest : public URPGMiniMapWidget
{
    GENERATED_BODY()

public:
    void ConstructForTest() { NativeConstruct(); }
    void DestructForTest() { NativeDestruct(); }
    void TickForTest(FVector2D Size = FVector2D(256.0, 256.0), float LayoutScale = 1.0f)
    {
        NativeTick(FGeometry::MakeRoot(Size, FSlateLayoutTransform(LayoutScale)), 1.0f / 60.0f);
    }
    const FMiniMapViewState& GetViewForTest() const { return ViewState; }
    const FResolvedGameZoneMapSheet& GetSheetForTest() const { return CurrentSheet; }
    FRPGId GetZoneForTest() const { return CurrentZoneId; }
    void ConfigureRangeForTest(float Minimum, float Maximum)
    {
        MinimumViewRangeMeters = Minimum;
        MaximumViewRangeMeters = Maximum;
    }
    void ConfigurePollingForTest(float Interval) { TrackedMarkerRefreshInterval = Interval; }
    void ConfigureBackgroundForTest(UImage* Image, UMaterialInterface* Material) { MapImage = Image; MapMaterial = Material; }
    UMaterialInstanceDynamic* GetMaterialForTest() const { return MapDynamicMaterial; }
    UTexture2D* GetTextureForTest() const { return CurrentTexture; }
    bool IsTexturePendingForTest() const { return bTexturePending; }
    void ConfigureMarkersForTest(UCanvasPanel* Canvas, UCanvasPanel* EdgeCanvas = nullptr)
    {
        MarkerCanvas = Canvas;
        EdgeMarkerCanvas = EdgeCanvas;
        DefaultMarkerWidgetClass = URPGMiniMapMarkerWidgetTest::StaticClass();
        EdgeMarkerWidgetClass = URPGMiniMapEdgeMarkerWidgetTest::StaticClass();
        MarkerSize = FVector2D(24.0);
        MarkerWidgetPoolLimit = 1;
    }

protected:
    virtual void OnMiniMapPresentationReady_Implementation() override { ++ReadyCount; }
    virtual void OnMiniMapPresentationFailed_Implementation() override { ++FailedCount; }
    virtual void OnMiniMapSheetChanged_Implementation(const FResolvedGameZoneMapSheet& Sheet) override { ++SheetCount; }
    virtual void OnMiniMapSheetUnavailable_Implementation() override { ++UnavailableCount; }
    virtual void OnMiniMapViewChanged_Implementation(const FMiniMapViewState& View) override { ++ViewCount; }
    virtual void OnMiniMapTextureChanged_Implementation(const FGameZoneMapTextureResult& Result) override
    {
        ++TextureCount;
        LastTextureResult = Result;
    }

public:
    int32 ReadyCount = 0;
    int32 FailedCount = 0;
    int32 SheetCount = 0;
    int32 UnavailableCount = 0;
    int32 ViewCount = 0;
    int32 TextureCount = 0;
    FGameZoneMapTextureResult LastTextureResult;
};
