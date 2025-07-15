// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RPGIdPropCustomization.h"
#include "RPGCoreMinimal.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "RPGIdCustomization"

void FRPGIdCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TSharedPtr<IPropertyHandle> IdHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRPGId, Id));
	if (!IdHandle.IsValid() || !IdHandle->IsValidHandle())
	{
		return;
	}

	Handler = PropertyHandle;

	IdHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([this]()
		{
			if (DisplayNameTextBlock.IsValid())
			{
				DisplayNameTextBlock->SetText(GetDisplayNameText());
			}
		}));

	HeaderRow
		.NameContent()
		[
			IdHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
		.MaxDesiredWidth(400.0f)
		[
			IdHandle->CreatePropertyValueWidget()
		]
		.ExtensionContent()
		.MinDesiredWidth(200.0f)
		.MaxDesiredWidth(400.0f)
		[
			CreateDisplayNameWidget()
		];
}

void FRPGIdCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

TSharedRef<SWidget> FRPGIdCustomization::CreateDisplayNameWidget()
{
	return SAssignNew(DisplayNameTextBlock, STextBlock)
		.Text(GetDisplayNameText());
}

FText FRPGIdCustomization::GetDisplayNameText()
{
	void* ValuePtr = nullptr;
	if (!Handler->GetValueData(ValuePtr) || !ValuePtr)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get value pointer"));
		return FText::FromString(TEXT("Invalid"));
	}

	const FRPGId* RPGIdPtr = static_cast<const FRPGId*>(ValuePtr);
	if (!RPGIdPtr)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to cast to FRPGId"));
		return FText::FromString(TEXT("Invalid"));
	}

	const FRPGId& RPGId = *RPGIdPtr;

	auto* CharacterAsset = GET_PRIMARY_ASSET_BY_RPGID(FName(RPGId.GetIdTypeString()), RPGId, UCharacterPrimaryAsset);
	
	return CharacterAsset ? CharacterAsset->GetDisplayName() : FText::FromString(TEXT("Invalid"));
}

#undef LOCTEXT_NAMESPACE