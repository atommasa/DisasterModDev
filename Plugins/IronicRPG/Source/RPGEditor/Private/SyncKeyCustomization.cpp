// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "SyncKeyCustomization.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "PropertyCustomizationHelpers.h"

#include "Misc/SynchronousTableMaster.h"

#define LOCTEXT_NAMESPACE "RPGIdCustomization"

void SSyncKeyComboBox::Construct(const FArguments& InArgs)
{
    CurrentValueAttr = InArgs._CurrentValue;
    OnSelectionChanged = InArgs._OnSelectionChanged;
    bAllowNoneOption = InArgs._AllowNoneOption;

    RebuildOptions();
    FilteredOptions = AllOptions;

    ChildSlot
        [
            SAssignNew(ComboButton, SComboButton)
                .ButtonContent()
                [
                    SNew(STextBlock)
                        .Text(this, &SSyncKeyComboBox::GetCurrentText)
                ]
                .MenuContent()
                [
                    SNew(SVerticalBox)

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(4)
                        [
                            SAssignNew(SearchBox, SSearchBox)
                                .OnTextChanged(this, &SSyncKeyComboBox::HandleFilterTextChanged)
                        ]

                        // ¤º®e¦Cªí
                        + SVerticalBox::Slot()
                        .MaxHeight(300.f)
                        [
                            SAssignNew(ListView, SListView<TSharedPtr<FString>>)
                                .ListItemsSource(&FilteredOptions)
                                .OnGenerateRow(this, &SSyncKeyComboBox::GenerateRowWidget)
                                .SelectionMode(ESelectionMode::Single)
                                .OnSelectionChanged(this, &SSyncKeyComboBox::HandleSelectionChanged)
                        ]
                ]
        ];
}

void SSyncKeyComboBox::RebuildOptions()
{
    AllOptions.Reset();

    if (bAllowNoneOption)
    {
        AllOptions.Add(MakeShared<FString>(TEXT("None")));
    }

    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* Class = *It;

        if (!Class->IsChildOf(USynchronousTableMaster::StaticClass()))
        {
            continue;
        }

        if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        {
            continue;
        }

        if (USynchronousTableMaster* MasterCDO = Cast<USynchronousTableMaster>(Class->GetDefaultObject()))
        {
            for (const FSyncTableEntry& Entry : MasterCDO->MasterKeys)
            {
                FString TableName = MasterCDO->GetName();
                TableName.RemoveFromStart(TEXT("Default__"));
                AllOptions.AddUnique(MakeShared<FString>(FString::Printf(TEXT("%s.%s.%s"), *TableName, *Entry.Category.ToString(), *Entry.Key.ToString())));
            }
        }
    }

    AllOptions.Sort([](const TSharedPtr<FString>& A, const TSharedPtr<FString>& B)
        {
            return *A < *B;
        });

    if (SearchBox.IsValid() && !SearchBox->GetText().IsEmpty())
    {
        HandleFilterTextChanged(SearchBox->GetText());
    }
    else
    {
        FilteredOptions = AllOptions;
    }

    if (ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }
}

TSharedRef<ITableRow> SSyncKeyComboBox::GenerateRowWidget(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
        [
            SNew(STextBlock)
                .Text(FText::FromString(*Item))
        ];
}

void SSyncKeyComboBox::HandleSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
    if (!NewSelection.IsValid())
    {
        return;
    }

    if (OnSelectionChanged.IsBound())
    {
        OnSelectionChanged.Execute(*NewSelection);
    }

    if (ComboButton.IsValid())
    {
        ComboButton->SetIsOpen(false);
    }
}

FText SSyncKeyComboBox::GetCurrentText() const
{
    const FString Current = CurrentValueAttr.Get();
    return FText::FromString(Current);
}

void SSyncKeyComboBox::HandleFilterTextChanged(const FText& InFilterText)
{
    const FString Query = InFilterText.ToString();

    FilteredOptions.Reset();

    for (const TSharedPtr<FString>& Option : AllOptions)
    {
        if (!Option.IsValid())
        {
            continue;
        }

        if (Query.IsEmpty() || Option->Contains(Query))
        {
            FilteredOptions.Add(Option);
        }
    }

    if (ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }
}

void FSyncKeyCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    Handler = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSyncKey, KeyName));

    HeaderRow
        .NameContent()
        [
            PropertyHandle->CreatePropertyNameWidget()
        ]
        .ValueContent()
        .MinDesiredWidth(250.f)
        [
            SNew(SSyncKeyComboBox)
                .CurrentValue_Lambda([this]()
                    {
                        FString Value;
                        if (Handler.IsValid())
                        {
                            Handler->GetValue(Value);
                        }

                        return Value;
                    })
                .OnSelectionChanged(SSyncKeyComboBox::FOnSyncKeyChanged::CreateLambda([this](FString NewKey)
                    {
                        TArray<FString> Parts;
                        NewKey.ParseIntoArray(Parts, TEXT("."), true);

                        FString RowNameString = Parts.Last();

                        if (Handler.IsValid())
                        {
                            Handler->SetValue(FName(RowNameString));
                        }
                    }))
        ];
}

void FSyncKeyCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

#undef LOCTEXT_NAMESPACE