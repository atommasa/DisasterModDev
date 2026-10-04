// Copyright Epic Games, Inc. All Rights Reserved.

#include "RPGCore.h"

#if WITH_EDITOR
#include "Levels/RPGWorldSettings.h"
#include "Assets/RPGAssetManager.h"

#include "Engine.h"

#include "Developer/Settings/Public/ISettingsModule.h"

#include "Settings/CharacterSystemSettings.h"
#include "Settings/GameZoneSystemSettings.h"
#include "Settings/SaveSystemSettings.h"
#endif // WITH_EDITOR

#define LOCTEXT_NAMESPACE "FRPGCoreModule"

void FRPGCoreModule::StartupModule()
{
#if WITH_EDITOR
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FRPGCoreModule::OnPostEngineInit);
	
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings(
			TEXT("Project"),
			TEXT("Ironic"),
			TEXT("CharacterSystemSettings"),
			FText::FromString(TEXT("Character System Settings")),
			FText::FromString(TEXT("Settings for character system.")),
			GetMutableDefault<UCharacterSystemSettings>()
		);

		SettingsModule->RegisterSettings(
			TEXT("Project"),
			TEXT("Ironic"),
			TEXT("GameZoneSystemSettings"),
			FText::FromString(TEXT("Game Zone System Settings")),
			FText::FromString(TEXT("Settings for game zone system.")),
			GetMutableDefault<UGameZoneSystemSettings>()
		);

		SettingsModule->RegisterSettings(
			TEXT("Project"),
			TEXT("Ironic"),
			TEXT("SaveSystemSettings"),
			FText::FromString(TEXT("Save System Settings")),
			FText::FromString(TEXT("Settings for save system.")),
			GetMutableDefault<USaveSystemSettings>()
		);
	}
#endif // WITH_EDITOR
}

void FRPGCoreModule::ShutdownModule()
{
#if WITH_EDITOR
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void FRPGCoreModule::OnPostEngineInit()
{
	if (!ensureAlwaysMsgf(GEngine != nullptr, TEXT("GEngine is null during FRPGCoreModule::OnPostEngineInit.")))
	{
		return;
	}

	// Validate WorldSettings class
	const UClass* WorldSettingsClass = GEngine->WorldSettingsClass;

	ensureAlwaysMsgf(
		WorldSettingsClass &&
		WorldSettingsClass->IsChildOf(ARPGWorldSettings::StaticClass()),
		TEXT(
			"Invalid WorldSettingsClass.\n"
			"Configured path: %s\n"
			"Resolved class: %s\n"
			"Expected class: %s\n"
			"GameZoneSubsystem might be unable to resolve the RPGId of a level."
		),
		*GEngine->WorldSettingsClassName.ToString(),
		*GetNameSafe(WorldSettingsClass),
		*ARPGWorldSettings::StaticClass()->GetPathName()
	);

	// Validate the actual AssetManager instance, not only its configured class path
	const UAssetManager* AssetManager = GEngine->AssetManager;

	ensureAlwaysMsgf(
		AssetManager &&
		AssetManager->IsA<URPGAssetManager>(),
		TEXT(
			"Invalid AssetManager instance.\n"
			"Configured path: %s\n"
			"Actual class: %s\n"
			"Expected class: %s"
		),
		*GEngine->AssetManagerClassName.ToString(),
		AssetManager
		? *AssetManager->GetClass()->GetPathName()
		: TEXT("None"),
		*URPGAssetManager::StaticClass()->GetPathName()
	);
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRPGCoreModule, RPGCore)