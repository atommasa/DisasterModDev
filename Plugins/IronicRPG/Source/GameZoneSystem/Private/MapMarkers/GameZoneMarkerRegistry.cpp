// Copyright Ironic Studio. All Rights Reserved.

#include "MapMarkers/GameZoneMarkerRegistry.h"

#include "Levels/GameZonePointComponent.h"

namespace
{
    bool ArePointDataEqual(const FGameZonePointData& Left, const FGameZonePointData& Right)
    {
        return FGameZonePointData::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
    }

    bool AreResolvedPointsEqual(const FResolvedGameZonePoint& Left, const FResolvedGameZonePoint& Right)
    {
        return Left.Source == Right.Source
            && Left.UpdateMode == Right.UpdateMode
            && ArePointDataEqual(Left.Data, Right.Data);
    }

    void SyncPersistentDataFromLive(TOptional<FGameZonePointData>& SavedData, const FRPGId& LiveZoneId, UGameZonePointComponent& Component)
    {
        FGameZonePointData LiveSnapshot = Component.MakePointSnapshot();
        LiveSnapshot.ZoneId = LiveZoneId;

        switch (LiveSnapshot.SavePolicy)
        {
        case EGameZonePointSavePolicy::NotSaveable:
            SavedData.Reset();
            break;
        case EGameZonePointSavePolicy::Saveable:
            SavedData = MoveTemp(LiveSnapshot);
            break;
        case EGameZonePointSavePolicy::Custom:
            break;
        }
    }
}

FGameZoneMarkerMutationResult FGameZoneMarkerRegistry::CommitBakedZone(const FRPGId& ZoneId, const TMap<FGuid, FGameZonePointData>& Points)
{
    FGameZoneMarkerMutationResult Result;

    for (const TPair<FGuid, FGameZonePointData>& Pair : Points)
    {
        if (!Pair.Key.IsValid() || Pair.Value.PointId != Pair.Key)
        {
            Result.Error = EGameZoneMarkerMutationError::InvalidPointId;
            Result.Revision = Revision;
            return Result;
        }

        const FRecord* Existing = Records.Find(Pair.Key);
        if (Existing
            && Existing->BakedData.IsSet()
            && Existing->BakedSourceZoneId != ZoneId)
        {
            Result.Error = EGameZoneMarkerMutationError::DuplicateBakedSource;
            Result.Revision = Revision;
            return Result;
        }
    }

    for (const TPair<FGuid, FGameZonePointData>& Pair : Points)
    {
        FRecord& Record = Records.FindOrAdd(Pair.Key);
        Record.BakedData = Pair.Value;
        Record.BakedSourceZoneId = ZoneId;
        if (Pair.Value.SavePolicy == EGameZonePointSavePolicy::NotSaveable)
        {
            Record.SavedData.Reset();
        }
        else if (Record.SavedData.IsSet())
        {
            Record.SavedData->SavePolicy = Pair.Value.SavePolicy;
        }
        Record.LastRevision = ++Revision;

        FMapMarkerChange& Change = Result.Changes.AddDefaulted_GetRef();
        Change.PointId = Pair.Key;
        Change.Revision = Record.LastRevision;
        Change.Kind = EMapMarkerChangeKind::Changed;
    }

    Result.Revision = Revision;
    return Result;
}

FGameZoneMarkerMutationResult FGameZoneMarkerRegistry::ReplaceSaveOverrides(const TMap<FGuid, FGameZonePointData>& Overrides)
{
    FGameZoneMarkerMutationResult Result;

    for (const TPair<FGuid, FGameZonePointData>& Pair : Overrides)
    {
        if (!Pair.Key.IsValid()
            || Pair.Value.PointId != Pair.Key
            || Pair.Value.SavePolicy == EGameZonePointSavePolicy::NotSaveable)
        {
            Result.Error = EGameZoneMarkerMutationError::InvalidSaveOverride;
            Result.Revision = Revision;
            return Result;
        }
    }

    TSet<FGuid> ChangedPointIds;
    for (const TPair<FGuid, FRecord>& Pair : Records)
    {
        if (Pair.Value.SavedData.IsSet())
        {
            ChangedPointIds.Add(Pair.Key);
        }
    }
    for (const TPair<FGuid, FGameZonePointData>& Pair : Overrides)
    {
        ChangedPointIds.Add(Pair.Key);
    }

    TArray<FGuid> SortedPointIds = ChangedPointIds.Array();
    SortedPointIds.Sort();

    for (const FGuid& PointId : SortedPointIds)
    {
        FResolvedGameZonePoint Before;
        const bool bWasVisible = TryResolve(PointId, Before);

        const FGameZonePointData* NewOverride = Overrides.Find(PointId);
        FRecord* Record = Records.Find(PointId);
        TOptional<FGameZonePointData> EffectiveOverride;
        if (NewOverride)
        {
            EffectiveOverride = *NewOverride;
            if (Record)
            {
                const FGameZonePointData* AuthoredData = Record->LiveInitialData.IsSet()
                    ? &Record->LiveInitialData.GetValue()
                    : (Record->BakedData.IsSet() ? &Record->BakedData.GetValue() : nullptr);
                if (AuthoredData)
                {
                    if (AuthoredData->SavePolicy == EGameZonePointSavePolicy::NotSaveable)
                    {
                        EffectiveOverride.Reset();
                    }
                    else
                    {
                        EffectiveOverride->SavePolicy = AuthoredData->SavePolicy;
                    }
                }
            }
        }

        if (EffectiveOverride.IsSet())
        {
            Record = &Records.FindOrAdd(PointId);
            Record->SavedData = EffectiveOverride.GetValue();
        }
        else if (Record)
        {
            Record->SavedData.Reset();
        }

        if (Record)
        {
            if (UGameZonePointComponent* LiveComponent = Record->LiveComponent.Get())
            {
                const FGameZonePointData* InitializationData = EffectiveOverride.IsSet()
                    ? &EffectiveOverride.GetValue()
                    : nullptr;
                if (!InitializationData && Record->LiveInitialData.IsSet())
                {
                    InitializationData = &Record->LiveInitialData.GetValue();
                }

                if (InitializationData)
                {
                    LiveComponent->ApplyPointDataInitialization(*InitializationData);
                }
            }
        }

        FResolvedGameZonePoint After;
        const bool bIsVisible = TryResolve(PointId, After);
        const bool bVisibleResultChanged = bWasVisible != bIsVisible
            || (bWasVisible && bIsVisible && !AreResolvedPointsEqual(Before, After));

        if (bVisibleResultChanged)
        {
            const int64 ChangeRevision = ++Revision;
            if (FRecord* ChangedRecord = Records.Find(PointId))
            {
                ChangedRecord->LastRevision = ChangeRevision;
            }

            FMapMarkerChange& Change = Result.Changes.AddDefaulted_GetRef();
            Change.PointId = PointId;
            Change.Revision = ChangeRevision;
            Change.Kind = bIsVisible ? EMapMarkerChangeKind::Changed : EMapMarkerChangeKind::Removed;
        }

        Record = Records.Find(PointId);
        if (Record
            && !Record->BakedData.IsSet()
            && !Record->SavedData.IsSet()
            && Record->LiveComponent.IsExplicitlyNull())
        {
            Records.Remove(PointId);
        }
    }

    Result.Revision = Revision;
    return Result;
}

FGameZoneMarkerMutationResult FGameZoneMarkerRegistry::SetSaveOverride(const FGameZonePointData& Override)
{
    FGameZoneMarkerMutationResult Result;
    if (!Override.PointId.IsValid()
        || Override.SavePolicy == EGameZonePointSavePolicy::NotSaveable)
    {
        Result.Error = EGameZoneMarkerMutationError::InvalidSaveOverride;
        Result.Revision = Revision;
        return Result;
    }

    FResolvedGameZonePoint Before;
    const bool bWasVisible = TryResolve(Override.PointId, Before);
    if (bWasVisible && Before.Data.SavePolicy == EGameZonePointSavePolicy::NotSaveable)
    {
        Result.Error = EGameZoneMarkerMutationError::InvalidSaveOverride;
        Result.Revision = Revision;
        return Result;
    }

    FGameZonePointData StoredOverride = Override;
    if (bWasVisible)
    {
        StoredOverride.SavePolicy = Before.Data.SavePolicy;
    }

    FRecord& Record = Records.FindOrAdd(Override.PointId);
    Record.SavedData = MoveTemp(StoredOverride);

    FResolvedGameZonePoint After;
    const bool bIsVisible = TryResolve(Override.PointId, After);
    if (bWasVisible != bIsVisible
        || (bWasVisible && bIsVisible && !AreResolvedPointsEqual(Before, After)))
    {
        Record.LastRevision = ++Revision;

        FMapMarkerChange& Change = Result.Changes.AddDefaulted_GetRef();
        Change.PointId = Override.PointId;
        Change.Revision = Record.LastRevision;
        Change.Kind = bIsVisible ? EMapMarkerChangeKind::Changed : EMapMarkerChangeKind::Removed;
    }

    Result.Revision = Revision;
    return Result;
}

FGameZoneMarkerMutationResult FGameZoneMarkerRegistry::ClearSaveOverride(const FGuid& PointId)
{
    FGameZoneMarkerMutationResult Result;
    if (!PointId.IsValid())
    {
        Result.Error = EGameZoneMarkerMutationError::InvalidPointId;
        Result.Revision = Revision;
        return Result;
    }

    FRecord* Record = Records.Find(PointId);
    if (!Record || !Record->SavedData.IsSet())
    {
        Result.Revision = Revision;
        return Result;
    }

    FResolvedGameZonePoint Before;
    const bool bWasVisible = TryResolve(PointId, Before);
    Record->SavedData.Reset();

    FResolvedGameZonePoint After;
    const bool bIsVisible = TryResolve(PointId, After);
    if (bWasVisible != bIsVisible
        || (bWasVisible && bIsVisible && !AreResolvedPointsEqual(Before, After)))
    {
        Record->LastRevision = ++Revision;

        FMapMarkerChange& Change = Result.Changes.AddDefaulted_GetRef();
        Change.PointId = PointId;
        Change.Revision = Record->LastRevision;
        Change.Kind = bIsVisible ? EMapMarkerChangeKind::Changed : EMapMarkerChangeKind::Removed;
    }

    if (!Record->BakedData.IsSet() && Record->LiveComponent.IsExplicitlyNull())
    {
        Records.Remove(PointId);
    }

    Result.Revision = Revision;
    return Result;
}

TMap<FGuid, FGameZonePointData> FGameZoneMarkerRegistry::CaptureSaveOverrides()
{
    TMap<FGuid, FGameZonePointData> Overrides;

    for (TPair<FGuid, FRecord>& Pair : Records)
    {
        FRecord& Record = Pair.Value;
        if (UGameZonePointComponent* LiveComponent = Record.LiveComponent.Get())
        {
            FGameZonePointData LiveSnapshot = LiveComponent->MakePointSnapshot();
            LiveSnapshot.ZoneId = Record.LiveZoneId;

            switch (LiveSnapshot.SavePolicy)
            {
            case EGameZonePointSavePolicy::NotSaveable:
                Record.SavedData.Reset();
                break;
            case EGameZonePointSavePolicy::Saveable:
                Record.SavedData = MoveTemp(LiveSnapshot);
                break;
            case EGameZonePointSavePolicy::Custom:
                break;
            }
        }

        if (Record.SavedData.IsSet()
            && Record.SavedData->SavePolicy != EGameZonePointSavePolicy::NotSaveable)
        {
            Overrides.Add(Pair.Key, Record.SavedData.GetValue());
        }
    }

    return Overrides;
}

bool FGameZoneMarkerRegistry::TryResolve(const FGuid& PointId, FResolvedGameZonePoint& OutPoint) const
{
    const FRecord* Record = Records.Find(PointId);
    if (!Record)
    {
        return false;
    }

    if (UGameZonePointComponent* LiveComponent = Record->LiveComponent.Get())
    {
        OutPoint.Data = LiveComponent->MakePointSnapshot();
        OutPoint.Data.ZoneId = Record->LiveZoneId;
        OutPoint.Source = EGameZonePointResolvedSource::Live;
        OutPoint.UpdateMode = LiveComponent->GetUpdateMode();
        OutPoint.Revision = Record->LastRevision;
        return true;
    }

    if (!Record->BakedData.IsSet())
    {
        return false;
    }

    if (Record->SavedData.IsSet())
    {
        OutPoint.Data = Record->SavedData.GetValue();
        OutPoint.Source = EGameZonePointResolvedSource::Saved;
    }
    else
    {
        OutPoint.Data = Record->BakedData.GetValue();
        OutPoint.Source = EGameZonePointResolvedSource::Baked;
    }
    OutPoint.UpdateMode = EGameZonePointUpdateMode::EventDriven;
    OutPoint.Revision = Record->LastRevision;
    return true;
}

TArray<FResolvedGameZonePoint> FGameZoneMarkerRegistry::BuildSnapshot(
    const TSet<FRPGId>& ZoneIds,
    EMapMarkerDisplayMode Mode) const
{
    TArray<FResolvedGameZonePoint> Snapshot;
    if (ZoneIds.IsEmpty() || Mode == EMapMarkerDisplayMode::None)
    {
        return Snapshot;
    }

    for (const TPair<FGuid, FRecord>& Pair : Records)
    {
        FResolvedGameZonePoint ResolvedPoint;
        if (!TryResolve(Pair.Key, ResolvedPoint)
            || !ZoneIds.Contains(ResolvedPoint.Data.ZoneId)
            || ResolvedPoint.Data.MarkerState == EGameZonePointState::Hide)
        {
            continue;
        }

        const EMapMarkerDisplayMode MarkerMode =
            static_cast<EMapMarkerDisplayMode>(ResolvedPoint.Data.DisplayMode);
        if (!EnumHasAnyFlags(MarkerMode, Mode))
        {
            continue;
        }

        Snapshot.Add(MoveTemp(ResolvedPoint));
    }

    Snapshot.Sort(
        [](const FResolvedGameZonePoint& Left, const FResolvedGameZonePoint& Right)
        {
            return Left.Data.PointId < Right.Data.PointId;
        });
    return Snapshot;
}

FGameZoneMarkerMutationResult FGameZoneMarkerRegistry::RemoveBakedZone(const FRPGId& ZoneId)
{
    FGameZoneMarkerMutationResult Result;
    TArray<FGuid> PointIdsToRemove;
    TArray<FGuid> PointIdsHidden;

    for (TPair<FGuid, FRecord>& Pair : Records)
    {
        FRecord& Record = Pair.Value;
        if (!Record.BakedData.IsSet() || Record.BakedSourceZoneId != ZoneId)
        {
            continue;
        }

        Record.BakedData.Reset();
        Record.BakedSourceZoneId = {};

        if (!Record.LiveComponent.IsValid())
        {
            PointIdsHidden.Add(Pair.Key);
            if (!Record.SavedData.IsSet())
            {
                PointIdsToRemove.Add(Pair.Key);
            }
        }
    }

    PointIdsHidden.Sort();
    for (const FGuid& PointId : PointIdsHidden)
    {
        const int64 ChangeRevision = ++Revision;
        if (FRecord* Record = Records.Find(PointId))
        {
            Record->LastRevision = ChangeRevision;
        }

        FMapMarkerChange& Change = Result.Changes.AddDefaulted_GetRef();
        Change.PointId = PointId;
        Change.Revision = ChangeRevision;
        Change.Kind = EMapMarkerChangeKind::Removed;
    }

    for (const FGuid& PointId : PointIdsToRemove)
    {
        Records.Remove(PointId);
    }

    Result.Revision = Revision;
    return Result;
}

FGameZoneMarkerMutationResult FGameZoneMarkerRegistry::RegisterLive(UGameZonePointComponent& Component, const FRPGId& RuntimeZoneId)
{
    FGameZoneMarkerMutationResult Result;
    const FGuid PointId = Component.GetPointId();

    if (!PointId.IsValid())
    {
        Result.Error = EGameZoneMarkerMutationError::InvalidPointId;
        Result.Revision = Revision;
        return Result;
    }

    FRecord& Record = Records.FindOrAdd(PointId);
    if (UGameZonePointComponent* ExistingLive = Record.LiveComponent.Get())
    {
        if (ExistingLive != &Component)
        {
            Result.Error = EGameZoneMarkerMutationError::DuplicateLiveSource;
        }

        Result.Revision = Revision;
        return Result;
    }

    Record.LiveInitialData = Component.MakePointSnapshot();
    Record.LiveInitialData->ZoneId = RuntimeZoneId;
    if (Record.LiveInitialData->SavePolicy == EGameZonePointSavePolicy::NotSaveable)
    {
        Record.SavedData.Reset();
    }
    else if (Record.SavedData.IsSet())
    {
        Record.SavedData->SavePolicy = Record.LiveInitialData->SavePolicy;
    }

    if (Record.SavedData.IsSet() && !Component.ApplyPointDataInitialization(Record.SavedData.GetValue()))
    {
        Record.LiveInitialData.Reset();
        Result.Error = EGameZoneMarkerMutationError::InvalidSaveOverride;
        Result.Revision = Revision;
        return Result;
    }

    Record.LiveComponent = &Component;
    Record.LiveZoneId = RuntimeZoneId;
    Record.LastRevision = ++Revision;

    FMapMarkerChange& Change = Result.Changes.AddDefaulted_GetRef();
    Change.PointId = PointId;
    Change.Revision = Record.LastRevision;
    Change.Kind = EMapMarkerChangeKind::Changed;
    Result.Revision = Revision;
    return Result;
}

FGameZoneMarkerMutationResult FGameZoneMarkerRegistry::UnregisterLive(UGameZonePointComponent& Component)
{
    FGameZoneMarkerMutationResult Result;
    const FGuid PointId = Component.GetPointId();
    FRecord* Record = Records.Find(PointId);

    if (!Record || Record->LiveComponent.Get() != &Component)
    {
        Result.Error = EGameZoneMarkerMutationError::LiveSourceMismatch;
        Result.Revision = Revision;
        return Result;
    }

    SyncPersistentDataFromLive(Record->SavedData, Record->LiveZoneId, Component);

    Record->LiveComponent.Reset();
    Record->LiveInitialData.Reset();
    Record->LiveZoneId = {};

    if (!Record->BakedData.IsSet())
    {
        if (!Record->SavedData.IsSet())
        {
            Records.Remove(PointId);
        }
        const int64 ChangeRevision = ++Revision;

        FMapMarkerChange& Change = Result.Changes.AddDefaulted_GetRef();
        Change.PointId = PointId;
        Change.Revision = ChangeRevision;
        Change.Kind = EMapMarkerChangeKind::Removed;
        Result.Revision = Revision;
        return Result;
    }

    Record->LastRevision = ++Revision;

    FMapMarkerChange& Change = Result.Changes.AddDefaulted_GetRef();
    Change.PointId = PointId;
    Change.Revision = Record->LastRevision;
    Change.Kind = EMapMarkerChangeKind::Changed;
    Result.Revision = Revision;
    return Result;
}

FGameZoneMarkerMutationResult FGameZoneMarkerRegistry::NotifyLiveChanged(UGameZonePointComponent& Component)
{
    FGameZoneMarkerMutationResult Result;
    const FGuid PointId = Component.GetPointId();
    FRecord* Record = Records.Find(PointId);

    if (!Record || Record->LiveComponent.Get() != &Component)
    {
        Result.Error = EGameZoneMarkerMutationError::LiveSourceMismatch;
        Result.Revision = Revision;
        return Result;
    }

    SyncPersistentDataFromLive(Record->SavedData, Record->LiveZoneId, Component);

    Record->LastRevision = ++Revision;

    FMapMarkerChange& Change = Result.Changes.AddDefaulted_GetRef();
    Change.PointId = PointId;
    Change.Revision = Record->LastRevision;
    Change.Kind = EMapMarkerChangeKind::Changed;
    Result.Revision = Revision;
    return Result;
}

FGameZoneMarkerMutationResult FGameZoneMarkerRegistry::SanitizeInvalidLive()
{
    FGameZoneMarkerMutationResult Result;
    TArray<FGuid> PointIdsToSanitize;

    for (const TPair<FGuid, FRecord>& Pair : Records)
    {
        const FRecord& Record = Pair.Value;
        if (Record.LiveComponent.IsStale())
        {
            PointIdsToSanitize.Add(Pair.Key);
        }
    }

    PointIdsToSanitize.Sort();

    for (const FGuid& PointId : PointIdsToSanitize)
    {
        FRecord& Record = Records.FindChecked(PointId);
        const bool bFallsBackToBaked = Record.BakedData.IsSet();
        const int64 ChangeRevision = ++Revision;

        if (bFallsBackToBaked)
        {
            Record.LiveComponent.Reset();
            Record.LiveInitialData.Reset();
            Record.LiveZoneId = {};
            Record.LastRevision = ChangeRevision;
        }
        else
        {
            Record.LiveComponent.Reset();
            Record.LiveInitialData.Reset();
            Record.LiveZoneId = {};
            if (Record.SavedData.IsSet())
            {
                Record.LastRevision = ChangeRevision;
            }
            else
            {
                Records.Remove(PointId);
            }
        }

        FMapMarkerChange& Change = Result.Changes.AddDefaulted_GetRef();
        Change.PointId = PointId;
        Change.Revision = ChangeRevision;
        Change.Kind = bFallsBackToBaked
            ? EMapMarkerChangeKind::Changed
            : EMapMarkerChangeKind::Removed;
    }

    Result.Revision = Revision;
    return Result;
}
