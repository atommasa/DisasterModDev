// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RPGMiniMapWidget.h"
#include "UI/RPGMapMarkerWidget.h"
#include "UI/RPGMiniMapMarkerIndex.h"
#include "RPGMiniMapMarkerRenderer.generated.h"

class UCanvasPanel;
struct FRPGMapPresentationMarkerChange;

struct FRPGMiniMapMarkerSettings
{
    TSubclassOf<URPGMapMarkerWidget> WidgetClass;
    TSubclassOf<URPGMapMarkerWidget> EdgeWidgetClass;
    FVector2D Size = FVector2D(24.0);
    float EdgePadding = 4.0f;
    float RetentionMeters = 20.0f;
    int32 PoolLimit = 64;
};

/** Private visual cache. Model owns the data; callbacks only maintain the index and dirty IDs. */
UCLASS()
class URPGMiniMapMarkerRenderer : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URPGMiniMapWidget& InOwner, UCanvasPanel* InCanvas, UCanvasPanel* InEdgeCanvas, URPGMapPresentationModel& InModel,
        const FRPGMiniMapMarkerSettings& InSettings);
    void SetTrackedMarker(const FGuid& Id);
    void Reset(bool bDropPool);
    void Rebuild(const FGameZonePresentationSnapshot& Snapshot);
    void RecordChange(const FRPGMapPresentationMarkerChange& Change);
    void Update(const FMiniMapViewState& View, const FResolvedGameZoneMapSheet& Sheet, UGameZoneSubsystem* Subsystem);

    // Read-only private seam used by acceptance tests; never exposed to Blueprint.
    URPGMapMarkerWidget* FindWidget(const FGuid& Id) const { return Widgets.FindRef(Id); }
    int32 NumWidgets() const { return Widgets.Num(); }
    int32 NumPooled() const { return Pool.Num(); }
    int32 NumIndexed() const { return Index.Num(); }
    uint64 GetQueryCount() const { return QueryCount; }
    uint64 GetCreatedCount() const { return CreatedCount; }
    uint64 GetAppliedCount() const { return AppliedCount; }
    uint64 GetPositionCount() const { return PositionCount; }

private:
    TSubclassOf<URPGMapMarkerWidget> ResolveWidgetClass(bool bPlayerTracked) const;
    URPGMapMarkerWidget* Acquire(TSubclassOf<URPGMapMarkerWidget> WidgetClass);
    void Release(const FGuid& Id);
    void ReleaseAll(bool bDropPool);
    EWorldMapMarkerLayerRelation Relation(const FGameZoneMapLayerId& MarkerLayer, const FGameZoneMapLayerId& ViewLayer) const;

private:
    UPROPERTY(Transient)
    TMap<FGuid, TObjectPtr<URPGMapMarkerWidget>> Widgets;
    UPROPERTY(Transient)
    TArray<TObjectPtr<URPGMapMarkerWidget>> Pool;
    TWeakObjectPtr<URPGMiniMapWidget> Owner;
    TWeakObjectPtr<UCanvasPanel> Canvas;
    TWeakObjectPtr<UCanvasPanel> EdgeCanvas;
    TWeakObjectPtr<URPGMapPresentationModel> Model;
    FRPGMiniMapMarkerSettings Settings;
    FRPGMiniMapMarkerIndex Index;
    TMap<FGameZoneMapLayerId, int32> LayerOrders;
    TSet<FGuid> DirtyIds;
    FGuid TrackedId;
    FMiniMapViewState LastView;
    FResolvedGameZoneMapSheet LastSheet;
    bool bHasSample = false;
    bool bUpdating = false;
    uint64 Generation = 0;
    uint64 QueryCount = 0, CreatedCount = 0, AppliedCount = 0, PositionCount = 0;
};
