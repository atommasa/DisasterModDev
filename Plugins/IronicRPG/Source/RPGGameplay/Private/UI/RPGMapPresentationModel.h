// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameZoneSubsystem.h"

#include "RPGMapPresentationModel.generated.h"

struct FStreamableHandle;
class UMapMarkerTypeAsset;

struct FRPGMapPresentationConfig
{
    TArray<FRPGId> ZoneIds;
    EMapMarkerDisplayMode Mode = EMapMarkerDisplayMode::None;
    float TrackedMarkerRefreshInterval = 0.1f;
};

struct FRPGMapPresentationMarker
{
    FResolvedGameZonePoint Point;
    UMapMarkerTypeAsset* MarkerType = nullptr;
};

struct FRPGMapPresentationMarkerChange
{
    FGuid PointId;
    EMapMarkerChangeKind Kind = EMapMarkerChangeKind::Changed;
    FRPGMapPresentationMarker Marker;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FRPGMapPresentationReadyEvent, const FGameZonePresentationSnapshot&);
DECLARE_MULTICAST_DELEGATE(FRPGMapPresentationFailedEvent);
DECLARE_MULTICAST_DELEGATE_OneParam(FRPGMapPresentationMarkerChangedEvent, const FRPGMapPresentationMarkerChange&);
DECLARE_MULTICAST_DELEGATE_OneParam(FRPGMapPresentationTextureReadyEvent, const FGameZoneMapTextureResult&);

/** Owns map presentation data and async lifetime without owning any map Widget hierarchy. */
UCLASS()
class URPGMapPresentationModel : public UObject
{
    GENERATED_BODY()

public:
    bool Start(UGameZoneSubsystem& InSubsystem, const FRPGMapPresentationConfig& InConfig);

public:
    void Stop();

public:
    bool RequestSheetTexture(const FRPGId& ZoneId, const FGameZoneMapSheetId& SheetId);

public:
    void RefreshTrackedMarkers();

public:
    bool IsActive() const;

public:
    bool IsReady() const;

public:
    TArray<FRPGMapPresentationMarker> GetMarkers() const;

public:
    bool TryGetMarker(const FGuid& PointId, FRPGMapPresentationMarker& OutMarker) const;

public:
    bool TryGetMarkerWorldLocation(const FGuid& PointId, FVector& OutWorldLocation) const;

public:
    FRPGMapPresentationReadyEvent OnReady;
    FRPGMapPresentationFailedEvent OnFailed;
    FRPGMapPresentationMarkerChangedEvent OnMarkerChanged;
    FRPGMapPresentationTextureReadyEvent OnTextureReady;

protected:
    virtual UWorld* GetWorld() const override;

private:
    bool IsVisibleMarker(const FResolvedGameZonePoint& Point) const;
    void ResetMarkers(const TArray<FResolvedGameZonePoint>& Markers);
    void ApplyResolvedMarker(const FResolvedGameZonePoint& Marker, bool bNotify);
    void RemoveMarker(const FGuid& PointId, bool bNotify);
    void UpdateTrackedMarkerTimer();
    void EnsureMarkerTypeLoaded(const FRPGId& MarkerTypeId);
    void HandleMarkerTypeLoaded(const FRPGId& MarkerTypeId, class URPGPrimaryAsset* LoadedAsset, uint32 LoadGeneration);
    UMapMarkerTypeAsset* FindMarkerType(const FRPGId& MarkerTypeId) const;
    void ResetState();

private:
    UFUNCTION()
    void HandlePresentationReady(const FGameZonePresentationSnapshot& Snapshot);

private:
    UFUNCTION()
    void HandleMapTextureReady(const FGameZoneMapTextureResult& Result);

private:
    UFUNCTION()
    void HandleMapMarkerChange(const FMapMarkerChange& Change);

private:
    UPROPERTY(Transient)
    TObjectPtr<UGameZoneSubsystem> Subsystem = nullptr;

    UPROPERTY(Transient)
    TMap<FGuid, FResolvedGameZonePoint> MarkerStates;

    UPROPERTY(Transient)
    TMap<FRPGId, TObjectPtr<UMapMarkerTypeAsset>> LoadedMarkerTypes;

    TMap<FRPGId, TSharedPtr<FStreamableHandle>> PendingMarkerTypeLoads;
    TSet<FRPGId> ZoneIds;
    TSet<FGuid> TrackedPointIds;
    FGameZonePresentationHandle PresentationHandle;
    FGuid PendingTextureRequestId;
    FRPGId PendingTextureZoneId;
    FGameZoneMapSheetId PendingTextureSheetId;
    FTimerHandle TrackedMarkerTimer;
    EMapMarkerDisplayMode Mode = EMapMarkerDisplayMode::None;
    float TrackedMarkerRefreshInterval = 0.1f;
    int64 MarkerRevision = 0;
    uint32 Generation = 0;
    bool bReady = false;
};
