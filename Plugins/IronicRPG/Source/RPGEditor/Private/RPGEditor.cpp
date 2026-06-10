// Copyright Epic Games, Inc. All Rights Reserved.


#include "RPGEditor.h"
#include "AssetToolsModule.h"
#include "IPropertyChangeListener.h"
#include "PropertyEditorModule.h"
#include "RPGIdPropCustomization.h"
#include "SyncKeyCustomization.h"
#include "CharacterSaveDataCustomization.h"
#include "Assets/CharacterAssetCustomization.h"

#define LOCTEXT_NAMESPACE "FRPGEditorModule"

void FRPGEditorModule::StartupModule()
{
	FRPGEditorStyleSet& RPGEditorStyleSet = FRPGEditorStyleSet::Get();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.RegisterCustomPropertyTypeLayout("RPGId", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FRPGIdCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("CharacterSaveData", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCharacterSaveDataCustomization::MakeInstance));
	
	TArray<UClass*> Classes;
	GetDerivedClasses(URPGPrimaryAsset::StaticClass(), Classes, true);

	Classes.Add(URPGPrimaryAsset::StaticClass());

	for (UClass* Class : Classes)
	{
		if (!Class)
		{
			continue;
		}

		if (Class == UCharacterAsset::StaticClass())
		{
			PropertyModule.RegisterCustomClassLayout("CharacterAsset", FOnGetDetailCustomizationInstance::CreateStatic(&FCharacterAssetCustomization::MakeInstance));

			continue;
		}

		PropertyModule.RegisterCustomClassLayout(
			Class->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FRPGPrimaryAssetCustomization<URPGPrimaryAsset>::MakeInstance)
		);
	}

	RPGIdPinFactory = MakeShared<FRPGIdGraphPinFactory>();
	FEdGraphUtilities::RegisterVisualPinFactory(RPGIdPinFactory);
}

void FRPGEditorModule::ShutdownModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.UnregisterCustomClassLayout("CharacterAsset");

	if (RPGIdPinFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinFactory(RPGIdPinFactory);
		RPGIdPinFactory.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRPGEditorModule, RPGEditor)