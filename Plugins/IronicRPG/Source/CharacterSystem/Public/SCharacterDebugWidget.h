// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class CHARACTERSYSTEM_API SCharacterDebugWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCharacterDebugWidget)
	{}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
};
