// Copyright Ironic Studio. All Rights Reserved.
#include "RPGReleaseSealService.h"
#include "RPGIdClaimSnapshot.h"
#include "RPGIdPendingReservationCoordinator.h"
#include "Assets/RPGPrimaryAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EditorValidatorSubsystem.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectIterator.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#else
#include <cstdio>
#endif

namespace
{
	bool bSealing = false;
	bool bAborting = false;
	FDelegateHandle DeleteHandle;
	const FString ManifestRoot = TEXT("/Game/RPGReleaseManifests/");

	bool Fail(FText& Error, const FString& Message)
	{
		Error = FText::FromString(Message);
		return false;
	}

	bool ReplaceHeadFile(const FString& Destination, const FString& Source)
	{
		// Same-directory replacement must not delete the committed head before the replacement succeeds.
#if PLATFORM_WINDOWS
		return MoveFileExW(*FPaths::ConvertRelativePathToFull(Source), *FPaths::ConvertRelativePathToFull(Destination),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
		return std::rename(TCHAR_TO_UTF8(*Source), TCHAR_TO_UTF8(*Destination)) == 0;
#endif
	}

	bool IsClean(FText& Error)
	{
		// Seal is an explicit release boundary. Any authored project/plugin package must be saved first.
		for (TObjectIterator<UPackage> It; It; ++It)
		{
			const FString Name = It->GetName();
			if ((Name.StartsWith(TEXT("/Game/")) || Name.StartsWith(TEXT("/IronicRPG/"))) && It->IsDirty())
			{
				return Fail(Error, TEXT("Save or revert the dirty package before sealing: ") + Name);
			}
		}
		return true;
	}

	bool ReadHeadFile(FString& Path, FString& Hash, FText& Error)
	{
		const FString Filename = GetDefault<URPGReleaseSettings>()->GetDefaultConfigFilename();
		FConfigFile Config;
		FString Contents;
		if (IFileManager::Get().FileExists(*Filename) && !FFileHelper::LoadFileToString(Contents, *Filename))
		{
			return Fail(Error, TEXT("Cannot read source-controlled release settings: ") + Filename);
		}
		Config.ProcessInputFileContents(Contents, Filename);
		Config.GetString(TEXT("/Script/RPGCore.RPGReleaseSettings"), TEXT("HeadManifest"), Path);
		Config.GetString(TEXT("/Script/RPGCore.RPGReleaseSettings"), TEXT("HeadHash"), Hash);
		return true;
	}

	bool CheckOwners(const FRPGReleaseHistory& History, FText& Error)
	{
		if (History.ReservedIds.IsEmpty()) { return true; }
		TMap<FSoftObjectPath, FName> Owners;
		const FRPGIdClaimResult Result = RPGIdClaimPrivate::ReadOwners(Owners);
		if (!Result.CanApply()) { Error = Result.Message; return false; }
		for (FName Id : History.ReservedIds)
		{
			int32 Count = 0;
			for (const auto& Owner : Owners) { Count += Owner.Value == Id ? 1 : 0; }
			const int32 ExpectedCount = History.RetiredIds.Contains(Id) ? 0 : 1;
			if (Count != ExpectedCount)
			{
				return Fail(Error, ExpectedCount == 0
					? TEXT("A retired RPG Id must not have a current owner: ") + Id.ToString()
					: TEXT("Released/prepared Id must have exactly one current owner: ") + Id.ToString());
			}
		}
		return true;
	}

	bool CollectSnapshot(FRPGReleaseSnapshot& Snapshot, FText& Error)
	{
		if (!IsClean(Error)) { return false; }
		TSet<FName> PendingIds;
		if (!FRPGIdPendingReservationCoordinator::ReadReservedIds(PendingIds, Error)) { return false; }
		if (!PendingIds.IsEmpty())
		{
			return Fail(Error, TEXT("Resolve pending RPGId reservations before sealing. Save/revert the owner and complete a zero-reference audit."));
		}
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		FARFilter Filter;
		Filter.PackagePaths = { FName(TEXT("/Game")), FName(TEXT("/IronicRPG")) };
		Filter.bRecursivePaths = true;
		Filter.bIncludeOnlyOnDiskAssets = true;
		TArray<FAssetData> Assets;
		if (!Registry.GetAssets(Filter, Assets)) { return Fail(Error, TEXT("Release asset discovery failed.")); }
		TMap<FSoftObjectPath, FName> Owners;
		const FRPGIdClaimResult Ownership = RPGIdClaimPrivate::ReadOwners(Owners);
		if (!Ownership.CanApply()) { Error = Ownership.Message; return false; }
		for (const auto& Entry : Owners)
		{
			UObject* Object = Entry.Key.TryLoad();
			URPGPrimaryAsset* Owner = Cast<URPGPrimaryAsset>(Object);
			if (!Owner || !FPackageName::DoesPackageExist(Entry.Key.GetLongPackageName()))
			{
				return Fail(Error, TEXT("Release owner is unreadable or unsaved: ") + Entry.Key.ToString());
			}
			Assets.AddUnique(FAssetData(Owner));
			Snapshot.Claims.Add({ Owner->GetId().Id, Entry.Key, FSoftClassPath(Owner->GetClass()) });
		}
		if (!Snapshot.Validate(Error)) { return false; }
		UEditorValidatorSubsystem* Validator = GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>();
		if (!Validator) { return Fail(Error, TEXT("Editor validation is unavailable.")); }
		FValidateAssetsSettings Settings;
		Settings.ValidationUsecase = EDataValidationUsecase::Manual;
		Settings.bSkipExcludedDirectories = false;
		Settings.bShowIfNoFailures = false;
		FValidateAssetsResults Results;
		Validator->ValidateAssetsWithSettings(Assets, Settings, Results);
		if (Results.NumInvalid || Results.bAssetLimitReached || Results.NumSkipped || Results.NumChecked == 0)
		{
			return Fail(Error, TEXT("Release validation failed or was incomplete. Inspect the Data Validation log; no release was committed."));
		}
		return IsClean(Error);
	}

	bool PrepareSaveMigration(FRPGReleaseSnapshot& Snapshot, const FRPGReleaseSnapshot* Previous, const FString& RequestedVersion,
		TConstArrayView<FRPGIdRedirect> Redirects, FText& Error)
	{
		if (!Previous && !Redirects.IsEmpty())
		{
			return Fail(Error, TEXT("The first release cannot redirect Ids that were never released."));
		}
		const uint16 PreviousVersion = Previous ? Previous->SaveDataVersion : 0;
		uint16 TargetVersion = Previous ? PreviousVersion : 1;
		if (!RequestedVersion.IsEmpty() && !LexTryParseString(TargetVersion, *RequestedVersion))
		{
			return Fail(Error, TEXT("SaveDataVersion must be an integer from 1 to 65535."));
		}
		if (TargetVersion == 0 || TargetVersion < PreviousVersion || TargetVersion > PreviousVersion + 1)
		{
			return Fail(Error, TEXT("SaveDataVersion must remain unchanged or advance by exactly one."));
		}

		Snapshot.SaveDataVersion = TargetVersion;
		Snapshot.SaveGameMigrations = Previous ? Previous->SaveGameMigrations : TArray<FRPGIdMigrationStep>();
		if (TargetVersion == PreviousVersion + 1)
		{
			Snapshot.SaveGameMigrations.Add(FRPGIdMigrationStep(PreviousVersion, TargetVersion, TArray<FRPGIdRedirect>(Redirects)));
		}
		else if (!Redirects.IsEmpty())
		{
			return Fail(Error, TEXT("RPG Id redirects require advancing SaveDataVersion by exactly one."));
		}
		return true;
	}

	bool RedirectsEqual(TConstArrayView<FRPGIdRedirect> Left, TConstArrayView<FRPGIdRedirect> Right)
	{
		if (Left.Num() != Right.Num()) { return false; }
		for (const FRPGIdRedirect& Redirect : Left)
		{
			if (!Right.ContainsByPredicate([&](const FRPGIdRedirect& Candidate)
			{
				return Candidate.OldId == Redirect.OldId && Candidate.NewId == Redirect.NewId;
			}))
			{
				return false;
			}
		}
		return true;
	}

	bool MigrationStepsEqual(const FRPGIdMigrationStep& Left, const FRPGIdMigrationStep& Right)
	{
		return Left.FromVersion == Right.FromVersion && Left.ToVersion == Right.ToVersion
			&& RedirectsEqual(Left.Redirects, Right.Redirects);
	}

	bool ValidateMigrationLineage(const FRPGReleaseSnapshot& Previous, const FRPGReleaseSnapshot& Current, FText& Error)
	{
		if (Current.SaveDataVersion < Previous.SaveDataVersion || Current.SaveDataVersion > Previous.SaveDataVersion + 1)
		{
			return Fail(Error, TEXT("Release SaveDataVersion must remain unchanged or advance by exactly one."));
		}
		for (const FRPGIdMigrationStep& PreviousStep : Previous.SaveGameMigrations)
		{
			const FRPGIdMigrationStep* CurrentStep = Current.SaveGameMigrations.FindByPredicate(
				[&](const FRPGIdMigrationStep& Step) { return Step.FromVersion == PreviousStep.FromVersion; });
			if (!CurrentStep || !MigrationStepsEqual(PreviousStep, *CurrentStep))
			{
				return Fail(Error, TEXT("A committed save migration step was removed or rewritten."));
			}
		}
		const int32 ExpectedSteps = Previous.SaveGameMigrations.Num()
			+ (Current.SaveDataVersion == Previous.SaveDataVersion + 1 ? 1 : 0);
		if (Current.SaveGameMigrations.Num() != ExpectedSteps)
		{
			return Fail(Error, TEXT("Release migration catalog does not match its SaveDataVersion transition."));
		}
		if (Current.SaveDataVersion == Previous.SaveDataVersion)
		{
			return true;
		}

		const FRPGIdMigrationStep* NewStep = Current.SaveGameMigrations.FindByPredicate(
			[&](const FRPGIdMigrationStep& Step) { return Step.FromVersion == Previous.SaveDataVersion; });
		if (!NewStep || NewStep->ToVersion != Current.SaveDataVersion)
		{
			return Fail(Error, TEXT("The new release is missing its adjacent save migration step."));
		}
		TSet<FName> PreviousIds;
		TSet<FName> CurrentIds;
		for (const FRPGReleaseClaim& Claim : Previous.Claims) { PreviousIds.Add(Claim.Id); }
		for (const FRPGReleaseClaim& Claim : Current.Claims) { CurrentIds.Add(Claim.Id); }
		for (const FRPGIdRedirect& Redirect : NewStep->Redirects)
		{
			if (!PreviousIds.Contains(Redirect.OldId.Id) || !CurrentIds.Contains(Redirect.NewId.Id)
				|| CurrentIds.Contains(Redirect.OldId.Id))
			{
				return Fail(Error, TEXT("A redirect source must be released previously, retired now and map to a current Claim."));
			}
		}
		return true;
	}
}

bool FRPGReleaseSealService::EvaluateHistory(FRPGReleaseHistory& History, const FString& HeadPath, const FString& HeadHash, FText& Error)
{
	History.ReservedIds.Reset();
	History.CurrentIds.Reset();
	History.RetiredIds.Reset();
	History.PreparedIndex = INDEX_NONE;
	TMap<FString, int32> ByHash;
	TSet<FString> Releases;
	for (int32 Index = 0; Index < History.Entries.Num(); ++Index)
	{
		const FRPGReleaseHistoryEntry& Entry = History.Entries[Index];
		if (!Entry.Snapshot.Validate(Error)) { return false; }
		if (Entry.Hash != Entry.Snapshot.ComputeHash() || ByHash.Contains(Entry.Hash) || Releases.Contains(Entry.Snapshot.ReleaseId))
		{
			return Fail(Error, TEXT("Release history has a hash mismatch or duplicate release."));
		}
		ByHash.Add(Entry.Hash, Index);
		Releases.Add(Entry.Snapshot.ReleaseId);
		for (const FRPGReleaseClaim& Claim : Entry.Snapshot.Claims) { History.ReservedIds.Add(Claim.Id); }
	}
	for (const FRPGReleaseHistoryEntry& Entry : History.Entries)
	{
		if (Entry.Snapshot.PreviousHash.IsEmpty()) { continue; }
		const int32* PreviousIndex = ByHash.Find(Entry.Snapshot.PreviousHash);
		if (!PreviousIndex)
		{
			return Fail(Error, TEXT("Release history references a missing previous manifest."));
		}
		if (!ValidateMigrationLineage(History.Entries[*PreviousIndex].Snapshot, Entry.Snapshot, Error))
		{
			return false;
		}
	}
	if (HeadPath.IsEmpty() != HeadHash.IsEmpty()) { return Fail(Error, TEXT("Release head path/hash are inconsistent.")); }
	FString Cursor = HeadHash;
	TSet<int32> Visited;
	while (!Cursor.IsEmpty())
	{
		const int32* Index = ByHash.Find(Cursor);
		if (!Index || Visited.Contains(*Index)) { return Fail(Error, TEXT("Release head chain is missing or cyclic.")); }
		const FRPGReleaseHistoryEntry& Entry = History.Entries[*Index];
		if (Visited.IsEmpty() && Entry.Path.ToString() != HeadPath) { return Fail(Error, TEXT("Release head points to a different manifest.")); }
		Visited.Add(*Index);
		Cursor = Entry.Snapshot.PreviousHash;
	}
	for (int32 Index = 0; Index < History.Entries.Num(); ++Index)
	{
		if (Visited.Contains(Index)) { continue; }
		if (History.PreparedIndex != INDEX_NONE || History.Entries[Index].Snapshot.PreviousHash != HeadHash)
		{
			return Fail(Error, TEXT("Release history contains a fork or orphan. Restore the canonical chain from source control."));
		}
		History.PreparedIndex = Index;
	}
	const FRPGReleaseHistoryEntry* Tip = History.Entries.IsValidIndex(History.PreparedIndex)
		? &History.Entries[History.PreparedIndex]
		: History.Entries.FindByPredicate([&](const FRPGReleaseHistoryEntry& Entry) { return Entry.Hash == HeadHash; });
	if (Tip)
	{
		for (const FRPGReleaseClaim& Claim : Tip->Snapshot.Claims) { History.CurrentIds.Add(Claim.Id); }
		for (const FRPGIdMigrationStep& Step : Tip->Snapshot.SaveGameMigrations)
		{
			for (const FRPGIdRedirect& Redirect : Step.Redirects) { History.RetiredIds.Add(Redirect.OldId.Id); }
		}
	}
	return true;
}

bool FRPGReleaseSealService::ReadHistory(FRPGReleaseHistory& History, FText& Error)
{
	History = {};
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	if (Registry.IsLoadingAssets() || !Registry.IsSearchAllAssets()) { return Fail(Error, TEXT("Wait for complete asset discovery before checking releases.")); }
	FString HeadPath, HeadHash;
	if (!ReadHeadFile(HeadPath, HeadHash, Error)) { return false; }
	const URPGReleaseSettings* Settings = GetDefault<URPGReleaseSettings>();
	if (Settings->GetHeadManifest() != HeadPath || Settings->GetHeadHash() != HeadHash)
	{
		return Fail(Error, TEXT("In-memory release settings differ from the source-controlled head. Restart after resolving config changes."));
	}
	FARFilter Filter;
	Filter.ClassPaths.Add(URPGReleaseManifest::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	TArray<FAssetData> Assets;
	if (!Registry.GetAssets(Filter, Assets)) { return Fail(Error, TEXT("Manifest discovery failed.")); }
	TSet<FSoftObjectPath> Seen;
	auto Add = [&](URPGReleaseManifest* Manifest)
	{
		if (!Manifest || !Manifest->IsAsset() || Manifest->IsTemplate()) { return Fail(Error, TEXT("Cannot load release manifest.")); }
		const FSoftObjectPath Path(Manifest);
		if (Seen.Contains(Path)) { return true; }
		Seen.Add(Path);
		if (Manifest->GetOutermost()->IsDirty() || !FPackageName::DoesPackageExist(Path.GetLongPackageName()))
		{
			return Fail(Error, TEXT("Manifest is dirty or unsaved: ") + Path.ToString());
		}
		History.Entries.Add({ Path, Manifest->GetSnapshot(), Manifest->GetSemanticHash() });
		return true;
	};
	for (const FAssetData& Data : Assets)
	{
		if (!Add(Cast<URPGReleaseManifest>(Data.GetAsset()))) { return false; }
	}
	for (TObjectIterator<URPGReleaseManifest> It; It; ++It)
	{
		if (It->IsAsset() && !It->IsTemplate() && !It->HasAnyFlags(RF_Transient) && !Add(*It)) { return false; }
	}
	return EvaluateHistory(History, HeadPath, HeadHash, Error);
}

bool FRPGReleaseSealService::CheckMutation(const FRPGId& Previous, const FRPGId& Candidate, FText& Error)
{
	FRPGReleaseHistory History;
	if (!ReadHistory(History, Error)) { return false; }
	if (Previous != Candidate && (History.ReservedIds.Contains(Previous.Id) || History.ReservedIds.Contains(Candidate.Id)))
	{
		return Fail(Error, TEXT("This Id is sealed or reserved by a prepared release. Identity mutation requires versioned migration support."));
	}
	return true;
}

bool FRPGReleaseSealService::ValidateHistory(FText& Error)
{
	FRPGReleaseHistory History;
	if (!ReadHistory(History, Error) || !CheckOwners(History, Error)) { return false; }
	if (!bSealing && History.PreparedIndex != INDEX_NONE) { return Fail(Error, TEXT("A prepared release requires explicit Resume or Abort before release validation.")); }
	return true;
}

bool FRPGReleaseSealService::CanDelete(const TArray<UObject*>& Objects, FText& Error)
{
	if (bAborting) { return true; }
	bool bHasOwner = false;
	for (UObject* Object : Objects)
	{
		if (Cast<URPGReleaseManifest>(Object)) { return Fail(Error, TEXT("Release manifests are immutable. Only a prepared manifest can be aborted by the Seal service.")); }
		bHasOwner |= Cast<URPGPrimaryAsset>(Object) != nullptr;
	}
	if (!bHasOwner) { return true; }
	FRPGReleaseHistory History;
	if (!ReadHistory(History, Error)) { return false; }
	for (UObject* Object : Objects)
	{
		const URPGPrimaryAsset* Owner = Cast<URPGPrimaryAsset>(Object);
		if (Owner && History.ReservedIds.Contains(Owner->GetId().Id)) { return Fail(Error, TEXT("Cannot delete a sealed/prepared RPG Id owner.")); }
	}
	return true;
}

void FRPGReleaseSealService::Register()
{
	DeleteHandle = FEditorDelegates::OnAssetsCanDelete.AddLambda([](const TArray<UObject*>& Objects, FCanDeleteAssetResult& Result)
	{
		FText Error;
		if (!CanDelete(Objects, Error))
		{
			Result.Set(false);
			UE_LOG(LogTemp, Warning, TEXT("RPG Release: %s"), *Error.ToString());
		}
	});
}

void FRPGReleaseSealService::Unregister()
{
	FEditorDelegates::OnAssetsCanDelete.Remove(DeleteHandle);
}

bool FRPGReleaseSealService::CommitHeadFile(const FString& Filename, const FString& HeadPath, const FString& HeadHash, FText& Error)
{
	FString Contents;
	IFileManager& Files = IFileManager::Get();
	if (Files.FileExists(*Filename) && (Files.IsReadOnly(*Filename) || !FFileHelper::LoadFileToString(Contents, *Filename)))
	{
		return Fail(Error, TEXT("Release settings must be readable and writable: ") + Filename);
	}
	FConfigFile Config;
	Config.ProcessInputFileContents(Contents, Filename);
	Config.SetString(TEXT("/Script/RPGCore.RPGReleaseSettings"), TEXT("HeadManifest"), *HeadPath);
	Config.SetString(TEXT("/Script/RPGCore.RPGReleaseSettings"), TEXT("HeadHash"), *HeadHash);
	const FString Temporary = Filename + TEXT(".seal-") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
	if (!Config.Write(Temporary))
	{
		Files.Delete(*Temporary);
		return Fail(Error, TEXT("Cannot prepare the release head config."));
	}
	FConfigFile Verify;
	Verify.Read(Temporary);
	FString SavedPath, SavedHash;
	Verify.GetString(TEXT("/Script/RPGCore.RPGReleaseSettings"), TEXT("HeadManifest"), SavedPath);
	Verify.GetString(TEXT("/Script/RPGCore.RPGReleaseSettings"), TEXT("HeadHash"), SavedHash);
	if (SavedPath != HeadPath || SavedHash != HeadHash || !ReplaceHeadFile(Filename, Temporary))
	{
		Files.Delete(*Temporary);
		return Fail(Error, TEXT("Head commit failed. Manifest remains prepared; resolve config/source control and Resume."));
	}
	return true;
}

bool FRPGReleaseSealService::CommitHead(URPGReleaseManifest& Manifest, FText& Error)
{
	FRPGReleaseHistory History;
	if (!ReadHistory(History, Error) || !CheckOwners(History, Error) || !IsClean(Error)) { return false; }
	if (!History.Entries.IsValidIndex(History.PreparedIndex)
		|| History.Entries[History.PreparedIndex].Path != FSoftObjectPath(&Manifest))
	{
		return Fail(Error, TEXT("The prepared manifest or release head changed before commit. Recheck the release."));
	}
	URPGReleaseSettings* Settings = GetMutableDefault<URPGReleaseSettings>();
	const FString Path = FSoftObjectPath(&Manifest).ToString();
	if (!CommitHeadFile(Settings->GetDefaultConfigFilename(), Path, Manifest.GetSemanticHash(), Error)) { return false; }
	Settings->HeadManifest = Path;
	Settings->HeadHash = Manifest.GetSemanticHash();
	if (!IsRunningCommandlet()) { GEditor->ResetTransaction(FText::FromString(TEXT("RPG release sealed"))); }
	Error = FText::FromString(TEXT("Release sealed. Commit the manifest and release head together in source control."));
	return true;
}

URPGReleaseManifest* FRPGReleaseSealService::SavePrepared(const FRPGReleaseSnapshot& Snapshot, const FString& PackageName, FText& Message)
{
	if (!Snapshot.Validate(Message)) { return nullptr; }
	if (FPackageName::DoesPackageExist(PackageName) || FindPackage(nullptr, *PackageName))
	{
		Fail(Message, TEXT("Manifest destination already exists; choose a different ReleaseId."));
		return nullptr;
	}
	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	UPackage* Package = CreatePackage(*PackageName);
	URPGReleaseManifest* Manifest = NewObject<URPGReleaseManifest>(Package, *FPackageName::GetLongPackageAssetName(PackageName), RF_Public | RF_Standalone);
	Manifest->Snapshot = Snapshot;
	Manifest->SemanticHash = Snapshot.ComputeHash();
	Manifest->CreatedAt = FDateTime::UtcNow();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, Manifest, *Filename, Args))
	{
		Manifest->ClearFlags(RF_Public | RF_Standalone);
		Manifest->SetFlags(RF_Transient);
		Fail(Message, TEXT("Manifest save failed. Head unchanged; inspect the destination before retrying."));
		return nullptr;
	}
	FAssetRegistryModule::AssetCreated(Manifest);
	return Manifest;
}

bool FRPGReleaseSealService::Run(ERPGReleaseSealAction Action, const FString& ReleaseId, const FString& SaveVersion, FText& Message)
{
	return Run(Action, ReleaseId, SaveVersion, TConstArrayView<FRPGIdRedirect>(), Message);
}

bool FRPGReleaseSealService::Run(ERPGReleaseSealAction Action, const FString& ReleaseId, const FString& SaveVersion,
	TConstArrayView<FRPGIdRedirect> Redirects, FText& Message)
{
	if (!IsInGameThread() || !GEditor || GEditor->PlayWorld || GIsPlayInEditorWorld || IsRunningCookCommandlet() || bSealing)
	{
		return Fail(Message, TEXT("Seal is unavailable during play, cook or another Seal operation."));
	}
	TGuardValue<bool> Guard(bSealing, true);
	FRPGReleaseHistory History;
	if (!ReadHistory(History, Message)) { return false; }
	if (!FRPGReleaseSnapshot::IsValidReleaseId(ReleaseId)) { return Fail(Message, TEXT("Enter a valid ReleaseId: 1-64 lowercase ASCII letters, digits, '-' or '_'.")); }
	const FRPGReleaseHistoryEntry* Prepared = History.Entries.IsValidIndex(History.PreparedIndex) ? &History.Entries[History.PreparedIndex] : nullptr;
	if (Action == ERPGReleaseSealAction::Abort)
	{
		if (!Prepared || Prepared->Snapshot.ReleaseId != ReleaseId) { return Fail(Message, TEXT("Abort requires the exact prepared ReleaseId.")); }
		URPGReleaseManifest* Manifest = Cast<URPGReleaseManifest>(Prepared->Path.TryLoad());
		if (!Manifest) { return Fail(Message, TEXT("Prepared manifest could not be loaded.")); }
		TGuardValue<bool> AbortGuard(bAborting, true);
		const FString Filename = FPackageName::LongPackageNameToFilename(Prepared->Path.GetLongPackageName(), FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().IsReadOnly(*Filename)) { return Fail(Message, TEXT("Make the prepared manifest writable before Abort.")); }
		if (ObjectTools::DeleteObjectsUnchecked({ Manifest }) != 1 || IFileManager::Get().FileExists(*Filename))
		{
			return Fail(Message, TEXT("Prepared manifest cleanup failed. Inspect the package/source control before retrying."));
		}
		Message = FText::FromString(TEXT("Prepared manifest removed; committed history is unchanged. Recover through source control if needed."));
		return true;
	}
	if (!CheckOwners(History, Message)) { return false; }
	if (Prepared && (Action != ERPGReleaseSealAction::Resume || Prepared->Snapshot.ReleaseId != ReleaseId))
	{
		return Fail(Message, TEXT("A prepared release exists. Use Resume or Abort with its exact ReleaseId: ") + Prepared->Snapshot.ReleaseId);
	}
	if (!Prepared && Action == ERPGReleaseSealAction::Resume) { return Fail(Message, TEXT("No prepared release to resume.")); }
	FRPGReleaseSnapshot Snapshot;
	Snapshot.ReleaseId = ReleaseId;
	Snapshot.PreviousHash = GetDefault<URPGReleaseSettings>()->GetHeadHash();
	const FRPGReleaseHistoryEntry* Existing = History.Entries.FindByPredicate([&](const FRPGReleaseHistoryEntry& Entry)
	{
		return Entry.Snapshot.ReleaseId == ReleaseId;
	});
	if (Existing) { Snapshot.PreviousHash = Existing->Snapshot.PreviousHash; }
	const FRPGReleaseHistoryEntry* Previous = History.Entries.FindByPredicate([&](const FRPGReleaseHistoryEntry& Entry)
	{
		return Entry.Hash == Snapshot.PreviousHash;
	});
	if (Existing)
	{
		Snapshot.SaveDataVersion = Existing->Snapshot.SaveDataVersion;
		Snapshot.SaveGameMigrations = Existing->Snapshot.SaveGameMigrations;
		uint16 RequestedVersion = Snapshot.SaveDataVersion;
		if ((!SaveVersion.IsEmpty() && (!LexTryParseString(RequestedVersion, *SaveVersion)
			|| RequestedVersion != Snapshot.SaveDataVersion)) || !Redirects.IsEmpty())
		{
			return Fail(Message, TEXT("An existing ReleaseId must be resumed or repeated with its original migration catalog."));
		}
	}
	else if (!PrepareSaveMigration(Snapshot, Previous ? &Previous->Snapshot : nullptr, SaveVersion, Redirects, Message))
	{
		return false;
	}
	if (!CollectSnapshot(Snapshot, Message)) { return false; }
	if (Previous && !ValidateMigrationLineage(Previous->Snapshot, Snapshot, Message)) { return false; }
	const FString Hash = Snapshot.ComputeHash();
	if (Existing)
	{
		if (Existing->Hash != Hash) { return Fail(Message, TEXT("ReleaseId already exists with a different snapshot. Existing history cannot be overwritten.")); }
		if (Prepared) { return CommitHead(*CastChecked<URPGReleaseManifest>(Prepared->Path.TryLoad()), Message); }
		Message = FText::FromString(TEXT("Identical release already committed; no changes."));
		return true;
	}
	if (Action == ERPGReleaseSealAction::Preview)
	{
		Message = FText::FromString(FString::Printf(TEXT("Ready to seal %s: %d Claims. Hash %s. Seal repeats all checks and writes manifest then head."),
			*ReleaseId, Snapshot.Claims.Num(), *Hash));
		return true;
	}
	const FString PackageName = ManifestRoot + TEXT("Release_") + ReleaseId;
	URPGReleaseManifest* Manifest = SavePrepared(Snapshot, PackageName, Message);
	return Manifest && CommitHead(*Manifest, Message);
}
