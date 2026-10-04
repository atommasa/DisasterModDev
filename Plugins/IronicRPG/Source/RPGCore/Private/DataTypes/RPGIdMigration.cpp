// Copyright Ironic Studio. All Rights Reserved.

#include "DataTypes/RPGIdMigration.h"

namespace
{
	FRPGIdMigrationResult MakeFailure(ERPGIdMigrationResult Code, const FString& Diagnostic)
	{
		FRPGIdMigrationResult Result;
		Result.Code = Code;
		Result.Diagnostic = Diagnostic;
		return Result;
	}
}

FRPGIdMigrationResult FRPGIdMigrationChain::Build(uint16 SourceVersion, uint16 TargetVersion, TConstArrayView<FRPGIdMigrationStep> Steps,
	FRPGIdMigrationChain& OutChain)
{
	OutChain = FRPGIdMigrationChain();
	if (SourceVersion > TargetVersion)
	{
		return MakeFailure(ERPGIdMigrationResult::SourceVersionIsNewer,
			FString::Printf(TEXT("Save version %u is newer than supported version %u."), SourceVersion, TargetVersion));
	}

	TMap<uint16, const FRPGIdMigrationStep*> StepsBySourceVersion;
	for (const FRPGIdMigrationStep& Step : Steps)
	{
		if (StepsBySourceVersion.Contains(Step.FromVersion))
		{
			return MakeFailure(ERPGIdMigrationResult::DuplicateVersionStep,
				FString::Printf(TEXT("More than one migration step starts at version %u."), Step.FromVersion));
		}
		StepsBySourceVersion.Add(Step.FromVersion, &Step);

		if (Step.ToVersion != Step.FromVersion + 1)
		{
			return MakeFailure(ERPGIdMigrationResult::NonSequentialVersionStep,
				FString::Printf(TEXT("Migration step %u -> %u is not sequential."), Step.FromVersion, Step.ToVersion));
		}

		TSet<FName> Sources;
		TSet<FName> Targets;
		for (const FRPGIdRedirect& Redirect : Step.Redirects)
		{
			const FName OldType = Redirect.OldId.GetRebuiltIdType();
			const FName NewType = Redirect.NewId.GetRebuiltIdType();
			if (!Redirect.OldId.IsValid() || !Redirect.NewId.IsValid() || OldType.IsNone() || NewType.IsNone())
			{
				return MakeFailure(ERPGIdMigrationResult::InvalidRedirectId,
					FString::Printf(TEXT("Migration step %u contains an invalid redirect from '%s' to '%s'."), Step.FromVersion,
						*Redirect.OldId.ToString(), *Redirect.NewId.ToString()));
			}
			if (OldType != NewType)
			{
				return MakeFailure(ERPGIdMigrationResult::RedirectTypeMismatch,
					FString::Printf(TEXT("Migration step %u changes Id type from %s to %s."), Step.FromVersion, *OldType.ToString(),
						*NewType.ToString()));
			}
			if (Sources.Contains(Redirect.OldId.Id))
			{
				return MakeFailure(ERPGIdMigrationResult::DuplicateRedirectSource,
					FString::Printf(TEXT("Migration step %u redirects '%s' more than once."), Step.FromVersion, *Redirect.OldId.ToString()));
			}
			if (Targets.Contains(Redirect.NewId.Id))
			{
				return MakeFailure(ERPGIdMigrationResult::DuplicateRedirectTarget,
					FString::Printf(TEXT("Migration step %u redirects more than one Id to '%s'."), Step.FromVersion, *Redirect.NewId.ToString()));
			}
			Sources.Add(Redirect.OldId.Id);
			Targets.Add(Redirect.NewId.Id);
		}
	}

	for (uint16 Version = SourceVersion; Version < TargetVersion; ++Version)
	{
		const FRPGIdMigrationStep* const* Step = StepsBySourceVersion.Find(Version);
		if (!Step)
		{
			return MakeFailure(ERPGIdMigrationResult::MissingVersionStep,
				FString::Printf(TEXT("No migration step exists for version %u -> %u."), Version, Version + 1));
		}

		TMap<FName, FName>& RedirectMap = OutChain.OrderedRedirects.AddDefaulted_GetRef();
		for (const FRPGIdRedirect& Redirect : (*Step)->Redirects)
		{
			RedirectMap.Add(Redirect.OldId.Id, Redirect.NewId.Id);
		}
	}

	OutChain.bIsValid = true;
	return {};
}

bool FRPGIdMigrationChain::TryResolve(const FRPGId& SourceId, FRPGId& OutId) const
{
	if (!bIsValid)
	{
		return false;
	}

	OutId = SourceId;
	for (const TMap<FName, FName>& Redirects : OrderedRedirects)
	{
		if (const FName* RedirectedId = Redirects.Find(OutId.Id))
		{
			OutId = FRPGId(*RedirectedId);
		}
	}
	return true;
}
