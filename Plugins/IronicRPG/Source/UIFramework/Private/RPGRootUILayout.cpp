// Copyright Ironic Studio. All Rights Reserved.

#include "RPGRootUILayout.h"

#include "Components/PanelWidget.h"
#include "Components/OverlaySlot.h"
#include "Widgets/UIWidgetStack.h"

UUIWidgetStack* URPGRootUILayout::GetStackForLayer(ERPGUILayer Layer) const
{
    switch (Layer)
    {
    case ERPGUILayer::Menu:
        return MenuStack;
    case ERPGUILayer::Modal:
        return ModalStack;
    case ERPGUILayer::System:
        return SystemStack;
    default:
        return nullptr;
    }
}

bool URPGRootUILayout::AttachGameplayHUD(UUserWidget* Widget)
{
    if (!HUDLayer || !Widget)
    {
        return false;
    }

    if (HUDLayer->GetChildrenCount() > 0)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("AttachGameplayHUD failed: HUDLayer already owns its single HUD widget."));
        return false;
    }

    if (Widget->GetParent())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("AttachGameplayHUD failed: %s already has a parent."),
            *Widget->GetName()
        );

        return false;
    }

    UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(HUDLayer->AddChild(Widget));

    if (!OverlaySlot)
    {
        HUDLayer->RemoveChild(Widget);
        UE_LOG(
            LogTemp,
            Error,
            TEXT("AttachGameplayHUD failed: HUDLayer must be an Overlay."));
        return false;
    }

    OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
    OverlaySlot->SetVerticalAlignment(VAlign_Fill);

    Widget->SetVisibility(ESlateVisibility::Visible);

    return true;
}

bool URPGRootUILayout::DetachGameplayHUD(UUserWidget* Widget)
{
    if (!Widget || !HUDLayer || Widget->GetParent() != HUDLayer)
    {
        return false;
    }

    return HUDLayer->RemoveChild(Widget);
}
