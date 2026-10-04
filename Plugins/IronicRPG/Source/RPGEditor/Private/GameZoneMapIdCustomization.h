// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

template <typename OptionType>
class SComboBox;
class STextBlock;
class UGameZoneAsset;

struct FGameZoneMapIdOption
{
    FName Value = NAME_None;
    FText DisplayName;
};

/** Renders Map ID definitions inline and Zone-scoped references as combo boxes. */
class FGameZoneMapIdCustomization : public IPropertyTypeCustomization
{
public:
    using FOptionCollector = TFunction<void(const UGameZoneAsset&, TArray<FGameZoneMapIdOption>&)>;

protected:
    FGameZoneMapIdCustomization(FName InReferenceMetaName, FOptionCollector InOptionCollector);

public:
    virtual void CustomizeHeader(
        TSharedRef<IPropertyHandle> PropertyHandle,
        FDetailWidgetRow& HeaderRow,
        IPropertyTypeCustomizationUtils& CustomizationUtils) override;

    virtual void CustomizeChildren(
        TSharedRef<IPropertyHandle> PropertyHandle,
        IDetailChildrenBuilder& ChildBuilder,
        IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
    void RefreshOptions();
    void RefreshComboBox(FName CurrentValue);
    UGameZoneAsset* ResolveZoneAsset() const;
    TSharedPtr<FGameZoneMapIdOption> FindOption(FName Value) const;
    TSharedRef<SWidget> MakeOptionWidget(TSharedPtr<FGameZoneMapIdOption> Option) const;
    FText GetOptionText(TSharedPtr<FGameZoneMapIdOption> Option) const;

private:
    FName ReferenceMetaName;
    FOptionCollector OptionCollector;
    TSharedPtr<IPropertyHandle> Handler;
    TSharedPtr<IPropertyHandle> ValueHandle;
    TArray<TSharedPtr<FGameZoneMapIdOption>> Options;
    TSharedPtr<SComboBox<TSharedPtr<FGameZoneMapIdOption>>> ComboBox;
    TSharedPtr<STextBlock> SelectedText;
};
