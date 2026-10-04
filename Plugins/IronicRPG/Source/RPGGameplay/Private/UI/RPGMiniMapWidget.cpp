// Copyright Ironic Studio. All Rights Reserved.

#include "UI/RPGMiniMapWidget.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/Image.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/RPGMapPresentationModel.h"
#include "UI/RPGMiniMapProjection.h"
#include "UI/RPGMiniMapMarkerRenderer.h"
#include "UI/MapTrackingSubsystem.h"

namespace
{
bool AreMiniMapViewsEqual(const FMiniMapViewState& A, const FMiniMapViewState& B)
{
    return A.CenterWorldLocation.Equals(B.CenterWorldLocation)
        && A.ViewportSize.Equals(B.ViewportSize)
        && A.ViewRangeMeters == B.ViewRangeMeters
        && A.LocalUnitsPerWorldMeter == B.LocalUnitsPerWorldMeter
        && A.bPresentationReady == B.bPresentationReady
        && A.bHasFollowTarget == B.bHasFollowTarget
        && A.FocusedMarkerId == B.FocusedMarkerId
        && A.bHasValidViewport == B.bHasValidViewport
        && A.bHasSheet == B.bHasSheet
        && A.MapCenterUV.Equals(B.MapCenterUV) && A.MapUVAxisX.Equals(B.MapUVAxisX) && A.MapUVAxisY.Equals(B.MapUVAxisY)
        && A.bHasMapProjection == B.bHasMapProjection
        && A.PawnAngle == B.PawnAngle && A.CameraAngle == B.CameraAngle
        && A.bHasPawnDirection == B.bHasPawnDirection && A.bHasCameraDirection == B.bHasCameraDirection;
}
}

void URPGMiniMapWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (bConstructed || IsDesignTime())
    {
        return;
    }

    bConstructed = true;
    InitializeMapMaterial();
    ViewRangeMeters = ClampRange(FMath::IsFinite(ViewRangeMeters) && ViewRangeMeters > 0.0f ? ViewRangeMeters : 100.0f);
    PresentationModel = NewObject<URPGMapPresentationModel>(this);
    MarkerRenderer = NewObject<URPGMiniMapMarkerRenderer>(this);
    FRPGMiniMapMarkerSettings MarkerSettings;
    MarkerSettings.WidgetClass = DefaultMarkerWidgetClass;
    MarkerSettings.EdgeWidgetClass = EdgeMarkerWidgetClass;
    MarkerSettings.Size = MarkerSize;
    MarkerSettings.EdgePadding = MarkerEdgePadding;
    MarkerSettings.RetentionMeters = MarkerRetentionMeters;
    MarkerSettings.PoolLimit = MarkerWidgetPoolLimit;
    MarkerRenderer->Initialize(*this, MarkerCanvas, EdgeMarkerCanvas, *PresentationModel, MarkerSettings);
    PresentationModel->OnMarkerChanged.AddUObject(this, &URPGMiniMapWidget::HandleMarkerChanged);
    PresentationModel->OnReady.AddUObject(this, &URPGMiniMapWidget::HandlePresentationReady);
    PresentationModel->OnFailed.AddUObject(this, &URPGMiniMapWidget::HandlePresentationFailed);
    PresentationModel->OnTextureReady.AddUObject(this, &URPGMiniMapWidget::HandleMapTextureReady);
    FWorldDelegates::OnWorldCleanup.AddUObject(this, &URPGMiniMapWidget::HandleWorldCleanup);
    RefreshMiniMap();
}

void URPGMiniMapWidget::NativeDestruct()
{
    bConstructed = false;
    FWorldDelegates::OnWorldCleanup.RemoveAll(this);
    if (APlayerController* Controller = BoundController.Get())
    {
        Controller->OnPossessedPawnChanged.RemoveDynamic(this, &URPGMiniMapWidget::HandlePawnChanged);
    }
    BoundController.Reset();
    if (TrackingSubsystem)
    {
        TrackingSubsystem->OnTrackingChanged.RemoveDynamic(this, &URPGMiniMapWidget::HandleTrackingChanged);
        TrackingSubsystem = nullptr;
    }
    if (GameZoneSubsystem)
    {
        GameZoneSubsystem->OnGameZoneTravelCompleted.RemoveDynamic(this, &URPGMiniMapWidget::HandleTravelCompleted);
    }
    ResetSession();
    if (PresentationModel)
    {
        PresentationModel->OnReady.RemoveAll(this);
        PresentationModel->OnFailed.RemoveAll(this);
        PresentationModel->OnTextureReady.RemoveAll(this);
        PresentationModel->OnMarkerChanged.RemoveAll(this);
        PresentationModel = nullptr;
    }
    if (MarkerRenderer) { MarkerRenderer->Reset(true); }
    MarkerRenderer = nullptr;
    GameZoneSubsystem = nullptr;
    CurrentSheet = {};
    ViewState = {};
    ViewState.ViewRangeMeters = ViewRangeMeters;
    LastViewportSize = FVector2D::ZeroVector;
    bHasVisibleFrame = false;
    CleanedUpWorld.Reset();
    if (MapImage && MapImage->GetBrush().GetResourceObject() == MapDynamicMaterial)
    {
        MapImage->SetBrushFromMaterial(nullptr);
    }
    MapDynamicMaterial = nullptr;
    Super::NativeDestruct();
}

void URPGMiniMapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!bConstructed) { return; }
    // Slate does not tick descendants of hidden ancestors. Refresh after a gap, before presenting the first visible sample.
    const bool bVisible = IsVisible();
    const bool bResuming = bVisible && (!bHasVisibleFrame || GFrameCounter > LastVisibleFrame + 1);
    bHasVisibleFrame = bVisible;
    LastVisibleFrame = GFrameCounter;
    LastViewportSize = MyGeometry.GetLocalSize();
    RefreshState(false);
    if (bConstructed && bResuming && PresentationModel) { PresentationModel->RefreshTrackedMarkers(); }
    if (bConstructed && MarkerRenderer) { MarkerRenderer->Update(ViewState, CurrentSheet, GameZoneSubsystem); }
}

float URPGMiniMapWidget::ClampRange(float Range) const
{
    const float Minimum = FMath::IsFinite(MinimumViewRangeMeters) ? FMath::Max(0.01f, MinimumViewRangeMeters) : 1.0f;
    const float Maximum = FMath::IsFinite(MaximumViewRangeMeters) ? FMath::Max(Minimum, MaximumViewRangeMeters) : FMath::Max(Minimum, 10000.0f);
    return FMath::Clamp(Range, Minimum, Maximum);
}

void URPGMiniMapWidget::SetViewRangeMeters(float NewRangeMeters)
{
    if (!FMath::IsFinite(NewRangeMeters) || NewRangeMeters <= 0.0f)
    {
        return;
    }

    ViewRangeMeters = ClampRange(NewRangeMeters);
    if (bConstructed)
    {
        RefreshState(false);
    }
    else
    {
        ViewState.ViewRangeMeters = ViewRangeMeters;
    }
}

bool URPGMiniMapWidget::IsPresentationActive() const
{
    return PresentationModel && PresentationModel->IsActive();
}

bool URPGMiniMapWidget::FocusOnMarker(const FGuid& PointId)
{
    FVector MarkerLocation;
    if (!bConstructed || !PresentationModel || !PresentationModel->IsReady()
        || !PresentationModel->TryGetMarkerWorldLocation(PointId, MarkerLocation))
    {
        return false;
    }

    FocusedMarkerId = PointId;
    RefreshState(true);
    return true;
}

bool URPGMiniMapWidget::FocusOnPlayer()
{
    FocusedMarkerId.Invalidate();
    if (!bConstructed)
    {
        ViewState.FocusedMarkerId.Invalidate();
        return false;
    }

    RefreshState(true);
    return ViewState.bHasFollowTarget && !ViewState.FocusedMarkerId.IsValid();
}

void URPGMiniMapWidget::RefreshMiniMap()
{
    if (bPresentationFailed)
    {
        ResetSession();
    }
    RefreshState(true);
    if (PresentationModel)
    {
        PresentationModel->RefreshTrackedMarkers();
    }
}

void URPGMiniMapWidget::BindSources()
{
    UMapTrackingSubsystem* NewTracking = GetOwningLocalPlayer() ? GetOwningLocalPlayer()->GetSubsystem<UMapTrackingSubsystem>() : nullptr;
    if (TrackingSubsystem != NewTracking)
    {
        if (TrackingSubsystem) { TrackingSubsystem->OnTrackingChanged.RemoveDynamic(this, &URPGMiniMapWidget::HandleTrackingChanged); }
        TrackingSubsystem = NewTracking;
        if (TrackingSubsystem) { TrackingSubsystem->OnTrackingChanged.AddUniqueDynamic(this, &URPGMiniMapWidget::HandleTrackingChanged); }
    }
    // Read after binding, including late LocalPlayer arrival or reconstruction while a target is already tracked.
    if (MarkerRenderer) { MarkerRenderer->SetTrackedMarker(TrackingSubsystem ? TrackingSubsystem->GetTrackedMarkerId() : FGuid()); }
    UGameZoneSubsystem* NewSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGameZoneSubsystem>() : nullptr;
    if (GameZoneSubsystem != NewSubsystem)
    {
        if (GameZoneSubsystem)
        {
            GameZoneSubsystem->OnGameZoneTravelCompleted.RemoveDynamic(this, &URPGMiniMapWidget::HandleTravelCompleted);
        }
        ResetSession();
        GameZoneSubsystem = NewSubsystem;
        if (GameZoneSubsystem)
        {
            GameZoneSubsystem->OnGameZoneTravelCompleted.AddUniqueDynamic(this, &URPGMiniMapWidget::HandleTravelCompleted);
        }
    }

    APlayerController* Controller = GetOwningPlayer();
    if (BoundController.Get() != Controller)
    {
        if (APlayerController* Previous = BoundController.Get())
        {
            Previous->OnPossessedPawnChanged.RemoveDynamic(this, &URPGMiniMapWidget::HandlePawnChanged);
        }
        BoundController = Controller;
        if (Controller)
        {
            Controller->OnPossessedPawnChanged.AddUniqueDynamic(this, &URPGMiniMapWidget::HandlePawnChanged);
        }
    }
}

void URPGMiniMapWidget::ResetSession()
{
    FocusedMarkerId.Invalidate();
    ClearTexture();
    if (PresentationModel)
    {
        PresentationModel->Stop();
    }
    // Pooled UserWidgets cache their World/player context; never carry them into a new session/world.
    if (MarkerRenderer) { MarkerRenderer->Reset(true); }
    SessionWorld.Reset();
    CurrentZoneId = {};
    bPresentationFailed = false;
}

void URPGMiniMapWidget::RefreshState(bool bForceSheetQuery)
{
    if (!bConstructed || bRefreshing)
    {
        return;
    }
    TGuardValue<bool> RefreshGuard(bRefreshing, true);
    BindSources();

    UWorld* World = GetWorld();
    const bool bCanUseWorld = World && CleanedUpWorld.Get() != World && GameZoneSubsystem && !GameZoneSubsystem->IsTravelling();
    const FRPGId ZoneId = bCanUseWorld ? GameZoneSubsystem->GetCurrentContext().ZoneId : FRPGId();
    if (CurrentZoneId != ZoneId || (SessionWorld.IsValid() && SessionWorld.Get() != World))
    {
        ResetSession();
        bForceSheetQuery = true;
    }

    if (ZoneId.IsValid() && PresentationModel && !PresentationModel->IsActive() && !bPresentationFailed)
    {
        CurrentZoneId = ZoneId;
        SessionWorld = World;
        FRPGMapPresentationConfig Config;
        Config.ZoneIds = {ZoneId};
        Config.Mode = EMapMarkerDisplayMode::MiniMap;
        Config.TrackedMarkerRefreshInterval = TrackedMarkerRefreshInterval;
        PresentationModel->Start(*GameZoneSubsystem, Config);
    }
    if (bConstructed)
    {
        UpdateFollowState(bForceSheetQuery);
    }
}

void URPGMiniMapWidget::UpdateFollowState(bool bForceSheetQuery)
{
    FMiniMapViewState Next;
    Next.ViewRangeMeters = ViewRangeMeters;
    Next.bPresentationReady = PresentationModel && PresentationModel->IsReady();
    if (FMath::IsFinite(LastViewportSize.X) && FMath::IsFinite(LastViewportSize.Y) && LastViewportSize.X > 0.0 && LastViewportSize.Y > 0.0)
    {
        Next.ViewportSize = LastViewportSize;
        Next.LocalUnitsPerWorldMeter = FMath::Min(LastViewportSize.X, LastViewportSize.Y) / (2.0 * ViewRangeMeters);
        Next.bHasValidViewport = FMath::IsFinite(Next.LocalUnitsPerWorldMeter) && Next.LocalUnitsPerWorldMeter > 0.0f;
        if (!Next.bHasValidViewport) { Next.LocalUnitsPerWorldMeter = 0.0f; }
    }

    APawn* Pawn = GetOwningPlayerPawn();
    if (CurrentZoneId.IsValid() && IsValid(Pawn) && Pawn->GetWorld() == SessionWorld.Get() && !Pawn->GetActorLocation().ContainsNaN())
    {
        Next.CenterWorldLocation = Pawn->GetActorLocation();
        Next.bHasFollowTarget = true;
        Next.bHasPawnDirection = FRPGMiniMapProjection::TryGetNorthUpAngle(Pawn->GetActorRotation().Yaw, Next.PawnAngle);
        if (APlayerController* Controller = GetOwningPlayer())
        {
            if (IsValid(Controller->PlayerCameraManager))
            {
                Next.bHasCameraDirection = FRPGMiniMapProjection::TryGetNorthUpAngle(
                    Controller->PlayerCameraManager->GetCameraRotation().Yaw, Next.CameraAngle);
            }
            if (!Next.bHasCameraDirection)
            {
                Next.bHasCameraDirection = FRPGMiniMapProjection::TryGetNorthUpAngle(Controller->GetControlRotation().Yaw, Next.CameraAngle);
            }
        }
    }

    if (Next.bPresentationReady && FocusedMarkerId.IsValid())
    {
        FVector MarkerLocation;
        if (PresentationModel->TryGetMarkerWorldLocation(FocusedMarkerId, MarkerLocation))
        {
            Next.CenterWorldLocation = MarkerLocation;
            Next.FocusedMarkerId = FocusedMarkerId;
            Next.bHasFollowTarget = true;
        }
        else
        {
            FocusedMarkerId.Invalidate();
        }
    }

    FResolvedGameZoneMapSheet NextSheet = CurrentSheet;
    bool bQueriedSheet = false;
    if (!Next.bPresentationReady || !Next.bHasFollowTarget)
    {
        NextSheet = {};
    }
    else if (bForceSheetQuery || !ViewState.bPresentationReady || !ViewState.bHasFollowTarget
        || !ViewState.CenterWorldLocation.Equals(Next.CenterWorldLocation))
    {
        bQueriedSheet = true;
        if (!GameZoneSubsystem->ResolveMapSheetAtLocation(CurrentZoneId, Next.CenterWorldLocation, NextSheet))
        {
            NextSheet = {};
        }
    }

    Next.bHasSheet = NextSheet.SheetId.IsValid();
    if (Next.bHasSheet && Next.bHasValidViewport)
    {
        const FRPGMiniMapUVBasis Basis = FRPGMiniMapProjection::BuildUVBasis(
            NextSheet, Next.CenterWorldLocation, Next.ViewportSize, Next.LocalUnitsPerWorldMeter);
        Next.MapCenterUV = Basis.Center;
        Next.MapUVAxisX = Basis.AxisX;
        Next.MapUVAxisY = Basis.AxisY;
        Next.bHasMapProjection = Basis.bIsValid;
    }
    const bool bSheetChanged = NextSheet.ZoneId != CurrentSheet.ZoneId || NextSheet.SheetId != CurrentSheet.SheetId
        || NextSheet.LayerId != CurrentSheet.LayerId || NextSheet.MapBakeRevision != CurrentSheet.MapBakeRevision;
    const bool bSheetUnavailable = (ViewState.bHasSheet && !Next.bHasSheet)
        || (bQueriedSheet && !Next.bHasSheet && (!ViewState.bPresentationReady || !ViewState.bHasFollowTarget));
    const bool bViewChanged = !AreMiniMapViewsEqual(ViewState, Next);
    CurrentSheet = NextSheet;
    ViewState = Next;
    const bool bRequestTexture = Next.bHasSheet && (bSheetChanged || (bForceSheetQuery && !CurrentTexture && !bTexturePending));
    if (bSheetChanged || !Next.bHasSheet)
    {
        ClearTexture();
    }
    bMaterialDirty |= bViewChanged;
    UpdateMapMaterial();

    // The Model owns request IDs and stale completion filtering. Do not duplicate them in the Widget.
    bool bRequestFailed = false;
    if (bRequestTexture && PresentationModel)
    {
        bTexturePending = PresentationModel->RequestSheetTexture(CurrentZoneId, CurrentSheet.SheetId);
        bRequestFailed = !bTexturePending;
    }

    // Commit the complete state before invoking Blueprint; callbacks may remove this Widget.
    if (bSheetChanged && Next.bHasSheet)
    {
        OnMiniMapSheetChanged(CurrentSheet);
    }
    else if (bSheetUnavailable)
    {
        OnMiniMapSheetUnavailable();
    }
    if (bConstructed && bViewChanged)
    {
        OnMiniMapViewChanged(ViewState);
    }
    if (bConstructed && bRequestFailed && CurrentSheet.ZoneId == NextSheet.ZoneId && CurrentSheet.SheetId == NextSheet.SheetId)
    {
        FGameZoneMapTextureResult Result;
        Result.ZoneId = CurrentZoneId;
        Result.SheetId = CurrentSheet.SheetId;
        OnMiniMapTextureChanged(Result);
    }
}

void URPGMiniMapWidget::InitializeMapMaterial()
{
    MapDynamicMaterial = MapMaterial && MapMaterial->IsUIMaterial() ? UMaterialInstanceDynamic::Create(MapMaterial, this) : nullptr;
    if (MapMaterial && !MapDynamicMaterial)
    {
        UE_LOG(LogTemp, Warning, TEXT("MiniMap MapMaterial must use the User Interface domain: %s"), *MapMaterial->GetPathName());
    }
    if (MapImage)
    {
        MapImage->SetBrushFromMaterial(MapDynamicMaterial);
        MapImage->SetVisibility(ESlateVisibility::Hidden);
    }
    bMaterialDirty = true;
}

void URPGMiniMapWidget::ClearTexture()
{
    if (!CurrentTexture && !bTexturePending && !bMaterialDirty)
    {
        return;
    }
    bTexturePending = false;
    if (CurrentTexture)
    {
        CurrentTexture = nullptr;
        bMaterialDirty = true;
    }
    // Hide immediately, including during cleanup when no further Tick will occur.
    if (MapImage) { MapImage->SetVisibility(ESlateVisibility::Hidden); }
    if (MapDynamicMaterial)
    {
        MapDynamicMaterial->SetScalarParameterValue(TEXT("MapVisible"), 0.0f);
        // MID ignores null texture assignments. Bind an engine-owned placeholder while hidden to release the old override.
        MapDynamicMaterial->SetTextureParameterValue(TEXT("MapTexture"), GEngine ? GEngine->DefaultTexture.Get() : nullptr);
    }
}

void URPGMiniMapWidget::UpdateMapMaterial()
{
    if (!bMaterialDirty)
    {
        return;
    }
    bMaterialDirty = false;
    const bool bShowMap = bConstructed && ViewState.bHasMapProjection && CurrentTexture && MapDynamicMaterial;
    if (MapDynamicMaterial)
    {
        const auto ToVector = [](const FVector2D& Value) { return FLinearColor(Value.X, Value.Y, 0.0f, 0.0f); };
        MapDynamicMaterial->SetVectorParameterValue(TEXT("MapCenterUV"), ToVector(ViewState.MapCenterUV));
        MapDynamicMaterial->SetVectorParameterValue(TEXT("MapUVAxisX"), ToVector(ViewState.MapUVAxisX));
        MapDynamicMaterial->SetVectorParameterValue(TEXT("MapUVAxisY"), ToVector(ViewState.MapUVAxisY));
        MapDynamicMaterial->SetVectorParameterValue(TEXT("MapViewportSize"), ToVector(ViewState.ViewportSize));
        UTexture2D* BoundTexture = CurrentTexture ? CurrentTexture.Get() : (GEngine ? GEngine->DefaultTexture.Get() : nullptr);
        MapDynamicMaterial->SetTextureParameterValue(TEXT("MapTexture"), BoundTexture);
        MapDynamicMaterial->SetScalarParameterValue(TEXT("MapVisible"), bShowMap ? 1.0f : 0.0f);
    }
    if (MapImage) { MapImage->SetVisibility(bShowMap ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden); }
}

void URPGMiniMapWidget::HandleMapTextureReady(const FGameZoneMapTextureResult& Result)
{
    if (!bConstructed || !bTexturePending || !ViewState.bHasSheet
        || Result.ZoneId != CurrentZoneId || Result.SheetId != CurrentSheet.SheetId)
    {
        return;
    }
    bTexturePending = false;
    CurrentTexture = Result.bSucceeded ? Result.Texture : nullptr;
    bMaterialDirty = true;
    UpdateMapMaterial();
    OnMiniMapTextureChanged(Result);
}

void URPGMiniMapWidget::HandlePresentationReady(const FGameZonePresentationSnapshot& Snapshot)
{
    if (bConstructed && PresentationModel && PresentationModel->IsReady())
    {
        if (MarkerRenderer) { MarkerRenderer->Rebuild(Snapshot); }
        RefreshState(true);
        if (bConstructed && PresentationModel && PresentationModel->IsReady())
        {
            OnMiniMapPresentationReady();
        }
    }
}

void URPGMiniMapWidget::HandleMarkerChanged(const FRPGMapPresentationMarkerChange& Change)
{
    if (bConstructed && MarkerRenderer) { MarkerRenderer->RecordChange(Change); }
}

void URPGMiniMapWidget::HandlePresentationFailed()
{
    bPresentationFailed = true;
    if (bConstructed)
    {
        UpdateFollowState(true);
        OnMiniMapPresentationFailed();
    }
}

void URPGMiniMapWidget::HandleTravelCompleted(const FGameZoneContext& Context)
{
    // This notification is raised before IsTravelling becomes false. The next frame can start the new session.
    ResetSession();
    // A surviving HUD must not keep UUserWidget's cached World or a player context pinned to the old World.
    const FLocalPlayerContext& PreviousContext = GetPlayerContext();
    ULocalPlayer* LocalPlayer = PreviousContext.IsInitialized() ? PreviousContext.GetLocalPlayer() : nullptr;
    SetPlayerContext(LocalPlayer ? FLocalPlayerContext(LocalPlayer, GameZoneSubsystem->GetWorld()) : FLocalPlayerContext());
    RefreshState(true);
}

void URPGMiniMapWidget::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
    RefreshMiniMap();
}

void URPGMiniMapWidget::HandleTrackingChanged(FGuid PreviousPointId, FGuid CurrentPointId)
{
    // Callbacks only invalidate cached visuals. Hidden HUDs must not construct or lay out marker widgets.
    if (bConstructed && MarkerRenderer) { MarkerRenderer->SetTrackedMarker(CurrentPointId); }
}

void URPGMiniMapWidget::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
    if (World && (World == SessionWorld.Get() || World == GetWorld()))
    {
        CleanedUpWorld = World;
        ResetSession();
        UpdateFollowState(true);
    }
}

void URPGMiniMapWidget::OnMiniMapPresentationReady_Implementation() {}
void URPGMiniMapWidget::OnMiniMapPresentationFailed_Implementation() {}
void URPGMiniMapWidget::OnMiniMapSheetChanged_Implementation(const FResolvedGameZoneMapSheet& Sheet) {}
void URPGMiniMapWidget::OnMiniMapSheetUnavailable_Implementation() {}
void URPGMiniMapWidget::OnMiniMapViewChanged_Implementation(const FMiniMapViewState& View) {}
void URPGMiniMapWidget::OnMiniMapTextureChanged_Implementation(const FGameZoneMapTextureResult& Result) {}
