// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class SRPGSourceTreeArea : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SRPGSourceTreeArea) {}
        SLATE_ATTRIBUTE(FText, Label)
		SLATE_ATTRIBUTE(FText, EmptyBodyLabel)
        SLATE_ATTRIBUTE(bool, IsEmpty)
        SLATE_NAMED_SLOT(FArguments, HeaderContent)
		SLATE_EVENT(FOnBooleanValueChanged, OnExpansionChanged)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TSharedRef<IScrollableWidget> InBody);

private:
    TSharedPtr<IScrollableWidget> BodyScrollableWidget;

	TSharedPtr<SExpandableArea> ExpandableArea;

private:
    // Called when the area is expanded or collapsed
    FOnBooleanValueChanged OnExpansionChanged;

    // Callback for when the area expansion changes
    void OnAreaExpansionChanged(bool bInIsExpanded);
};
