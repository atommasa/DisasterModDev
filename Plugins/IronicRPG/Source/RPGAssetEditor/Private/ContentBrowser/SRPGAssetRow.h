// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "RPGAssetEditor.h"

/**
 * 
 */
class SRPGAssetRow : public SMultiColumnTableRow<TSharedPtr<FRPGAssetItem>>
{
public:
    SLATE_BEGIN_ARGS(SRPGAssetRow) {}
        SLATE_ARGUMENT(TSharedPtr<FRPGAssetItem>, Item)
        SLATE_ATTRIBUTE(FLinearColor, RowColor)
    SLATE_END_ARGS()

    TSharedPtr<FRPGAssetItem> Item;

	FLinearColor RowColor;

    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView)
    {
        Item = InArgs._Item;
		RowColor = InArgs._RowColor.Get();

		SMultiColumnTableRow<TSharedPtr<FRPGAssetItem>>::Construct(
            typename SMultiColumnTableRow<TSharedPtr<FRPGAssetItem>>::FArguments()
            .Style(FCoreStyle::Get(), "TableView.AlternatingRow")
            , OwnerTableView
        );
    }

    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
    {
        if (ColumnName == "Name")
        {
            return SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(FMargin(15.0f, 1.0f, 2.0f, 1.0f))
                    [
                        SNew(SImage)
                            .Image_Lambda([this]()
                                {
                                    if (!Item.IsValid())
                                    {
                                        return FCoreStyle::Get().GetBrush("DefaultBrush");
									}

                                    return FSlateIcon(FRPGAssetEditorStyleSet::Get().GetStyleSetName(), *Item->GetIconStyleName()).GetIcon();
                                })
                            .ColorAndOpacity(RowColor)
                    ]

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(3)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromName(Item->GetName()))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                    ];
        }
        else if (ColumnName == "Id")
        {
            return SNew(SBox)
                    .Padding(3)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromName(Item->GetId()))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                    ];
        }
        else if (ColumnName == "Type")
        {
            return SNew(SBox)
                .Padding(3)
                [
                    SNew(STextBlock)
                        .Text(FText::FromName(Item->GetType()))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                ]; 
        }

        return SNullWidget::NullWidget;
    }
};