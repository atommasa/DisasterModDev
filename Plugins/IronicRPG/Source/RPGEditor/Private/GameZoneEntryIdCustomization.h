// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

template <typename OptionType>
class SComboBox;
class STextBlock;
struct FRPGId;

/** Details customization for FGameZoneEntryId. */
class FGameZoneEntryIdCustomization final : public IPropertyTypeCustomization
{
public:
    static TSharedRef<IPropertyTypeCustomization> MakeInstance()
    {
        return MakeShared<FGameZoneEntryIdCustomization>();
    }

    virtual void CustomizeHeader(
        TSharedRef<IPropertyHandle> PropertyHandle,
        FDetailWidgetRow& HeaderRow,
        IPropertyTypeCustomizationUtils& CustomizationUtils) override;

    virtual void CustomizeChildren(
        TSharedRef<IPropertyHandle> PropertyHandle,
        IDetailChildrenBuilder& ChildBuilder,
        IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
    TSharedRef<SWidget> GenerateEntryGuidComboBox();
    TSharedPtr<IPropertyHandle> ResolveSourceWorldHandle() const;
    bool GetSourceZoneId(FRPGId& OutZoneId) const;

    void UpdateEntryGuidComboBox();
    void RefreshEntryGuidComboBox(const FGuid& CurrentGuid);

    TSharedPtr<FGuid> FindEntryOption(const FGuid& EntryGuid) const;
    bool GetCurrentEntryGuid(FGuid& OutEntryGuid) const;
    FText CreateEntryGuidText(const TSharedPtr<FGuid>& EntryGuid) const;

    TSharedPtr<IPropertyHandle> Handler;
    TSharedPtr<IPropertyHandle> EntryGuidHandle;
    TSharedPtr<IPropertyHandle> SourceWorldHandle;

    TArray<TSharedPtr<FGuid>> EntryGuidOptions;
    TMap<FGuid, FText> EntryNames;

    TSharedPtr<SComboBox<TSharedPtr<FGuid>>> EntryGuidComboBox;
    TSharedPtr<STextBlock> ComboBoxButtonTextBlock;
};