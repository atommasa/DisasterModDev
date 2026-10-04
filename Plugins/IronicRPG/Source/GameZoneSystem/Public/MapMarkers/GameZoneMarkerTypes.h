// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Levels/GameZonePointData.h"
#include "Maps/GameZoneMapTypes.h"
#include "GameZoneMarkerTypes.generated.h"

UENUM(BlueprintType)
enum class EGameZonePointResolvedSource : uint8
{
    Baked,
    Live,
    Saved,
};

USTRUCT(BlueprintType)
struct GAMEZONESYSTEM_API FResolvedGameZonePoint
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGameZonePointData Data;

    UPROPERTY(BlueprintReadOnly)
    EGameZonePointResolvedSource Source = EGameZonePointResolvedSource::Baked;

    UPROPERTY(BlueprintReadOnly)
    EGameZonePointUpdateMode UpdateMode = EGameZonePointUpdateMode::EventDriven;

    UPROPERTY(BlueprintReadOnly)
    int64 Revision = 0;
};

UENUM(BlueprintType)
enum class EMapMarkerChangeKind : uint8
{
    Changed,
    Removed,
};

USTRUCT(BlueprintType)
struct GAMEZONESYSTEM_API FMapMarkerChange
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid PointId;

    UPROPERTY(BlueprintReadOnly)
    int64 Revision = 0;

    UPROPERTY(BlueprintReadOnly)
    EMapMarkerChangeKind Kind = EMapMarkerChangeKind::Changed;
};

USTRUCT(BlueprintType)
struct GAMEZONESYSTEM_API FGameZonePresentationHandle
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid SessionId;

    bool IsValid() const { return SessionId.IsValid(); }

    bool operator==(const FGameZonePresentationHandle& Other) const
    {
        return SessionId == Other.SessionId;
    }
};

FORCEINLINE uint32 GetTypeHash(const FGameZonePresentationHandle& Handle)
{
    return GetTypeHash(Handle.SessionId);
}

USTRUCT(BlueprintType)
struct GAMEZONESYSTEM_API FGameZonePresentationSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGameZonePresentationHandle Handle;

    UPROPERTY(BlueprintReadOnly)
    int64 Revision = 0;

    UPROPERTY(BlueprintReadOnly)
    TArray<FResolvedGameZonePoint> Markers;

    UPROPERTY(BlueprintReadOnly)
    TArray<FGameZoneMapLayerCatalogEntry> MapLayers;

    UPROPERTY(BlueprintReadOnly)
    TArray<FGameZoneMapSheetCatalogEntry> MapSheets;

    UPROPERTY(BlueprintReadOnly)
    TArray<FGameZoneMapWorldBounds> MapWorldBounds;
};
