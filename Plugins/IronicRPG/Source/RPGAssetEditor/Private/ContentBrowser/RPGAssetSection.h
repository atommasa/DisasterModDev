// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Assets/RPGAssetManager.h"
#include "RPGAssetSection.generated.h"

DECLARE_DELEGATE_OneParam(FOnRPGAssetSectionCreated, const struct FRPGAssetSection);

USTRUCT()
struct FRPGAssetSection
{
	GENERATED_BODY()

	UPROPERTY()
	FName SectionName;

	UPROPERTY()
	FName SectionType;

	UPROPERTY()
	FLinearColor SectionColor;

	FRPGAssetSection()
		: SectionName("New Section")
		, SectionType("None")
		, SectionColor(FLinearColor::White)
	{
	}

	FRPGAssetSection(FName InName, FName InType, FLinearColor InColor)
		: SectionName(InName)
		, SectionType(InType)
		, SectionColor(InColor)
	{
	}

	bool operator==(const FRPGAssetSection& Other) const
	{
		return SectionName == Other.SectionName && SectionType == Other.SectionType;
	}
};

class SRPGAssetSectionCreator : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRPGAssetSectionCreator) {}
		SLATE_EVENT(FOnRPGAssetSectionCreated, OnRPGAssetSectionCreated)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs)
	{
		OnRPGAssetSectionCreated = InArgs._OnRPGAssetSectionCreated;

        for (const FName& Type : URPGAssetManager::Get().GetAllAssetTypes())
        {
            AssetTypeOptions.AddUnique(MakeShareable(new FString(Type.ToString())));
        }

		ChildSlot
		[
            SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .Padding(10)
                .AutoHeight()
                [
                    SNew(STextBlock).Text(FText::FromString("Enter New Section Name"))
                ]

                + SVerticalBox::Slot()
                .Padding(10)
                .AutoHeight()
                [
                    SNew(SEditableTextBox)
                        .HintText(FText::FromString("New Section Name"))
                        .OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type)
                            {
                                SectionName = NewText.ToString();
                            })
                ]

                + SVerticalBox::Slot()
                .Padding(10)
                .AutoHeight()
                [
                    SNew(STextBlock).Text(FText::FromString("Select Asset Type"))
                ]

                + SVerticalBox::Slot()
                .Padding(10)
                .AutoHeight()
                [
                    SNew(SComboBox<TSharedPtr<FString>>)
                        .OptionsSource(&AssetTypeOptions)
                        .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
                            {
                                return SNew(STextBlock).Text(FText::FromString(*Item));
                            })
                        .OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
                            {
                                SectionType = NewSelection;
                            })
                        [
                            SNew(STextBlock)
                                .Text_Lambda([this]()
                                    {
                                        return SectionType.IsValid()
                                            ? FText::FromString(*SectionType)
                                            : FText::FromString("Select Type");
                                    })
                        ]
                ]
           
                + SVerticalBox::Slot()
                    .Padding(10)
                    .AutoHeight()
                    .HAlign(HAlign_Right)
                    [
                        SNew(SColorPicker)
                            .OnColorCommitted_Lambda([this](FLinearColor NewColor)
                                {
                                    SectionColor = NewColor;
								})
                    ]

                + SVerticalBox::Slot()
                    .Padding(10)
                    .AutoHeight()
                    .HAlign(HAlign_Right)
                    [
                        SNew(SButton)
                            .Text(FText::FromString("Create"))
                            .OnClicked(this, &SRPGAssetSectionCreator::OnCreateClicked)
                    ]
		];
	}

private:
    FReply OnCreateClicked()
    {
		if (SectionName.IsEmpty() || !SectionType.IsValid())
        {
            const FText ErrorMessage = FText::FromString("Please fill all fields.");
            FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
			return FReply::Handled();
        }

        OnRPGAssetSectionCreated.ExecuteIfBound(FRPGAssetSection(FName(*SectionName), FName(**SectionType), SectionColor));

		return FReply::Handled();
    }

private:
    TArray<TSharedPtr<FString>> AssetTypeOptions;

private:
	FString SectionName;

    TSharedPtr<FString> SectionType;

	FLinearColor SectionColor;

	FOnRPGAssetSectionCreated OnRPGAssetSectionCreated;
};