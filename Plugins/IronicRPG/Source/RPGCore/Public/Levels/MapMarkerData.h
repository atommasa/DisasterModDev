// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "GameplayTagContainer.h"
#include "Assets/RPGPrimaryAsset.h"
#include "MapMarkerData.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_Type)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_State)

/**
 * This struct is used to store data about map markers, which can be used to display markers on the map or minimap.
 * It contains information about the marker type, its world location, and a reference to an asset that can be used to display the marker.
 */
USTRUCT(BlueprintType)
struct RPGCORE_API FMapMarkerData
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FGameplayTag MarkerType;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FGameplayTag MarkerState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector WorldLocation;

	UPROPERTY(BlueprintReadOnly)
	URPGPrimaryAsset* ReferenceAsset;
};
