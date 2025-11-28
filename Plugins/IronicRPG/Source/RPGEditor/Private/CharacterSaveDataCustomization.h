// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

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
	void CostomizeAttributeSection(IDetailChildrenBuilder& ChildBuilder, TSharedRef<IPropertyHandle> ChildHandle);

	TSharedPtr<SWidget> GenerateMaxValueButton(FStructProperty* StructProp);
	TSharedPtr<SWidget> GenerateAttributeEntryBox(FStructProperty* StructProp);

private:
	TSharedPtr<IPropertyHandle> Handler = nullptr;
};
