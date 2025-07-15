// Fill out your copyright notice in the Description page of Project Settings.

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
