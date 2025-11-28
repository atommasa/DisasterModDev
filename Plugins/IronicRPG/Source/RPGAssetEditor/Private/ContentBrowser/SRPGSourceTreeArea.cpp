// Copyright Ironic Studio. All Rights Reserved.


#include "ContentBrowser/SRPGSourceTreeArea.h"

#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Text/SRichTextBlock.h"

void SRPGSourceTreeArea::Construct(const FArguments& InArgs, TSharedRef<IScrollableWidget> InBody)
{
	BodyScrollableWidget = InBody;

	OnExpansionChanged = InArgs._OnExpansionChanged;

	constexpr float MinBodyHeight = 88.0f;
	constexpr float EmptyBodyLabelPadding = 8.0f;

	TAttribute<int32> BodyContentIndexAttribute;
	if (InArgs._IsEmpty.IsSet() || InArgs._IsEmpty.IsBound())
	{
		BodyContentIndexAttribute = TAttribute<int32>::Create(
			TAttribute<int32>::FGetter::CreateSPLambda(
				this,
				[IsEmptyAttribute = InArgs._IsEmpty]()
				{
					return IsEmptyAttribute.Get() ? 1 : 0;
				}));
	}
	else
	{
		BodyContentIndexAttribute.Set(0);
	}

	ChildSlot
	[
		SAssignNew(ExpandableArea, SExpandableArea)
			.HeaderPadding(5.0f)
			.Visibility(InArgs._Visibility)
			.Padding(5.0f)
			.AllowAnimatedTransition(true)
			.OnAreaExpansionChanged(this, &SRPGSourceTreeArea::OnAreaExpansionChanged)
			.HeaderContent()
			[
				SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Visibility(EVisibility::HitTestInvisible)
								.Text(InArgs._Label)
								.Justification(ETextJustify::Center)
								.TextStyle(FAppStyle::Get(), "ButtonText")
								.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
						]

					+ SHorizontalBox::Slot()
						.VAlign(VAlign_Fill)
						.HAlign(HAlign_Right)
						.AutoWidth()
						.Padding(0.0f)
						[
							InArgs._HeaderContent.Widget
						]

					/*+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Right)
						.AutoWidth()
						.Padding(FMargin(HorizontalPadding, 0.0f, SearchButtonRightPadding, 0.0f))
						[
							SearchButtonWidget.ToSharedRef()
						]*/
			]
			.BodyContent()
			[
				SNew(SWidgetSwitcher)
					.WidgetIndex(BodyContentIndexAttribute)

					+ SWidgetSwitcher::Slot()
						.Padding(0)
						[
							SNew(SVerticalBox)

								// Search bar (if applicable)
								+ SVerticalBox::Slot()
									.AutoHeight()
									[
										// Should blend in visually with the header but technically acts like part of the body
										SNew(SBorder)
											.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
											/*.Padding(FMargin(HorizontalPadding, 2.0f))
											[
												Search->GetWidget()
											]*/
									]

							+ SVerticalBox::Slot()
								.Padding(FMargin(0, 1))
								[
									// Surround scrollable with a scrollbox (adds drop shadows)
									SNew(SScrollBorder, InBody)
										[
											BodyScrollableWidget->GetScrollWidget()
										]
								]
						]

					+ SWidgetSwitcher::Slot()
						.Padding(0)
						[
							SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
								.Padding(0)
								[
									SNew(SBox)
										.HAlign(HAlign_Fill)
										.VAlign(VAlign_Top)
										.HeightOverride(MinBodyHeight)
										.Padding(EmptyBodyLabelPadding)
										[
											SNew(SRichTextBlock)
												.Text(InArgs._EmptyBodyLabel)
												.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("RichTextBlock.Italic"))
												.AutoWrapText(true)
												.Justification(ETextJustify::Center)
												.DecoratorStyleSet(&FAppStyle::Get())
												+ SRichTextBlock::ImageDecorator()
										]
								]
						]
			]
	];
}

void SRPGSourceTreeArea::OnAreaExpansionChanged(bool bInIsExpanded)
{
	/*if (SearchToggleButton.IsValid() && !bInIsExpanded)
	{
		SearchToggleButton->SetExpanded(false);
	}*/

	OnExpansionChanged.ExecuteIfBound(bInIsExpanded);
}