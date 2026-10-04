// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"
#include "SGraphPin.h"
#include "PropertyCustomizationHelpers.h"
#include "DataTypes/RPGId.h"

class URPGPrimaryAsset;

struct FRPGIdGraphPinFactory : public FGraphPanelPinFactory
{
public:
	virtual TSharedPtr<SGraphPin> CreatePin(UEdGraphPin* InPin) const override;
};

class SRPGIdGraphPin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SRPGIdGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	virtual TSharedRef<SWidget> GetDefaultValueWidget() override;

private:
	TSharedRef<SWidget> CreatePropertyEntryBox();

	void InitLimitedType();
	void UpdatePropertyEntryBox();

	FRPGId GetCurrentRPGId() const;
	void SetCurrentRPGId(const FRPGId& NewValue);
	void UpdatePinValue(const FAssetData& SelectedAsset);

	FString GetObjectPath() const;

private:
	UEdGraphPin* IdPin;

	TSharedPtr<class SRPGIdAssetPicker> PropertyEntryBox;

	TWeakObjectPtr<URPGPrimaryAsset> Asset;

	FName LimitedType = NAME_None;
};
