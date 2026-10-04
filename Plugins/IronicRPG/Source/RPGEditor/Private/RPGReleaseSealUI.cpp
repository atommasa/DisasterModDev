// Copyright Ironic Studio. All Rights Reserved.

#include "RPGReleaseSealUI.h"

#include "RPGReleaseSealWorkflow.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FDelegateHandle StartupHandle;

	void OpenSealWindow()
	{
		TSharedRef<FRPGReleaseSealWorkflow> Workflow = MakeShared<FRPGReleaseSealWorkflow>();
		TSharedRef<SEditableTextBox> Release = SNew(SEditableTextBox)
			.HintText(FText::FromString(TEXT("ReleaseId, e.g. release_001")));
		TSharedRef<SEditableTextBox> Version = SNew(SEditableTextBox)
			.HintText(FText::FromString(TEXT("Optional numeric SaveDataVersion")));
		TSharedRef<STextBlock> Result = SNew(STextBlock).AutoWrapText(true);
		TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
		Body->AddSlot().AutoHeight().Padding(4)[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(
			TEXT("Seal saves an immutable identity manifest and updates the release head. All content must be saved and valid. "
				"Successful Seal clears Editor Undo history. Use Preview first; Seal repeats validation.")))];
		Body->AddSlot().AutoHeight().Padding(4)[SNew(STextBlock).AutoWrapText(true)
			.Text_Lambda([Workflow]() { return Workflow->DescribePending(); })];
		Body->AddSlot().AutoHeight().Padding(4)[Release];
		Body->AddSlot().AutoHeight().Padding(4)[Version];
		for (const auto& Entry : TArray<TPair<FString, ERPGReleaseSealAction>>{
			{TEXT("Preview"), ERPGReleaseSealAction::Preview}, {TEXT("Seal"), ERPGReleaseSealAction::Seal},
			{TEXT("Resume"), ERPGReleaseSealAction::Resume}, {TEXT("Abort Prepared"), ERPGReleaseSealAction::Abort}})
		{
			Body->AddSlot().AutoHeight().Padding(4)[SNew(SButton).Text(FText::FromString(Entry.Key))
				.OnClicked_Lambda([Workflow, Release, Version, Result, Action = Entry.Value]()
				{
					if (Action != ERPGReleaseSealAction::Preview)
					{
						const FString Warning = Action == ERPGReleaseSealAction::Abort
							? TEXT("Remove this prepared manifest? Committed history is preserved.")
							: TEXT("Commit this release and clear Editor Undo history?");
						if (FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::No,
							FText::FromString(Warning)) != EAppReturnType::Yes)
						{
							return FReply::Handled();
						}
					}
					const FRPGReleaseSealWorkflowResult WorkflowResult = Workflow->Execute(
						Action, Release->GetText().ToString(), Version->GetText().ToString());
					Result->SetText(WorkflowResult.Message);
					return FReply::Handled();
				})];
		}
		Body->AddSlot().AutoHeight().Padding(4)[Result];
		FSlateApplication::Get().AddWindow(SNew(SWindow).Title(FText::FromString(TEXT("RPG Release Seal")))
			.ClientSize(FVector2D(650, 500))[Body]);
	}
}

void FRPGReleaseSealUI::Register()
{
	StartupHandle = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		FToolMenuOwnerScoped Owner(TEXT("RPGReleaseSeal"));
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		Menu->AddSection(TEXT("RPGRelease"), FText::FromString(TEXT("IronicRPG"))).AddMenuEntry(
			TEXT("RPGReleaseSeal"), FText::FromString(TEXT("RPG Release Seal")),
			FText::FromString(TEXT("Preview, seal or recover a release.")),
			FSlateIcon(), FUIAction(FExecuteAction::CreateStatic(&OpenSealWindow)));
	}));
}

void FRPGReleaseSealUI::Unregister()
{
	UToolMenus::UnRegisterStartupCallback(StartupHandle);
	UToolMenus::UnregisterOwner(TEXT("RPGReleaseSeal"));
}
