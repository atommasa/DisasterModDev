// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Overlay.h"
#include "UIWidgetStack.generated.h"

class UWidgetBase;

/**
 * A lightweight UMG-only page stack.
 *
 * The Root Layout owns one stack for each UI layer. A pushed page can either
 * hide its previous page or remain overlaid with every page that is already
 * visible. Unlike CommonUI's stack this class owns no input routing and has
 * no dependency on CommonUI.
 */
UCLASS(BlueprintType, meta = (DisplayName = "UI Widget Stack"))
class UIFRAMEWORK_API UUIWidgetStack : public UOverlay
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "RPG|UI|Stack")
    bool PushWidget(UWidgetBase* Widget, bool bClosePreviousWidget = true);

    UFUNCTION(BlueprintCallable, Category = "RPG|UI|Stack")
    UWidgetBase* PopWidget();

    UFUNCTION(BlueprintCallable, Category = "RPG|UI|Stack")
    bool RemoveWidget(UWidgetBase* Widget);

    /** Pops widgets until Widget becomes the top entry. */
    UFUNCTION(BlueprintCallable, Category = "RPG|UI|Stack")
    bool PopToWidget(UWidgetBase* Widget);

    UFUNCTION(BlueprintCallable, Category = "RPG|UI|Stack")
    void ClearWidgets();

    UFUNCTION(BlueprintPure, Category = "RPG|UI|Stack")
    UWidgetBase* GetTopWidget() const;

    UFUNCTION(BlueprintPure, Category = "RPG|UI|Stack")
    bool IsEmpty() const { return WidgetStack.IsEmpty(); }

    UFUNCTION(BlueprintPure, Category = "RPG|UI|Stack")
    int32 GetStackSize() const { return WidgetStack.Num(); }

    UFUNCTION(BlueprintPure, Category = "RPG|UI|Stack")
    bool ContainsWidget(const UWidgetBase* Widget) const;

private:
    UPROPERTY(Transient)
    TArray<TObjectPtr<UWidgetBase>> WidgetStack;

    /**
     * Entry-aligned record of the exact previous page hidden by each push.
     * This preserves visibility when close and overlay policies are mixed.
     */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UWidgetBase>> HiddenPreviousWidgets;

    void RestoreHiddenPreviousWidget(UWidgetBase* HiddenPreviousWidget);
};
