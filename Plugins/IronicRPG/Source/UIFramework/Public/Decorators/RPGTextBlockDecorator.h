// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/RichTextBlockImageDecorator.h"
#include "RPGTextBlockDecorator.generated.h"

/**
 * 
 */
UCLASS()
class UIFRAMEWORK_API URPGTextBlockDecorator : public URichTextBlockImageDecorator
{
	GENERATED_BODY()

public:
	URPGTextBlockDecorator(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer) {}

	virtual TSharedPtr<ITextDecorator> CreateDecorator(URichTextBlock* InOwner) override;

};
