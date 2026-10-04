// Copyright Ironic Studio. All Rights Reserved.

#include "SRPGIdClaimEditor.h"
#include "RPGIdCategoryService.h"
#include "RPGSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"

#include "Assets/RPGPrimaryAsset.h"
#include "Editor.h"
#include "PropertyEditorClipboard.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "RPGIdClaimWidget"

void SRPGIdClaimEditor::Construct(const FArguments& InArgs)
{
	Owners = InArgs._Owners;
	FRPGIdCategoryStore::Get();
	LastObservedId = CurrentId();
	SetResult(FRPGIdClaimEditor::Inspect(Owners));
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1)
			[
				SNew(STextBlock).Text_Lambda([this]() { return FText::FromName(CurrentId().Id); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("Copy", "Copy")).OnClicked(this, &SRPGIdClaimEditor::CopyId)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text_Lambda([this]()
				{
					return CurrentId().Id.IsNone() ? LOCTEXT("Claim", "Claim Id")
						: FRPGIdClaimEditor::SupportsExistingChange(Owners) ? LOCTEXT("Change", "Change Id") : LOCTEXT("PreviewChange", "Preview Change");
				})
				.IsEnabled_Lambda([this]()
				{
					const UObject* Owner = Owners.Num() == 1 ? Owners[0].Get() : nullptr;
					return !bEditing && (LastResult.Code == ERPGIdClaimResult::Unclaimed
						|| (Owner && !CurrentId().Id.IsNone() && (LastResult.Code == ERPGIdClaimResult::Success
							|| LastResult.Code == ERPGIdClaimResult::Collision || LastResult.Code == ERPGIdClaimResult::InvalidFormatOrType)));
				})
				.OnClicked(this, &SRPGIdClaimEditor::BeginDraft)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("Refresh", "Check Current")).OnClicked(this, &SRPGIdClaimEditor::RefreshAudit)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 3)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]()
			{
				const URPGPrimaryAsset* Owner = Owners.Num() == 1 ? Cast<URPGPrimaryAsset>(Owners[0].Get()) : nullptr;
				return Owner ? FText::Format(LOCTEXT("Type", "Type: {0}  |  Prefix: {1}  |  {2}"), FText::FromName(Owner->GetAssetType()),
					FText::FromName(Owner->GetAssetIdPrefix()), Owner->GetOutermost()->IsDirty() ? LOCTEXT("Dirty", "Unsaved") : LOCTEXT("Clean", "Package clean"))
					: LOCTEXT("Single", "Select one asset to edit its Claim.");
			})
		]
		+ SVerticalBox::Slot().AutoHeight()
		[SNew(STextBlock).AutoWrapText(true).Text(this, &SRPGIdClaimEditor::CategoryText)]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SVerticalBox).Visibility_Lambda([this]() { return bEditing ? EVisibility::Visible : EVisibility::Collapsed; })
			+ SVerticalBox::Slot().AutoHeight()
			[
				SAssignNew(DraftBox, SEditableTextBox).HintText(LOCTEXT("Hint", "Full Id, e.g. i1000"))
				.OnTextChanged_Lambda([this](const FText& Text)
				{
					Draft = Text.ToString();
					bChecked = false;
					LastResult.Conflicts.Reset();
					LastResult.References.Reset();
					LastResult.Context = FText::GetEmpty();
					ResetMigration();
					LastResult.Message = LOCTEXT("Unchecked", "Draft only. Check before applying; typing or losing focus never changes the asset.");
				})
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).Text(LOCTEXT("Suggest", "Suggest"))
					.IsEnabled_Lambda([this]()
					{
						return CurrentId().Id.IsNone() || (FRPGIdClaimEditor::SupportsExistingChange(Owners));
					})
					.OnClicked(this, &SRPGIdClaimEditor::SuggestDraft)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).Text(LOCTEXT("Check", "Check")).OnClicked(this, &SRPGIdClaimEditor::CheckDraft)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).Text(this, &SRPGIdClaimEditor::ApplyButtonText)
					.IsEnabled_Lambda([this]() { return bChecked && (LastResult.CanApply() || IsMigrationReady()) && CurrentId() == ExpectedId; })
					.OnClicked(this, &SRPGIdClaimEditor::ApplyDraft)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).Text(LOCTEXT("Cancel", "Cancel")).OnClicked(this, &SRPGIdClaimEditor::CancelDraft)
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 3)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]() { return LastResult.Message; })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]() { return LastResult.Context; })
			.Visibility_Lambda([this]() { return LastResult.Context.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SComboButton).OnGetMenuContent(this, &SRPGIdClaimEditor::BuildConflicts)
			.Visibility_Lambda([this]() { return LastResult.Conflicts.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.ButtonContent()[SNew(STextBlock).Text(LOCTEXT("Conflicts", "Open conflicting owner"))]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]()
			{
				const FText ClaimPolicy = FRPGIdClaimEditor::GetPolicyText(Owners);
				return CurrentId().IsValid()
					? FText::Format(LOCTEXT("MigrationPolicy",
						"{0}\nA sealed current-Release Id can offer Apply Migration after Check when every authored reference has a certified writer. "
						"Migration does not save or seal artifacts."), ClaimPolicy)
					: ClaimPolicy;
			})
		]
	];
}

FRPGId SRPGIdClaimEditor::CurrentId() const
{
	const URPGPrimaryAsset* Owner = Owners.Num() == 1 ? Cast<URPGPrimaryAsset>(Owners[0].Get()) : nullptr;
	return Owner ? Owner->GetId() : FRPGId();
}

void SRPGIdClaimEditor::Tick(const FGeometry& Geometry, double Time, float DeltaTime)
{
	SCompoundWidget::Tick(Geometry, Time, DeltaTime);
	if (CurrentId() != LastObservedId)
	{
		LastObservedId = CurrentId();
		bChecked = false;
		LastResult.Conflicts.Reset();
		LastResult.References.Reset();
		LastResult.Context = FText::GetEmpty();
		LastResult.Code = ERPGIdClaimResult::StaleRequest;
		LastResult.Message = LOCTEXT("Changed", "The current Id changed externally. Cancel the draft and Check Current again.");
		ResetMigration();
	}
}

FRPGIdClaimRequest SRPGIdClaimEditor::Request() const
{
	return { Owners, ExpectedId, FRPGId(FName(*Draft)), bChecked ? LastResult.IndexGeneration : 0 };
}

void SRPGIdClaimEditor::SetResult(const FRPGIdClaimResult& Value)
{
	LastResult = Value;
}

FReply SRPGIdClaimEditor::BeginDraft()
{
	ExpectedId = CurrentId();
	ResetMigration();
	bEditing = true;
	bChecked = false;
	DraftBox->SetText(ExpectedId.Id.IsNone() ? FText::GetEmpty() : FText::FromName(ExpectedId.Id));
	return FReply::Handled();
}

FReply SRPGIdClaimEditor::CancelDraft()
{
	bEditing = false;
	bChecked = false;
	Draft.Reset();
	ResetMigration();
	SetResult(FRPGIdClaimEditor::Inspect(Owners));
	return FReply::Handled();
}

FReply SRPGIdClaimEditor::CheckDraft()
{
	const FRPGIdClaimRequest ClaimRequest = Request();
	SetResult(FRPGIdClaimEditor::Preview(ClaimRequest));
	ResetMigration();
	const URPGPrimaryAsset* Owner = Owners.Num() == 1 ? Cast<URPGPrimaryAsset>(Owners[0].Get()) : nullptr;
	if (!LastResult.CanApply() && Owner && ClaimRequest.ExpectedId.IsValid() && ClaimRequest.Candidate.IsValid()
		&& ClaimRequest.ExpectedId != ClaimRequest.Candidate)
	{
		FRPGIdReferenceMigrationWorkflowResult Preview = MigrationWorkflow.Preview(
			{ClaimRequest.ExpectedId, ClaimRequest.Candidate, FSoftObjectPath(Owner)});
		if (Preview.IsApplicable())
		{
			LastResult.Message = Preview.Message;
			LastResult.Context = Preview.Context;
			LastResult.IndexGeneration = Preview.Plan.ReferenceIndexGeneration;
			MigrationResult = MoveTemp(Preview);
		}
	}
	bChecked = true;
	return FReply::Handled();
}

FReply SRPGIdClaimEditor::ApplyDraft()
{
	if (IsMigrationReady())
	{
		const FText Warning = FText::Format(LOCTEXT("ConfirmMigration",
			"Migrate the owner and {0} certified reference(s) in one Undo transaction? This does not save or seal anything."),
			FText::AsNumber(MigrationResult->Plan.Edits.Num()));
		if (FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::No, Warning) != EAppReturnType::Yes)
		{
			return FReply::Handled();
		}
		MigrationResult = MigrationWorkflow.Apply(MigrationResult.GetValue());
		LastResult.Message = MigrationResult->Message;
		LastResult.Context = MigrationResult->Context;
		LastResult.Code = MigrationResult->Code == ERPGIdReferenceMigrationWorkflowCode::Applied
			? ERPGIdClaimResult::Success : ERPGIdClaimResult::InspectionIncomplete;
	}
	else
	{
		SetResult(FRPGIdClaimEditor::Execute(Request()));
	}
	if (LastResult.CanApply())
	{
		bEditing = false;
		LastObservedId = CurrentId();
	}
	bChecked = false;
	return FReply::Handled();
}

bool SRPGIdClaimEditor::IsMigrationReady() const
{
	return MigrationResult.IsSet() && MigrationResult->CanApply();
}

FText SRPGIdClaimEditor::ApplyButtonText() const
{
	return IsMigrationReady() ? LOCTEXT("ApplyMigration", "Apply Migration") : LOCTEXT("Apply", "Apply");
}

void SRPGIdClaimEditor::ResetMigration()
{
	MigrationResult.Reset();
}

FReply SRPGIdClaimEditor::SuggestDraft()
{
	ResetMigration();
	const URPGPrimaryAsset* Owner = Owners.Num() == 1 ? Cast<URPGPrimaryAsset>(Owners[0].Get()) : nullptr;
	if (Owner && GetDefault<URPGSettings>()->NumericLen == 4)
	{
		TArray<FRPGIdCategoryScope> Scopes;
		FString Error;
		if (!FRPGIdCategoryStore::Get().ListScopes(Owner->GetAssetType(), Scopes, Error))
		{
			FRPGIdClaimResult Failure;
			Failure.Message = FText::FromString(Error);
			SetResult(Failure);
			bChecked = false;
			return FReply::Handled();
		}
		if (Scopes.Num() != 1 || Scopes[0].Kind != ERPGIdSuggestionScope::AllNumbers)
		{
			FMenuBuilder Menu(true, nullptr);
			for (const auto& Scope : Scopes)
			{
				TArray<FString> Ranges;
				for (const auto& Range : Scope.Ranges)
				{
					Ranges.Add(FString::Printf(TEXT("%s%04d–%s%04d"), *Owner->GetAssetIdPrefix().ToString(), Range.Start,
						*Owner->GetAssetIdPrefix().ToString(), Range.End));
				}
				const FText Label = FText::FromString(Scope.DisplayName + TEXT("\n") + FString::Join(Ranges, TEXT(", ")));
				const FRPGIdCategorySelection Selection{Scope.Kind, Scope.CategoryKey, Scope.Revision};
				Menu.AddMenuEntry(Label, Label, FSlateIcon(), FUIAction(FExecuteAction::CreateSPLambda(this,
					[this, Selection]() { SuggestInCategory(Selection); })));
			}
			FSlateApplication::Get().PushMenu(AsShared(), FWidgetPath(), Menu.MakeWidget(),
				FSlateApplication::Get().GetCursorPos(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
			return FReply::Handled();
		}
	}
	const FRPGIdClaimResult Suggestion = FRPGIdClaimEditor::Suggest(Request());
	if (Suggestion.Code == ERPGIdClaimResult::Success)
	{
		DraftBox->SetText(FText::FromName(Suggestion.SuggestedId.Id));
	}
	SetResult(Suggestion);
	bChecked = false;
	ResetMigration();
	return FReply::Handled();
}

void SRPGIdClaimEditor::SuggestInCategory(const FRPGIdCategorySelection& Selection)
{
	ResetMigration();
	if (!bEditing) { return; }
	const FRPGIdClaimResult Suggestion = FRPGIdClaimEditor::Suggest(Request(), &Selection);
	if (Suggestion.Code == ERPGIdClaimResult::Success) { DraftBox->SetText(FText::FromName(Suggestion.SuggestedId.Id)); }
	SetResult(Suggestion);
	bChecked = false;
	ResetMigration();
}

FText SRPGIdClaimEditor::CategoryText() const
{
	const URPGPrimaryAsset* Owner = Owners.Num() == 1 ? Cast<URPGPrimaryAsset>(Owners[0].Get()) : nullptr;
	if (!Owner) { return FText::GetEmpty(); }
	auto Describe = [](const FRPGIdCategoryResolution& Value)
	{
		switch (Value.State)
		{
		case ERPGIdCategoryState::Classified: return Value.DisplayName;
		case ERPGIdCategoryState::Unclassified: return FString(TEXT("None (Unclassified)"));
		case ERPGIdCategoryState::Unclaimed: return FString(TEXT("Unclaimed"));
		case ERPGIdCategoryState::InvalidId: return FString(TEXT("Invalid Id"));
		default: return FString(TEXT("Unavailable: ")) + Value.Error;
		}
	};
	FString Text = TEXT("Category: ") + Describe(FRPGIdCategoryStore::Get().Resolve(Owner->GetAssetType(), CurrentId().Id));
	if (bEditing) { Text += TEXT(" | Candidate: ") + Describe(FRPGIdCategoryStore::Get().Resolve(Owner->GetAssetType(), FName(*Draft))); }
	return FText::FromString(Text);
}

FReply SRPGIdClaimEditor::RefreshAudit()
{
	bChecked = false;
	ResetMigration();
	SetResult(FRPGIdClaimEditor::Inspect(Owners));
	return FReply::Handled();
}

FReply SRPGIdClaimEditor::CopyId()
{
	FPropertyEditorClipboard::ClipboardCopy(*CurrentId().ToString());
	return FReply::Handled();
}

TSharedRef<SWidget> SRPGIdClaimEditor::BuildConflicts()
{
	TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
	for (const FSoftObjectPath& Path : LastResult.Conflicts)
	{
		Menu->AddSlot().AutoHeight()
		[
			SNew(SButton).Text(FText::FromString(Path.ToString())).OnClicked_Lambda([Path]()
			{
				if (GEditor)
				{
					GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Path.TryLoad());
				}
				return FReply::Handled();
			})
		];
	}
	return SNew(SBox).MaxDesiredWidth(700)[Menu];
}

#undef LOCTEXT_NAMESPACE
