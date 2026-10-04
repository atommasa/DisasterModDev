// Copyright Ironic Studio. All Rights Reserved.

#include "RPGIdReferenceIndexUI.h"

#include "Misc/MessageDialog.h"
#include "RPGIdBlueprintReferenceIndex.h"
#include "ToolMenus.h"

namespace
{
	FDelegateHandle IndexUIStartupHandle;

	FString StateToString(const RPGIdReferenceIndexPrivate::ERPGIdReferenceIndexState State)
	{
		switch (State)
		{
		case RPGIdReferenceIndexPrivate::ERPGIdReferenceIndexState::Building:
			return TEXT("Building");
		case RPGIdReferenceIndexPrivate::ERPGIdReferenceIndexState::Ready:
			return TEXT("Ready");
		case RPGIdReferenceIndexPrivate::ERPGIdReferenceIndexState::Stale:
			return TEXT("Stale");
		default:
			return TEXT("Unavailable");
		}
	}

	void ShowIndexStatus()
	{
		const RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexStatus Status = FRPGIdBlueprintReferenceIndex::GetStatus();
		const FString LastBuild = Status.LastBuildTime.GetTicks() == 0 ? TEXT("Never") : Status.LastBuildTime.ToString();
		const FString LastShadow = Status.LastShadowComparisonTime.GetTicks() == 0 ? TEXT("Never") : Status.LastShadowComparisonTime.ToString();
		const FString Failure = Status.LastFailure.IsEmpty() ? TEXT("None") : Status.LastFailure;
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(
			TEXT("State: %s\nIndexed packages: %d / %d\nLast successful build: %s\nExact shadow comparisons: %d\n"
				"Last shadow comparison: %s\nLast shadow target: %s\nLast reason: %s\nCache: %s\n\n"
				"Interactive Check uses the verified index and live Blueprint data. Rebuild completion requires a new Check."),
			*StateToString(Status.State), Status.IndexedPackageCount, Status.TotalPackageCount, *LastBuild,
			Status.SuccessfulShadowComparisonCount, *LastShadow,
			Status.LastShadowTarget.IsEmpty() ? TEXT("None") : *Status.LastShadowTarget, *Failure,
			*FRPGIdBlueprintReferenceIndex::GetCachePath())));
	}

	void RequestIndexRebuild()
	{
		FRPGIdBlueprintReferenceIndex::RequestRebuild();
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::FromString(TEXT("The RPG Id reference index was invalidated and a background rebuild was queued.")));
	}

	void CancelIndexBuild()
	{
		FRPGIdBlueprintReferenceIndex::CancelBuild();
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::FromString(TEXT("The RPG Id reference index build was cancelled. The index remains unavailable.")));
	}
}

void FRPGIdReferenceIndexUI::Register()
{
	if (IsRunningCommandlet())
	{
		return;
	}
	IndexUIStartupHandle = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		FToolMenuOwnerScoped Owner(TEXT("RPGIdReferenceIndex"));
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		FToolMenuSection& Section = Menu->AddSection(TEXT("RPGIdReferenceIndex"), FText::FromString(TEXT("IronicRPG")));
		Section.AddMenuEntry(TEXT("RPGIdReferenceIndexStatus"), FText::FromString(TEXT("RPGId Reference Index Status")),
			FText::FromString(TEXT("Show persistent Blueprint reference index state and progress.")), FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&ShowIndexStatus)));
		Section.AddMenuEntry(TEXT("RPGIdReferenceIndexRebuild"), FText::FromString(TEXT("Rebuild RPGId Reference Index")),
			FText::FromString(TEXT("Invalidate trusted evidence and queue a full background rebuild.")), FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&RequestIndexRebuild)));
		Section.AddMenuEntry(TEXT("RPGIdReferenceIndexCancel"), FText::FromString(TEXT("Cancel RPGId Reference Index Build")),
			FText::FromString(TEXT("Stop the current build and leave the index unavailable.")), FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&CancelIndexBuild)));
	}));
}

void FRPGIdReferenceIndexUI::Unregister()
{
	if (!IndexUIStartupHandle.IsValid())
	{
		return;
	}
	UToolMenus::UnRegisterStartupCallback(IndexUIStartupHandle);
	UToolMenus::UnregisterOwner(TEXT("RPGIdReferenceIndex"));
	IndexUIStartupHandle.Reset();
}
