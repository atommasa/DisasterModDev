// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"

class FRPGAssetEditorStyleSet final : public FSlateStyleSet
{
public:

	FRPGAssetEditorStyleSet()
		: FSlateStyleSet("RPGAssetEditor")
	{
		SetParentStyleName(FAppStyle::GetAppStyleSetName());

		SetContentRoot(FPaths::ProjectPluginsDir() / TEXT("IronicRPG/Resources"));
		SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

		static const FVector2D Icon16(16.0f, 16.0f);
		static const FVector2D Icon64(64.0f, 64.0f);

		Set("AssetType.Character", new IMAGE_BRUSH("Icons/RPGCharacterIcon", Icon16));

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	~FRPGAssetEditorStyleSet()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}

	static FRPGAssetEditorStyleSet& Get()
	{
		static FRPGAssetEditorStyleSet Inst;
		return Inst;
	}
};

class FRPGAssetEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private: // Tool Menu Registration
	TSharedRef<FExtender> ExtendAdvancedAssetMenu(const TArray<FString>& Paths);
	void RegisterMenus(FMenuBuilder& MenuBuilder);

private: // Asset Creation Wizard
	void OpenRPGAssetWizard();

private:
	void RegisterTabSpawner();
	void UnregisterTabSpawner();

	TSharedRef<class SDockTab> SpawnRPGContentBrowserTab(const class FSpawnTabArgs& SpawnTabArgs);

public:
	class URPGAssetEditorSave* GetEditorSave() const { return _EditorSave; }

private:
	TSharedPtr<FSlateStyleSet> _StyleSet = nullptr;

	class URPGAssetEditorSave* _EditorSave = nullptr;
};
