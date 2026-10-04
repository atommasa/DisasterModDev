// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MapMarkers/GameZoneMarkerTypes.h"
#include "RPGMapMarkerWidget.generated.h"

class UMapMarkerTypeAsset;

UENUM(BlueprintType)
enum class EWorldMapMarkerLayerRelation : uint8
{
    Current,
    Above,
    Below,
    Other,
};

USTRUCT(BlueprintType)
struct RPGGAMEPLAY_API FWorldMapMarkerView
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly)
    FResolvedGameZonePoint Point;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<UMapMarkerTypeAsset> MarkerType = nullptr;

    UPROPERTY(BlueprintReadOnly)
    EWorldMapMarkerLayerRelation LayerRelation = EWorldMapMarkerLayerRelation::Current;

    UPROPERTY(BlueprintReadOnly)
    bool bSelected = false;

    /** Player tracking intent, not EGameZonePointUpdateMode::Tracked. */
    UPROPERTY(BlueprintReadOnly)
    bool bPlayerTracked = false;

    UPROPERTY(BlueprintReadOnly)
    bool bClampedToEdge = false;

    /** Unit direction from the viewport center. Useful for rotating edge arrows. */
    UPROPERTY(BlueprintReadOnly)
    FVector2D DirectionFromViewCenter = FVector2D::ZeroVector;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnRPGMapMarkerHovered, const FGuid&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRPGMapMarkerActivated, const FGuid&);

/** Visual adapter for one point marker. It never queries gameplay systems. */
UCLASS(Abstract, Blueprintable)
class RPGGAMEPLAY_API URPGMapMarkerWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void ApplyMarkerView(const FWorldMapMarkerView& View);
    void ReleaseMarker();

    UFUNCTION(BlueprintPure, Category="RPG|Map Marker")
    FGuid GetPointId() const { return MarkerView.Point.Data.PointId; }

    UFUNCTION(BlueprintPure, Category="RPG|Map Marker")
    const FWorldMapMarkerView& GetMarkerView() const { return MarkerView; }

    UFUNCTION(BlueprintCallable, Category="RPG|Map Marker")
    void RequestMarkerActivation();

    FOnRPGMapMarkerHovered& OnHovered() { return HoveredDelegate; }
    FOnRPGMapMarkerHovered& OnUnhovered() { return UnhoveredDelegate; }
    FOnRPGMapMarkerActivated& OnActivated() { return ActivatedDelegate; }

protected:
    virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    UFUNCTION(BlueprintImplementableEvent, Category="RPG|Map Marker")
    void OnMarkerViewChanged(const FWorldMapMarkerView& View);

    UFUNCTION(BlueprintImplementableEvent, Category="RPG|Map Marker")
    void OnMarkerReleased();

protected:
    UPROPERTY(BlueprintReadOnly, Transient, Category="RPG|Map Marker")
    FWorldMapMarkerView MarkerView;

private:
    FOnRPGMapMarkerHovered HoveredDelegate;
    FOnRPGMapMarkerHovered UnhoveredDelegate;
    FOnRPGMapMarkerActivated ActivatedDelegate;
};
