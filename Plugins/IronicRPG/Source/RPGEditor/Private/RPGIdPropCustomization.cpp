// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "RPGIdPropCustomization.h"
#include "SRPGIdAssetPicker.h"
#include "SRPGIdClaimEditor.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "PropertyCustomizationHelpers.h"

#include "Assets/RPGPrimaryAsset.h"
#include "Assets/RPGAssetManager.h"

#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "RPGIdCustomization"

void FRPGIdCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	FName TypeLimitTag = NAME_None;

	TSharedPtr<IPropertyHandle> IdHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRPGId, Id));
	if (!IdHandle.IsValid() || !IdHandle->IsValidHandle())
	{
		return;
	}

	Handler = PropertyHandle;
	InitLimitedType();
	if (Handler->HasMetaData(ID_CLAIM_TAG))
	{
		TArray<UObject*> OuterObjects;
		Handler->GetOuterObjects(OuterObjects);
		TArray<TWeakObjectPtr<UObject>> Owners;
		if (Handler->GetProperty() == FindFProperty<FProperty>(URPGPrimaryAsset::StaticClass(), TEXT("Id")))
		{
			for (UObject* Outer : OuterObjects)
			{
				Owners.Add(Outer);
			}
		}
		HeaderRow.NameContent()[Handler->CreatePropertyNameWidget()]
			.ValueContent().MinDesiredWidth(420).MaxDesiredWidth(650)[SNew(SRPGIdClaimEditor).Owners(Owners)];
		// The property remains reflection-read-only; only this command widget is interactive.
		HeaderRow.IsEnabled(true).IsValueEnabled(true)
			.OverrideResetToDefault(FResetToDefaultOverride::Create(TAttribute<bool>(false)));
		return;
	}

	FCoreUObjectDelegates::OnObjectTransacted.AddRaw(this, &FRPGIdCustomization::OnObjectTransacted);
	
	HeaderRow
		.NameContent()
		[
			CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
		.MaxDesiredWidth(400.0f)
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.FillWidth(1.0f)
				[
					CreatePropertyEntryBox()
				]

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.AutoWidth()
				.Padding(FMargin(3))
				[
					SNew(STextBlock)
						.Text_Lambda([&]() -> FText
							{
								if (!Handler.IsValid())
								{
									return FText::GetEmpty();
								}

								TSharedPtr<IPropertyHandle> IdHandle = Handler->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRPGId, Id));
								if (!IdHandle.IsValid() || !IdHandle->IsValidHandle())
								{
									return FText::GetEmpty();
								}

								return Handler->HasMetaData(ID_TYPE_TAG) ?
									FText::FromString(Handler->GetMetaData(ID_TYPE_TAG)) :
									FText::GetEmpty();
							})
				]
		]
		.AddCustomContextMenuAction(
			FUIAction(
				FExecuteAction::CreateLambda([this]()
					{
						if (PropertyEntryBox.IsValid())
						{
							UpdatePropertyEntryBox();
						}
					}),
				FCanExecuteAction::CreateLambda([this]()
					{
						return PropertyEntryBox.IsValid();
					})),
			LOCTEXT("RPGIdCustomization_RefreshAsset", "Refresh"));

	UpdatePropertyEntryBox();
}

void FRPGIdCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

FRPGIdCustomization::~FRPGIdCustomization()
{
	FCoreUObjectDelegates::OnObjectTransacted.RemoveAll(this);
}

void FRPGIdCustomization::OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event)
{
	TArray<UObject*> Outers;
	Handler->GetOuterObjects(Outers);
	if (Outers.Contains(Object))
	{
		UpdatePropertyEntryBox();
	}
}

TSharedRef<SWidget> FRPGIdCustomization::CreatePropertyNameWidget()
{
	if (!Handler.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	TSharedPtr<IPropertyHandle> IdHandle = Handler->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRPGId, Id));
	if (!IdHandle.IsValid() || !IdHandle->IsValidHandle())
	{
		return SNullWidget::NullWidget;
	}

	return IdHandle->CreatePropertyNameWidget(Handler->GetPropertyDisplayName());
}

TSharedRef<SWidget> FRPGIdCustomization::CreatePropertyEntryBox()
{
	// Check if the property is valid and if it has the ID_CLAIM_TAG metadata
	if (Handler.IsValid() && Handler->HasMetaData(ID_CLAIM_TAG))
	{
		return SNullWidget::NullWidget;
	}
	
	return SAssignNew(PropertyEntryBox, SRPGIdAssetPicker)
		.LimitedType(LimitedType)
		.ObjectPath_Raw(this, &FRPGIdCustomization::GetObjectPath)
		.OnObjectChanged(this, &FRPGIdCustomization::UpdatePropertyValue)
		.OnShouldFilterAsset_Lambda([this](const FAssetData& AssetData)
			{
				if (!Handler.IsValid())
				{
					return false;
				}

				if (LimitedType != NAME_None)
				{
					const FPrimaryAssetId FoundId = AssetData.GetPrimaryAssetId();
					if (!FoundId.IsValid() || FoundId.PrimaryAssetType.GetName() != LimitedType)
					{
						return true;
					}
				}
				return false;
			})
		.IsEnabled_Lambda([this]()
			{
				return Handler->IsEditable();
			});
}

void FRPGIdCustomization::InitLimitedType()
{
	if (!Handler.IsValid())
	{
		return;
	}

	// If the ID_TYPE_TAG metadata is present, we use it to determine the type
	if (Handler->HasMetaData(ID_TYPE_TAG))
	{
		const FString TypeNameStr = Handler->GetMetaData(ID_TYPE_TAG);

		LimitedType = FName(*TypeNameStr);
	}
	// If the ID_CLAIM_TAG metadata is present, we assume it's a primary asset type
	else if (Handler->HasMetaData(ID_CLAIM_TAG))
	{
		if (const UClass* Class = Handler->GetOuterBaseClass())
		{
			if (const auto* CDOAsset = Cast<URPGPrimaryAsset>(Class->GetDefaultObject()))
			{
				LimitedType = CDOAsset->GetAssetType();
			}
		}
	}
}

void FRPGIdCustomization::UpdatePropertyEntryBox()
{
	if (!Handler.IsValid() || Handler->HasMetaData(ID_CLAIM_TAG))
	{
		return;
	}

	void* ValuePtr = nullptr;
	if (!Handler->GetValueData(ValuePtr) || !ValuePtr)
	{
		return;
	}
	
	// Check if the value is of type FRPGId
	FRPGId& RPGId = *static_cast<FRPGId*>(ValuePtr);
	FName IdType = RPGId.GetIdType();

	// If the IdType is None or does not match the LimitedType, reset the asset and show an error message
	if (IdType != LimitedType && LimitedType != NAME_None)
	{
		if (RPGId.IsValid())
		{
			FText ErrorMessage = FText::FromString(FString::Printf(TEXT("%s is not a valid id for this property!"), *RPGId.ToString()));
			FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		}
		
		_Asset.Reset();
		RPGId.Id = ID_None;
		
		if (PropertyEntryBox.IsValid())
		{
			PropertyEntryBox->Invalidate(EInvalidateWidgetReason::Layout);
		}

		return;
	}

	const FPrimaryAssetId AssetId(FName(*RPGId.GetIdTypeString()), RPGId.Id);

	TWeakPtr<SRPGIdAssetPicker> WeakEntryBox = PropertyEntryBox;
	TWeakObjectPtr<URPGPrimaryAsset>* WeakAssetRef = &_Asset;
	URPGAssetManager::Get().LoadPrimaryAsset(
		AssetId,
		{},
		FStreamableDelegate::CreateLambda([WeakEntryBox, WeakAssetRef, AssetId]()
			{
				// Check if the entry box is still valid
				TSharedPtr<SRPGIdAssetPicker> EntryBoxPinned = WeakEntryBox.Pin();
				if (!EntryBoxPinned.IsValid())
				{
					return;
				}

				// Get the loaded asset
				URPGPrimaryAsset* LoadedAsset = Cast<URPGPrimaryAsset>(URPGAssetManager::Get().GetPrimaryAssetObject(AssetId));

				// Update the weak reference
				if (LoadedAsset)
				{
					*WeakAssetRef = LoadedAsset;
				}
				else
				{
					WeakAssetRef->Reset();
				}

				// Refresh the entry box
				EntryBoxPinned->Invalidate(EInvalidateWidgetReason::Layout);
			})
	);
}

FString FRPGIdCustomization::GetObjectPath() const
{
	if (_Asset.IsValid())
	{
		return _Asset->GetPathName();
	}

	return TEXT("");
}

void FRPGIdCustomization::UpdatePropertyValue(const FAssetData& SelectedAsset)
{
	if (!Handler.IsValid())
	{
		return;
	}
	
	TSharedPtr<IPropertyHandle> IdHandle = Handler->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRPGId, Id));
	if (!IdHandle.IsValid() || !IdHandle->IsValidHandle())
	{
		return;
	}
	
	if (SelectedAsset.IsValid())
	{
		const FPrimaryAssetId AssetId = SelectedAsset.GetPrimaryAssetId();
		if (AssetId.IsValid())
		{
			// Update the FRPGId with the new asset's ID
			IdHandle->SetValue(AssetId.PrimaryAssetName);
			return;
		}
	}

	IdHandle->SetValue(ID_None);
}

#undef LOCTEXT_NAMESPACE
