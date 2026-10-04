// Copyright Ironic Studio. All Rights Reserved.


#include "RPGIdGraphPin.h"
#include "SRPGIdAssetPicker.h"

#include "Assets/RPGPrimaryAsset.h"
#include "Assets/RPGAssetManager.h"

#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"

#include "K2Node_CallFunction.h"
#include "K2Node_Variable.h"

#define LOCTEXT_NAMESPACE "RPGIdGraphPin"

TSharedPtr<SGraphPin> FRPGIdGraphPinFactory::CreatePin(UEdGraphPin* InPin) const
{
	if (!InPin)
	{
		return nullptr;
	}

	if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
		InPin->PinType.PinSubCategoryObject == FRPGId::StaticStruct())
	{
		return SNew(SRPGIdGraphPin, InPin);
	}

	return nullptr;
}

void SRPGIdGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);

	IdPin = InGraphPinObj;

	InitLimitedType();
	UpdatePropertyEntryBox();
}

TSharedRef<SWidget> SRPGIdGraphPin::GetDefaultValueWidget()
{
	return CreatePropertyEntryBox();
}

TSharedRef<SWidget> SRPGIdGraphPin::CreatePropertyEntryBox()
{
	SAssignNew(PropertyEntryBox, SRPGIdAssetPicker)
		.LimitedType(LimitedType)
		.ObjectPath_Raw(this, &SRPGIdGraphPin::GetObjectPath)
		.OnObjectChanged(this, &SRPGIdGraphPin::UpdatePinValue)
		.OnShouldFilterAsset_Lambda([this](const FAssetData& AssetData)
			{
				if (LimitedType != NAME_None)
				{
					const FPrimaryAssetId FoundId = AssetData.GetPrimaryAssetId();
					if (!FoundId.IsValid() || FoundId.PrimaryAssetType.GetName() != LimitedType)
					{
						return true;
					}
				}

				return false;
			});

	return SNew(SBox)
		.Visibility_Lambda([this]()
			{
				if (!IdPin)
				{
					return EVisibility::Collapsed;
				}

				return !IdPin->HasAnyConnections() ? EVisibility::Visible : EVisibility::Collapsed;
			})
		[
			PropertyEntryBox.ToSharedRef()
		];
}

FRPGId SRPGIdGraphPin::GetCurrentRPGId() const
{
	FRPGId Result;

	if (!GraphPinObj)
	{
		return Result;
	}

	const FString DefaultValue = GraphPinObj->GetDefaultAsString();

	if (!DefaultValue.IsEmpty())
	{
		FRPGId::StaticStruct()->ImportText(
			*DefaultValue,
			&Result,
			nullptr,
			PPF_None,
			nullptr,
			FRPGId::StaticStruct()->GetName()
		);
	}

	return Result;
}

void SRPGIdGraphPin::SetCurrentRPGId(const FRPGId& NewValue)
{
	if (!GraphPinObj)
	{
		return;
	}

	FString ExportedText;

	FRPGId::StaticStruct()->ExportText(
		ExportedText,
		&NewValue,
		nullptr,
		nullptr,
		PPF_None,
		nullptr
	);

	const FScopedTransaction Transaction(
		LOCTEXT("SetRPGIdPinValue", "Set RPG Id Pin Value")
	);

	GraphPinObj->Modify();

	if (const UEdGraphSchema* Schema = GraphPinObj->GetSchema())
	{
		Schema->TrySetDefaultValue(*GraphPinObj, ExportedText);
	}

	UpdatePropertyEntryBox();
}

void SRPGIdGraphPin::UpdatePinValue(const FAssetData& SelectedAsset)
{
	FRPGId NewValue = GetCurrentRPGId();

	if (SelectedAsset.IsValid())
	{
		const FPrimaryAssetId AssetId = SelectedAsset.GetPrimaryAssetId();

		if (AssetId.IsValid())
		{
			NewValue.Id = AssetId.PrimaryAssetName;
			SetCurrentRPGId(NewValue);
			return;
		}
	}

	NewValue.Id = ID_None;
	SetCurrentRPGId(NewValue);
}

void SRPGIdGraphPin::UpdatePropertyEntryBox()
{
	const FRPGId RPGId = GetCurrentRPGId();

	if (!RPGId.IsValid())
	{
		Asset.Reset();

		if (PropertyEntryBox.IsValid())
		{
			PropertyEntryBox->Invalidate(EInvalidateWidgetReason::Layout);
		}

		return;
	}

	const FName IdType = RPGId.GetIdType();

	if (IdType != LimitedType && LimitedType != NAME_None)
	{
		Asset.Reset();

		FRPGId NewValue = RPGId;
		NewValue.Id = ID_None;
		SetCurrentRPGId(NewValue);

		if (PropertyEntryBox.IsValid())
		{
			PropertyEntryBox->Invalidate(EInvalidateWidgetReason::Layout);
		}

		return;
	}

	const FPrimaryAssetId AssetId(
		FName(*RPGId.GetIdTypeString()),
		RPGId.Id
	);

	TWeakPtr<SRPGIdAssetPicker> WeakEntryBox = PropertyEntryBox;
	TWeakObjectPtr<URPGPrimaryAsset>* WeakAssetRef = &Asset;

	URPGAssetManager::Get().LoadPrimaryAsset(
		AssetId,
		{},
		FStreamableDelegate::CreateLambda([WeakEntryBox, WeakAssetRef, AssetId]()
			{
				TSharedPtr<SRPGIdAssetPicker> EntryBoxPinned = WeakEntryBox.Pin();
				if (!EntryBoxPinned.IsValid())
				{
					return;
				}

				URPGPrimaryAsset* LoadedAsset =
					Cast<URPGPrimaryAsset>(
						URPGAssetManager::Get().GetPrimaryAssetObject(AssetId)
					);

				if (LoadedAsset)
				{
					*WeakAssetRef = LoadedAsset;
				}
				else
				{
					WeakAssetRef->Reset();
				}

				EntryBoxPinned->Invalidate(EInvalidateWidgetReason::Layout);
			})
	);
}

FString SRPGIdGraphPin::GetObjectPath() const
{
	if (Asset.IsValid())
	{
		return Asset->GetPathName();
	}

	return TEXT("");
}

static FProperty* TryFindPropertyForPin(const UEdGraphPin* Pin)
{
	if (!Pin)
	{
		return nullptr;
	}

	const UEdGraphNode* Node = Pin->GetOwningNode();
	if (!Node)
	{
		return nullptr;
	}
	
	if (const UEdGraphPin* ParentPin = Pin->ParentPin)
	{
		FProperty* ParentProperty = TryFindPropertyForPin(ParentPin);
		if (ParentProperty)
		{
			if (FStructProperty* StructProperty = CastField<FStructProperty>(ParentProperty))
			{
				FString PinName = Pin->PinName.ToString();
				PinName.Split(TEXT("_"), nullptr, &PinName);
				return StructProperty->Struct->FindPropertyByName(FName(PinName));
			}
		}
	}

	if (const UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(Node))
	{
		if (UFunction* Function = CallFunctionNode->GetTargetFunction())
		{
			return Function->FindPropertyByName(Pin->PinName);
		}
	}

	if (const UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node))
	{
		return VariableNode->GetPropertyForVariable();
	}

	return nullptr;
}

void SRPGIdGraphPin::InitLimitedType()
{
	LimitedType = NAME_None;

	FProperty* Property = TryFindPropertyForPin(GraphPinObj);
	if (!Property)
	{
		return;
	}

	if (Property->HasMetaData(ID_TYPE_TAG))
	{
		const FString TypeNameStr = Property->GetMetaData(ID_TYPE_TAG);
		LimitedType = FName(*TypeNameStr);
		return;
	}
}
