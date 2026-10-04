// Copyright Ironic Studio. All Rights Reserved.

#include "UI/RPGMapPresentationModel.h"

#include "Assets/RPGAssetLibrary.h"
#include "Assets/RPGPrimaryAsset.h"
#include "Engine/StreamableManager.h"
#include "Levels/MapMarkerTypeAsset.h"
#include "TimerManager.h"

namespace
{
bool IsGuidLess(const FGuid& Left, const FGuid& Right)
{
    if (Left.A != Right.A)
    {
        return Left.A < Right.A;
    }
    if (Left.B != Right.B)
    {
        return Left.B < Right.B;
    }
    if (Left.C != Right.C)
    {
        return Left.C < Right.C;
    }
    return Left.D < Right.D;
}
}

bool URPGMapPresentationModel::Start(UGameZoneSubsystem& InSubsystem, const FRPGMapPresentationConfig& InConfig)
{
    Stop();

    for (const FRPGId& ZoneId : InConfig.ZoneIds)
    {
        if (ZoneId.IsValid())
        {
            ZoneIds.Add(ZoneId);
        }
    }

    if (ZoneIds.IsEmpty()
        || InConfig.Mode == EMapMarkerDisplayMode::None
        || !FMath::IsFinite(InConfig.TrackedMarkerRefreshInterval)
        || InConfig.TrackedMarkerRefreshInterval < 0.0f)
    {
        ResetState();
        OnFailed.Broadcast();
        return false;
    }

    Subsystem = &InSubsystem;
    Mode = InConfig.Mode;
    TrackedMarkerRefreshInterval = InConfig.TrackedMarkerRefreshInterval;
    Subsystem->OnMapMarkerChange.AddDynamic(this, &URPGMapPresentationModel::HandleMapMarkerChange);

    TArray<FRPGId> RequestedZoneIds = ZoneIds.Array();
    RequestedZoneIds.Sort([](const FRPGId& Left, const FRPGId& Right)
        {
            return Left.ToString() < Right.ToString();
        });

    FOnGameZonePresentationReady Completion;
    Completion.BindDynamic(this, &URPGMapPresentationModel::HandlePresentationReady);
    PresentationHandle = Subsystem->BeginMapPresentation(RequestedZoneIds, Mode, Completion);
    if (!PresentationHandle.IsValid())
    {
        Subsystem->OnMapMarkerChange.RemoveDynamic(this, &URPGMapPresentationModel::HandleMapMarkerChange);
        ResetState();
        OnFailed.Broadcast();
        return false;
    }

    return true;
}

void URPGMapPresentationModel::Stop()
{
    ++Generation;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TrackedMarkerTimer);
    }

    for (TPair<FRPGId, TSharedPtr<FStreamableHandle>>& Pair : PendingMarkerTypeLoads)
    {
        if (Pair.Value.IsValid() && !Pair.Value->HasLoadCompleted())
        {
            Pair.Value->CancelHandle();
        }
    }
    PendingMarkerTypeLoads.Reset();

    if (Subsystem)
    {
        Subsystem->OnMapMarkerChange.RemoveDynamic(this, &URPGMapPresentationModel::HandleMapMarkerChange);
        Subsystem->EndMapPresentation(PresentationHandle);
    }

    ResetState();
}

bool URPGMapPresentationModel::RequestSheetTexture(const FRPGId& ZoneId, const FGameZoneMapSheetId& SheetId)
{
    PendingTextureRequestId.Invalidate();
    PendingTextureZoneId = {};
    PendingTextureSheetId = {};
    if (!bReady || !Subsystem || !PresentationHandle.IsValid() || !ZoneIds.Contains(ZoneId) || !SheetId.IsValid())
    {
        return false;
    }

    FOnGameZoneMapTextureReady Completion;
    Completion.BindDynamic(this, &URPGMapPresentationModel::HandleMapTextureReady);
    const FGuid RequestId = Subsystem->RequestMapSheetTexture(PresentationHandle, ZoneId, SheetId, Completion);
    if (!RequestId.IsValid())
    {
        return false;
    }

    PendingTextureRequestId = RequestId;
    PendingTextureZoneId = ZoneId;
    PendingTextureSheetId = SheetId;
    return true;
}

void URPGMapPresentationModel::RefreshTrackedMarkers()
{
    if (!bReady || !Subsystem)
    {
        return;
    }

    const TArray<FGuid> PointIds = TrackedPointIds.Array();
    for (const FGuid& PointId : PointIds)
    {
        FResolvedGameZonePoint ResolvedPoint;
        if (!Subsystem->TryResolveMapMarker(PointId, ResolvedPoint) || !IsVisibleMarker(ResolvedPoint))
        {
            RemoveMarker(PointId, true);
            continue;
        }

        const FResolvedGameZonePoint* Existing = MarkerStates.Find(PointId);
        if (!Existing
            || Existing->Revision != ResolvedPoint.Revision
            || Existing->UpdateMode != ResolvedPoint.UpdateMode
            || !Existing->Data.WorldTransform.Equals(ResolvedPoint.Data.WorldTransform))
        {
            ApplyResolvedMarker(ResolvedPoint, true);
        }
    }
}

bool URPGMapPresentationModel::IsActive() const
{
    return Subsystem && PresentationHandle.IsValid();
}

bool URPGMapPresentationModel::IsReady() const
{
    return bReady;
}

TArray<FRPGMapPresentationMarker> URPGMapPresentationModel::GetMarkers() const
{
    TArray<FRPGMapPresentationMarker> Result;
    Result.Reserve(MarkerStates.Num());
    for (const TPair<FGuid, FResolvedGameZonePoint>& Pair : MarkerStates)
    {
        FRPGMapPresentationMarker& Marker = Result.AddDefaulted_GetRef();
        Marker.Point = Pair.Value;
        Marker.MarkerType = FindMarkerType(Pair.Value.Data.MarkerTypeId);
    }

    Result.Sort([](const FRPGMapPresentationMarker& Left, const FRPGMapPresentationMarker& Right)
        {
            return IsGuidLess(Left.Point.Data.PointId, Right.Point.Data.PointId);
        });
    return Result;
}

bool URPGMapPresentationModel::TryGetMarker(const FGuid& PointId, FRPGMapPresentationMarker& OutMarker) const
{
    OutMarker = {};
    const FResolvedGameZonePoint* Point = MarkerStates.Find(PointId);
    if (!Point)
    {
        return false;
    }

    OutMarker.Point = *Point;
    OutMarker.MarkerType = FindMarkerType(Point->Data.MarkerTypeId);
    return true;
}

bool URPGMapPresentationModel::TryGetMarkerWorldLocation(const FGuid& PointId, FVector& OutWorldLocation) const
{
    OutWorldLocation = FVector::ZeroVector;
    const FResolvedGameZonePoint* Point = MarkerStates.Find(PointId);
    if (!Point)
    {
        return false;
    }

    const FVector WorldLocation = Point->Data.WorldTransform.GetLocation();
    if (WorldLocation.ContainsNaN())
    {
        return false;
    }

    OutWorldLocation = WorldLocation;
    return true;
}

UWorld* URPGMapPresentationModel::GetWorld() const
{
    return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

bool URPGMapPresentationModel::IsVisibleMarker(const FResolvedGameZonePoint& Point) const
{
    const EMapMarkerDisplayMode DisplayMode = static_cast<EMapMarkerDisplayMode>(Point.Data.DisplayMode);
    return ZoneIds.Contains(Point.Data.ZoneId)
        && Point.Data.MarkerState != EGameZonePointState::Hide
        && EnumHasAnyFlags(DisplayMode, Mode);
}

void URPGMapPresentationModel::ResetMarkers(const TArray<FResolvedGameZonePoint>& Markers)
{
    MarkerStates.Reset();
    TrackedPointIds.Reset();
    LoadedMarkerTypes.Reset();

    for (const FResolvedGameZonePoint& Marker : Markers)
    {
        if (IsVisibleMarker(Marker))
        {
            ApplyResolvedMarker(Marker, false);
        }
    }
    UpdateTrackedMarkerTimer();
}

void URPGMapPresentationModel::ApplyResolvedMarker(const FResolvedGameZonePoint& Marker, bool bNotify)
{
    if (!IsVisibleMarker(Marker))
    {
        RemoveMarker(Marker.Data.PointId, bNotify);
        return;
    }

    MarkerStates.Add(Marker.Data.PointId, Marker);
    if (Marker.Source == EGameZonePointResolvedSource::Live && Marker.UpdateMode == EGameZonePointUpdateMode::Tracked)
    {
        TrackedPointIds.Add(Marker.Data.PointId);
    }
    else
    {
        TrackedPointIds.Remove(Marker.Data.PointId);
    }

    EnsureMarkerTypeLoaded(Marker.Data.MarkerTypeId);
    UpdateTrackedMarkerTimer();
    if (bNotify)
    {
        FRPGMapPresentationMarkerChange Change;
        Change.PointId = Marker.Data.PointId;
        Change.Marker.Point = Marker;
        Change.Marker.MarkerType = FindMarkerType(Marker.Data.MarkerTypeId);
        OnMarkerChanged.Broadcast(Change);
    }
}

void URPGMapPresentationModel::RemoveMarker(const FGuid& PointId, bool bNotify)
{
    const bool bRemoved = MarkerStates.Remove(PointId) > 0;
    TrackedPointIds.Remove(PointId);
    UpdateTrackedMarkerTimer();
    if (bRemoved && bNotify)
    {
        FRPGMapPresentationMarkerChange Change;
        Change.PointId = PointId;
        Change.Kind = EMapMarkerChangeKind::Removed;
        OnMarkerChanged.Broadcast(Change);
    }
}

void URPGMapPresentationModel::UpdateTrackedMarkerTimer()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FTimerManager& TimerManager = World->GetTimerManager();
    if (!bReady || TrackedPointIds.IsEmpty() || TrackedMarkerRefreshInterval <= 0.0f)
    {
        TimerManager.ClearTimer(TrackedMarkerTimer);
        return;
    }

    if (!TimerManager.IsTimerActive(TrackedMarkerTimer))
    {
        TimerManager.SetTimer(
            TrackedMarkerTimer,
            this,
            &URPGMapPresentationModel::RefreshTrackedMarkers,
            TrackedMarkerRefreshInterval,
            true);
    }
}

void URPGMapPresentationModel::EnsureMarkerTypeLoaded(const FRPGId& MarkerTypeId)
{
    if (!MarkerTypeId.IsValid() || LoadedMarkerTypes.Contains(MarkerTypeId) || PendingMarkerTypeLoads.Contains(MarkerTypeId))
    {
        return;
    }

    if (UMapMarkerTypeAsset* Existing = URPGAssetLibrary::GetRPGAsset<UMapMarkerTypeAsset>(MarkerTypeId))
    {
        LoadedMarkerTypes.Add(MarkerTypeId, Existing);
        return;
    }

    const uint32 LoadGeneration = Generation;
    TWeakObjectPtr<URPGMapPresentationModel> WeakThis(this);
    TSharedPtr<FStreamableHandle> Handle = URPGAssetLibrary::LoadAssetByRPGIdAsync(
        MarkerTypeId,
        {},
        [WeakThis, MarkerTypeId, LoadGeneration](URPGPrimaryAsset* LoadedAsset)
        {
            if (URPGMapPresentationModel* Model = WeakThis.Get())
            {
                Model->HandleMarkerTypeLoaded(MarkerTypeId, LoadedAsset, LoadGeneration);
            }
        });
    if (Handle.IsValid())
    {
        PendingMarkerTypeLoads.Add(MarkerTypeId, MoveTemp(Handle));
    }
}

void URPGMapPresentationModel::HandleMarkerTypeLoaded(
    const FRPGId& MarkerTypeId,
    URPGPrimaryAsset* LoadedAsset,
    uint32 LoadGeneration)
{
    if (LoadGeneration != Generation)
    {
        return;
    }

    PendingMarkerTypeLoads.Remove(MarkerTypeId);
    if (!bReady)
    {
        return;
    }

    UMapMarkerTypeAsset* MarkerType = Cast<UMapMarkerTypeAsset>(LoadedAsset);
    if (!MarkerType)
    {
#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Warning, TEXT("[MapPresentation] Failed to load marker type %s."), *MarkerTypeId.ToString());
#endif
        return;
    }

    LoadedMarkerTypes.Add(MarkerTypeId, MarkerType);
    for (const TPair<FGuid, FResolvedGameZonePoint>& Pair : MarkerStates)
    {
        if (Pair.Value.Data.MarkerTypeId != MarkerTypeId)
        {
            continue;
        }

        FRPGMapPresentationMarkerChange Change;
        Change.PointId = Pair.Key;
        Change.Marker.Point = Pair.Value;
        Change.Marker.MarkerType = MarkerType;
        OnMarkerChanged.Broadcast(Change);
    }
}

UMapMarkerTypeAsset* URPGMapPresentationModel::FindMarkerType(const FRPGId& MarkerTypeId) const
{
    const TObjectPtr<UMapMarkerTypeAsset>* MarkerType = LoadedMarkerTypes.Find(MarkerTypeId);
    return MarkerType ? MarkerType->Get() : nullptr;
}

void URPGMapPresentationModel::ResetState()
{
    Subsystem = nullptr;
    MarkerStates.Reset();
    LoadedMarkerTypes.Reset();
    ZoneIds.Reset();
    TrackedPointIds.Reset();
    PresentationHandle = {};
    PendingTextureRequestId.Invalidate();
    PendingTextureZoneId = {};
    PendingTextureSheetId = {};
    Mode = EMapMarkerDisplayMode::None;
    TrackedMarkerRefreshInterval = 0.1f;
    MarkerRevision = 0;
    bReady = false;
}

void URPGMapPresentationModel::HandlePresentationReady(const FGameZonePresentationSnapshot& Snapshot)
{
    if (!Subsystem || (PresentationHandle.IsValid() && !(Snapshot.Handle == PresentationHandle)))
    {
        return;
    }

    PresentationHandle = Snapshot.Handle;
    MarkerRevision = Snapshot.Revision;
    bReady = true;
    ResetMarkers(Snapshot.Markers);
    OnReady.Broadcast(Snapshot);
}

void URPGMapPresentationModel::HandleMapTextureReady(const FGameZoneMapTextureResult& Result)
{
    if (Result.RequestId != PendingTextureRequestId
        || Result.ZoneId != PendingTextureZoneId
        || Result.SheetId != PendingTextureSheetId)
    {
        return;
    }

    PendingTextureRequestId.Invalidate();
    PendingTextureZoneId = {};
    PendingTextureSheetId = {};
    OnTextureReady.Broadcast(Result);
}

void URPGMapPresentationModel::HandleMapMarkerChange(const FMapMarkerChange& Change)
{
    if (!bReady || !Subsystem || Change.Revision <= MarkerRevision)
    {
        return;
    }

    MarkerRevision = Change.Revision;
    if (Change.Kind == EMapMarkerChangeKind::Removed)
    {
        RemoveMarker(Change.PointId, true);
        return;
    }

    FResolvedGameZonePoint ResolvedPoint;
    if (!Subsystem->TryResolveMapMarker(Change.PointId, ResolvedPoint) || !IsVisibleMarker(ResolvedPoint))
    {
        RemoveMarker(Change.PointId, true);
        return;
    }

    ApplyResolvedMarker(ResolvedPoint, true);
}
