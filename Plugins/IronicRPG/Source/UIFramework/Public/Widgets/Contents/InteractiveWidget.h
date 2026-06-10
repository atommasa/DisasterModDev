// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Contents/ElementWidget.h"
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
    // The default text content for this widget
    UPROPERTY(EditAnywhere, Category = "Text")
	FText DefaultButtonText;

    // The text block for displaying content text
    UPROPERTY(meta = (BindWidgetOptional))
    class UTextBlock* ContentText;

	// The rich text block for displaying content text
    UPROPERTY(meta = (BindWidgetOptional))
    class URichTextBlock* ContentRichText;

	// The sound played when the widget is hovered
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    USoundBase* HoverSound;

	// The sound played when the widget is clicked
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    USoundBase* ClickSound;

	// The animation played when the widget is hovered
    UPROPERTY(meta = (BindWidgetAnimOptional), Transient)
    UWidgetAnimation* HoverAnimation;

	// The animation played when the widget is clicked
    UPROPERTY(meta = (BindWidgetAnimOptional), Transient)
    UWidgetAnimation* ClickAnimation;

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
