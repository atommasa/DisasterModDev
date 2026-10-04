// Copyright Ironic Studio. All Rights Reserved.

#include "RPGReleaseSealWorkflow.h"

#include "RPGIdReferenceMigrationHandoff.h"

#define LOCTEXT_NAMESPACE "RPGReleaseSealWorkflow"

namespace
{
	class FProductionBackend final : public IRPGReleaseSealWorkflowBackend
	{
	public:
		virtual bool ReadPending(TArray<FRPGIdRedirect>& OutRedirects, FText& OutError) override
		{
			return FRPGIdReferenceMigrationHandoff::ReadPending(OutRedirects, OutError);
		}

		virtual bool Run(const ERPGReleaseSealAction Action, const FString& ReleaseId, const FString& SaveVersion,
			TConstArrayView<FRPGIdRedirect> Redirects, FText& OutMessage) override
		{
			return FRPGReleaseSealService::Run(Action, ReleaseId, SaveVersion, Redirects, OutMessage);
		}

		virtual bool Consume(TConstArrayView<FRPGIdRedirect> Redirects, FText& OutError) override
		{
			return FRPGIdReferenceMigrationHandoff::Consume(Redirects, OutError);
		}
	};

	bool PassesPendingRedirects(const ERPGReleaseSealAction Action)
	{
		return Action == ERPGReleaseSealAction::Preview || Action == ERPGReleaseSealAction::Seal;
	}

	bool CommitsPendingRedirects(const ERPGReleaseSealAction Action)
	{
		return Action == ERPGReleaseSealAction::Seal || Action == ERPGReleaseSealAction::Resume;
	}

	FText FormatPending(const TArray<FRPGIdRedirect>& Redirects)
	{
		if (Redirects.IsEmpty())
		{
			return LOCTEXT("NoPending", "Pending RPG Id redirects: None.");
		}

		FString Text = FString::Printf(TEXT("Pending RPG Id redirects: %d. Save all migrated artifacts and advance "
			"SaveDataVersion by exactly one before Seal.\n"), Redirects.Num());
		const int32 DisplayLimit = 16;
		for (int32 Index = 0; Index < FMath::Min(Redirects.Num(), DisplayLimit); ++Index)
		{
			Text += Redirects[Index].OldId.ToString() + TEXT(" -> ") + Redirects[Index].NewId.ToString() + TEXT("\n");
		}
		if (Redirects.Num() > DisplayLimit)
		{
			Text += FString::Printf(TEXT("... and %d more redirect(s)."), Redirects.Num() - DisplayLimit);
		}
		return FText::FromString(Text.TrimEnd());
	}
}

FRPGReleaseSealWorkflow::FRPGReleaseSealWorkflow()
	: Backend(MakeShared<FProductionBackend>())
{
}

FRPGReleaseSealWorkflow::FRPGReleaseSealWorkflow(TSharedRef<IRPGReleaseSealWorkflowBackend> InBackend)
	: Backend(MoveTemp(InBackend))
{
}

FText FRPGReleaseSealWorkflow::DescribePending() const
{
	TArray<FRPGIdRedirect> Redirects;
	FText Error;
	return Backend->ReadPending(Redirects, Error) ? FormatPending(Redirects) : Error;
}

FRPGReleaseSealWorkflowResult FRPGReleaseSealWorkflow::Execute(const ERPGReleaseSealAction Action,
	const FString& ReleaseId, const FString& SaveVersion) const
{
	FRPGReleaseSealWorkflowResult Result;
	FText Error;
	if (!Backend->ReadPending(Result.Redirects, Error))
	{
		Result.Message = Error;
		return Result;
	}

	const TConstArrayView<FRPGIdRedirect> PassedRedirects = PassesPendingRedirects(Action)
		? TConstArrayView<FRPGIdRedirect>(Result.Redirects)
		: TConstArrayView<FRPGIdRedirect>();
	Result.bSucceeded = Backend->Run(Action, ReleaseId, SaveVersion, PassedRedirects, Result.Message);
	if (!Result.bSucceeded || !CommitsPendingRedirects(Action) || Result.Redirects.IsEmpty())
	{
		return Result;
	}

	FText CleanupError;
	if (!Backend->Consume(Result.Redirects, CleanupError))
	{
		Result.bCleanupWarning = true;
		Result.Message = FText::Format(LOCTEXT("CleanupWarning",
			"{0}\nThe release succeeded, but its pending redirect handoff could not be cleared: {1}"),
			Result.Message, CleanupError);
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
