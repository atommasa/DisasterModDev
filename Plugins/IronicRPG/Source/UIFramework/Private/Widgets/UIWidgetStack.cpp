// Copyright Ironic Studio. All Rights Reserved.

#include "Widgets/UIWidgetStack.h"

#include "Widgets/WidgetBase.h"

#include "Components/OverlaySlot.h"

bool UUIWidgetStack::PushWidget(UWidgetBase* Widget, bool bClosePreviousWidget)
{
    if (!IsValid(Widget) || WidgetStack.Contains(Widget))
    {
        return false;
    }

    if (Widget->GetParent())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("UIWidgetStack::PushWidget failed: %s already has a parent."),
            *Widget->GetName()
        );

        return false;
    }

    UWidgetBase* PreviousTop = GetTopWidget();

    UOverlaySlot* NewSlot = AddChildToOverlay(Widget);

    if (!NewSlot)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("UIWidgetStack::PushWidget failed: cannot create Overlay Slot.")
        );

        return false;
    }

    NewSlot->SetHorizontalAlignment(HAlign_Fill);
    NewSlot->SetVerticalAlignment(VAlign_Fill);

    UWidgetBase* HiddenPreviousWidget = nullptr;
    if (bClosePreviousWidget && PreviousTop)
    {
        PreviousTop->SetVisibility(ESlateVisibility::Hidden);
        PreviousTop->NotifyHiddenByStack();
        HiddenPreviousWidget = PreviousTop;
    }

    WidgetStack.Add(Widget);
    HiddenPreviousWidgets.Add(HiddenPreviousWidget);

    Widget->SetVisibility(ESlateVisibility::Visible);
    Widget->NotifyPushedToStack();

    return true;
}

UWidgetBase* UUIWidgetStack::PopWidget()
{
    UWidgetBase* PoppedWidget = GetTopWidget();
    if (!PoppedWidget)
    {
        return nullptr;
    }

    UWidgetBase* HiddenPreviousWidget = HiddenPreviousWidgets.Pop();
    WidgetStack.Pop();

    PoppedWidget->NotifyPoppedFromStack();
    PoppedWidget->RemoveFromParent();

    RestoreHiddenPreviousWidget(HiddenPreviousWidget);

    return PoppedWidget;
}

bool UUIWidgetStack::RemoveWidget(UWidgetBase* Widget)
{
    if (!Widget)
    {
        return false;
    }

    const int32 WidgetIndex = WidgetStack.IndexOfByKey(Widget);
    if (WidgetIndex == INDEX_NONE)
    {
        return false;
    }

    UWidgetBase* HiddenPreviousWidget = HiddenPreviousWidgets[WidgetIndex];
    WidgetStack.RemoveAt(WidgetIndex);
    HiddenPreviousWidgets.RemoveAt(WidgetIndex);

    Widget->NotifyPoppedFromStack();
    Widget->RemoveFromParent();

    RestoreHiddenPreviousWidget(HiddenPreviousWidget);

    return true;
}

bool UUIWidgetStack::PopToWidget(UWidgetBase* Widget)
{
    if (!ContainsWidget(Widget))
    {
        return false;
    }

    while (GetTopWidget() != Widget)
    {
        PopWidget();
    }

    return true;
}

void UUIWidgetStack::ClearWidgets()
{
    while (!IsEmpty())
    {
        PopWidget();
    }
}

UWidgetBase* UUIWidgetStack::GetTopWidget() const
{
    return WidgetStack.IsEmpty() ? nullptr : WidgetStack.Last();
}

bool UUIWidgetStack::ContainsWidget(const UWidgetBase* Widget) const
{
    return Widget && WidgetStack.Contains(Widget);
}

void UUIWidgetStack::RestoreHiddenPreviousWidget(UWidgetBase* HiddenPreviousWidget)
{
    if (HiddenPreviousWidget && WidgetStack.Contains(HiddenPreviousWidget))
    {
        HiddenPreviousWidget->SetVisibility(ESlateVisibility::Visible);
        HiddenPreviousWidget->NotifyRestoredByStack();
    }
}
