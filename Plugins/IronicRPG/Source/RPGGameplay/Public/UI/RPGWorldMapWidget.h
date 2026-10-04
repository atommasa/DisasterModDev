// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameZoneSubsystem.h"
#include "UI/RPGMapMarkerWidget.h"
#include "Widgets/WidgetBase.h"
#include "RPGWorldMapWidget.generated.h"

class UCanvasPanel;
class URPGMapPresentationModel;
class UMapTrackingSubsystem;
class UTexture2D;
struct FRPGMapPresentationMarker;
struct FRPGMapPresentationMarkerChange;

UENUM(BlueprintType)
enum class EWorldMapMarkerLayerMode : uint8
{
    AllLayers,
    SelectedLayerOnly,
};

/** Blueprint-facing result of one World Map marker click. */
USTRUCT(BlueprintType)
struct RPGGAMEPLAY_API FMapMarkerCandidateSet
{
    GENERATED_BODY()

public:
    /** Position in the World Map widget's local space. Keep popup UI outside MapContentRoot. */
    UPROPERTY(BlueprintReadOnly)
    FVector2D AnchorLocalPosition = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    TArray<FWorldMapMarkerView> Candidates;
};

/** Native owner of the first-stage World Map presentation and point markers. */
UCLASS(Abstract, Blueprintable)
class RPGGAMEPLAY_API URPGWorldMapWidget : public UWidgetBase
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

public:
    UFUNCTION(BlueprintCallable, Category="RPG|WorldMap")
    bool SelectLayer(const FGameZoneMapLayerId& LayerId);

    UFUNCTION(BlueprintCallable, Category="RPG|WorldMap")
    bool FocusOnPlayer();

    /** Displays the marker's Sheet when needed and moves the view center to its current presentation location. */
    UFUNCTION(BlueprintCallable, Category="RPG|WorldMap|Marker")
    bool FocusOnMarker(const FGuid& PointId);

    UFUNCTION(BlueprintCallable, Category="RPG|WorldMap")
    void RefreshTrackedMarkers();

    UFUNCTION(BlueprintCallable, Category="RPG|WorldMap")
    void SetViewCenterWorldLocation(const FVector& WorldLocation);

    UFUNCTION(BlueprintCallable, Category="RPG|WorldMap")
    void SetViewZoom(float NewZoom);

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="RPG|WorldMap|Input")
    void ApplyViewZoomInput(float InputAmount);
    virtual void ApplyViewZoomInput_Implementation(float InputAmount);

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="RPG|WorldMap|Input")
    void ApplyViewPanInput(const FVector2D& ScreenDelta);
    virtual void ApplyViewPanInput_Implementation(const FVector2D& ScreenDelta);

    UFUNCTION(BlueprintPure, Category="RPG|WorldMap")
    FVector2D WorldLocationToCanvasPosition(const FVector& WorldLocation) const;

    UFUNCTION(BlueprintCallable, Category="RPG|WorldMap")
    bool ProjectWorldLocationToDisplayedSheet(const FVector& WorldLocation, FGameZoneMapProjection& OutProjection) const;

    UFUNCTION(BlueprintCallable, Category="RPG|WorldMap|Marker")
    bool RequestSelectedMarkerAction(const FGameplayTag& ActionTag);

    UFUNCTION(BlueprintCallable, Category="RPG|WorldMap|Marker")
    void ConfirmPendingMarkerAction(bool bConfirmed);

    UFUNCTION(BlueprintCallable, Category="RPG|WorldMap|Marker")
    void ClearMarkerSelection();

    UFUNCTION(BlueprintCallable, Category="RPG|WorldMap|Marker")
    bool ChooseMarkerCandidate(const FGuid& PointId);

    UFUNCTION(BlueprintCallable, Category="RPG|WorldMap|Marker")
    void DismissMarkerCandidates();

protected:
    UFUNCTION(BlueprintImplementableEvent, Category="RPG|WorldMap")
    void OnWorldMapPresentationReady();

    UFUNCTION(BlueprintImplementableEvent, Category="RPG|WorldMap")
    void OnWorldMapPresentationFailed();

    UFUNCTION(BlueprintImplementableEvent, Category="RPG|WorldMap")
    void OnWorldMapDisplayedSheetChanged(const FResolvedGameZoneMapSheet& Sheet);

    UFUNCTION(BlueprintImplementableEvent, Category="RPG|WorldMap")
    void OnWorldMapSheetUnavailable(const FGameZoneMapLayerId& LayerId);

    UFUNCTION(BlueprintImplementableEvent, Category="RPG|WorldMap")
    void OnWorldMapTextureChanged(const FGameZoneMapTextureResult& Result);

    UFUNCTION(BlueprintImplementableEvent, Category="RPG|WorldMap|Marker")
    void OnWorldMapMarkerHovered(const FWorldMapMarkerView& Marker);

    UFUNCTION(BlueprintImplementableEvent, Category="RPG|WorldMap|Marker")
    void OnWorldMapMarkerUnhovered(const FGuid& PointId);

    UFUNCTION(BlueprintImplementableEvent, Category="RPG|WorldMap|Marker")
    void OnWorldMapSelectionChanged(bool bHasSelection, const FWorldMapMarkerView& Marker);

    /** An empty set closes the candidate picker. */
    UFUNCTION(BlueprintImplementableEvent, Category="RPG|WorldMap|Marker")
    void OnMapMarkerCandidatesChanged(const FMapMarkerCandidateSet& CandidateSet);

    UFUNCTION(BlueprintImplementableEvent, Category="RPG|WorldMap|Marker")
    void OnWorldMapActionsChanged(const TArray<FGameZoneMarkerActionOption>& Actions);

    UFUNCTION(BlueprintImplementableEvent, Category="RPG|WorldMap|Marker")
    void OnWorldMapActionConfirmationRequested(const FGameZoneMarkerActionOption& Action);

    UFUNCTION(BlueprintImplementableEvent, Category="RPG|WorldMap|Marker")
    void OnWorldMapActionPendingChanged(bool bPending);

    UFUNCTION(BlueprintImplementableEvent, Category="RPG|WorldMap|Marker")
    void OnWorldMapActionRejected(const FText& Reason);

    UFUNCTION(BlueprintImplementableEvent, Category="RPG|WorldMap|Marker")
    void OnWorldMapActionCompleted(const FGameZoneMarkerActionResult& Result);

protected:
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UCanvasPanel> MapContentRoot = nullptr;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UCanvasPanel> MarkerCanvas = nullptr;

    /** Optional viewport-sized sibling of MapContentRoot used by the player's tracked marker. */
    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UCanvasPanel> EdgeMarkerCanvas = nullptr;

    UPROPERTY(EditDefaultsOnly, Category="RPG|WorldMap|Marker")
    TSubclassOf<URPGMapMarkerWidget> DefaultMarkerWidgetClass;

    /** Visual class for the player-tracked marker. Null falls back to DefaultMarkerWidgetClass. */
    UPROPERTY(EditDefaultsOnly, Category="RPG|WorldMap|Marker")
    TSubclassOf<URPGMapMarkerWidget> EdgeMarkerWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category="RPG|WorldMap|Marker", meta=(ClampMin="0"))
    int32 MarkerWidgetPoolLimit = 128;

    UPROPERTY(EditDefaultsOnly, Category="RPG|WorldMap|Marker", meta=(ClampMin="0.01"))
    float TrackedMarkerRefreshInterval = 0.1f;

    /** Extra local Slate units around each rendered marker that count as a pointer hit. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="RPG|WorldMap|Marker", meta=(ClampMin="0"))
    float MarkerSelectionPadding = 8.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="RPG|WorldMap|Marker")
    EWorldMapMarkerLayerMode MarkerLayerMode = EWorldMapMarkerLayerMode::AllLayers;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="RPG|WorldMap|Marker", meta=(ClampMin="0"))
    float MarkerEdgePadding = 8.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="RPG|WorldMap|View", meta=(ClampMin="0.001"))
    float CanvasUnitsPerWorldMeter = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="RPG|WorldMap|View", meta=(ClampMin="0"))
    float ViewPanBoundsPaddingMeters = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="RPG|WorldMap|View", meta=(ClampMin="0.01"))
    float MinimumViewZoom = 0.1f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="RPG|WorldMap|View", meta=(ClampMin="0.01"))
    float MaximumViewZoom = 8.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="RPG|WorldMap|Input", meta=(ClampMin="0.001"))
    float ViewZoomStep = 0.05f;

    UPROPERTY(BlueprintReadOnly, Category="RPG|WorldMap")
    FRPGId CurrentZoneId;

    UPROPERTY(BlueprintReadOnly, Category="RPG|WorldMap")
    FGameZoneMapWorldBounds CurrentMapWorldBounds;

    UPROPERTY(BlueprintReadOnly, Category="RPG|WorldMap")
    FVector FocusWorldLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="RPG|WorldMap")
    FVector ViewCenterWorldLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="RPG|WorldMap")
    float ViewZoom = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category="RPG|WorldMap")
    FGameZonePresentationHandle PresentationHandle;

    UPROPERTY(BlueprintReadOnly, Category="RPG|WorldMap")
    TArray<FGameZoneMapLayerCatalogEntry> MapLayers;

    UPROPERTY(BlueprintReadOnly, Category="RPG|WorldMap")
    TArray<FGameZoneMapSheetCatalogEntry> MapSheets;

    UPROPERTY(BlueprintReadOnly, Category="RPG|WorldMap")
    FGameZoneMapLayerId CurrentLayerId;

    UPROPERTY(BlueprintReadOnly, Category="RPG|WorldMap")
    FResolvedGameZoneMapSheet CurrentSheet;

    UPROPERTY(BlueprintReadOnly, Category="RPG|WorldMap")
    TObjectPtr<UTexture2D> CurrentTexture = nullptr;

    UPROPERTY(BlueprintReadOnly, Category="RPG|WorldMap|Marker")
    TArray<FGameZoneMarkerActionOption> CurrentActionOptions;

    UPROPERTY(BlueprintReadOnly, Category="RPG|WorldMap")
    bool bPresentationReady = false;

    UPROPERTY(BlueprintReadOnly, Transient, Category="RPG|WorldMap|Input")
    bool bIsPanning = false;

private:
    friend struct FRPGWorldMapTrackingTestAccess;

    UFUNCTION()
    void HandleTrackingChanged(FGuid PreviousPointId, FGuid CurrentPointId);

    void StartPresentation();
    void StopPresentation();
    FVector ClampViewCenterWorldLocation(const FVector& WorldLocation) const;
    bool DisplaySheet(const FResolvedGameZoneMapSheet& Sheet);
    void ResetMarkers();
    void ApplyPresentationMarker(const FRPGMapPresentationMarker& Marker);
    void RemoveMarker(const FGuid& PointId);
    void ReconcileAllMarkers();
    void ReconcileMarker(const FGuid& PointId);
    bool BuildMarkerView(const FRPGMapPresentationMarker& Marker, bool bSelected, FWorldMapMarkerView& OutView) const;
    bool TryGetPresentedMarkerView(const FGuid& PointId, bool bSelected, FWorldMapMarkerView& OutView) const;
    bool ShouldMaterializeMarker(const FWorldMapMarkerView& View) const;
    EWorldMapMarkerLayerRelation ResolveLayerRelation(const FGameZoneMapLayerId& MarkerLayerId) const;
    TSubclassOf<URPGMapMarkerWidget> ResolveMarkerWidgetClass(bool bPlayerTracked) const;
    URPGMapMarkerWidget* AcquireMarkerWidget(TSubclassOf<URPGMapMarkerWidget> WidgetClass);
    void ReleaseMarkerWidget(const FGuid& PointId);
    void ReleaseAllMarkerWidgets(bool bRetainPool);
    bool UpdateMarkerWidgetPlacement(const FGuid& PointId, URPGMapMarkerWidget& Widget, FWorldMapMarkerView View);
    void UpdateViewTransform();
    void UpdateMarkerWidgetScale(URPGMapMarkerWidget& Widget) const;
    void HandleMarkerHovered(const FGuid& PointId);
    void HandleMarkerUnhovered(const FGuid& PointId);
    void HandleMarkerActivated(const FGuid& PointId);
    void ResolveMarkerClick(const FGeometry& InGeometry, const FVector2D& LocalPosition);
    void SetMarkerCandidates(const FVector2D& AnchorLocalPosition, const TArray<FWorldMapMarkerView>& Candidates);
    void ClearMarkerCandidates(bool bNotifyBlueprint = true);
    void SelectMarker(const FGuid& PointId);
    void ClearMarkerSelectionInternal();
    void RefreshSelectedMarker();
    bool BeginMarkerAction(const FGameZoneMarkerActionOption& Action);

    void HandlePresentationReady(const FGameZonePresentationSnapshot& Snapshot);

    void HandlePresentationFailed();

    void HandleMapTextureReady(const FGameZoneMapTextureResult& Result);

    void HandleMapMarkerChange(const FRPGMapPresentationMarkerChange& Change);

    UFUNCTION()
    void HandleMarkerActionCompleted(const FGameZoneMarkerActionResult& Result);

private:
    UPROPERTY(Transient)
    TObjectPtr<UGameZoneSubsystem> GameZoneSubsystem = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<URPGMapPresentationModel> PresentationModel = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UMapTrackingSubsystem> TrackingSubsystem = nullptr;

    UPROPERTY(Transient)
    TMap<FGuid, TObjectPtr<URPGMapMarkerWidget>> ActiveMarkerWidgets;

    UPROPERTY(Transient)
    TArray<TObjectPtr<URPGMapMarkerWidget>> MarkerWidgetPool;

    TSet<FGuid> EdgeMarkerPointIds;
    TSet<FGuid> PendingCandidatePointIds;
    FGuid SelectedPointId;
    FGuid ActiveActionExecutionId;
    FGameplayTag PendingConfirmationActionTag;
    FGameZoneMarkerActionOption PendingConfirmationAction;
    bool bAwaitingActionConfirmation = false;
    bool bActionPending = false;
    bool bPointerDown = false;
    bool bPanTriggered = false;
    bool bViewTransformDirty = true;
    FVector2D LastEdgeMarkerViewportSize = FVector2D::ZeroVector;
    FVector2D PointerDownLocalPosition = FVector2D::ZeroVector;
};
