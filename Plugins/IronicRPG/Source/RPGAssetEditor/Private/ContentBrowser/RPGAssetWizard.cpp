// Copyright Ironic Studio. All Rights Reserved.


#include "ContentBrowser/RPGAssetWizard.h"
#include "ContentBrowser/RPGPrimaryAssetFactory.h"
#include "AssetToolsModule.h"
#include "FileHelpers.h"

#include "Assets/RPGPrimaryAsset.h"
#include "Assets/RPGAssetManager.h"

void SRPGAssetWizard::Construct(const FArguments& InArgs)
{
    OnAssetCreated = InArgs._OnAssetCreated;

    // Reset state
    SelectedType.Reset();
    EnteredId.Empty();
    EnteredName.Empty();

    for (const FName& Type : URPGAssetManager::Get().GetAllAssetTypes())
    {
        AssetTypeOptions.AddUnique(MakeShareable(new FString(Type.ToString())));
    }

    ChildSlot
        [
            SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .Padding(10)
                .AutoHeight()
                [
                    SNew(STextBlock).Text(FText::FromString("Select Asset Type"))
                ]

                + SVerticalBox::Slot()
                .Padding(10)
                .AutoHeight()
                [
                    SNew(SComboBox<TSharedPtr<FString>>)
                        .OptionsSource(&AssetTypeOptions)
                        .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
                            {
                                return SNew(STextBlock).Text(FText::FromString(*Item));
                            })
                        .OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
                            {
                                SelectedType = NewSelection;
                                UpdateIdPrefix();
                            })
                        [
                            SNew(STextBlock)
                                .Text_Lambda([this]()
                                {
                                    return SelectedType.IsValid()
                                        ? FText::FromString(*SelectedType)
                                        : FText::FromString("Select Type");
                                })
                        ]
                ]
            + SVerticalBox::Slot()
                .Padding(10)
                .AutoHeight()
                [
                    SAssignNew(IdEditor, SEditableTextBox)
                        .HintText(FText::FromString("Enter ID"))
                        .OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type)
                            {
                                EnteredId = NewText.ToString();
                            })
                ]
            + SVerticalBox::Slot()
                .Padding(10)
                .AutoHeight()
                [
                    SNew(SEditableTextBox)
                        .HintText(FText::FromString("Enter Name"))
                        .OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type)
                            {
                                EnteredName = NewText.ToString();
                            })
                ]
            + SVerticalBox::Slot()
                .Padding(10)
                .AutoHeight()
                .HAlign(HAlign_Right)
                [
                    SNew(SButton)
                        .Text(FText::FromString("Create"))
                        .OnClicked(this, &SRPGAssetWizard::OnCreateClicked)
                ]
        ];
}

void SRPGAssetWizard::UpdateIdPrefix()
{
    if (!IdEditor.IsValid())
    {
        return;
    }

    FName Prefix = URPGAssetManager::Get().GetIdPrefix(**SelectedType);

    IdEditor->SetText(FText::FromName(Prefix));
}

FReply SRPGAssetWizard::OnCreateClicked()
{
    if (!SelectedType.IsValid() || EnteredId.IsEmpty() || EnteredName.IsEmpty())
    {
		const FText ErrorMessage = FText::FromString("Please fill all fields.");
        FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);

        return FReply::Handled();
    }

    EnteredName.RemoveFromStart(TEXT("PDA_"));

    FString FolderPath = FString::Printf(TEXT("/Game/DataAssets/%s"), **SelectedType);
    FString AssetName = FString::Printf(TEXT("PDA_%s"), *EnteredName);

    UFactory* Factory = NewObject<URPGPrimaryAssetFactory>();
    FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    
    UClass* AssetClass = URPGAssetManager::Get().GetAssetTypeClass(FName(*SelectedType));
    UObject* NewAsset = AssetTools.Get().CreateAsset(AssetName, FPackageName::GetLongPackagePath(FolderPath / AssetName), AssetClass, Factory);

    auto* NewRPGAsset = Cast<URPGPrimaryAsset>(NewAsset);
    if (!NewRPGAsset)
    {
        const FText ErrorMessage = FText::FromString("Failed to create asset. Please check the logs for more details.");
        FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
        return FReply::Handled();
	}

    NewRPGAsset->GetId().Id = FName(EnteredId);
    NewRPGAsset->GetDisplayName() = FText::FromString(EnteredName);

    UPackage* Package = NewRPGAsset->GetPackage();
	Package->MarkPackageDirty();

    UEditorLoadingAndSavingUtils::SavePackages({ Package }, true);

	// Execute the outer logic to create the asset
    if (OnAssetCreated.IsBound())
    {
        OnAssetCreated.Execute(NewRPGAsset, *SelectedType, EnteredId, EnteredName);
    }

    return FReply::Handled();
}