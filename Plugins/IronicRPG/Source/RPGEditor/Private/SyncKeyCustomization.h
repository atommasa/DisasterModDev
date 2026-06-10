// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"

class USynchronousTableMaster;

/**
 * 
 */
class SSyncKeyComboBox : public SCompoundWidget
{
public:
    DECLARE_DELEGATE_OneParam(FOnSyncKeyChanged, FString);

    SLATE_BEGIN_ARGS(SSyncKeyComboBox)
        : _CurrentValue()
        , _AllowNoneOption(true)
        {
        }
        SLATE_ATTRIBUTE(FString, CurrentValue)

        SLATE_ARGUMENT(bool, AllowNoneOption)

        SLATE_EVENT(FOnSyncKeyChanged, OnSelectionChanged)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    void RebuildOptions();

    TSharedRef<ITableRow> GenerateRowWidget(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable);

    void HandleSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

    FText GetCurrentText() const;

    void HandleFilterTextChanged(const FText& InFilterText);

private:
    TAttribute<FString> CurrentValueAttr;

    FOnSyncKeyChanged OnSelectionChanged;

    bool bAllowNoneOption = true;

    TArray<TSharedPtr<FString>> AllOptions;

    TArray<TSharedPtr<FString>> FilteredOptions;

    TSharedPtr<SSearchBox> SearchBox;

    TSharedPtr<SComboButton> ComboButton;

    TSharedPtr<SListView<TSharedPtr<FString>>> ListView;

};

class FSyncKeyCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() { return MakeShareable(new FSyncKeyCustomization()); }

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
    TSharedPtr<IPropertyHandle> Handler = nullptr;

};
