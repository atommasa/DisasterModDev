// Copyright Ironic Studio. All Rights Reserved.


#include "Assets/NarrativeAssetAppMode.h"
#include "Assets/NarrativeAssetEditorApp.h"
#include "Assets/NarrativeAssetPrimaryTabFactory.h"
#include "Assets/NarrativeAssetPropertyTabFactory.h"
#include "Assets/EventGraphTabFactory.h"

const FName FNarrativeAssetAppModes::NarrativeDefaultMode(TEXT("DefaultsName"));
const FName FNarrativeAssetAppModes::NarrativeEventGraphMode(TEXT("EventGraphName"));

NarrativeAssetAppMode::NarrativeAssetAppMode(TSharedPtr<class NarrativeAssetEditorApp> App)
	: FBlueprintEditorApplicationMode(App, FNarrativeAssetAppModes::NarrativeDefaultMode, &FNarrativeAssetAppModes::GetLocalizedMode, true, true)
{
	_App = App;

    // Set the tab layout for this mode
    BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new NarrativeAssetPrimaryTabFactory(App)));
	BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new NarrativeAssetPropertyTabFactory(App)));

    TabLayout = FTabManager::NewLayout("NarrativeAssetAppMode_Layout")
        ->AddArea(
            FTabManager::NewPrimaryArea()
            ->SetOrientation(Orient_Vertical)
            ->Split(
                FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
                ->Split(
                    FTabManager::NewStack()
                    ->SetSizeCoefficient(0.6f)
                    ->AddTab("NarrativeAssetPrimaryTab", ETabState::OpenedTab)
                )
                ->Split(
                    FTabManager::NewStack()
                    ->SetSizeCoefficient(0.4f)
                    ->AddTab("Inspector", ETabState::OpenedTab)
                )
            )
        );
}

void NarrativeAssetAppMode::RegisterTabFactories(TSharedPtr<class FTabManager> InTabManager)
{
	TSharedPtr<NarrativeAssetEditorApp> App = _App.Pin();
	App->PushTabFactories(BlueprintEditorTabFactories);
	FApplicationMode::RegisterTabFactories(InTabManager);
}

void NarrativeAssetAppMode::PreDeactivateMode()
{
	FApplicationMode::PreDeactivateMode();
}

void NarrativeAssetAppMode::PostActivateMode()
{
	FApplicationMode::PostActivateMode();
}
//
//NarrativeEventGraphAppMode::NarrativeEventGraphAppMode(TSharedPtr<class NarrativeAssetEditorApp> App)
//    : FApplicationMode(TEXT("NarrativeEventGraphAppMode"))
//{
//    _App = App;
//
//    // Set the tab layout for this mode
//    _Tabs.RegisterFactory(MakeShareable(new EventGraphTabFactory(App)));
//
//    TabLayout = FTabManager::NewLayout("NarrativeEventGraphAppMode_Layout")
//        ->AddArea
//        (
//            FTabManager::NewPrimaryArea()
//            ->SetOrientation(Orient_Vertical)
//            ->Split
//            (
//                FTabManager::NewSplitter()
//                ->SetOrientation(Orient_Horizontal)
//                ->Split
//                (
//                    FTabManager::NewStack()
//                    ->SetSizeCoefficient(0.6f)
//                    ->AddTab("EventGraphTab", ETabState::OpenedTab)
//                )
//            )
//        );
//}
//
//void NarrativeEventGraphAppMode::RegisterTabFactories(TSharedPtr<class FTabManager> InTabManager)
//{
//    TSharedPtr<NarrativeAssetEditorApp> App = _App.Pin();
//    App->PushTabFactories(_Tabs);
//    FApplicationMode::RegisterTabFactories(InTabManager);
//}
//
//void NarrativeEventGraphAppMode::PreDeactivateMode()
//{
//    FApplicationMode::PreDeactivateMode();
//}
//
//void NarrativeEventGraphAppMode::PostActivateMode()
//{
//    FApplicationMode::PostActivateMode();
//}

FText FNarrativeAssetAppModes::GetLocalizedMode(FName InMode)
{
    if (InMode == FNarrativeAssetAppModes::NarrativeDefaultMode)
    {
        return NSLOCTEXT("NarrativeAssetAppModes", "NarrativeDefaultMode", "Narrative Asset");
    }

    return FText::FromName(InMode);
}
