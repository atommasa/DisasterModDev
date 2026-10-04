// Copyright Ironic Studio. All Rights Reserved.
#include "Assets/RPGReleaseManifest.h"
#include "IO/IoHash.h"

bool FRPGReleaseSnapshot::IsValidReleaseId(const FString& Value)
{
	if (Value.IsEmpty() || Value.Len() > 64) { return false; }
	for (TCHAR C : Value)
	{
		if (!(C >= 'a' && C <= 'z') && !(C >= '0' && C <= '9') && C != '-' && C != '_') { return false; }
	}
	return true;
}

bool FRPGReleaseSnapshot::Validate(FText& OutError) const
{
	OutError = FText::GetEmpty();
	if (Schema != 2 || !IsValidReleaseId(ReleaseId) || SaveDataVersion == 0)
	{
		OutError = FText::FromString(TEXT("Unsupported manifest schema, invalid ReleaseId or invalid SaveDataVersion."));
		return false;
	}
	TSet<FName> Ids;
	TSet<FSoftObjectPath> Paths;
	for (const FRPGReleaseClaim& Claim : Claims)
	{
		if (Claim.Id.IsNone() || Claim.OwnerPath.IsNull() || Claim.OwnerClass.IsNull() || Ids.Contains(Claim.Id) || Paths.Contains(Claim.OwnerPath))
		{
			OutError = FText::FromString(TEXT("Manifest contains an empty or duplicate Claim/owner."));
			return false;
		}
		Ids.Add(Claim.Id);
		Paths.Add(Claim.OwnerPath);
	}
	FRPGIdMigrationChain Chain;
	const FRPGIdMigrationResult ChainResult = FRPGIdMigrationChain::Build(0, SaveDataVersion, SaveGameMigrations, Chain);
	if (!ChainResult.IsSuccess())
	{
		OutError = FText::FromString(TEXT("Invalid save migration catalog: ") + ChainResult.Diagnostic);
		return false;
	}
	for (const FRPGIdMigrationStep& Step : SaveGameMigrations)
	{
		for (const FRPGIdRedirect& Redirect : Step.Redirects)
		{
			FRPGId Resolved;
			if (!Chain.TryResolve(Redirect.OldId, Resolved) || !Ids.Contains(Resolved.Id) || Ids.Contains(Redirect.OldId.Id))
			{
				OutError = FText::FromString(TEXT("Every redirect must resolve to one current Claim and retire its source Id."));
				return false;
			}
		}
	}
	return true;
}

FString FRPGReleaseSnapshot::ComputeHash() const
{
	// Length-prefixed UTF-8 fields avoid separator ambiguity and archive/platform dependencies.
	FString Canonical;
	auto Append = [&Canonical](const FString& Value)
	{
		const FTCHARToUTF8 Bytes(*Value);
		Canonical += FString::FromInt(Bytes.Length()) + TEXT(":") + Value;
	};
	Append(FString::FromInt(Schema));
	Append(ReleaseId);
	Append(FString::FromInt(SaveDataVersion));
	Append(PreviousHash);
	TArray<FRPGIdMigrationStep> SortedSteps = SaveGameMigrations;
	SortedSteps.Sort([](const FRPGIdMigrationStep& A, const FRPGIdMigrationStep& B) { return A.FromVersion < B.FromVersion; });
	Append(FString::FromInt(SortedSteps.Num()));
	for (FRPGIdMigrationStep& Step : SortedSteps)
	{
		Append(FString::FromInt(Step.FromVersion));
		Append(FString::FromInt(Step.ToVersion));
		Step.Redirects.Sort([](const FRPGIdRedirect& A, const FRPGIdRedirect& B)
		{
			if (A.OldId.Id != B.OldId.Id) { return A.OldId.Id.LexicalLess(B.OldId.Id); }
			return A.NewId.Id.LexicalLess(B.NewId.Id);
		});
		Append(FString::FromInt(Step.Redirects.Num()));
		for (const FRPGIdRedirect& Redirect : Step.Redirects)
		{
			Append(Redirect.OldId.Id.ToString().ToLower());
			Append(Redirect.NewId.Id.ToString().ToLower());
		}
	}
	TArray<FRPGReleaseClaim> Sorted = Claims;
	Sorted.Sort([](const FRPGReleaseClaim& A, const FRPGReleaseClaim& B) { return A.Id.LexicalLess(B.Id); });
	Append(FString::FromInt(Sorted.Num()));
	for (const FRPGReleaseClaim& Claim : Sorted)
	{
		Append(Claim.Id.ToString().ToLower());
		Append(Claim.OwnerPath.ToString());
		Append(Claim.OwnerClass.ToString());
	}
	const FTCHARToUTF8 Bytes(*Canonical);
	return LexToString(FIoHash::HashBuffer(Bytes.Get(), Bytes.Length()));
}

FRPGReleaseMigrationCatalogResult FRPGReleaseMigrationCatalogReader::ReadCurrent(FRPGReleaseMigrationCatalog& OutCatalog)
{
	OutCatalog = {};
	const URPGReleaseSettings* Settings = GetDefault<URPGReleaseSettings>();
	const FString& HeadPath = Settings->GetHeadManifest();
	const FString& HeadHash = Settings->GetHeadHash();
	if (HeadPath.IsEmpty() && HeadHash.IsEmpty())
	{
		OutCatalog.CurrentVersion = 1;
		OutCatalog.Steps = { FRPGIdMigrationStep(0, 1, {}) };
		return {};
	}
	if (HeadPath.IsEmpty() || HeadHash.IsEmpty())
	{
		return { ERPGReleaseMigrationCatalogResult::IncompleteHead,
			TEXT("The RPG release head path and hash must either both be set or both be empty.") };
	}

	const URPGReleaseManifest* Manifest = Cast<URPGReleaseManifest>(FSoftObjectPath(HeadPath).TryLoad());
	if (!Manifest)
	{
		return { ERPGReleaseMigrationCatalogResult::ManifestUnavailable,
			TEXT("The canonical RPG release head manifest is unavailable: ") + HeadPath };
	}
	if (Manifest->GetSemanticHash() != HeadHash)
	{
		return { ERPGReleaseMigrationCatalogResult::HashMismatch,
			TEXT("The canonical RPG release head manifest hash does not match the source-controlled head.") };
	}
	return ReadSnapshot(Manifest->GetSnapshot(), HeadHash, OutCatalog);
}

FRPGReleaseMigrationCatalogResult FRPGReleaseMigrationCatalogReader::ReadSnapshot(const FRPGReleaseSnapshot& Snapshot,
	const FString& ExpectedHash, FRPGReleaseMigrationCatalog& OutCatalog)
{
	OutCatalog = {};
	if (ExpectedHash.IsEmpty() || Snapshot.ComputeHash() != ExpectedHash)
	{
		return { ERPGReleaseMigrationCatalogResult::HashMismatch,
			TEXT("The RPG release head manifest does not match its source-controlled semantic hash.") };
	}
	FText Error;
	if (!Snapshot.Validate(Error))
	{
		return { ERPGReleaseMigrationCatalogResult::InvalidSnapshot, Error.ToString() };
	}
	OutCatalog.CurrentVersion = Snapshot.SaveDataVersion;
	OutCatalog.Steps = Snapshot.SaveGameMigrations;
	return {};
}
