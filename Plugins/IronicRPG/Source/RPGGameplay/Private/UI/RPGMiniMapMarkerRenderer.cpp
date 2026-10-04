// Copyright Ironic Studio. All Rights Reserved.

#include "UI/RPGMiniMapMarkerRenderer.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Levels/MapMarkerTypeAsset.h"
#include "UI/RPGMapPresentationModel.h"
#include "UI/RPGMapViewportTransform.h"

void URPGMiniMapMarkerRenderer::Initialize(URPGMiniMapWidget& InOwner, UCanvasPanel* InCanvas, UCanvasPanel* InEdgeCanvas,
    URPGMapPresentationModel& InModel, const FRPGMiniMapMarkerSettings& InSettings)
{
    Owner = &InOwner;
    Canvas = InCanvas;
    EdgeCanvas = InEdgeCanvas;
    Model = &InModel;
    Settings = InSettings;
    if (InCanvas) { InCanvas->SetVisibility(ESlateVisibility::HitTestInvisible); }
    if (InEdgeCanvas) { InEdgeCanvas->SetVisibility(ESlateVisibility::HitTestInvisible); }
}

void URPGMiniMapMarkerRenderer::SetTrackedMarker(const FGuid& Id)
{
    if (TrackedId == Id) { return; }
    DirtyIds.Add(TrackedId);
    DirtyIds.Add(Id);
    TrackedId = Id;
    // Also invalidate an in-progress update if a Blueprint visual callback changes tracking.
    ++Generation;
    bHasSample = false;
}

void URPGMiniMapMarkerRenderer::Reset(bool bDropPool)
{
    ++Generation;
    Index.Reset();
    DirtyIds.Reset();
    LayerOrders.Reset();
    bHasSample = false;
    ReleaseAll(bDropPool);
}

void URPGMiniMapMarkerRenderer::Rebuild(const FGameZonePresentationSnapshot& Snapshot)
{
    Reset(false);
    for (const FGameZoneMapLayerCatalogEntry& Layer : Snapshot.MapLayers) { LayerOrders.Add(Layer.LayerId, Layer.ElevationOrder); }
    if (Model.IsValid())
    {
        const TArray<FRPGMapPresentationMarker> Markers = Model->GetMarkers();
        for (const FRPGMapPresentationMarker& Marker : Markers)
        {
            Index.Upsert(Marker.Point.Data.PointId, Marker.Point.Data.WorldTransform.GetLocation());
        }
    }
}

void URPGMiniMapMarkerRenderer::RecordChange(const FRPGMapPresentationMarkerChange& Change)
{
    if (Change.Kind == EMapMarkerChangeKind::Removed) { Index.Remove(Change.PointId); }
    else { Index.Upsert(Change.PointId, Change.Marker.Point.Data.WorldTransform.GetLocation()); }
    DirtyIds.Add(Change.PointId);
}

EWorldMapMarkerLayerRelation URPGMiniMapMarkerRenderer::Relation(
    const FGameZoneMapLayerId& MarkerLayer, const FGameZoneMapLayerId& ViewLayer) const
{
    if (MarkerLayer == ViewLayer) { return EWorldMapMarkerLayerRelation::Current; }
    const int32* MarkerOrder = LayerOrders.Find(MarkerLayer);
    const int32* ViewOrder = LayerOrders.Find(ViewLayer);
    if (!MarkerOrder || !ViewOrder || *MarkerOrder == *ViewOrder) { return EWorldMapMarkerLayerRelation::Other; }
    return *MarkerOrder > *ViewOrder ? EWorldMapMarkerLayerRelation::Above : EWorldMapMarkerLayerRelation::Below;
}

TSubclassOf<URPGMapMarkerWidget> URPGMiniMapMarkerRenderer::ResolveWidgetClass(bool bPlayerTracked) const
{
    return bPlayerTracked && Settings.EdgeWidgetClass ? Settings.EdgeWidgetClass : Settings.WidgetClass;
}

URPGMapMarkerWidget* URPGMiniMapMarkerRenderer::Acquire(TSubclassOf<URPGMapMarkerWidget> WidgetClass)
{
    if (!WidgetClass)
    {
        return nullptr;
    }

    const int32 PoolIndex = Pool.IndexOfByPredicate(
        [WidgetClass](const URPGMapMarkerWidget* Candidate)
        {
            return Candidate && Candidate->GetClass() == WidgetClass.Get();
        });
    if (PoolIndex != INDEX_NONE)
    {
        URPGMapMarkerWidget* Widget = Pool[PoolIndex];
        Pool.RemoveAtSwap(PoolIndex, EAllowShrinking::No);
        return Widget;
    }

    URPGMapMarkerWidget* Widget = CreateWidget<URPGMapMarkerWidget>(Owner.Get(), WidgetClass);
    if (Widget)
    {
        ++CreatedCount;
    }
    return Widget;
}

void URPGMiniMapMarkerRenderer::Release(const FGuid& Id)
{
    TObjectPtr<URPGMapMarkerWidget> Widget;
    if (!Widgets.RemoveAndCopyValue(Id, Widget)) { return; }
    const uint64 ExpectedGeneration = Generation;
    Widget->RemoveFromParent();
    Widget->ReleaseMarker();
    if (Generation == ExpectedGeneration && Pool.Num() < FMath::Max(0, Settings.PoolLimit)) { Pool.Add(Widget); }
}

void URPGMiniMapMarkerRenderer::ReleaseAll(bool bDropPool)
{
    TArray<FGuid> Ids;
    Widgets.GetKeys(Ids);
    for (const FGuid& Id : Ids) { Release(Id); }
    if (bDropPool) { Pool.Reset(); }
}

void URPGMiniMapMarkerRenderer::Update(const FMiniMapViewState& View, const FResolvedGameZoneMapSheet& Sheet,
    UGameZoneSubsystem* Subsystem)
{
    if (bUpdating) { return; }
    TGuardValue<bool> UpdatingGuard(bUpdating, true);
    if (!Owner.IsValid() || !Owner->IsVisible()) { return; }
    if ((!Canvas.IsValid() && !EdgeCanvas.IsValid()) || !Model.IsValid() || !Model->IsReady() || !Subsystem || !View.bHasMapProjection
        || !View.bHasFollowTarget || !View.bPresentationReady || !Settings.WidgetClass
        || Settings.Size.ContainsNaN() || Settings.Size.X <= 0.0 || Settings.Size.Y <= 0.0)
    {
        bHasSample = false;
        ReleaseAll(false);
        return;
    }
    const bool bSheetChanged = !bHasSample || Sheet.LayerId != LastSheet.LayerId || Sheet.SheetId != LastSheet.SheetId
        || Sheet.MapBakeRevision != LastSheet.MapBakeRevision || Sheet.ZoneId != LastSheet.ZoneId;
    const bool bViewChanged = !bHasSample || !View.CenterWorldLocation.Equals(LastView.CenterWorldLocation)
        || View.LocalUnitsPerWorldMeter != LastView.LocalUnitsPerWorldMeter || View.ViewportSize != LastView.ViewportSize;
    if (!bSheetChanged && !bViewChanged && DirtyIds.IsEmpty()) { return; }
    const uint64 ExpectedGeneration = Generation;
    TSet<FGuid> ChangedIds = MoveTemp(DirtyIds);
    DirtyIds.Reset();
    LastView = View;
    LastSheet = Sheet;
    bHasSample = true;

    const double Buffer = FMath::IsFinite(Settings.RetentionMeters) ? FMath::Max(0.0f, Settings.RetentionMeters) : 0.0;
    TArray<FGuid> Nearby;
    Index.Query(FVector2D(View.CenterWorldLocation.X, View.CenterWorldLocation.Y), (View.ViewRangeMeters + Buffer) * 100.0, Nearby);
    ++QueryCount;
    // One direct lookup supplements nearby candidates; tracking never broadens the spatial query or loads another Zone.
    if (TrackedId.IsValid() && EdgeCanvas.IsValid()) { Nearby.AddUnique(TrackedId); }
    TSet<FGuid> Retained(Nearby);
    TArray<FGuid> Previous;
    Widgets.GetKeys(Previous);
    for (const FGuid& Id : Previous)
    {
        if (!Retained.Contains(Id)) { Release(Id); }
        if (Generation != ExpectedGeneration) { return; }
    }

    FRPGMapViewportParameters Parameters;
    Parameters.ViewportSize = View.ViewportSize;
    Parameters.ViewCenterWorldLocation = View.CenterWorldLocation;
    Parameters.LocalUnitsPerWorldMeter = View.LocalUnitsPerWorldMeter;
    Parameters.EdgePadding = FMath::IsFinite(Settings.EdgePadding) ? FMath::Max(0.0f, Settings.EdgePadding) : 0.0f;
    Parameters.Shape = ERPGMapViewportShape::Circle;
    const FRPGMapViewportTransform Transform(Parameters);
    for (const FGuid& Id : Nearby)
    {
        const bool bPlayerTracked = Id == TrackedId;
        const bool bUsesEdgeCanvas = bPlayerTracked && EdgeCanvas.IsValid();
        UCanvasPanel* TargetCanvas = bUsesEdgeCanvas ? EdgeCanvas.Get() : Canvas.Get();
        FRPGMapPresentationMarker Marker;
        FResolvedGameZoneMapSheet MarkerSheet;
        if (!TargetCanvas || !Model->TryGetMarker(Id, Marker) || !Marker.MarkerType
            || !Subsystem->ResolveMapSheetAtLocation(Sheet.ZoneId, Marker.Point.Data.WorldTransform.GetLocation(), MarkerSheet))
        {
            Release(Id);
            if (Generation != ExpectedGeneration) { return; }
            continue;
        }
        const FRPGMapMarkerPlacement Placement = Transform.PlaceMarker(
            Marker.Point.Data.WorldTransform.GetLocation(), Settings.Size * 0.5, bUsesEdgeCanvas);
        URPGMapMarkerWidget* Widget = FindWidget(Id);
        const TSubclassOf<URPGMapMarkerWidget> DesiredWidgetClass = ResolveWidgetClass(bPlayerTracked);
        if (Widget && Widget->GetClass() != DesiredWidgetClass.Get())
        {
            Release(Id);
            if (Generation != ExpectedGeneration) { return; }
            Widget = nullptr;
        }
        bool bAcquired = false;
        // Do not construct Widgets for the buffer alone. Previously visible Widgets may remain hidden there.
        if (!Widget && Placement.bIsVisible)
        {
            Widget = Acquire(DesiredWidgetClass);
            if (Generation != ExpectedGeneration) { return; }
            if (!Widget) { continue; }
            Widgets.Add(Id, Widget);
            bAcquired = true;
        }
        if (!Widget) { continue; }
        if (Widget->GetParent() != TargetCanvas)
        {
            Widget->RemoveFromParent();
            UCanvasPanelSlot* Slot = TargetCanvas->AddChildToCanvas(Widget);
            if (Generation != ExpectedGeneration) { return; }
            Slot->SetAnchors(FAnchors(0.0f));
            Slot->SetAlignment(FVector2D(0.5));
            Slot->SetAutoSize(false);
            Slot->SetSize(Settings.Size);
        }
        const FWorldMapMarkerView& PreviousView = Widget->GetMarkerView();
        const FVector2D Direction = bUsesEdgeCanvas ? Placement.DirectionFromCenter : FVector2D::ZeroVector;
        const bool bPlacementChanged = PreviousView.bPlayerTracked != bPlayerTracked
            || PreviousView.bClampedToEdge != Placement.bIsClampedToEdge || !PreviousView.DirectionFromViewCenter.Equals(Direction);
        if (bAcquired || ChangedIds.Contains(Id) || bSheetChanged || bPlacementChanged)
        {
            FWorldMapMarkerView MarkerView;
            MarkerView.Point = Marker.Point;
            MarkerView.MarkerType = Marker.MarkerType;
            MarkerView.LayerRelation = Relation(MarkerSheet.LayerId, Sheet.LayerId);
            MarkerView.bPlayerTracked = bPlayerTracked;
            MarkerView.bClampedToEdge = Placement.bIsClampedToEdge;
            MarkerView.DirectionFromViewCenter = Direction;
            Widget->ApplyMarkerView(MarkerView);
            ++AppliedCount;
            if (Generation != ExpectedGeneration) { return; }
        }
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
        {
            if (!Slot->GetPosition().Equals(Placement.LocalPosition)) { Slot->SetPosition(Placement.LocalPosition); ++PositionCount; }
            if (Slot->GetZOrder() != Marker.MarkerType->GetZOrder()) { Slot->SetZOrder(Marker.MarkerType->GetZOrder()); }
        }
        const ESlateVisibility Visibility = Placement.bIsVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;
        if (Widget->GetVisibility() != Visibility) { Widget->SetVisibility(Visibility); }
    }
}
