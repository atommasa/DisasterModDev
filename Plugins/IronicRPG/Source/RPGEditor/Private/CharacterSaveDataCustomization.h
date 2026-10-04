// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

#include "DataTypes/RPGId.h"

#include "Characters/Attributes/RPGAttributeSet.h"

/**
 * 
 */
class FCharacterSaveDataCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() { return MakeShareable(new FCharacterSaveDataCustomization()); }

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	void UpdateLearnedOptions();

	TSharedPtr<IPropertyHandle> LearnedAbilitiesHandle = nullptr;

	TArray<TSharedPtr<FRPGId>> LearnedOptions;

private:
	void CostomizeEntryIdSection(IDetailChildrenBuilder& ChildBuilder, TSharedRef<IPropertyHandle> ChildHandle);

	TSharedPtr<SWidget> GenerateMaxValueButton(const FGameplayAttribute& Attribute);
	TSharedPtr<SWidget> GenerateAttributeEntryBox(const FGameplayAttribute& Attribute);

	TSharedPtr<IPropertyHandle> Handler = nullptr;
};
