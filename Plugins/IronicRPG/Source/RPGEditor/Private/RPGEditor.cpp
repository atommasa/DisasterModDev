// Copyright Epic Games, Inc. All Rights Reserved.


#include "RPGEditor.h"
#include "AssetToolsModule.h"
#include "IPropertyChangeListener.h"
#include "PropertyEditorModule.h"
#include "RPGIdPropCustomization.h"
#include "CharacterSaveDataCustomization.h"
#include "Assets/CharacterAssetCustomization.h"

#define LOCTEXT_NAMESPACE "FRPGEditorModule"

void FRPGEditorModule::StartupModule()
{
	FRPGEditorStyleSet& RPGEditorStyleSet = FRPGEditorStyleSet::Get();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.RegisterCustomPropertyTypeLayout("RPGId", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FRPGIdCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("CharacterSaveData", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCharacterSaveDataCustomization::MakeInstance));
	
	PropertyModule.RegisterCustomClassLayout("CharacterAsset", FOnGetDetailCustomizationInstance::CreateStatic(&FCharacterAssetCustomization::MakeInstance));
}

void FRPGEditorModule::ShutdownModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.UnregisterCustomClassLayout("CharacterAsset");
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRPGEditorModule, RPGEditor)