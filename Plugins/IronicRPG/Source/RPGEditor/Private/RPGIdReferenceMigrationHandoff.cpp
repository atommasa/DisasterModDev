// Copyright Ironic Studio. All Rights Reserved.

#include "RPGIdReferenceMigrationHandoff.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

#define LOCTEXT_NAMESPACE "RPGIdReferenceMigrationHandoff"

DEFINE_LOG_CATEGORY_STATIC(LogRPGIdReferenceMigrationHandoff, Log, All);

namespace
{
	constexpr uint32 HandoffMagic = 0x524D4846;
	constexpr uint32 HandoffSchema = 1;
	constexpr int32 MaximumRedirects = 10000;
	TUniquePtr<FRPGIdReferenceMigrationHandoffStore> ProductionStore;

	bool SameRedirect(const FRPGIdRedirect& A, const FRPGIdRedirect& B)
	{
		return A.OldId == B.OldId && A.NewId == B.NewId;
	}

	void SortRedirects(TArray<FRPGIdRedirect>& Redirects)
	{
		Redirects.Sort([](const FRPGIdRedirect& A, const FRPGIdRedirect& B)
		{
			const int32 OldCompare = A.OldId.ToString().Compare(B.OldId.ToString(), ESearchCase::CaseSensitive);
			return OldCompare == 0
				? A.NewId.ToString().Compare(B.NewId.ToString(), ESearchCase::CaseSensitive) < 0
				: OldCompare < 0;
		});
	}

	bool ValidateRedirect(const FRPGIdRedirect& Redirect, FText& OutError)
	{
		if (!Redirect.OldId.IsValid() || !Redirect.NewId.IsValid() || Redirect.OldId == Redirect.NewId
			|| Redirect.OldId.GetIdType() != Redirect.NewId.GetIdType())
		{
			OutError = LOCTEXT("InvalidRedirect", "A pending redirect must contain distinct valid RPG Ids of the same AssetType.");
			return false;
		}
		return true;
	}

	bool ValidateSet(const TArray<FRPGIdRedirect>& Redirects, FText& OutError)
	{
		if (Redirects.Num() > MaximumRedirects)
		{
			OutError = LOCTEXT("TooManyRedirects", "The pending redirect handoff exceeds its safety limit.");
			return false;
		}
		for (int32 Index = 0; Index < Redirects.Num(); ++Index)
		{
			if (!ValidateRedirect(Redirects[Index], OutError))
			{
				return false;
			}
			for (int32 OtherIndex = Index + 1; OtherIndex < Redirects.Num(); ++OtherIndex)
			{
				const FRPGIdRedirect& A = Redirects[Index];
				const FRPGIdRedirect& B = Redirects[OtherIndex];
				if (A.OldId == B.OldId || A.NewId == B.NewId)
				{
					OutError = LOCTEXT("ConflictingRedirects",
						"Pending redirects cannot reuse a source or target Id before the next Release Seal.");
					return false;
				}
			}
		}
		return true;
	}

	bool SaveBytesAtomically(const FString& Path, const TArray<uint8>& Bytes, FText& OutError)
	{
		IFileManager& Files = IFileManager::Get();
		if (!Files.MakeDirectory(*FPaths::GetPath(Path), true))
		{
			OutError = LOCTEXT("CreateDirectory", "The pending redirect directory could not be created.");
			return false;
		}
		const FString Temporary = Path + TEXT(".tmp");
		if (!FFileHelper::SaveArrayToFile(Bytes, *Temporary))
		{
			OutError = LOCTEXT("WriteTemporary", "The pending redirect handoff could not be written.");
			return false;
		}
		if (!Files.Move(*Path, *Temporary, true, false, false, true))
		{
			Files.Delete(*Temporary, false, true);
			OutError = LOCTEXT("ReplaceHandoff", "The pending redirect handoff could not be replaced atomically.");
			return false;
		}
		return true;
	}

	FRPGIdReferenceMigrationHandoffStore* GetProductionStore(FText& OutError)
	{
		if (!ProductionStore)
		{
			OutError = LOCTEXT("NotRegistered", "The pending redirect handoff is unavailable.");
			return nullptr;
		}
		return ProductionStore.Get();
	}
}

bool URPGIdReferenceMigrationHandoffState::Save(FText& OutError) const
{
	FBufferArchive Archive;
	uint32 Magic = HandoffMagic;
	uint32 Schema = HandoffSchema;
	int32 Count = Redirects.Num();
	Archive << Magic;
	Archive << Schema;
	Archive << Count;
	for (const FRPGIdRedirect& Redirect : Redirects)
	{
		FString OldId = Redirect.OldId.ToString();
		FString NewId = Redirect.NewId.ToString();
		Archive << OldId;
		Archive << NewId;
	}
	return !Archive.IsError() && SaveBytesAtomically(StoragePath, Archive, OutError);
}

void URPGIdReferenceMigrationHandoffState::PostEditUndo()
{
	Super::PostEditUndo();
	FText Error;
	if (!Save(Error))
	{
		UE_LOG(LogRPGIdReferenceMigrationHandoff, Error, TEXT("%s"), *Error.ToString());
	}
}

FRPGIdReferenceMigrationHandoffStore::FRPGIdReferenceMigrationHandoffStore(FString InStoragePath)
{
	State.Reset(NewObject<URPGIdReferenceMigrationHandoffState>(GetTransientPackage(), NAME_None, RF_Transactional));
	State->StoragePath = FPaths::ConvertRelativePathToFull(MoveTemp(InStoragePath));
	Load(InitializationError);
}

bool FRPGIdReferenceMigrationHandoffStore::Load(FText& OutError)
{
	State->Redirects.Reset();
	if (!IFileManager::Get().FileExists(*State->StoragePath))
	{
		return true;
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *State->StoragePath))
	{
		OutError = LOCTEXT("ReadHandoff", "The pending redirect handoff could not be read.");
		return false;
	}
	FMemoryReader Archive(Bytes);
	uint32 Magic = 0;
	uint32 Schema = 0;
	int32 Count = 0;
	Archive << Magic;
	Archive << Schema;
	Archive << Count;
	if (Archive.IsError() || Magic != HandoffMagic || Schema != HandoffSchema || Count < 0 || Count > MaximumRedirects)
	{
		OutError = LOCTEXT("InvalidHeader", "The pending redirect handoff is malformed or uses an unsupported schema.");
		return false;
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FString OldId;
		FString NewId;
		Archive << OldId;
		Archive << NewId;
		if (Archive.IsError())
		{
			OutError = LOCTEXT("TruncatedHandoff", "The pending redirect handoff is truncated.");
			State->Redirects.Reset();
			return false;
		}
		State->Redirects.Emplace(FRPGId(FName(*OldId)), FRPGId(FName(*NewId)));
	}
	if (!Archive.AtEnd() || !ValidateSet(State->Redirects, OutError))
	{
		State->Redirects.Reset();
		OutError = OutError.IsEmpty()
			? LOCTEXT("TrailingHandoff", "The pending redirect handoff contains unexpected trailing data.")
			: OutError;
		return false;
	}
	SortRedirects(State->Redirects);
	return true;
}

bool FRPGIdReferenceMigrationHandoffStore::ReadPending(TArray<FRPGIdRedirect>& OutRedirects, FText& OutError) const
{
	OutRedirects.Reset();
	if (!InitializationError.IsEmpty())
	{
		OutError = InitializationError;
		return false;
	}
	OutRedirects = State->Redirects;
	return true;
}

bool FRPGIdReferenceMigrationHandoffStore::Stage(const FRPGIdRedirect& Redirect, bool& OutAdded, FText& OutError)
{
	OutAdded = false;
	if (!InitializationError.IsEmpty())
	{
		OutError = InitializationError;
		return false;
	}
	if (!ValidateRedirect(Redirect, OutError))
	{
		return false;
	}
	if (State->Redirects.ContainsByPredicate([&Redirect](const FRPGIdRedirect& Entry) { return SameRedirect(Entry, Redirect); }))
	{
		return true;
	}

	TArray<FRPGIdRedirect> Candidate = State->Redirects;
	Candidate.Add(Redirect);
	if (!ValidateSet(Candidate, OutError))
	{
		return false;
	}

	State->Modify();
	State->Redirects = MoveTemp(Candidate);
	SortRedirects(State->Redirects);
	if (!State->Save(OutError))
	{
		State->Redirects.RemoveAll([&Redirect](const FRPGIdRedirect& Entry) { return SameRedirect(Entry, Redirect); });
		FText RestoreError;
		State->Save(RestoreError);
		return false;
	}
	OutAdded = true;
	return true;
}

bool FRPGIdReferenceMigrationHandoffStore::RollbackStage(const FRPGIdRedirect& Redirect, const bool bWasAdded, FText& OutError)
{
	if (!bWasAdded)
	{
		return true;
	}
	const int32 Removed = State->Redirects.RemoveAll([&Redirect](const FRPGIdRedirect& Entry)
	{
		return SameRedirect(Entry, Redirect);
	});
	if (Removed != 1 || !State->Save(OutError))
	{
		OutError = OutError.IsEmpty()
			? LOCTEXT("RollbackMissing", "The staged pending redirect could not be rolled back exactly.")
			: OutError;
		return false;
	}
	return true;
}

bool FRPGIdReferenceMigrationHandoffStore::Consume(TConstArrayView<FRPGIdRedirect> Redirects, FText& OutError)
{
	if (!InitializationError.IsEmpty())
	{
		OutError = InitializationError;
		return false;
	}
	TArray<FRPGIdRedirect> Remaining = State->Redirects;
	for (const FRPGIdRedirect& Redirect : Redirects)
	{
		if (Remaining.RemoveAll([&Redirect](const FRPGIdRedirect& Entry) { return SameRedirect(Entry, Redirect); }) != 1)
		{
			OutError = LOCTEXT("ConsumeMismatch", "The sealed redirects do not exactly match the pending handoff.");
			return false;
		}
	}
	const TArray<FRPGIdRedirect> Original = State->Redirects;
	State->Redirects = MoveTemp(Remaining);
	if (!State->Save(OutError))
	{
		State->Redirects = Original;
		return false;
	}
	return true;
}

void FRPGIdReferenceMigrationHandoff::Register()
{
	if (!ProductionStore)
	{
		ProductionStore = MakeUnique<FRPGIdReferenceMigrationHandoffStore>(GetStoragePath());
	}
}

void FRPGIdReferenceMigrationHandoff::Unregister()
{
	ProductionStore.Reset();
}

bool FRPGIdReferenceMigrationHandoff::ReadPending(TArray<FRPGIdRedirect>& OutRedirects, FText& OutError)
{
	FRPGIdReferenceMigrationHandoffStore* Store = GetProductionStore(OutError);
	return Store && Store->ReadPending(OutRedirects, OutError);
}

bool FRPGIdReferenceMigrationHandoff::Stage(const FRPGIdRedirect& Redirect, bool& OutAdded, FText& OutError)
{
	FRPGIdReferenceMigrationHandoffStore* Store = GetProductionStore(OutError);
	return Store && Store->Stage(Redirect, OutAdded, OutError);
}

bool FRPGIdReferenceMigrationHandoff::RollbackStage(const FRPGIdRedirect& Redirect, const bool bWasAdded, FText& OutError)
{
	FRPGIdReferenceMigrationHandoffStore* Store = GetProductionStore(OutError);
	return Store && Store->RollbackStage(Redirect, bWasAdded, OutError);
}

bool FRPGIdReferenceMigrationHandoff::Consume(TConstArrayView<FRPGIdRedirect> Redirects, FText& OutError)
{
	FRPGIdReferenceMigrationHandoffStore* Store = GetProductionStore(OutError);
	return Store && Store->Consume(Redirects, OutError);
}

FString FRPGIdReferenceMigrationHandoff::GetStoragePath()
{
	return FPaths::ProjectSavedDir() / TEXT("IronicRPG/RPGIdReferenceMigrationPending.bin");
}

#undef LOCTEXT_NAMESPACE
