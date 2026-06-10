// Copyright Ironic Studio. All Rights Reserved.


#include "Widgets/Contents/InteractiveWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Animation/WidgetAnimation.h"

void UInteractiveWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    if (ContentText)
    {
        ContentText->SetText(DefaultButtonText);
    }

    if (ContentRichText)
    {
        ContentRichText->SetText(DefaultButtonText);
    }
}

void UInteractiveWidget::NativeConstruct()
{
	Super::NativeConstruct();

}

void UInteractiveWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
    PlayHover();
}

void UInteractiveWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseLeave(InMouseEvent);
    PlayUnhover();
}

FReply UInteractiveWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        PlayClick();
        return FReply::Handled();
    }
    
    return FReply::Unhandled();
}

FReply UInteractiveWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

void UInteractiveWidget::PlayHover()
{
    if (HoverAnimation)
    {
        PlayAnimation(HoverAnimation, 0.0f);
    }

    if (HoverSound)
    {
        UGameplayStatics::PlaySound2D(this, HoverSound);
    }

    OnWidgetHovered.Broadcast(this);
}

void UInteractiveWidget::PlayUnhover()
{
    if (HoverAnimation)
    {
        PlayAnimation(HoverAnimation, 0.0f, 1, EUMGSequencePlayMode::Reverse);
    }

    OnWidgetUnhovered.Broadcast(this);
}

void UInteractiveWidget::PlayClick()
{
    if (ClickAnimation)
    {
        PlayAnimation(HoverAnimation, 0.0f);
    }

    if (ClickSound)
    {
        UGameplayStatics::PlaySound2D(this, ClickSound);
    }

    OnWidgetClicked.Broadcast(this);
}

void UInteractiveWidget::ResetAnimation()
{
    StopAllAnimations();

    // Reset animation playback position to 0 
    if (HoverAnimation)
    {
        SetAnimationCurrentTime(HoverAnimation, 0.0f);
        PlayAnimation(HoverAnimation, HoverAnimation->GetEndTime(), 1, EUMGSequencePlayMode::Reverse, 1.0f);
        StopAnimation(HoverAnimation);
    }

    if (ClickAnimation)
    {
        SetAnimationCurrentTime(ClickAnimation, 0.0f);
        PlayAnimation(ClickAnimation, ClickAnimation->GetEndTime(), 1, EUMGSequencePlayMode::Reverse, 1.0f);
        StopAnimation(ClickAnimation);
    }

    UE_LOG(LogTemp, Warning, TEXT("%s reset animation successfully"), *GetName());
}
