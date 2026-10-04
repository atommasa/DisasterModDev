// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UIDataTypes.h"
#include "Widgets/WidgetBase.h"
#include "NavigationInterface.h"
#include "NavigationWidget.generated.h"

class UInteractiveWidget;
class UPanelWidget;

/** A runtime directional connection in a Navigation Graph. */
USTRUCT()
struct FRPGRuntimeNavigationEdge
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    FName TargetNavigationId = NAME_None;

    UPROPERTY(Transient)
    bool bRequireNewPress = false;

    UPROPERTY(Transient)
    bool bDynamic = false;

    bool IsValid() const
    {
        return bDynamic || !TargetNavigationId.IsNone();
    }
};

/** A runtime node which represents one interactive widget. */
USTRUCT()
struct FRPGRuntimeNavigationNode
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    TObjectPtr<UInteractiveWidget> Widget = nullptr;

    UPROPERTY(Transient)
    FRPGRuntimeNavigationEdge Up;

    UPROPERTY(Transient)
    FRPGRuntimeNavigationEdge Down;

    UPROPERTY(Transient)
    FRPGRuntimeNavigationEdge Left;

    UPROPERTY(Transient)
    FRPGRuntimeNavigationEdge Right;
};

/** Registered at runtime by Blueprint or C++ to create automatic links for one panel. */
struct FRPGRuntimeNavigationRegion
{
    FName RegionId = NAME_None;
    TWeakObjectPtr<UPanelWidget> SourceContainer;
    ERPGNavigationRegionLayout Layout = ERPGNavigationRegionLayout::Manual;
    bool bAllowWrap = false;
    bool bRequireNewPressForWrap = true;
};

/**
 * Navigation Graph owner.
 *
 * Existing subclasses can continue to provide CollectNavigatableButtons() for a legacy grid.
 * Static UMG panels can be authored in StaticNavigationRegions. Dynamic panels can be
 * registered at runtime from ConfigureNavigationRegions() or normal Blueprint/C++ code.
 * NavigationLinks and per-widget NavigationOverrides handle exceptional links.
 */
UCLASS(Abstract)
class UIFRAMEWORK_API UNavigationWidget : public UWidgetBase, public INavigationInterface
{
    GENERATED_BODY()

protected: // UWidgetBase
    virtual void NativeConstruct() override;

protected:
    /** Rebuilds all nodes and links. Call this after dynamically creating/removing navigatable widgets. */
    UFUNCTION(BlueprintCallable, Category = "Navigation")
    virtual void RebuildNavigation();

    /**
     * Legacy grid collector retained for UGridWidget and existing pages.
     * Override it only when this widget uses the old coordinate-grid workflow.
     */
    virtual void CollectNavigatableButtons(TMap<FIntPoint, UInteractiveWidget*>& OutButtons) {}

    /**
     * Optional Blueprint hook for dynamic regions. Register panels here by calling
     * RegisterNavigationRegion(). This event runs before every graph rebuild.
     *
     * Static UMG regions should normally be authored in StaticNavigationRegions instead.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Navigation|Regions")
    void ConfigureNavigationRegions();

protected:
    UPROPERTY(BlueprintReadWrite, Category = "Buttons")
    TMap<FIntPoint, TObjectPtr<UInteractiveWidget>> ButtonCoordMap;

    UPROPERTY(BlueprintReadWrite, Category = "Buttons")
    FIntPoint CurrentFocusCoord = FIntPoint::ZeroValue;

    UPROPERTY(EditDefaultsOnly, Category = "Buttons")
    FIntPoint DefaultFocusCoord = FIntPoint::ZeroValue;

    /** Used by graph-only pages when DefaultFocusCoord has no matching legacy button. */
    UPROPERTY(EditDefaultsOnly, Category = "RPG|Navigation", meta=(DisplayName="Default Focus Widget Name"))
    FName DefaultFocusNavigationId = NAME_None;

    UPROPERTY(BlueprintReadWrite, Category = "Container")
    int32 MaxRow = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Container")
    int32 MaxCol = 0;

    /**
     * Legacy grid wrap settings. They only affect auto-generated links from CollectNavigatableButtons().
     * Region-specific wrap settings are passed to RegisterNavigationRegion().
     */
    UPROPERTY(EditDefaultsOnly, Category = "RPG|Navigation")
    bool bAllowWrap = true;

    UPROPERTY(EditDefaultsOnly, Category = "RPG|Navigation")
    bool bWarpProtection = true;

    /**
     * Authored UMG regions resolved from WidgetTree by the SourceWidget picker.
     * Use these for fixed page layouts such as a main menu, static tab bar, or action bar.
     * Dynamic regions registered with RegisterNavigationRegion() are applied after these
     * definitions and therefore replace a static region with the same RegionId.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Navigation|Regions")
    TArray<FRPGNavigationRegionDefinition> StaticNavigationRegions;

    /** Authored links that are applied after auto links and per-widget overrides. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Navigation")
    TArray<FRPGNavigationLink> NavigationLinks;

    /** Runtime graph, inspectable from Blueprint for debugging but never serialized. */
    UPROPERTY(Transient)
    TMap<FName, FRPGRuntimeNavigationNode> NavigationNodes;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "RPG|Navigation")
    FName CurrentFocusNavigationId = NAME_None;

public:
    /**
     * Register or replace one runtime navigation region.
     *
     * Use this for panels created dynamically. A registration made from normal Blueprint/C++ code
     * persists across RebuildNavigation() calls until removed explicitly or its source is destroyed.
     * When called inside ConfigureNavigationRegions(), it instead applies only to that rebuild.
     */
    UFUNCTION(BlueprintCallable, Category = "Navigation|Regions")
    void RegisterNavigationRegion(
        FName RegionId,
        UPanelWidget* SourceContainer,
        ERPGNavigationRegionLayout Layout,
        bool bRegionAllowWrap = false,
        bool bRequireNewPressForWrap = true
    );

    /** Remove one runtime-only region. Static authored regions are unaffected. */
    UFUNCTION(BlueprintCallable, Category = "Navigation|Regions")
    bool UnregisterNavigationRegion(FName RegionId);

    /** Remove every runtime-only region. Static authored regions are unaffected. */
    UFUNCTION(BlueprintCallable, Category = "Navigation|Regions")
    void ClearDynamicNavigationRegions();

    /** Add or replace a graph link at runtime, useful for dynamic page state. */
    UFUNCTION(BlueprintCallable, Category = "Navigation")
    bool SetNavigationLink(
        FName SourceNavigationId,
        EUINavigation Direction,
        FName TargetNavigationId,
        bool bRequireNewPress = false
    );

    UFUNCTION(BlueprintCallable, Category = "Navigation")
    bool ClearNavigationLink(FName SourceNavigationId, EUINavigation Direction);

    UFUNCTION(BlueprintCallable, Category = "Navigation")
    bool SetNavigationFocusById(FName NavigationId);

    UFUNCTION(BlueprintPure, Category = "Navigation")
    UInteractiveWidget* GetFocusedNavigationWidget() const;

    UFUNCTION(BlueprintPure, Category = "Navigation")
    FName GetFocusedNavigationId() const { return CurrentFocusNavigationId; }

    UFUNCTION(BlueprintCallable)
    virtual void AddButton(FIntPoint Coord, UInteractiveWidget* Button) PURE_VIRTUAL(UNavigationWidget::AddButton, );

    UFUNCTION(BlueprintCallable)
    virtual void RemoveButton(FIntPoint Coord) PURE_VIRTUAL(UNavigationWidget::RemoveButton, );

    UFUNCTION(BlueprintCallable)
    virtual void SetNavigationFocus(FIntPoint Coord) override;

    UFUNCTION(BlueprintCallable)
    virtual void SetNavigationFocusByWidget(UInteractiveWidget* Widget) override;

    UFUNCTION(BlueprintCallable)
    virtual void ResetNavigationFocus() override;

    UFUNCTION(BlueprintCallable)
    virtual bool Navigate(EUINavigation Direction) override;

    virtual bool Confirm_Implementation() override;

    UFUNCTION(BlueprintImplementableEvent, Category = "Navigation")
    void OnNavigate(UInteractiveWidget* CurrentWidget, UInteractiveWidget* LastWidget, FName CurrentNavigationId, FName LastNavigationId);

    /**
     * Used only for ERPGNavigationRule::Dynamic.
     * Return nullptr to block the direction for the current navigation attempt.
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Navigation")
    UInteractiveWidget* ResolveDynamicNavigationTarget(UInteractiveWidget* CurrentWidget, EUINavigation Direction);
    virtual UInteractiveWidget* ResolveDynamicNavigationTarget_Implementation(UInteractiveWidget* CurrentWidget, EUINavigation Direction);

protected:
    /** Adds a node and returns its final unique id. */
    FName AddNavigationNode(UInteractiveWidget* Widget);

    void BuildLegacyGridNavigation();
    void BuildRegionNavigation(const FRPGRuntimeNavigationRegion& Region);
    void BuildRegisteredNavigationRegions();
    void ApplyWidgetNavigationOverrides();
    void ApplyAuthoredNavigationLinks();

    void BuildGridLinks(const TMap<FIntPoint, UInteractiveWidget*>& Buttons, int32 InMaxRow, int32 InMaxCol, bool bInAllowWrap, bool bRequireNewPressForWrap);
    void BuildLinearLinks(const TArray<UInteractiveWidget*>& Widgets, bool bVertical, bool bInAllowWrap, bool bRequireNewPressForWrap);

    void CollectInteractiveWidgets(UPanelWidget* SourceContainer, TArray<UInteractiveWidget*>& OutWidgets) const;
    void CollectInteractiveWidgetsRecursive(UWidget* Widget, TArray<UInteractiveWidget*>& OutWidgets) const;

    UPanelWidget* ResolveStaticRegionContainer(const FWidgetChild& SourceWidget) const;
    void AddOrReplaceRuntimeNavigationRegion(
        TArray<FRPGRuntimeNavigationRegion>& TargetRegions,
        const FRPGRuntimeNavigationRegion& Region
    );

    FName GetNavigationIdForWidget(const UInteractiveWidget* Widget) const;
    FName MakeUniqueNavigationId(FName DesiredId) const;
    FRPGRuntimeNavigationEdge* GetEdge(FRPGRuntimeNavigationNode& Node, EUINavigation Direction);
    const FRPGRuntimeNavigationEdge* GetEdge(const FRPGRuntimeNavigationNode& Node, EUINavigation Direction) const;
    FRPGNavigationOverride GetOverrideForDirection(const FRPGNavigationOverrides& Overrides, EUINavigation Direction) const;

    bool SetNavigationEdge(FName SourceNavigationId, EUINavigation Direction, FName TargetNavigationId, bool bRequireNewPress, bool bDynamic = false);
    bool IsWidgetNavigatable(const UInteractiveWidget* Widget) const;
    void BindWidgetToNavigation(UInteractiveWidget* Widget);
    void SetNavigationFocusInternal(FName NewNavigationId, bool bPlayVisualFeedback);

    /** Fully resolved regions used for the current graph build. */
    TArray<FRPGRuntimeNavigationRegion> NavigationRegions;

    /** Persistent manual runtime registrations. Dynamic entries override static entries with the same id. */
    TArray<FRPGRuntimeNavigationRegion> DynamicNavigationRegions;

    /** Per-rebuild registrations supplied by ConfigureNavigationRegions(). */
    TArray<FRPGRuntimeNavigationRegion> ConfiguredNavigationRegions;

    /** Prevents ConfigureNavigationRegions() registrations from becoming persistent by accident. */
    bool bIsConfiguringNavigationRegions = false;

    /**
     * Only used when two runtime widgets happen to share the same object name.
     * Normal authored UMG widgets always keep their own Designer name as the graph id.
     */
    TMap<const UInteractiveWidget*, FName> RuntimeNavigationIdAliases;

protected: // Sound Effect
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    TObjectPtr<USoundBase> NavigationSound = nullptr;

#if WITH_EDITORONLY_DATA
private: // Debug
    UPROPERTY(EditDefaultsOnly, Category = "Debugging")
    bool bHighlightFocusing = false;
#endif
};
