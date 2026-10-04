// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UIDataTypes.h"
#include "RPGRootUILayout.generated.h"

class UPanelWidget;
class UUIWidgetStack;
class UUserWidget;

/**
 * The one long-lived widget added to the viewport for a UI scene.
 *
 * All pages are children of one of its stacks.  Individual menus must never
 * call AddToViewport directly.
 */
UCLASS(Abstract, Blueprintable)
class UIFRAMEWORK_API URPGRootUILayout : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "RPG|UI")
    UUIWidgetStack* GetMenuStack() const { return MenuStack; }

    UFUNCTION(BlueprintPure, Category = "RPG|UI")
    UUIWidgetStack* GetModalStack() const { return ModalStack; }

    UFUNCTION(BlueprintPure, Category = "RPG|UI")
    UUIWidgetStack* GetSystemStack() const { return SystemStack; }

    UFUNCTION(BlueprintPure, Category = "RPG|UI")
    UUIWidgetStack* GetStackForLayer(ERPGUILayer Layer) const;

    UFUNCTION(BlueprintPure, Category = "RPG|UI")
    UPanelWidget* GetHUDLayer() const { return HUDLayer; }

protected:
    // The corresponding Root Layout Blueprint must contain widgets with these
    // names, mark them Is Variable, and use UUIWidgetStack for the stacks.
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UUIWidgetStack> MenuStack;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UUIWidgetStack> ModalStack;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UUIWidgetStack> SystemStack;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> HUDLayer;

private:
    friend class UUIControlComponent;

    bool AttachGameplayHUD(UUserWidget* Widget);
    bool DetachGameplayHUD(UUserWidget* Widget);
};
