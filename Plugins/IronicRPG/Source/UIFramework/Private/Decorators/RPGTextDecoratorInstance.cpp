// Copyright Ironic Studio. All Rights Reserved.


#include "Decorators/RPGTextDecoratorInstance.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Components/RichTextBlock.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Text/SlateTextRun.h"

bool FRPGTextDecorator::Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const
{
	return false;
}

TSharedRef<ISlateRun> FRPGTextDecorator::Create(const TSharedRef<class FTextLayout>& TextLayout, const FTextRunParseResults& RunParseResult, const FString& OriginalText, const TSharedRef< FString >& InOutModelText, const ISlateStyle* Style)
{
	FTextRange ModelRange;
	ModelRange.BeginIndex = InOutModelText->Len();

	FTextRunInfo RunInfo(RunParseResult.Name, FText::FromString(OriginalText.Mid(RunParseResult.ContentRange.BeginIndex, RunParseResult.ContentRange.EndIndex - RunParseResult.ContentRange.BeginIndex)));
	for (const TPair<FString, FTextRange>& Pair : RunParseResult.MetaData)
	{
		RunInfo.MetaData.Add(Pair.Key, OriginalText.Mid(Pair.Value.BeginIndex, Pair.Value.EndIndex - Pair.Value.BeginIndex));
	}

	const FTextBlockStyle& TextStyle = Owner ? Owner->GetCurrentDefaultTextStyle() : FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");

	TSharedPtr<ISlateRun> SlateRun;
	TSharedPtr<SWidget> DecoratorWidget = CreateDecoratorWidget(RunInfo, TextStyle);
	if (DecoratorWidget.IsValid())
	{
		*InOutModelText += TEXT('\u200B'); // Zero-Width Breaking Space
		ModelRange.EndIndex = InOutModelText->Len();

		// Calculate the baseline of the text within the owning rich text
		// Requested on demand as the font may not be loaded right now
		const FSlateFontInfo Font = TextStyle.Font;
		const float ShadowOffsetY = FMath::Min(0.0f, TextStyle.ShadowOffset.Y);

		TAttribute<int16> GetBaseline = TAttribute<int16>::CreateLambda([Font, ShadowOffsetY]()
			{
				const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
				return static_cast<int16>(FontMeasure->GetBaseline(Font) - ShadowOffsetY);
			});

		FSlateWidgetRun::FWidgetRunInfo WidgetRunInfo(DecoratorWidget.ToSharedRef(), GetBaseline);
		SlateRun = FSlateWidgetRun::Create(TextLayout, RunInfo, InOutModelText, WidgetRunInfo, ModelRange);
	}
	else
	{
		// Assume there's a text handler if widget is empty, if there isn't one it will just display an empty string
		FTextBlockStyle TempStyle = TextStyle;
		CreateDecoratorText(RunInfo, TempStyle, *InOutModelText);

		ModelRange.EndIndex = InOutModelText->Len();
		SlateRun = FSlateTextRun::Create(RunInfo, InOutModelText, TempStyle, ModelRange);
	}

	return SlateRun.ToSharedRef();
}

bool FRPGTextStyleDecorator::Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const
{
	return StyleTable && StyleTable->FindRow<FRichTextStyleRow>(*RunParseResult.Name, TEXT("RichText Lookup"), false) != nullptr;
}

void FRPGTextStyleDecorator::CreateDecoratorText(const FTextRunInfo& RunInfo, FTextBlockStyle& InOutTextStyle, FString& InOutString) const
{
	if (const FString* ColorString = RunInfo.MetaData.Find(TEXT("color")))
	{
		if (*ColorString == TEXT("red"))
		{
			InOutTextStyle.SetColorAndOpacity(FLinearColor::Red);
		}
		else
		{
			InOutTextStyle.ColorAndOpacity = FLinearColor::White;
		}
	}

	if (const FString* SizeString = RunInfo.MetaData.Find(TEXT("size")))
	{
		InOutTextStyle.SetFontSize(FCString::Atoi(**SizeString));
	}

	// Append inner text
	InOutString.Append(RunInfo.Content.ToString());
}

