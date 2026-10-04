// Copyright Epic Games, Inc. All Rights Reserved.


#include "RPGEditor.h"
#include "AssetToolsModule.h"
#include "IPropertyChangeListener.h"
#include "PropertyEditorModule.h"
#include "RPGIdPropCustomization.h"
#include "CharacterSaveDataCustomization.h"
#include "GameZoneEntryIdCustomization.h"
#include "GameZoneMapLayerIdCustomization.h"
#include "GameZoneMapSheetIdCustomization.h"
#include "Assets/CharacterAssetCustomization.h"
#include "GameZoneBakeCoordinator.h"
#include "GameZoneBindingCoordinator.h"
#include "GameZonePIEAuthorizer.h"
#include "RPGReleaseSealService.h"
#include "RPGReleaseSealUI.h"
#include "RPGIdPendingReservationCoordinator.h"
#include "RPGIdReferenceMigrationHandoff.h"
#include "RPGIdBlueprintReferenceIndex.h"
#include "RPGIdReferenceIndexUI.h"
#include "RPGIdCategoryUI.h"

#define LOCTEXT_NAMESPACE "FRPGEditorModule"

void FRPGEditorModule::StartupModule()
{
	FGameZoneBindingCoordinator::Register();
	FGameZoneBakeCoordinator::Register();
	FGameZonePIEAuthorizer::Register();
	FRPGReleaseSealService::Register();
	FRPGIdPendingReservationCoordinator::Register();
	FRPGIdReferenceMigrationHandoff::Register();
	FRPGReleaseSealUI::Register();
	FRPGIdBlueprintReferenceIndex::Register();
	FRPGIdReferenceIndexUI::Register();
	FRPGIdCategoryUI::Register();

	FRPGEditorStyleSet& RPGEditorStyleSet = FRPGEditorStyleSet::Get();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.RegisterCustomPropertyTypeLayout(
		"RPGId",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FRPGIdCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(
		"CharacterSaveData",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCharacterSaveDataCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(
		"GameZoneEntryId",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameZoneEntryIdCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(
		"GameZoneMapLayerId",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameZoneMapLayerIdCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(
		"GameZoneMapSheetId",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameZoneMapSheetIdCustomization::MakeInstance));

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
			PropertyModule.RegisterCustomClassLayout(
				"CharacterAsset",
				FOnGetDetailCustomizationInstance::CreateStatic(&FCharacterAssetCustomization::MakeInstance));

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
	FRPGIdCategoryUI::Unregister();
	FRPGIdReferenceIndexUI::Unregister();
	FRPGIdBlueprintReferenceIndex::Unregister();
	FRPGReleaseSealUI::Unregister();
	FRPGIdReferenceMigrationHandoff::Unregister();
	FRPGIdPendingReservationCoordinator::Unregister();
	FRPGReleaseSealService::Unregister();
	FGameZonePIEAuthorizer::Unregister();
	FGameZoneBakeCoordinator::Unregister();
	FGameZoneBindingCoordinator::Unregister();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.UnregisterCustomClassLayout("CharacterAsset");
	PropertyModule.UnregisterCustomPropertyTypeLayout("GameZoneMapLayerId");
	PropertyModule.UnregisterCustomPropertyTypeLayout("GameZoneMapSheetId");

	if (RPGIdPinFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinFactory(RPGIdPinFactory);
		RPGIdPinFactory.Reset();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRPGEditorModule, RPGEditor)
