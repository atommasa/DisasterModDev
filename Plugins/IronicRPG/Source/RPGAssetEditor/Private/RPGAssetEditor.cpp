// Copyright Epic Games, Inc. All Rights Reserved.


#include "RPGAssetEditor.h"
#include "RPGAssetEditorSave.h"

#include "ContentBrowserModule.h"
#include "ContentBrowser/RPGAssetWizard.h"

#include "ContentBrowser/SRPGAssetContentBrowser.h"

#define LOCTEXT_NAMESPACE "FRPGAssetEditorModule"

void FRPGAssetEditorModule::StartupModule()
{
	FRPGAssetEditorStyleSet& RPGAssetEditorStyleSet = FRPGAssetEditorStyleSet::Get();

	_EditorSave = NewObject<URPGAssetEditorSave>(GetTransientPackage(), "RPGAssetEditorSave", RF_ClassDefaultObject);

	auto& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	auto& ACMExtenders = ContentBrowserModule.GetAllAssetContextMenuExtenders();
	ACMExtenders.Add(FContentBrowserMenuExtender_SelectedPaths::CreateRaw(this, &FRPGAssetEditorModule::ExtendAdvancedAssetMenu));

	RegisterTabSpawner();
}

void FRPGAssetEditorModule::ShutdownModule()
{
	UnregisterTabSpawner();

	// Unregister style
	if (_StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*_StyleSet);

		ensure(_StyleSet.IsUnique());
		_StyleSet.Reset();
	}
}

TSharedRef<FExtender> FRPGAssetEditorModule::ExtendAdvancedAssetMenu(const TArray<FString>& Paths)
{
	TSharedRef<FExtender> Extender = MakeShareable(new FExtender());

	Extender->AddMenuExtension(
		"ContentBrowserNewAdvancedAsset",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateRaw(this, &FRPGAssetEditorModule::RegisterMenus)
	);

	return Extender;
}

void FRPGAssetEditorModule::RegisterMenus(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		FText::FromString("RPG Asset"),
		FText::FromString("Create a new RPG Asset"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FRPGAssetEditorModule::OpenRPGAssetWizard))
	);
}

void FRPGAssetEditorModule::OpenRPGAssetWizard()
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString("RPG Asset Wizard"))
		.ClientSize(FVector2D(400, 300));

	Window->SetContent(
		SNew(SRPGAssetWizard)
		.OnAssetCreated(FOnRPGAssetCreated::CreateLambda([WeakWindow = TWeakPtr<SWindow>(Window)](const URPGPrimaryAsset*, FString, FString, FString)
			{
				if (WeakWindow.IsValid())
				{
					WeakWindow.Pin()->RequestDestroyWindow();
				}
			}))
	);

	FSlateApplication::Get().AddWindow(Window);
}

void FRPGAssetEditorModule::RegisterTabSpawner()
{
	if (!FGlobalTabmanager::Get()->HasTabSpawner(TEXT("RPGContentBrowser")))
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			TEXT("RPGContentBrowser"),
			FOnSpawnTab::CreateRaw(this, &FRPGAssetEditorModule::SpawnRPGContentBrowserTab))
			.SetDisplayName(NSLOCTEXT("RPGContentBrowser", "TabTitle", "RPG Content Browser"))
			.SetIcon(FSlateIcon())
			.SetAutoGenerateMenuEntry(false);
	}

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");

	FToolMenuSection& Section = Menu->AddSection("RPGTools", FText::FromString("RPG Tools"));
	Section.AddMenuEntry(
		"OpenRPGContentBrowser",
		FText::FromString("RPG Content Browser"),
		FText::FromString("Open the RPG-specific content browser"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(FTabId("RPGContentBrowser"));
			}))
	);
}

void FRPGAssetEditorModule::UnregisterTabSpawner()
{
	FGlobalTabmanager::Get()->UnregisterTabSpawner(TEXT("RPGContentBrowser"));
}

TSharedRef<class SDockTab> FRPGAssetEditorModule::SpawnRPGContentBrowserTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SRPGAssetContentBrowser)
		];
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRPGAssetEditorModule, RPGAssetEditor)