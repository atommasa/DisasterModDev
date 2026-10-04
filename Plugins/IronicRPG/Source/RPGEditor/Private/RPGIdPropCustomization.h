// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "DataTypes/RPGId.h"

class FRPGIdCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() { return MakeShareable(new FRPGIdCustomization()); }

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	~FRPGIdCustomization();

protected:
	void OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event);

private:
	TSharedRef<SWidget> CreatePropertyNameWidget();

	TSharedRef<SWidget> CreatePropertyEntryBox();
	void InitLimitedType();
	void UpdatePropertyEntryBox();

	FString GetObjectPath() const;
	void UpdatePropertyValue(const FAssetData& SelectedAsset);

	TWeakObjectPtr<class URPGPrimaryAsset> _Asset = nullptr;

private:
	TSharedPtr<IPropertyHandle> Handler = nullptr;
	TSharedPtr<class SRPGIdAssetPicker> PropertyEntryBox = nullptr;

	FName LimitedType = NAME_None;
};
