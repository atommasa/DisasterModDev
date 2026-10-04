// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Assets/RPGPrimaryAsset.h"
#include "Levels/GameZoneMarkerAction.h"
#include "Levels/GameZonePointData.h"
#include "MapMarkerTypeAsset.generated.h"

/**
 * 
 */
UCLASS()
class RPGCORE_API UMapMarkerTypeAsset : public URPGPrimaryAsset
{
	GENERATED_BODY()
	DEFINE_ASSET_TYPE(MapMarker, mm)
	
protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UTexture2D> ActivatedIcon;
    ASSET_PROP_GETTER(TObjectPtr<UTexture2D>, ActivatedIcon);

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UTexture2D> DeactivatedIcon;
    ASSET_PROP_GETTER(TObjectPtr<UTexture2D>, DeactivatedIcon);

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 ZOrder = 0;
    ASSET_PROP_GETTER(int32, ZOrder);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Actions")
    TArray<FGameZoneMarkerActionDefinition> Actions;

public:
    const TArray<FGameZoneMarkerActionDefinition>& GetActions() const { return Actions; }

#if WITH_EDITOR
public:
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

};
