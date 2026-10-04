// Copyright Ironic Studio. All Rights Reserved.

#include "UI/RPGWorldMapWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "Levels/MapMarkerTypeAsset.h"
#include "UI/MapMarkerSelectionResolver.h"
#include "UI/RPGMapPresentationModel.h"
#include "UI/RPGMapViewportTransform.h"
#include "UI/MapTrackingSubsystem.h"

void URPGWorldMapWidget::NativeConstruct()
{
    Super::NativeConstruct();
    StartPresentation();
}

void URPGWorldMapWidget::NativeDestruct()
{
    bPointerDown = false;
    bPanTriggered = false;
    bIsPanning = false;
    StopPresentation();
    Super::NativeDestruct();
}

void URPGWorldMapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (EdgeMarkerCanvas
        && !EdgeMarkerPointIds.IsEmpty()
        && !EdgeMarkerCanvas->GetCachedGeometry().GetLocalSize().Equals(LastEdgeMarkerViewportSize))
    {
        bViewTransformDirty = true;
    }

    if (bViewTransformDirty)
    {
        UpdateViewTransform();
    }
}

FReply URPGWorldMapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
    }

    bPointerDown = true;
    bPanTriggered = false;
    bIsPanning = false;
    PointerDownLocalPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply URPGWorldMapWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (!bPointerDown || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
    }

    const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    const bool bWasPanning = bPanTriggered;
    bPointerDown = false;
    bPanTriggered = false;
    bIsPanning = false;

    if (!bWasPanning && !bActionPending && !bAwaitingActionConfirmation)
    {
        ResolveMarkerClick(InGeometry, LocalPosition);
    }
    return FReply::Handled().ReleaseMouseCapture();
}

FReply URPGWorldMapWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (!bPointerDown)
    {
        return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
    }

    if (!InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
    {
        bPointerDown = false;
        bPanTriggered = false;
        bIsPanning = false;
        return FReply::Handled().ReleaseMouseCapture();
    }

    const FVector2D CurrentLocalPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    if (!bPanTriggered)
    {
        const float DragTriggerDistance = FSlateApplication::Get().GetDragTriggerDistance();
        if (FVector2D::Distance(CurrentLocalPosition, PointerDownLocalPosition) < DragTriggerDistance)
        {
            return FReply::Handled();
        }

        bPanTriggered = true;
        bIsPanning = true;
        ClearMarkerCandidates();
    }

    const FVector2D PreviousLocalPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetLastScreenSpacePosition());
    ApplyViewPanInput(CurrentLocalPosition - PreviousLocalPosition);
    return FReply::Handled();
}

FReply URPGWorldMapWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    const float WheelDelta = InMouseEvent.GetWheelDelta();
    if (FMath::IsNearlyZero(WheelDelta))
    {
        return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
    }

    ClearMarkerCandidates();
    ApplyViewZoomInput(WheelDelta);
    return FReply::Handled();
}

void URPGWorldMapWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
    bPointerDown = false;
    bPanTriggered = false;
    bIsPanning = false;
    Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

bool URPGWorldMapWidget::SelectLayer(const FGameZoneMapLayerId& LayerId)
{
    if (!bPresentationReady || !GameZoneSubsystem || !LayerId.IsValid())
    {
        return false;
    }

    FResolvedGameZoneMapSheet ResolvedSheet;
    if (!GameZoneSubsystem->ResolveMapSheetInLayerAtLocation(CurrentZoneId, LayerId, FocusWorldLocation, ResolvedSheet))
    {
        OnWorldMapSheetUnavailable(LayerId);
        return false;
    }

    return DisplaySheet(ResolvedSheet);
}

bool URPGWorldMapWidget::FocusOnPlayer()
{
    APawn* Pawn = GetOwningPlayerPawn();
    if (!bPresentationReady || !GameZoneSubsystem || !Pawn)
    {
        return false;
    }

    FocusWorldLocation = Pawn->GetActorLocation();
    SetViewCenterWorldLocation(FocusWorldLocation);

    FResolvedGameZoneMapSheet ResolvedSheet;
    if (!GameZoneSubsystem->ResolveMapSheetAtLocation(CurrentZoneId, FocusWorldLocation, ResolvedSheet))
    {
        OnWorldMapSheetUnavailable({});
        return false;
    }

    return DisplaySheet(ResolvedSheet);
}

bool URPGWorldMapWidget::FocusOnMarker(const FGuid& PointId)
{
    FVector MarkerLocation;
    if (!bPresentationReady || !GameZoneSubsystem || !PresentationModel
        || !PresentationModel->TryGetMarkerWorldLocation(PointId, MarkerLocation))
    {
        return false;
    }

    FResolvedGameZoneMapSheet MarkerSheet;
    if (!GameZoneSubsystem->ResolveMapSheetAtLocation(CurrentZoneId, MarkerLocation, MarkerSheet))
    {
        return false;
    }

    if (CurrentSheet.ZoneId != MarkerSheet.ZoneId
        || CurrentSheet.LayerId != MarkerSheet.LayerId
        || CurrentSheet.SheetId != MarkerSheet.SheetId)
    {
        DisplaySheet(MarkerSheet);
        if (CurrentSheet.ZoneId != MarkerSheet.ZoneId
            || CurrentSheet.LayerId != MarkerSheet.LayerId
            || CurrentSheet.SheetId != MarkerSheet.SheetId)
        {
            return false;
        }
    }

    FocusWorldLocation = MarkerLocation;
    SetViewCenterWorldLocation(MarkerLocation);
    return true;
}

void URPGWorldMapWidget::RefreshTrackedMarkers()
{
    if (PresentationModel)
    {
        PresentationModel->RefreshTrackedMarkers();
    }
}

void URPGWorldMapWidget::SetViewCenterWorldLocation(const FVector& WorldLocation)
{
    const FVector ClampedWorldLocation = ClampViewCenterWorldLocation(WorldLocation);
    if (ViewCenterWorldLocation.Equals(ClampedWorldLocation))
    {
        return;
    }

    ViewCenterWorldLocation = ClampedWorldLocation;
    ClearMarkerCandidates();
    bViewTransformDirty = true;
}

void URPGWorldMapWidget::SetViewZoom(float NewZoom)
{
    const float ClampedMinimum = FMath::Max(0.01f, MinimumViewZoom);
    const float ClampedMaximum = FMath::Max(ClampedMinimum, MaximumViewZoom);
    const float ClampedZoom = FMath::Clamp(NewZoom, ClampedMinimum, ClampedMaximum);
    if (FMath::IsNearlyEqual(ViewZoom, ClampedZoom))
    {
        return;
    }

    ViewZoom = ClampedZoom;
    ClearMarkerCandidates();
    bViewTransformDirty = true;
}

void URPGWorldMapWidget::ApplyViewZoomInput_Implementation(float InputAmount)
{
    SetViewZoom(ViewZoom + InputAmount * ViewZoomStep);
}

void URPGWorldMapWidget::ApplyViewPanInput_Implementation(const FVector2D& ScreenDelta)
{
    if (!CurrentMapWorldBounds.bIsValid)
    {
        return;
    }

    const float SafeCanvasUnitsPerWorldMeter = FMath::Max(CanvasUnitsPerWorldMeter, UE_SMALL_NUMBER);
    const float SafeZoom = FMath::Max(ViewZoom, UE_SMALL_NUMBER);
    constexpr float WorldUnitsPerMeter = 100.0f;
    const float WorldUnitsPerScreenUnit = WorldUnitsPerMeter / (SafeCanvasUnitsPerWorldMeter * SafeZoom);

    SetViewCenterWorldLocation(ViewCenterWorldLocation + FVector(ScreenDelta.X, ScreenDelta.Y, 0.0f) * WorldUnitsPerScreenUnit);
}

FVector2D URPGWorldMapWidget::WorldLocationToCanvasPosition(const FVector& WorldLocation) const
{
    return FRPGMapViewportTransform::ProjectNorthUpWorldOffset(WorldLocation, CanvasUnitsPerWorldMeter);
}

bool URPGWorldMapWidget::ProjectWorldLocationToDisplayedSheet(const FVector& WorldLocation, FGameZoneMapProjection& OutProjection) const
{
    OutProjection = {};
    return bPresentationReady
        && GameZoneSubsystem
        && CurrentSheet.SheetId.IsValid()
        && GameZoneSubsystem->ProjectWorldLocationToMapSheet(CurrentZoneId, CurrentSheet.SheetId, WorldLocation, OutProjection);
}

bool URPGWorldMapWidget::RequestSelectedMarkerAction(const FGameplayTag& ActionTag)
{
    if (!SelectedPointId.IsValid() || bActionPending || bAwaitingActionConfirmation || !ActionTag.IsValid())
    {
        return false;
    }

    const FGameZoneMarkerActionOption* Action = CurrentActionOptions.FindByPredicate([&ActionTag](const FGameZoneMarkerActionOption& Candidate)
        {
            return Candidate.Definition.ActionTag == ActionTag;
        });
    if (!Action)
    {
        return false;
    }

    if (Action->Availability.Availability != EGameZoneMarkerActionAvailability::Available)
    {
        OnWorldMapActionRejected(Action->Availability.Reason);
        return false;
    }

    if (Action->Definition.Confirmation == EGameZoneMarkerActionConfirmation::Required)
    {
        PendingConfirmationActionTag = ActionTag;
        PendingConfirmationAction = *Action;
        bAwaitingActionConfirmation = true;
        OnWorldMapActionConfirmationRequested(*Action);
        return true;
    }

    return BeginMarkerAction(*Action);
}

void URPGWorldMapWidget::ConfirmPendingMarkerAction(bool bConfirmed)
{
    if (!bAwaitingActionConfirmation)
    {
        return;
    }

    const FGameZoneMarkerActionOption ConfirmedAction = PendingConfirmationAction;
    bAwaitingActionConfirmation = false;
    PendingConfirmationActionTag = {};
    PendingConfirmationAction = {};

    if (bConfirmed)
    {
        BeginMarkerAction(ConfirmedAction);
    }
}

void URPGWorldMapWidget::ClearMarkerSelection()
{
    if (!bActionPending && !bAwaitingActionConfirmation)
    {
        ClearMarkerCandidates();
        ClearMarkerSelectionInternal();
    }
}

bool URPGWorldMapWidget::ChooseMarkerCandidate(const FGuid& PointId)
{
    if (bActionPending || bAwaitingActionConfirmation || !PendingCandidatePointIds.Contains(PointId))
    {
        return false;
    }

    URPGMapMarkerWidget* Widget = ActiveMarkerWidgets.FindRef(PointId);
    if (!Widget || !Widget->IsVisible() || Widget->GetMarkerView().Point.Data.MarkerState == EGameZonePointState::Hide)
    {
        ClearMarkerCandidates();
        return false;
    }

    ClearMarkerCandidates();
    SelectMarker(PointId);
    return SelectedPointId == PointId;
}

void URPGWorldMapWidget::DismissMarkerCandidates()
{
    ClearMarkerCandidates();
}

void URPGWorldMapWidget::HandleTrackingChanged(FGuid PreviousPointId, FGuid CurrentPointId)
{
    ClearMarkerCandidates();
    if (PreviousPointId.IsValid())
    {
        ReconcileMarker(PreviousPointId);
    }
    if (CurrentPointId.IsValid())
    {
        ReconcileMarker(CurrentPointId);
    }
    RefreshSelectedMarker();
    bViewTransformDirty = true;
}

void URPGWorldMapWidget::StartPresentation()
{
    StopPresentation();

    GameZoneSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGameZoneSubsystem>() : nullptr;
    APawn* Pawn = GetOwningPlayerPawn();
    if (!GameZoneSubsystem || !Pawn || !MapContentRoot || !MarkerCanvas || !DefaultMarkerWidgetClass)
    {
        OnWorldMapPresentationFailed();
        return;
    }

    CurrentZoneId = GameZoneSubsystem->GetCurrentContext().ZoneId;
    if (!CurrentZoneId.IsValid())
    {
        OnWorldMapPresentationFailed();
        return;
    }

    FocusWorldLocation = Pawn->GetActorLocation();
    ViewCenterWorldLocation = FocusWorldLocation;

    TrackingSubsystem = GetOwningLocalPlayer() ? GetOwningLocalPlayer()->GetSubsystem<UMapTrackingSubsystem>() : nullptr;
    if (TrackingSubsystem)
    {
        TrackingSubsystem->OnTrackingChanged.AddDynamic(this, &URPGWorldMapWidget::HandleTrackingChanged);
    }

    PresentationModel = NewObject<URPGMapPresentationModel>(this);
    PresentationModel->OnReady.AddUObject(this, &URPGWorldMapWidget::HandlePresentationReady);
    PresentationModel->OnFailed.AddUObject(this, &URPGWorldMapWidget::HandlePresentationFailed);
    PresentationModel->OnMarkerChanged.AddUObject(this, &URPGWorldMapWidget::HandleMapMarkerChange);
    PresentationModel->OnTextureReady.AddUObject(this, &URPGWorldMapWidget::HandleMapTextureReady);

    FRPGMapPresentationConfig Config;
    Config.ZoneIds = {CurrentZoneId};
    Config.Mode = EMapMarkerDisplayMode::WorldMap;
    Config.TrackedMarkerRefreshInterval = TrackedMarkerRefreshInterval;
    if (!PresentationModel->Start(*GameZoneSubsystem, Config))
    {
        PresentationModel = nullptr;
    }
}

void URPGWorldMapWidget::StopPresentation()
{
    if (TrackingSubsystem)
    {
        TrackingSubsystem->OnTrackingChanged.RemoveDynamic(this, &URPGWorldMapWidget::HandleTrackingChanged);
        TrackingSubsystem = nullptr;
    }
    if (PresentationModel)
    {
        PresentationModel->Stop();
        PresentationModel = nullptr;
    }
    ClearMarkerCandidates(false);

    ReleaseAllMarkerWidgets(false);
    GameZoneSubsystem = nullptr;
    PresentationHandle = {};
    CurrentZoneId = {};
    CurrentMapWorldBounds = {};
    CurrentLayerId = {};
    CurrentSheet = {};
    CurrentTexture = nullptr;
    SelectedPointId.Invalidate();
    ActiveActionExecutionId.Invalidate();
    PendingConfirmationActionTag = {};
    PendingConfirmationAction = {};
    MapLayers.Reset();
    MapSheets.Reset();
    CurrentActionOptions.Reset();
    EdgeMarkerPointIds.Reset();
    bPresentationReady = false;
    bAwaitingActionConfirmation = false;
    bActionPending = false;
    bViewTransformDirty = true;
    LastEdgeMarkerViewportSize = FVector2D::ZeroVector;
}

FVector URPGWorldMapWidget::ClampViewCenterWorldLocation(const FVector& WorldLocation) const
{
    if (!CurrentMapWorldBounds.bIsValid)
    {
        return WorldLocation;
    }

    constexpr double WorldUnitsPerMeter = 100.0;
    const double PaddingWorldUnits = FMath::Max(0.0, static_cast<double>(ViewPanBoundsPaddingMeters)) * WorldUnitsPerMeter;
    FVector Result = WorldLocation;
    Result.X = FMath::Clamp(
        Result.X,
        CurrentMapWorldBounds.MinimumWorldXY.X - PaddingWorldUnits,
        CurrentMapWorldBounds.MaximumWorldXY.X + PaddingWorldUnits);
    Result.Y = FMath::Clamp(
        Result.Y,
        CurrentMapWorldBounds.MinimumWorldXY.Y - PaddingWorldUnits,
        CurrentMapWorldBounds.MaximumWorldXY.Y + PaddingWorldUnits);
    return Result;
}

bool URPGWorldMapWidget::DisplaySheet(const FResolvedGameZoneMapSheet& Sheet)
{
    if (!PresentationModel
        || !PresentationModel->IsActive()
        || Sheet.ZoneId != CurrentZoneId
        || !Sheet.LayerId.IsValid()
        || !Sheet.SheetId.IsValid())
    {
        return false;
    }

    ClearMarkerCandidates();
    CurrentLayerId = Sheet.LayerId;
    CurrentSheet = Sheet;
    CurrentTexture = nullptr;
    OnWorldMapDisplayedSheetChanged(CurrentSheet);
    ReconcileAllMarkers();

    if (!PresentationModel->RequestSheetTexture(CurrentZoneId, CurrentSheet.SheetId))
    {
        FGameZoneMapTextureResult FailedResult;
        FailedResult.ZoneId = CurrentZoneId;
        FailedResult.SheetId = CurrentSheet.SheetId;
        OnWorldMapTextureChanged(FailedResult);
        return false;
    }

    return true;
}

void URPGWorldMapWidget::ResetMarkers()
{
    ClearMarkerCandidates();
    ReleaseAllMarkerWidgets(true);
    ClearMarkerSelectionInternal();

    if (!PresentationModel)
    {
        return;
    }

    for (const FRPGMapPresentationMarker& Marker : PresentationModel->GetMarkers())
    {
        ApplyPresentationMarker(Marker);
    }
}

void URPGWorldMapWidget::ApplyPresentationMarker(const FRPGMapPresentationMarker& Marker)
{
    const FGuid PointId = Marker.Point.Data.PointId;
    if (PendingCandidatePointIds.Contains(PointId))
    {
        ClearMarkerCandidates();
    }

    ReconcileMarker(PointId);

    if (SelectedPointId == PointId)
    {
        RefreshSelectedMarker();
    }
}

void URPGWorldMapWidget::RemoveMarker(const FGuid& PointId)
{
    if (PendingCandidatePointIds.Contains(PointId))
    {
        ClearMarkerCandidates();
    }

    ReleaseMarkerWidget(PointId);

    if (SelectedPointId == PointId)
    {
        ClearMarkerSelectionInternal();
    }
}

void URPGWorldMapWidget::ReconcileAllMarkers()
{
    if (!PresentationModel)
    {
        ReleaseAllMarkerWidgets(true);
        RefreshSelectedMarker();
        return;
    }

    const TArray<FRPGMapPresentationMarker> Markers = PresentationModel->GetMarkers();
    TSet<FGuid> CurrentPointIds;
    CurrentPointIds.Reserve(Markers.Num());
    for (const FRPGMapPresentationMarker& Marker : Markers)
    {
        CurrentPointIds.Add(Marker.Point.Data.PointId);
    }

    TArray<FGuid> MaterializedPointIds;
    ActiveMarkerWidgets.GenerateKeyArray(MaterializedPointIds);
    for (const FGuid& PointId : MaterializedPointIds)
    {
        if (!CurrentPointIds.Contains(PointId))
        {
            ReleaseMarkerWidget(PointId);
        }
    }

    for (const FRPGMapPresentationMarker& Marker : Markers)
    {
        ReconcileMarker(Marker.Point.Data.PointId);
    }

    RefreshSelectedMarker();
}

void URPGWorldMapWidget::ReconcileMarker(const FGuid& PointId)
{
    FRPGMapPresentationMarker Marker;
    if (!PresentationModel || !PresentationModel->TryGetMarker(PointId, Marker))
    {
        ReleaseMarkerWidget(PointId);
        return;
    }

    FWorldMapMarkerView View;
    if (!BuildMarkerView(Marker, SelectedPointId == PointId, View) || !ShouldMaterializeMarker(View))
    {
        ReleaseMarkerWidget(PointId);
        return;
    }

    URPGMapMarkerWidget* Widget = ActiveMarkerWidgets.FindRef(PointId);
    const TSubclassOf<URPGMapMarkerWidget> DesiredWidgetClass = ResolveMarkerWidgetClass(View.bPlayerTracked);
    if (Widget && Widget->GetClass() != DesiredWidgetClass.Get())
    {
        ReleaseMarkerWidget(PointId);
        Widget = nullptr;
    }
    if (!Widget)
    {
        Widget = AcquireMarkerWidget(DesiredWidgetClass);
        if (!Widget)
        {
            return;
        }

        ActiveMarkerWidgets.Add(PointId, Widget);
    }

    Widget->ApplyMarkerView(View);
    Widget->ForceLayoutPrepass();
    UpdateMarkerWidgetPlacement(PointId, *Widget, View);
}

bool URPGWorldMapWidget::BuildMarkerView(
    const FRPGMapPresentationMarker& Marker,
    bool bSelected,
    FWorldMapMarkerView& OutView) const
{
    OutView = {};
    if (!GameZoneSubsystem)
    {
        return false;
    }

    FResolvedGameZoneMapSheet MarkerSheet;
    if (!GameZoneSubsystem->ResolveMapSheetAtLocation(CurrentZoneId, Marker.Point.Data.WorldTransform.GetLocation(), MarkerSheet))
    {
#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Warning, TEXT("[WorldMap] Could not resolve a Layer for marker %s."), *Marker.Point.Data.PointId.ToString());
#endif
        return false;
    }

    OutView.Point = Marker.Point;
    OutView.MarkerType = Marker.MarkerType;
    OutView.LayerRelation = ResolveLayerRelation(MarkerSheet.LayerId);
    OutView.bSelected = bSelected;
    OutView.bPlayerTracked = TrackingSubsystem && TrackingSubsystem->IsMarkerTracked(Marker.Point.Data.PointId);
    return true;
}

bool URPGWorldMapWidget::TryGetPresentedMarkerView(
    const FGuid& PointId,
    bool bSelected,
    FWorldMapMarkerView& OutView) const
{
    OutView = {};
    const URPGMapMarkerWidget* Widget = ActiveMarkerWidgets.FindRef(PointId);
    if (!IsValid(Widget) || Widget->GetPointId() != PointId)
    {
        return false;
    }

    OutView = Widget->GetMarkerView();
    OutView.bSelected = bSelected;
    return true;
}

bool URPGWorldMapWidget::ShouldMaterializeMarker(const FWorldMapMarkerView& View) const
{
    if (!CurrentSheet.SheetId.IsValid()
        || View.Point.Data.ZoneId != CurrentZoneId
        || View.Point.Data.MarkerState == EGameZonePointState::Hide
        || (View.Point.Data.DisplayMode & static_cast<int32>(EMapMarkerDisplayMode::WorldMap)) == 0)
    {
        return false;
    }

    // Tracking uses world XY bearing, not the currently displayed Sheet's UV bounds.
    if (View.bPlayerTracked)
    {
        return true;
    }
    if (MarkerLayerMode == EWorldMapMarkerLayerMode::SelectedLayerOnly
        && View.LayerRelation != EWorldMapMarkerLayerRelation::Current)
    {
        return false;
    }

    FGameZoneMapProjection Projection;
    return ProjectWorldLocationToDisplayedSheet(View.Point.Data.WorldTransform.GetLocation(), Projection) && Projection.bIsInsideSheet;
}

EWorldMapMarkerLayerRelation URPGWorldMapWidget::ResolveLayerRelation(const FGameZoneMapLayerId& MarkerLayerId) const
{
    if (MarkerLayerId == CurrentLayerId)
    {
        return EWorldMapMarkerLayerRelation::Current;
    }

    const FGameZoneMapLayerCatalogEntry* CurrentLayer = MapLayers.FindByPredicate([this](const FGameZoneMapLayerCatalogEntry& Layer)
        {
            return Layer.ZoneId == CurrentZoneId && Layer.LayerId == CurrentLayerId;
        });
    const FGameZoneMapLayerCatalogEntry* MarkerLayer = MapLayers.FindByPredicate([this, &MarkerLayerId](const FGameZoneMapLayerCatalogEntry& Layer)
        {
            return Layer.ZoneId == CurrentZoneId && Layer.LayerId == MarkerLayerId;
        });
    if (!CurrentLayer || !MarkerLayer || CurrentLayer->ElevationOrder == MarkerLayer->ElevationOrder)
    {
        return EWorldMapMarkerLayerRelation::Other;
    }

    return MarkerLayer->ElevationOrder > CurrentLayer->ElevationOrder
        ? EWorldMapMarkerLayerRelation::Above
        : EWorldMapMarkerLayerRelation::Below;
}

TSubclassOf<URPGMapMarkerWidget> URPGWorldMapWidget::ResolveMarkerWidgetClass(bool bPlayerTracked) const
{
    return bPlayerTracked && EdgeMarkerWidgetClass ? EdgeMarkerWidgetClass : DefaultMarkerWidgetClass;
}

URPGMapMarkerWidget* URPGWorldMapWidget::AcquireMarkerWidget(TSubclassOf<URPGMapMarkerWidget> WidgetClass)
{
    if (!WidgetClass)
    {
        return nullptr;
    }

    URPGMapMarkerWidget* Widget = nullptr;
    const int32 PoolIndex = MarkerWidgetPool.IndexOfByPredicate(
        [WidgetClass](const URPGMapMarkerWidget* Candidate)
        {
            return Candidate && Candidate->GetClass() == WidgetClass.Get();
        });
    if (PoolIndex != INDEX_NONE)
    {
        Widget = MarkerWidgetPool[PoolIndex];
        MarkerWidgetPool.RemoveAtSwap(PoolIndex, EAllowShrinking::No);
    }
    else
    {
        Widget = CreateWidget<URPGMapMarkerWidget>(GetOwningPlayer(), WidgetClass);
        if (Widget)
        {
            Widget->OnHovered().AddUObject(this, &URPGWorldMapWidget::HandleMarkerHovered);
            Widget->OnUnhovered().AddUObject(this, &URPGWorldMapWidget::HandleMarkerUnhovered);
            Widget->OnActivated().AddUObject(this, &URPGWorldMapWidget::HandleMarkerActivated);
        }
    }

    if (Widget && Widget->GetParent() != MarkerCanvas)
    {
        MarkerCanvas->AddChildToCanvas(Widget);
    }
    return Widget;
}

void URPGWorldMapWidget::ReleaseMarkerWidget(const FGuid& PointId)
{
    if (PendingCandidatePointIds.Contains(PointId))
    {
        ClearMarkerCandidates();
    }

    TObjectPtr<URPGMapMarkerWidget> Widget;
    if (!ActiveMarkerWidgets.RemoveAndCopyValue(PointId, Widget) || !Widget)
    {
        return;
    }

    EdgeMarkerPointIds.Remove(PointId);
    Widget->RemoveFromParent();
    Widget->ReleaseMarker();
    if (MarkerWidgetPool.Num() < MarkerWidgetPoolLimit)
    {
        MarkerWidgetPool.Add(Widget);
    }
}

void URPGWorldMapWidget::ReleaseAllMarkerWidgets(bool bRetainPool)
{
    TArray<FGuid> PointIds;
    ActiveMarkerWidgets.GenerateKeyArray(PointIds);
    for (const FGuid& PointId : PointIds)
    {
        ReleaseMarkerWidget(PointId);
    }

    if (!bRetainPool)
    {
        for (URPGMapMarkerWidget* Widget : MarkerWidgetPool)
        {
            if (Widget)
            {
                Widget->RemoveFromParent();
            }
        }
        MarkerWidgetPool.Reset();
    }
}

bool URPGWorldMapWidget::UpdateMarkerWidgetPlacement(
    const FGuid& PointId,
    URPGMapMarkerWidget& Widget,
    FWorldMapMarkerView View)
{
    const bool bUsesEdgeCanvas = EdgeMarkerCanvas && View.bPlayerTracked;
    UCanvasPanel* TargetCanvas = bUsesEdgeCanvas ? EdgeMarkerCanvas.Get() : MarkerCanvas.Get();
    if (!TargetCanvas)
    {
        Widget.SetVisibility(ESlateVisibility::Hidden);
        return false;
    }

    if (Widget.GetParent() != TargetCanvas)
    {
        Widget.RemoveFromParent();
        TargetCanvas->AddChildToCanvas(&Widget);
    }

    UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget.Slot);
    if (!CanvasSlot)
    {
        Widget.SetVisibility(ESlateVisibility::Hidden);
        return false;
    }
    CanvasSlot->SetAutoSize(true);
    CanvasSlot->SetAlignment(FVector2D(0.5f));

    if (!bUsesEdgeCanvas)
    {
        EdgeMarkerPointIds.Remove(PointId);
        View.bClampedToEdge = false;
        View.DirectionFromViewCenter = FVector2D::ZeroVector;
        Widget.ApplyMarkerView(View);
        Widget.SetVisibility(ESlateVisibility::Visible);
        UpdateMarkerWidgetScale(Widget);
        CanvasSlot->SetPosition(WorldLocationToCanvasPosition(View.Point.Data.WorldTransform.GetLocation()));
        return true;
    }

    EdgeMarkerPointIds.Add(PointId);
    const FVector2D ViewportSize = EdgeMarkerCanvas->GetCachedGeometry().GetLocalSize();
    FVector2D MarkerSize = Widget.GetCachedGeometry().GetLocalSize();
    if (MarkerSize.IsNearlyZero())
    {
        MarkerSize = Widget.GetDesiredSize();
    }

    FRPGMapViewportParameters Parameters;
    Parameters.ViewportSize = ViewportSize;
    Parameters.ViewCenterWorldLocation = ViewCenterWorldLocation;
    Parameters.LocalUnitsPerWorldMeter = CanvasUnitsPerWorldMeter * ViewZoom;
    Parameters.EdgePadding = MarkerEdgePadding;
    Parameters.Shape = ERPGMapViewportShape::Rectangle;
    const FRPGMapViewportTransform Transform(Parameters);
    const FRPGMapMarkerPlacement Placement = Transform.PlaceMarker(
        View.Point.Data.WorldTransform.GetLocation(),
        MarkerSize * 0.5f,
        View.bPlayerTracked);
    if (!Placement.bHasValidGeometry)
    {
        Widget.SetVisibility(ESlateVisibility::Hidden);
        return false;
    }

    View.bClampedToEdge = Placement.bIsClampedToEdge;
    View.DirectionFromViewCenter = Placement.DirectionFromCenter;
    Widget.ApplyMarkerView(View);
    Widget.SetRenderScale(FVector2D(1.0f));
    Widget.SetVisibility(Placement.bIsVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
    CanvasSlot->SetPosition(Placement.LocalPosition);
    return true;
}

void URPGWorldMapWidget::UpdateViewTransform()
{
    if (!MapContentRoot)
    {
        return;
    }

    FVector2D ViewportSize = MapContentRoot->GetCachedGeometry().GetLocalSize();
    if (ViewportSize.IsNearlyZero())
    {
        return;
    }

    FWidgetTransform Transform;
    Transform.Scale = FVector2D(ViewZoom);
    Transform.Translation = ViewportSize * 0.5f - WorldLocationToCanvasPosition(ViewCenterWorldLocation) * ViewZoom;
    MapContentRoot->SetRenderTransformPivot(FVector2D::ZeroVector);
    MapContentRoot->SetRenderTransform(Transform);
    LastEdgeMarkerViewportSize = EdgeMarkerCanvas
        ? EdgeMarkerCanvas->GetCachedGeometry().GetLocalSize()
        : FVector2D::ZeroVector;

    bool bHasValidMarkerPlacement = true;
    for (const TPair<FGuid, TObjectPtr<URPGMapMarkerWidget>>& Pair : ActiveMarkerWidgets)
    {
        if (!Pair.Value)
        {
            continue;
        }

        if (!EdgeMarkerPointIds.Contains(Pair.Key))
        {
            UpdateMarkerWidgetScale(*Pair.Value);
            continue;
        }

        FRPGMapPresentationMarker Marker;
        FWorldMapMarkerView View;
        if (!PresentationModel
            || !PresentationModel->TryGetMarker(Pair.Key, Marker)
            || !BuildMarkerView(Marker, SelectedPointId == Pair.Key, View)
            || !UpdateMarkerWidgetPlacement(Pair.Key, *Pair.Value, View))
        {
            bHasValidMarkerPlacement = false;
        }
    }
    bViewTransformDirty = !bHasValidMarkerPlacement;
}

void URPGWorldMapWidget::UpdateMarkerWidgetScale(URPGMapMarkerWidget& Widget) const
{
    const float SafeZoom = FMath::Max(ViewZoom, 0.01f);
    Widget.SetRenderTransformPivot(FVector2D(0.5f));
    Widget.SetRenderScale(FVector2D(1.0f / SafeZoom));
}

void URPGWorldMapWidget::HandleMarkerHovered(const FGuid& PointId)
{
    FWorldMapMarkerView View;
    if (TryGetPresentedMarkerView(PointId, SelectedPointId == PointId, View))
    {
        OnWorldMapMarkerHovered(View);
    }
}

void URPGWorldMapWidget::HandleMarkerUnhovered(const FGuid& PointId)
{
    OnWorldMapMarkerUnhovered(PointId);
}

void URPGWorldMapWidget::HandleMarkerActivated(const FGuid& PointId)
{
    if (!bActionPending && !bAwaitingActionConfirmation)
    {
        ClearMarkerCandidates();
        SelectMarker(PointId);
    }
}

void URPGWorldMapWidget::ResolveMarkerClick(const FGeometry& InGeometry, const FVector2D& LocalPosition)
{
    TArray<FMapMarkerSelectionTarget> Targets;
    Targets.Reserve(ActiveMarkerWidgets.Num());

    for (const TPair<FGuid, TObjectPtr<URPGMapMarkerWidget>>& Pair : ActiveMarkerWidgets)
    {
        URPGMapMarkerWidget* Widget = Pair.Value;
        if (!Widget || !Widget->IsVisible())
        {
            continue;
        }

        const FGeometry& MarkerGeometry = Widget->GetCachedGeometry();
        const FVector2D MarkerSize = MarkerGeometry.GetLocalSize();
        const FVector2D Corners[] = {
            InGeometry.AbsoluteToLocal(MarkerGeometry.LocalToAbsolute(FVector2D::ZeroVector)),
            InGeometry.AbsoluteToLocal(MarkerGeometry.LocalToAbsolute(FVector2D(MarkerSize.X, 0.0f))),
            InGeometry.AbsoluteToLocal(MarkerGeometry.LocalToAbsolute(FVector2D(0.0f, MarkerSize.Y))),
            InGeometry.AbsoluteToLocal(MarkerGeometry.LocalToAbsolute(MarkerSize)),
        };

        FMapMarkerSelectionTarget& Target = Targets.AddDefaulted_GetRef();
        Target.View = Widget->GetMarkerView();
        Target.BoundsMinimum = Corners[0];
        Target.BoundsMaximum = Corners[0];
        for (int32 CornerIndex = 1; CornerIndex < UE_ARRAY_COUNT(Corners); ++CornerIndex)
        {
            Target.BoundsMinimum.X = FMath::Min(Target.BoundsMinimum.X, Corners[CornerIndex].X);
            Target.BoundsMinimum.Y = FMath::Min(Target.BoundsMinimum.Y, Corners[CornerIndex].Y);
            Target.BoundsMaximum.X = FMath::Max(Target.BoundsMaximum.X, Corners[CornerIndex].X);
            Target.BoundsMaximum.Y = FMath::Max(Target.BoundsMaximum.Y, Corners[CornerIndex].Y);
        }
        Target.ZOrder = Target.View.MarkerType ? Target.View.MarkerType->GetZOrder() : 0;
    }

    TArray<FWorldMapMarkerView> Candidates = FMapMarkerSelectionResolver::Resolve(
        LocalPosition,
        MarkerSelectionPadding,
        Targets);
    if (Candidates.IsEmpty())
    {
        ClearMarkerCandidates();
        ClearMarkerSelectionInternal();
        return;
    }

    if (Candidates.Num() == 1)
    {
        ClearMarkerCandidates();
        SelectMarker(Candidates[0].Point.Data.PointId);
        return;
    }

    for (FWorldMapMarkerView& Candidate : Candidates)
    {
        Candidate.bSelected = false;
    }
    ClearMarkerSelectionInternal();
    SetMarkerCandidates(LocalPosition, Candidates);
}

void URPGWorldMapWidget::SetMarkerCandidates(
    const FVector2D& AnchorLocalPosition,
    const TArray<FWorldMapMarkerView>& Candidates)
{
    PendingCandidatePointIds.Reset();
    for (const FWorldMapMarkerView& Candidate : Candidates)
    {
        PendingCandidatePointIds.Add(Candidate.Point.Data.PointId);
    }

    FMapMarkerCandidateSet CandidateSet;
    CandidateSet.AnchorLocalPosition = AnchorLocalPosition;
    CandidateSet.Candidates = Candidates;
    OnMapMarkerCandidatesChanged(CandidateSet);
}

void URPGWorldMapWidget::ClearMarkerCandidates(bool bNotifyBlueprint)
{
    if (PendingCandidatePointIds.IsEmpty())
    {
        return;
    }

    PendingCandidatePointIds.Reset();
    if (bNotifyBlueprint)
    {
        const FMapMarkerCandidateSet EmptyCandidateSet;
        OnMapMarkerCandidatesChanged(EmptyCandidateSet);
    }
}

void URPGWorldMapWidget::SelectMarker(const FGuid& PointId)
{
    ClearMarkerCandidates();
    if (SelectedPointId == PointId || !ActiveMarkerWidgets.Contains(PointId))
    {
        return;
    }

    const FGuid PreviousPointId = SelectedPointId;
    SelectedPointId = PointId;
    if (PreviousPointId.IsValid())
    {
        ReconcileMarker(PreviousPointId);
    }
    ReconcileMarker(SelectedPointId);
    RefreshSelectedMarker();
}

void URPGWorldMapWidget::ClearMarkerSelectionInternal()
{
    const FGuid PreviousPointId = SelectedPointId;
    SelectedPointId.Invalidate();
    bAwaitingActionConfirmation = false;
    PendingConfirmationActionTag = {};
    PendingConfirmationAction = {};
    CurrentActionOptions.Reset();

    if (PreviousPointId.IsValid())
    {
        ReconcileMarker(PreviousPointId);
    }

    OnWorldMapSelectionChanged(false, {});
    OnWorldMapActionsChanged(CurrentActionOptions);
}

void URPGWorldMapWidget::RefreshSelectedMarker()
{
    if (!SelectedPointId.IsValid())
    {
        return;
    }

    FWorldMapMarkerView View;
    if (!TryGetPresentedMarkerView(SelectedPointId, true, View))
    {
        ClearMarkerSelectionInternal();
        return;
    }

    OnWorldMapSelectionChanged(true, View);
    CurrentActionOptions.Reset();
    if (GameZoneSubsystem)
    {
        GameZoneSubsystem->GetMapMarkerActionOptions(
            SelectedPointId,
            EMapMarkerDisplayMode::WorldMap,
            GetOwningPlayer(),
            CurrentActionOptions);
    }
    OnWorldMapActionsChanged(CurrentActionOptions);
}

bool URPGWorldMapWidget::BeginMarkerAction(const FGameZoneMarkerActionOption& Action)
{
    if (!GameZoneSubsystem || !SelectedPointId.IsValid() || bActionPending)
    {
        return false;
    }

    bActionPending = true;
    ActiveActionExecutionId.Invalidate();
    OnWorldMapActionPendingChanged(true);

    FOnGameZoneMarkerActionCompleted Completion;
    Completion.BindDynamic(this, &URPGWorldMapWidget::HandleMarkerActionCompleted);
    const FGuid ExecutionId = GameZoneSubsystem->ExecuteMapMarkerAction(
        SelectedPointId,
        Action.Definition.ActionTag,
        EMapMarkerDisplayMode::WorldMap,
        GetOwningPlayer(),
        Completion);

    if (!ExecutionId.IsValid())
    {
        if (bActionPending)
        {
            bActionPending = false;
            OnWorldMapActionPendingChanged(false);
            RefreshSelectedMarker();
        }
        return false;
    }

    if (bActionPending)
    {
        ActiveActionExecutionId = ExecutionId;
    }
    return true;
}

void URPGWorldMapWidget::HandlePresentationReady(const FGameZonePresentationSnapshot& Snapshot)
{
    if (!GameZoneSubsystem || !PresentationModel || !PresentationModel->IsReady())
    {
        return;
    }

    PresentationHandle = Snapshot.Handle;
    MapLayers = Snapshot.MapLayers;
    MapSheets = Snapshot.MapSheets;
    CurrentMapWorldBounds = {};
    if (const FGameZoneMapWorldBounds* Bounds = Snapshot.MapWorldBounds.FindByPredicate([this](const FGameZoneMapWorldBounds& Candidate)
        {
            return Candidate.ZoneId == CurrentZoneId;
        }))
    {
        CurrentMapWorldBounds = *Bounds;
    }
    SetViewCenterWorldLocation(ViewCenterWorldLocation);
    bPresentationReady = true;

    FocusOnPlayer();
    ResetMarkers();
    OnWorldMapPresentationReady();
}

void URPGWorldMapWidget::HandlePresentationFailed()
{
    bPresentationReady = false;
    OnWorldMapPresentationFailed();
}

void URPGWorldMapWidget::HandleMapTextureReady(const FGameZoneMapTextureResult& Result)
{
    if (Result.ZoneId != CurrentZoneId || Result.SheetId != CurrentSheet.SheetId)
    {
        return;
    }

    CurrentTexture = Result.bSucceeded ? Result.Texture : nullptr;
    OnWorldMapTextureChanged(Result);
}

void URPGWorldMapWidget::HandleMapMarkerChange(const FRPGMapPresentationMarkerChange& Change)
{
    if (!bPresentationReady || !PresentationModel)
    {
        return;
    }

    if (Change.Kind == EMapMarkerChangeKind::Removed)
    {
        RemoveMarker(Change.PointId);
        return;
    }

    ApplyPresentationMarker(Change.Marker);
}

void URPGWorldMapWidget::HandleMarkerActionCompleted(const FGameZoneMarkerActionResult& Result)
{
    ActiveActionExecutionId.Invalidate();
    bActionPending = false;
    OnWorldMapActionPendingChanged(false);
    OnWorldMapActionCompleted(Result);
    RefreshSelectedMarker();
}
