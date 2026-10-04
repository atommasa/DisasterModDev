// Copyright Ironic Studio. All Rights Reserved.

#include "GameZoneMapIdCustomization.h"

#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

#include "Assets/RPGAssetLibrary.h"
#include "Levels/GameZoneAsset.h"
#include "Levels/RPGWorldSettings.h"

FGameZoneMapIdCustomization::FGameZoneMapIdCustomization(
    const FName InReferenceMetaName,
    FOptionCollector InOptionCollector)
    : ReferenceMetaName(InReferenceMetaName)
    , OptionCollector(MoveTemp(InOptionCollector))
{
}

void FGameZoneMapIdCustomization::CustomizeHeader(
    TSharedRef<IPropertyHandle> PropertyHandle,
    FDetailWidgetRow& HeaderRow,
    IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    Handler = PropertyHandle;
    ValueHandle = PropertyHandle->GetChildHandle(TEXT("Value"));

    HeaderRow.NameContent()[PropertyHandle->CreatePropertyNameWidget()];
    if (!ValueHandle.IsValid())
    {
        HeaderRow.ValueContent()[PropertyHandle->CreatePropertyValueWidget()];
        return;
    }

    if (!PropertyHandle->HasMetaData(ReferenceMetaName))
    {
        HeaderRow.ValueContent()[ValueHandle->CreatePropertyValueWidget()];
        return;
    }

    RefreshOptions();
    FName InitialValue = NAME_None;
    ValueHandle->GetValue(InitialValue);
    const TSharedPtr<FGameZoneMapIdOption> InitiallySelectedItem = FindOption(InitialValue);

    HeaderRow
        .ValueContent()
        .MinDesiredWidth(250.0f)
        [
            SAssignNew(ComboBox, SComboBox<TSharedPtr<FGameZoneMapIdOption>>)
                .OptionsSource(&Options)
                .InitiallySelectedItem(InitiallySelectedItem)
                .OnComboBoxOpening(this, &FGameZoneMapIdCustomization::RefreshOptions)
                .OnGenerateWidget(this, &FGameZoneMapIdCustomization::MakeOptionWidget)
                .OnSelectionChanged_Lambda(
                    [this](TSharedPtr<FGameZoneMapIdOption> Selection, ESelectInfo::Type SelectInfo)
                    {
                        if (SelectInfo == ESelectInfo::Direct || !Selection.IsValid() || !ValueHandle.IsValid())
                        {
                            return;
                        }

                        ValueHandle->SetValue(Selection->Value);
                        if (SelectedText.IsValid())
                        {
                            SelectedText->SetText(GetOptionText(Selection));
                        }
                    })
            [
                SAssignNew(SelectedText, STextBlock)
                    .Text(GetOptionText(InitiallySelectedItem))
            ]
        ];

    RefreshComboBox(InitialValue);
}

void FGameZoneMapIdCustomization::CustomizeChildren(
    TSharedRef<IPropertyHandle> PropertyHandle,
    IDetailChildrenBuilder& ChildBuilder,
    IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    // The wrapped FName is always edited in the struct header.
}

void FGameZoneMapIdCustomization::RefreshOptions()
{
    FName CurrentValue = NAME_None;
    if (ValueHandle.IsValid())
    {
        ValueHandle->GetValue(CurrentValue);
    }

    TArray<FGameZoneMapIdOption> CollectedOptions;
    if (const UGameZoneAsset* ZoneAsset = ResolveZoneAsset())
    {
        OptionCollector(*ZoneAsset, CollectedOptions);
    }

    Options.Reset();
    Options.Add(MakeShared<FGameZoneMapIdOption>(
        FGameZoneMapIdOption{NAME_None, FText::FromName(NAME_None)}));
    for (FGameZoneMapIdOption& Option : CollectedOptions)
    {
        if (!Option.Value.IsNone())
        {
            Options.Add(MakeShared<FGameZoneMapIdOption>(MoveTemp(Option)));
        }
    }

    if (!CurrentValue.IsNone() && !Options.ContainsByPredicate(
        [CurrentValue](const TSharedPtr<FGameZoneMapIdOption>& Option)
        {
            return Option.IsValid() && Option->Value == CurrentValue;
        }))
    {
        Options.Add(MakeShared<FGameZoneMapIdOption>(
            FGameZoneMapIdOption{CurrentValue, FText::GetEmpty()}));
    }

    Options.Sort(
        [](const TSharedPtr<FGameZoneMapIdOption>& First, const TSharedPtr<FGameZoneMapIdOption>& Second)
        {
            if (!First.IsValid() || First->Value.IsNone())
            {
                return Second.IsValid() && !Second->Value.IsNone();
            }
            if (!Second.IsValid() || Second->Value.IsNone())
            {
                return false;
            }
            return First->Value.LexicalLess(Second->Value);
        });

    if (ComboBox.IsValid())
    {
        RefreshComboBox(CurrentValue);
    }
}

void FGameZoneMapIdCustomization::RefreshComboBox(const FName CurrentValue)
{
    const TSharedPtr<FGameZoneMapIdOption> MatchingOption = FindOption(CurrentValue);

    if (ComboBox.IsValid())
    {
        ComboBox->RefreshOptions();
        ComboBox->SetSelectedItem(MatchingOption);
    }

    if (SelectedText.IsValid())
    {
        SelectedText->SetText(
            MatchingOption.IsValid()
                ? GetOptionText(MatchingOption)
                : FText::FromName(CurrentValue));
    }
}

UGameZoneAsset* FGameZoneMapIdCustomization::ResolveZoneAsset() const
{
    if (!Handler.IsValid())
    {
        return nullptr;
    }

    TArray<UObject*> OuterObjects;
    Handler->GetOuterObjects(OuterObjects);
    if (OuterObjects.Num() != 1)
    {
        return nullptr;
    }

    if (UGameZoneAsset* ZoneAsset = Cast<UGameZoneAsset>(OuterObjects[0]))
    {
        return ZoneAsset;
    }

    const AActor* OwnerActor = Cast<AActor>(OuterObjects[0]);
    const ARPGWorldSettings* WorldSettings = OwnerActor && OwnerActor->GetWorld()
        ? Cast<ARPGWorldSettings>(OwnerActor->GetWorld()->GetWorldSettings())
        : nullptr;
    return WorldSettings
        ? Cast<UGameZoneAsset>(URPGAssetLibrary::LoadAssetByRPGId(WorldSettings->GetGameZoneId()))
        : nullptr;
}

TSharedPtr<FGameZoneMapIdOption> FGameZoneMapIdCustomization::FindOption(
    const FName Value) const
{
    const TSharedPtr<FGameZoneMapIdOption>* MatchingOption = Options.FindByPredicate(
        [Value](const TSharedPtr<FGameZoneMapIdOption>& Option)
        {
            return Option.IsValid() && Option->Value == Value;
        });
    return MatchingOption ? *MatchingOption : nullptr;
}

TSharedRef<SWidget> FGameZoneMapIdCustomization::MakeOptionWidget(
    TSharedPtr<FGameZoneMapIdOption> Option) const
{
    return SNew(STextBlock).Text(GetOptionText(Option));
}

FText FGameZoneMapIdCustomization::GetOptionText(
    TSharedPtr<FGameZoneMapIdOption> Option) const
{
    if (!Option.IsValid() || Option->Value.IsNone())
    {
        return FText::FromName(NAME_None);
    }

    FText DisplayName = Option->DisplayName;
    if (DisplayName.IsEmpty())
    {
        const TSharedPtr<FGameZoneMapIdOption>* MatchingOption = Options.FindByPredicate(
            [Option](const TSharedPtr<FGameZoneMapIdOption>& Candidate)
            {
                return Candidate.IsValid() && Candidate->Value == Option->Value;
            });
        if (MatchingOption && MatchingOption->IsValid())
        {
            DisplayName = (*MatchingOption)->DisplayName;
        }
    }

    return !DisplayName.IsEmpty()
        ? FText::Format(
            NSLOCTEXT("GameZoneMapId", "MapIdOption", "{0} [{1}]"),
            DisplayName,
            FText::FromName(Option->Value))
        : FText::FromName(Option->Value);
}
