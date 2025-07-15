// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/RichTextBlockDecorator.h"

/**
 *
 */
class FRPGTextDecorator : public ITextDecorator
{
public:
	FRPGTextDecorator(URichTextBlock* InOwner)
		: Owner(InOwner) {}

	virtual bool Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const override;

	virtual TSharedRef<ISlateRun> Create(const TSharedRef<class FTextLayout>& TextLayout, const FTextRunParseResults& RunParseResult, const FString& OriginalText, const TSharedRef< FString >& InOutModelText, const ISlateStyle* Style) override final;

protected:
	virtual TSharedPtr<SWidget> CreateDecoratorWidget(const FTextRunInfo& RunInfo, const FTextBlockStyle& DefaultTextStyle) const { return TSharedPtr<SWidget>(); }

	virtual void CreateDecoratorText(const FTextRunInfo& RunInfo, FTextBlockStyle& InOutTextStyle, FString& InOutString) const {}

protected:
	URichTextBlock* Owner = nullptr;

	UDataTable* StyleTable = nullptr;

};

/**
 *
 */
class FRPGTextStyleDecorator : public FRPGTextDecorator
{
public:
	UIFRAMEWORK_API FRPGTextStyleDecorator(URichTextBlock* InOwner)
		: FRPGTextDecorator(InOwner) {}

	UIFRAMEWORK_API FRPGTextStyleDecorator()
		: FRPGTextDecorator(nullptr) {}

	UIFRAMEWORK_API virtual bool Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const override;

	UIFRAMEWORK_API virtual void CreateDecoratorText(const FTextRunInfo& RunInfo, FTextBlockStyle& InOutTextStyle, FString& InOutString) const override;
};
