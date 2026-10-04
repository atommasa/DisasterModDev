// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Controllers/BaseControlComponent.h"
#include "UIDataTypes.h"
#include "UIControlComponent.generated.h"

class APlayerController;
struct FInputActionValue;
class UInputAction;
class UUserWidget;
class UWidgetBase;
class URPGRootUILayout;
class UUIWidgetStack;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRPGGameplayInputBlockChanged, bool, bBlocked);

/**
 * Central owner of UI Enhanced Input and Root Layout lifecycle.
 *
 * This component intentionally does not use CommonUI input routing.  It
 * preserves the old UIControlComponent model: InputActions are bound once
 * here, then sent to the top page of System > Modal > Menu.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class UIFRAMEWORK_API UUIControlComponent : public UBaseControlComponent
{
    GENERATED_BODY()

protected:
    UUIControlComponent(const FObjectInitializer& ObjectInitializer);

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    UFUNCTION(BlueprintCallable, Category = "UIControl")
    void Navigate(const FInputActionValue& Value);

    UFUNCTION(BlueprintCallable, Category = "UIControl")
    void StopNavigate(const FInputActionValue& Value);

    UFUNCTION(BlueprintCallable, Category = "UIControl")
    void SwitchUIPage(const FInputActionValue& Value);

    UFUNCTION(BlueprintCallable, Category = "UIControl")
    void Confirm();

    UFUNCTION(BlueprintCallable, Category = "UIControl")
    void Cancel();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UIControl")
    float NavigationCooldown = 0.15f;

    UPROPERTY(BlueprintReadOnly, Category = "UIControl")
    bool bCanNavigation = true;

    UPROPERTY(BlueprintReadOnly, Category = "UIControl")
    bool bIsNavigating = false;

public:
    UFUNCTION(BlueprintCallable, Category = "RPG|UI")
    bool InitializeRootLayout();

    /**
     * Adds a page to a UI layer. When bClosePreviousWidget is false, every
     * page that is already visible remains visible behind the new page.
     */
    UFUNCTION(BlueprintCallable, Category = "RPG|UI", meta=(AutoCreateRefTerm = "Args", DeterminesOutputType = "WidgetClass"))
    UWidgetBase* PushWidget(ERPGUILayer Layer, TSubclassOf<UWidgetBase> WidgetClass, const FRPGUIOpenArgs& Args, bool bClosePreviousWidget = true);

    UFUNCTION(BlueprintCallable, Category = "RPG|UI")
    void RemoveWidget(ERPGUILayer Layer, UWidgetBase* Widget);

    UFUNCTION(BlueprintCallable, Category = "RPG|UI")
    void CloseTopWidget(ERPGUILayer Layer);

    UFUNCTION(BlueprintCallable, Category = "RPG|UI")
    void CloseTopInteractiveWidget();

    /** Closes every widget in the selected layer. Returns false when the layer stack is unavailable. */
    UFUNCTION(BlueprintCallable, Category = "RPG|UI")
    bool CloseAllWidgetsInLayer(ERPGUILayer Layer);

    /**
     * Closes widgets from the top of the selected layer until WidgetInstance
     * becomes the top widget. Returns false and does nothing when WidgetInstance
     * is not in that layer.
     */
    UFUNCTION(BlueprintCallable, Category = "RPG|UI")
    bool PopToWidget(ERPGUILayer Layer, UWidgetBase* WidgetInstance);

    // Compatibility wrapper for existing Blueprints.
    UFUNCTION(BlueprintCallable, Category = "RPG|UI")
    void ClearLayer(ERPGUILayer Layer);

    UFUNCTION(BlueprintCallable, Category = "RPG|UI")
    void CloseAllMenus();

    /**
     * Creates the single Gameplay HUD owned by this component. Safe to call
     * repeatedly; an existing HUD is returned through GetGameplayHUD().
     */
    UFUNCTION(BlueprintCallable, Category = "RPG|UI|HUD")
    bool InitializeGameplayHUD();

    UFUNCTION(BlueprintPure, Category = "RPG|UI|HUD")
    UUserWidget* GetGameplayHUD() const { return GameplayHUD; }

    UFUNCTION(BlueprintCallable, Category = "RPG|UI|HUD")
    void SetGameplayHUDVisibility(ESlateVisibility InVisibility);

    // Compatibility wrapper for existing Blueprints.
    UFUNCTION(BlueprintCallable, Category = "RPG|UI")
    void SetHUDWidgetVisibility(ESlateVisibility InVisibility);

    UFUNCTION(BlueprintPure, Category = "RPG|UI")
    bool IsGameplayInputBlocked() const;

    UFUNCTION(BlueprintPure, Category = "RPG|UI")
    URPGRootUILayout* GetRootLayout() const { return RootLayout; }

    UFUNCTION(BlueprintPure, Category = "RPG|UI")
    UWidgetBase* GetTopWidget(ERPGUILayer Layer) const;

    UFUNCTION(BlueprintPure, Category = "RPG|UI")
    UWidgetBase* GetTopInteractiveWidget() const;

    UFUNCTION(BlueprintCallable, Category = "RPG|UI")
    void AddUIControlTag(FGameplayTag NewTag);

    UFUNCTION(BlueprintCallable, Category = "RPG|UI")
    void RemoveUIControlTag(FGameplayTag RemoveTag);

    UFUNCTION(BlueprintPure, Category = "RPG|UI")
    bool HasUIControlTag(FGameplayTag InTag, bool bExact = true) const;

    UPROPERTY(BlueprintAssignable, Category = "RPG|UI")
    FRPGGameplayInputBlockChanged OnGameplayInputBlockChanged;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "RPG|UI")
    TSubclassOf<URPGRootUILayout> RootLayoutClass;

    UPROPERTY(EditDefaultsOnly, Category = "RPG|UI")
    int32 RootLayoutZOrder = 0;

    UPROPERTY(EditDefaultsOnly, Category = "RPG|UI")
    bool bAutoInitializeRootLayout = true;

    UPROPERTY(EditDefaultsOnly, Category = "RPG|UI|HUD")
    TSubclassOf<UUserWidget> GameplayHUDClass;

    UPROPERTY(EditDefaultsOnly, Category = "RPG|UI|HUD")
    bool bAutoInitializeGameplayHUD = true;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> NavigateAction = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> SwitchUIPageAction = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> ConfirmAction = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> CancelAction = nullptr;

private:
    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> GameplayHUD;

    UPROPERTY(Transient)
    TObjectPtr<URPGRootUILayout> RootLayout;

    UPROPERTY(Transient)
    FGameplayTagContainer UIControlTags;

    APlayerController* GetOwningPlayerController() const;
    UUIWidgetStack* GetStackForLayer(ERPGUILayer Layer) const;

    void RefreshInputState();

    /**
     * Real-time timestamp for the next permitted repeated navigation input.
     * Uses FPlatformTime::Seconds(), so it keeps advancing while the game is paused.
     */
    double NextNavigationAllowedRealTime = 0.0;

    bool HasAnyActiveBlockingWidget() const;
};
