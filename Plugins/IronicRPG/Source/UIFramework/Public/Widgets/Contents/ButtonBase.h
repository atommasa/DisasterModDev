// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Contents/InteractiveWidget.h"
#include "Components/Image.h"
#include "Delegates/Delegate.h"
#include "ButtonBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnButtonPressed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnButtonHovered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnButtonUnhovered);

/**
 * 
 */
UCLASS()
class UIFRAMEWORK_API UButtonBase : public UInteractiveWidget
{
	GENERATED_BODY()
	
public:
    virtual void NativeConstruct() override;

#if WITH_EDITORONLY_DATA
private: // Debug
    virtual void DebugHighlightHoveredWidget(bool bHovered) override
    {
        if (DebugImage)
        {
            DebugImage->SetColorAndOpacity(bHovered ? FLinearColor::Green : FLinearColor::Gray);
        }
    }

    virtual void DebugHighlightClickedWidget(bool bClicked)
    {
        if (DebugImage)
        {
            DebugImage->SetColorAndOpacity(bClicked ? FLinearColor::Red : FLinearColor::Gray);
        }
    }
#endif

};
