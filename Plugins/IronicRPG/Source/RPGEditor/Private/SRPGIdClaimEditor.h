// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RPGIdClaimEditor.h"
#include "RPGIdReferenceMigrationWorkflow.h"
#include "Widgets/SCompoundWidget.h"

/** Draft-only UI. All ownership decisions and writes go through the Claim or reference migration workflow modules. */
class SRPGIdClaimEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRPGIdClaimEditor) {}
		SLATE_ARGUMENT(TArray<TWeakObjectPtr<UObject>>, Owners)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& Geometry, double Time, float DeltaTime) override;

private:
	FRPGId CurrentId() const;
	FRPGIdClaimRequest Request() const;
	FReply BeginDraft();
	FReply CancelDraft();
	FReply CheckDraft();
	FReply ApplyDraft();
	bool IsMigrationReady() const;
	FText ApplyButtonText() const;
	void ResetMigration();
	FReply SuggestDraft();
	void SuggestInCategory(const FRPGIdCategorySelection& Selection);
	FText CategoryText() const;
	FReply RefreshAudit();
	FReply CopyId();
	TSharedRef<SWidget> BuildConflicts();
	void SetResult(const FRPGIdClaimResult& Value);

private:
	TArray<TWeakObjectPtr<UObject>> Owners;
	FRPGId ExpectedId;
	FRPGId LastObservedId;
	FString Draft;
	FRPGIdClaimResult LastResult;
	FRPGIdReferenceMigrationWorkflow MigrationWorkflow;
	TOptional<FRPGIdReferenceMigrationWorkflowResult> MigrationResult;
	bool bEditing = false;
	bool bChecked = false;
	TSharedPtr<class SEditableTextBox> DraftBox;
};