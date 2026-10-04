// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameZoneSubsystem.h"
#include "RPGMiniMapWidget.generated.h"

class URPGMapPresentationModel;
class UImage;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UCanvasPanel;
class URPGMapMarkerWidget;
class URPGMiniMapMarkerRenderer;
class UMapTrackingSubsystem;
struct FRPGMapPresentationMarkerChange;

/** A single follow sample shared by MiniMap rendering. Range is center-to-edge, in meters. */
USTRUCT(BlueprintType)
struct RPGGAMEPLAY_API FMiniMapViewState
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category="MiniMap")
    FVector CenterWorldLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="MiniMap")
    float ViewRangeMeters = 100.0f;

    UPROPERTY(BlueprintReadOnly, Category="MiniMap")
    FVector2D ViewportSize = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="MiniMap")
    float LocalUnitsPerWorldMeter = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="MiniMap")
    bool bPresentationReady = false;

    UPROPERTY(BlueprintReadOnly, Category="MiniMap")
    bool bHasFollowTarget = false;

    /** Valid while the MiniMap is temporarily centered on a marker instead of the owning player's Pawn. */
    UPROPERTY(BlueprintReadOnly, Category="MiniMap")
    FGuid FocusedMarkerId;

    UPROPERTY(BlueprintReadOnly, Category="MiniMap")
    bool bHasValidViewport = false;

    UPROPERTY(BlueprintReadOnly, Category="MiniMap")
    bool bHasSheet = false;

    UPROPERTY(BlueprintReadOnly, Category="MiniMap")
    FVector2D MapCenterUV = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="MiniMap")
    FVector2D MapUVAxisX = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="MiniMap")
    FVector2D MapUVAxisY = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="MiniMap")
    bool bHasMapProjection = false;

    /** Render-transform degrees for an icon authored pointing up. World +Y is north and +X is west. */
    UPROPERTY(BlueprintReadOnly, Category="MiniMap")
    float PawnAngle = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="MiniMap")
    float CameraAngle = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="MiniMap")
    bool bHasPawnDirection = false;

    UPROPERTY(BlueprintReadOnly, Category="MiniMap")
    bool bHasCameraDirection = false;
};

/** HUD-owned, non-interactive MiniMap adapter. Does not own the HUD or a UI page stack entry. */
UCLASS(Abstract, Blueprintable)
class RPGGAMEPLAY_API URPGMiniMapWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Invalid input leaves the current range unchanged; finite positive input is clamped to the configured limits. */
    UFUNCTION(BlueprintCallable, Category="MiniMap")
    void SetViewRangeMeters(float NewRangeMeters);

    UFUNCTION(BlueprintPure, Category="MiniMap")
    float GetViewRangeMeters() const { return ViewRangeMeters; }

    /** Resamples current state and retries a failed start. Does not restart a healthy presentation. */
    UFUNCTION(BlueprintCallable, Category="MiniMap")
    void RefreshMiniMap();

    UFUNCTION(BlueprintPure, Category="MiniMap")
    bool IsPresentationActive() const;

    /** Temporarily follows the marker's current presentation location until FocusOnPlayer is called or the marker disappears. */
    UFUNCTION(BlueprintCallable, Category="MiniMap|Markers")
    bool FocusOnMarker(const FGuid& PointId);

    UFUNCTION(BlueprintCallable, Category="MiniMap")
    bool FocusOnPlayer();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UFUNCTION(BlueprintNativeEvent, Category="MiniMap")
    void OnMiniMapPresentationReady();

    UFUNCTION(BlueprintNativeEvent, Category="MiniMap")
    void OnMiniMapPresentationFailed();

    UFUNCTION(BlueprintNativeEvent, Category="MiniMap")
    void OnMiniMapSheetChanged(const FResolvedGameZoneMapSheet& Sheet);

    UFUNCTION(BlueprintNativeEvent, Category="MiniMap")
    void OnMiniMapSheetUnavailable();

    UFUNCTION(BlueprintNativeEvent, Category="MiniMap")
    void OnMiniMapViewChanged(const FMiniMapViewState& View);

    /** Completion only. Sheet changes/unavailability clear CurrentTexture before their respective events. */
    UFUNCTION(BlueprintNativeEvent, Category="MiniMap")
    void OnMiniMapTextureChanged(const FGameZoneMapTextureResult& Result);

protected:
    /** Fill the same local rect as this UserWidget. Optional to preserve data-only MiniMap subclasses. */
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="MiniMap|Background")
    TObjectPtr<UImage> MapImage;

    /** UI material implementing the native MiniMap parameter contract. Each Widget creates its own MID. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MiniMap|Background")
    TObjectPtr<UMaterialInterface> MapMaterial;

    UPROPERTY(Transient, BlueprintReadOnly, Category="MiniMap|Background")
    TObjectPtr<UMaterialInstanceDynamic> MapDynamicMaterial;

    UPROPERTY(Transient, BlueprintReadOnly, Category="MiniMap|Background")
    TObjectPtr<UTexture2D> CurrentTexture;

    UPROPERTY(Transient, BlueprintReadOnly, Category="MiniMap|Background")
    bool bTexturePending = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MiniMap|View", meta=(ClampMin="0.01", Units="m"))
    float ViewRangeMeters = 100.0f;

    UPROPERTY(EditDefaultsOnly, Category="MiniMap|View", meta=(ClampMin="0.01", Units="m"))
    float MinimumViewRangeMeters = 1.0f;

    UPROPERTY(EditDefaultsOnly, Category="MiniMap|View", meta=(ClampMin="0.01", Units="m"))
    float MaximumViewRangeMeters = 10000.0f;

    UPROPERTY(EditDefaultsOnly, Category="MiniMap|Markers", meta=(ClampMin="0.0", Units="s"))
    float TrackedMarkerRefreshInterval = 0.1f;

    /** Fill the same local rect as MapImage; this Canvas and its children never receive input. */
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="MiniMap|Markers")
    TObjectPtr<UCanvasPanel> MarkerCanvas;

    /** Same viewport rect as MarkerCanvas. Tracked markers stay here both inside and outside the safe circle. */
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="MiniMap|Markers")
    TObjectPtr<UCanvasPanel> EdgeMarkerCanvas;

    UPROPERTY(EditDefaultsOnly, Category="MiniMap|Markers")
    TSubclassOf<URPGMapMarkerWidget> DefaultMarkerWidgetClass;

    /** Visual class for the player-tracked marker. Null falls back to DefaultMarkerWidgetClass. */
    UPROPERTY(EditDefaultsOnly, Category="MiniMap|Markers")
    TSubclassOf<URPGMapMarkerWidget> EdgeMarkerWidgetClass;

    /** Fixed local-unit footprint, including any badges/arrows. BP visuals must stay inside this rectangle. */
    UPROPERTY(EditDefaultsOnly, Category="MiniMap|Markers", meta=(ClampMin="1.0"))
    FVector2D MarkerSize = FVector2D(24.0);

    UPROPERTY(EditDefaultsOnly, Category="MiniMap|Markers", meta=(ClampMin="0.0"))
    float MarkerEdgePadding = 4.0f;

    /** Previously visible Widgets are retained hidden in the expanded query rectangle to avoid boundary churn. */
    UPROPERTY(EditDefaultsOnly, Category="MiniMap|Markers", meta=(ClampMin="0.0", Units="m"))
    float MarkerRetentionMeters = 20.0f;

    UPROPERTY(EditDefaultsOnly, Category="MiniMap|Markers", meta=(ClampMin="0"))
    int32 MarkerWidgetPoolLimit = 64;

    UPROPERTY(Transient, BlueprintReadOnly, Category="MiniMap")
    FRPGId CurrentZoneId;

    UPROPERTY(Transient, BlueprintReadOnly, Category="MiniMap")
    FResolvedGameZoneMapSheet CurrentSheet;

    UPROPERTY(Transient, BlueprintReadOnly, Category="MiniMap")
    FMiniMapViewState ViewState;

    UPROPERTY(Transient, BlueprintReadOnly, Category="MiniMap")
    bool bPresentationFailed = false;

private:
    void RefreshState(bool bForceSheetQuery);
    void UpdateFollowState(bool bForceSheetQuery);
    void BindSources();
    void ResetSession();
    void InitializeMapMaterial();
    void UpdateMapMaterial();
    void ClearTexture();
    void HandleMapTextureReady(const FGameZoneMapTextureResult& Result);
    float ClampRange(float Range) const;
    void HandlePresentationReady(const FGameZonePresentationSnapshot& Snapshot);
    void HandlePresentationFailed();
    void HandleMarkerChanged(const FRPGMapPresentationMarkerChange& Change);
    void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

    UFUNCTION()
    void HandleTravelCompleted(const FGameZoneContext& Context);

    UFUNCTION()
    void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);

    UFUNCTION()
    void HandleTrackingChanged(FGuid PreviousPointId, FGuid CurrentPointId);

private:
    UPROPERTY(Transient)
    TObjectPtr<URPGMapPresentationModel> PresentationModel;

    UPROPERTY(Transient)
    TObjectPtr<URPGMiniMapMarkerRenderer> MarkerRenderer;

    UPROPERTY(Transient)
    TObjectPtr<UGameZoneSubsystem> GameZoneSubsystem;

    UPROPERTY(Transient)
    TObjectPtr<UMapTrackingSubsystem> TrackingSubsystem;

    TWeakObjectPtr<APlayerController> BoundController;
    TWeakObjectPtr<UWorld> SessionWorld;
    TWeakObjectPtr<UWorld> CleanedUpWorld;
    FGuid FocusedMarkerId;
    FVector2D LastViewportSize = FVector2D::ZeroVector;
    bool bConstructed = false;
    bool bRefreshing = false;
    bool bMaterialDirty = true;
    bool bHasVisibleFrame = false;
    uint64 LastVisibleFrame = 0;
};
