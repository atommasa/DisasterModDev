// Copyright Ironic Studio. All Rights Reserved.

#include "UI/RPGMapMarkerWidget.h"

#include "Input/Reply.h"
#include "InputCoreTypes.h"

void URPGMapMarkerWidget::ApplyMarkerView(const FWorldMapMarkerView& View)
{
    MarkerView = View;
    SetVisibility(ESlateVisibility::Visible);
    OnMarkerViewChanged(MarkerView);
}

void URPGMapMarkerWidget::ReleaseMarker()
{
    MarkerView = {};
    SetVisibility(ESlateVisibility::Collapsed);
    OnMarkerReleased();
}

void URPGMapMarkerWidget::RequestMarkerActivation()
{
    if (MarkerView.Point.Data.PointId.IsValid())
    {
        ActivatedDelegate.Broadcast(MarkerView.Point.Data.PointId);
    }
}

void URPGMapMarkerWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
    if (MarkerView.Point.Data.PointId.IsValid())
    {
        HoveredDelegate.Broadcast(MarkerView.Point.Data.PointId);
    }
}

void URPGMapMarkerWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
    if (MarkerView.Point.Data.PointId.IsValid())
    {
        UnhoveredDelegate.Broadcast(MarkerView.Point.Data.PointId);
    }
    Super::NativeOnMouseLeave(InMouseEvent);
}

FReply URPGMapMarkerWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // The owning map centrally arbitrates click-versus-drag and overlapping markers.
        return FReply::Unhandled();
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
