// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameZoneMapData.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct RPGCORE_API FGameZoneMapLayerId
{
    GENERATED_BODY()

public:
    FGameZoneMapLayerId() = default;
    explicit FGameZoneMapLayerId(FName InValue) : Value(InValue) {}

    bool IsValid() const { return !Value.IsNone(); }
    FString ToString() const { return Value.ToString(); }

    bool operator==(const FGameZoneMapLayerId& Other) const { return Value == Other.Value; }
    bool operator!=(const FGameZoneMapLayerId& Other) const { return !(*this == Other); }

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName Value = NAME_None;
};

FORCEINLINE uint32 GetTypeHash(const FGameZoneMapLayerId& Id)
{
    return GetTypeHash(Id.Value);
}

USTRUCT(BlueprintType)
struct RPGCORE_API FGameZoneMapSheetId
{
    GENERATED_BODY()

public:
    FGameZoneMapSheetId() = default;
    explicit FGameZoneMapSheetId(FName InValue) : Value(InValue) {}

    bool IsValid() const { return !Value.IsNone(); }
    FString ToString() const { return Value.ToString(); }

    bool operator==(const FGameZoneMapSheetId& Other) const { return Value == Other.Value; }
    bool operator!=(const FGameZoneMapSheetId& Other) const { return !(*this == Other); }

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName Value = NAME_None;
};

FORCEINLINE uint32 GetTypeHash(const FGameZoneMapSheetId& Id)
{
    return GetTypeHash(Id.Value);
}

USTRUCT(BlueprintType)
struct RPGCORE_API FGameZoneMapRegionId
{
    GENERATED_BODY()

public:
    FGameZoneMapRegionId() = default;
    explicit FGameZoneMapRegionId(FName InValue) : Value(InValue) {}

    bool IsValid() const { return !Value.IsNone(); }
    FString ToString() const { return Value.ToString(); }

    bool operator==(const FGameZoneMapRegionId& Other) const { return Value == Other.Value; }
    bool operator!=(const FGameZoneMapRegionId& Other) const { return !(*this == Other); }

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName Value = NAME_None;
};

FORCEINLINE uint32 GetTypeHash(const FGameZoneMapRegionId& Id)
{
    return GetTypeHash(Id.Value);
}

USTRUCT(BlueprintType)
struct RPGCORE_API FGameZoneMapLayer
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameZoneMapLayerId LayerId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SortOrder = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 ElevationOrder = 0;
};

USTRUCT(BlueprintType)
struct RPGCORE_API FGameZoneMapSheet
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameZoneMapSheetId SheetId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ZoneMapLayerReference))
    FGameZoneMapLayerId LayerId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SortOrder = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UTexture2D> MapTexture;
};

USTRUCT(BlueprintType)
struct RPGCORE_API FGameZoneMapProjection
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly)
    FVector2D UV = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    bool bIsInsideSheet = false;
};

USTRUCT(BlueprintType)
struct RPGCORE_API FGameZoneMapSheetMapping
{
    GENERATED_BODY()

public:
    bool ProjectWorldLocation(const FVector& WorldLocation, FGameZoneMapProjection& OutProjection) const;

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGameZoneMapSheetId SheetId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FVector WorldOrigin = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FVector2D WorldSize = FVector2D::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float WorldYaw = 0.0f;
};

USTRUCT()
struct RPGCORE_API FGameZoneMapRegion
{
    GENERATED_BODY()

public:
    bool Contains(const FVector& WorldLocation, double BoundaryTolerance = KINDA_SMALL_NUMBER) const;

public:
    UPROPERTY(VisibleAnywhere)
    FGameZoneMapRegionId RegionId;

    UPROPERTY(VisibleAnywhere)
    FGameZoneMapSheetId SheetId;

    UPROPERTY(VisibleAnywhere)
    int32 Priority = 0;

    UPROPERTY(VisibleAnywhere)
    FBox Bounds = FBox(EForceInit::ForceInit);

    UPROPERTY(VisibleAnywhere)
    TArray<FPlane> Planes;
};
