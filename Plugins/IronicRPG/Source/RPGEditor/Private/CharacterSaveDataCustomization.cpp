// Copyright Ironic Studio. All Rights Reserved.


#include "CharacterSaveDataCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "IDetailChildrenBuilder.h"

#include "Widgets/Input/SNumericEntryBox.h"

#include "Characters/CharacterDataTypes.h"
#include "Characters/Attributes/RPGAttributeSet.h"

#include "RPGEditor.h"

void FCharacterSaveDataCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    Handler = PropertyHandle;

    TArray<UObject*> OuterObjects;
    PropertyHandle->GetOuterObjects(OuterObjects);

    HeaderRow
        .NameContent()
        [
            PropertyHandle->CreatePropertyNameWidget()
        ]
        .ValueContent()
        .MinDesiredWidth(200.0f)
        .MaxDesiredWidth(400.0f)
        [
            PropertyHandle->CreatePropertyValueWidget()
		];
}

void FCharacterSaveDataCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    uint32 NumChildren;
    PropertyHandle->GetNumChildren(NumChildren);
    for (uint32 i = 0; i < NumChildren; ++i)
    {
        TSharedPtr<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(i);
        if (ChildHandle.IsValid())
        {
            if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FCharacterSaveData, Attributes))
            {
				CostomizeAttributeSection(ChildBuilder, ChildHandle.ToSharedRef());
                continue;
			}

            ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
        }
    }
}

void FCharacterSaveDataCustomization::CostomizeAttributeSection(IDetailChildrenBuilder& ChildBuilder, TSharedRef<IPropertyHandle> ChildHandle)
{
    TArray<UClass*> Classes;
    GetDerivedClasses(URPGAttributeSet::StaticClass(), Classes);

    IDetailGroup& AttributesGroup = ChildBuilder.AddGroup(GET_MEMBER_NAME_CHECKED(FCharacterSaveData, Attributes), FText::FromString("Attributes (Default Attributes)"), true);

    for (UClass* Class : Classes)
    {
        IDetailGroup& Group = AttributesGroup.AddGroup(Class->GetFName(), FText::FromName(Class->GetFName()), true);

        for (TFieldIterator<FProperty> It(Class); It; ++It)
        {
            if (FStructProperty* StructProp = CastField<FStructProperty>(*It))
            {
                // Collect attributes marked with SaveGame meta tag
                if (StructProp->Struct == FGameplayAttributeData::StaticStruct() && StructProp->HasMetaData(ATTRIBUTE_METATAG_SaveGame))
                {
                    Group.AddWidgetRow()
                        .NameContent()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromName(StructProp->GetFName()))
						]
                        .ValueContent()
                        [
                            GenerateAttributeEntryBox(StructProp).ToSharedRef()
                        ];
                }
            }
        }
    }
}

TSharedPtr<SWidget> FCharacterSaveDataCustomization::GenerateMaxValueButton(FStructProperty* StructProp)
{
    if (!StructProp->HasMetaData(ATTRIBUTE_METATAG_AttributeClampMax))
    {
		return SNullWidget::NullWidget;
    }

    return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "NoBorder")
        .ToolTipText(FText::FromString("This attribute has a maximum clamp value"))
        .ContentPadding(FMargin(2.0f, 0.0f))
        .VAlign(VAlign_Center)
        .HAlign(HAlign_Left)
		.OnClicked_Lambda([this, StructProp]() -> FReply {
        const FString& MaxValueProp = StructProp->GetMetaData(ATTRIBUTE_METATAG_AttributeClampMax);
            if (!MaxValueProp.IsEmpty())
            {
                FName MaxAttrName(*MaxValueProp);
                TArray<void*> RawData;
                Handler->AccessRawData(RawData);
                if (RawData.Num() > 0)
                {
                    if (FCharacterSaveData* SaveData = reinterpret_cast<FCharacterSaveData*>(RawData[0]))
                    {
                        for (auto& Pair : SaveData->Attributes)
                        {
                            if (Pair.Key.GetName() == MaxAttrName.ToString())
                            {
                                FScopedTransaction Transaction(NSLOCTEXT("CharacterSaveData", "SetToMaxValue", "Set Attribute to Max Value"));
                                Handler->NotifyPreChange();
                                for (void* DataPtr : RawData)
                                {
                                    if (FCharacterSaveData* InnerSaveData = reinterpret_cast<FCharacterSaveData*>(DataPtr))
                                    {
                                        InnerSaveData->Attributes.FindOrAdd(FGameplayAttribute(StructProp)) = Pair.Value;
                                    }
                                }
                                Handler->NotifyPostChange(EPropertyChangeType::ValueSet);
                                break;
                            }
                        }
                    }
                }
            }
			return FReply::Handled();
        })
        [
            SNew(SImage)
                .Image(FSlateIcon(FRPGEditorStyleSet::Get().GetStyleSetName(), "Icons.Maximize").GetIcon())
				.ColorAndOpacity(FColor::Silver)
		];
}

TSharedPtr<SWidget> FCharacterSaveDataCustomization::GenerateAttributeEntryBox(FStructProperty* StructProp)
{
    FGameplayAttribute Attribute(StructProp);

    TArray<void*> RawData;
    Handler->AccessRawData(RawData);

    return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
            .Padding(FMargin(5.0f, 2.0f, 2.0f, 2.0f))
            .HAlign(HAlign_Left)
            .VAlign(VAlign_Center)
            [
                SNew(SNumericEntryBox<float>)
                    .MinDesiredValueWidth(50.0f)
                    .Value_Lambda([this, StructProp, Attribute]() -> TOptional<float> {
                    TArray<void*> RawData;
                    Handler->AccessRawData(RawData);
                    if (RawData.Num() > 0)
                    {
                        if (FCharacterSaveData* SaveData = reinterpret_cast<FCharacterSaveData*>(RawData[0]))
                        {
                            if (const float* Found = SaveData->Attributes.Find(Attribute))
                            {
                                return *Found;
                            }
                            else if (URPGAttributeSet* AttributeSet = Cast<URPGAttributeSet>(StructProp->GetOwnerClass()->GetDefaultObject()))
                            {
							    // Use default value from attribute set
                                float DefaultValue = Attribute.GetNumericValue(AttributeSet);
							    SaveData->Attributes.Add(Attribute, DefaultValue);

							    return DefaultValue;
                            }
                        }
                    }

                    return TOptional<float>();
                })
                    .OnValueCommitted_Lambda([this, StructProp, Attribute](float NewValue, ETextCommit::Type CommitType) {
                    float ClampedValue = NewValue;
                    const FString& MaxValueProp = StructProp->GetMetaData(ATTRIBUTE_METATAG_AttributeClampMax);
                    if (!MaxValueProp.IsEmpty())
                    {
                        FName MaxAttrName(*MaxValueProp);

                        TArray<void*> RawData;
                        Handler->AccessRawData(RawData);
                        if (RawData.Num() > 0)
                        {
                            if (FCharacterSaveData* SaveData = reinterpret_cast<FCharacterSaveData*>(RawData[0]))
                            {
                                for (auto& Pair : SaveData->Attributes)
                                {
                                    if (Pair.Key.GetName() == MaxAttrName.ToString())
                                    {
                                        ClampedValue = FMath::Min(NewValue, Pair.Value);
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    FScopedTransaction Transaction(NSLOCTEXT("CharacterSaveData", "EditAttribute", "Edit Attribute Value"));
                    Handler->NotifyPreChange();

                    TArray<void*> RawData;
                    Handler->AccessRawData(RawData);
                    for (void* DataPtr : RawData)
                    {
                        if (FCharacterSaveData* SaveData = reinterpret_cast<FCharacterSaveData*>(DataPtr))
                        {
                            SaveData->Attributes.FindOrAdd(Attribute) = ClampedValue;
                        }
                    }

                    Handler->NotifyPostChange(EPropertyChangeType::ValueSet);
                })
            ]

	    + SHorizontalBox::Slot()
            .Padding(FMargin(5.0f, 2.0f, 2.0f, 2.0f))
            .VAlign(VAlign_Center)
            .HAlign(HAlign_Left)
            [
                GenerateMaxValueButton(StructProp).ToSharedRef()
			];
}
