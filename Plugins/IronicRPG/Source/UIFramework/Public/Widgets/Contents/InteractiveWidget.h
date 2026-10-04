// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Widgets/Contents/ElementWidget.h"
#include "UIDataTypes.h"
#include "InteractiveWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWidgetClicked, UInteractiveWidget*, Widget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWidgetHovered, UInteractiveWidget*, Widget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWidgetUnhovered, UInteractiveWidget*, Widget);

/**
 * Base class for interactive widgets in the UI framework.
 * Provides common logic for interactive visual elements such as animations, sound effects, icons, and text.
 */
UCLASS(Abstract)
class UIFRAMEWORK_API UInteractiveWidget : public UElementWidget
{
	GENERATED_BODY()
	
public:
    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;

    virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

public:
    UFUNCTION(BlueprintCallable, Category="Interactive|Context")
    void SetContext(const FInstancedStruct& InContext);

    UFUNCTION(BlueprintPure, Category="Interactive|Context")
    FInstancedStruct GetContext() const;

    UFUNCTION(BlueprintPure, Category="Interactive|Context")
    bool HasContext() const;

    UFUNCTION(BlueprintCallable, Category="Interactive|Context")
    void ClearContext();

public:
    /**
     * Navigation Graph nodes use this widget instance's own UMG/Object name as their id.
     * No separate authored NavigationId is required.
     */

    /** Per-direction rules applied after a region creates its automatic links. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
    FRPGNavigationOverrides NavigationOverrides;

    const FRPGNavigationOverrides& GetNavigationOverrides() const { return NavigationOverrides; }

    UFUNCTION(BlueprintCallable)
    virtual void PlayHover();

    UFUNCTION(BlueprintCallable)
    virtual void PlayUnhover();

    UFUNCTION(BlueprintCallable)
    virtual void PlayClick();

    UFUNCTION(BlueprintCallable)
    virtual void ResetAnimation();

    UPROPERTY(BlueprintAssignable, Category = "Interactive")
    FOnWidgetClicked OnWidgetClicked;

    UPROPERTY(BlueprintAssignable, Category = "Interactive")
    FOnWidgetHovered OnWidgetHovered;

    UPROPERTY(BlueprintAssignable, Category = "Interactive")
    FOnWidgetUnhovered OnWidgetUnhovered;

protected:
    UFUNCTION(BlueprintImplementableEvent)
    void OnClicked();

    UFUNCTION(BlueprintImplementableEvent)
    void OnHovered();

    UFUNCTION(BlueprintImplementableEvent)
    void OnUnhovered();

protected:
    // The default text content for this widget
    UPROPERTY(EditAnywhere, Category = "Text")
	FText DefaultButtonText;

    // The text block for displaying content text
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    class UTextBlock* ContentText;

	// The rich text block for displaying content text
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    class URichTextBlock* ContentRichText;

	// The sound played when the widget is hovered
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
    USoundBase* HoverSound;

	// The sound played when the widget is clicked
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
    USoundBase* ClickSound;

	// The animation played when the widget is hovered
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetAnimOptional), Transient)
    UWidgetAnimation* HoverAnimation;

	// The animation played when the widget is clicked
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetAnimOptional), Transient)
    UWidgetAnimation* ClickAnimation;

private:
    UPROPERTY(Transient)
    FInstancedStruct Context;

#if WITH_EDITORONLY_DATA
public: // Debug
    UPROPERTY(meta = (BindWidgetOptional))
    class UImage* DebugImage = nullptr;

    // Provides a way to highlight the hovered widget in the editor
    virtual void DebugHighlightHoveredWidget(bool bHovered) {}

    // Provides a way to highlight the clicked widget in the editor
    virtual void DebugHighlightClickedWidget(bool bClicked) {}
#endif

};
