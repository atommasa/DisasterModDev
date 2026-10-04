// Copyright Ironic Studio. All Rights Reserved.

#include "RPGIdBlueprintReferenceIndex.h"

#include "Algo/Sort.h"
#include "Algo/Unique.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "HAL/PlatformProperties.h"
#include "HAL/FileManager.h"
#include "IO/IoHash.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogRPGIdReferenceIndex, Log, All);

namespace
{
	constexpr uint32 CacheMagic = 0x52494749;
	constexpr uint32 CacheSchemaVersion = 1;
	constexpr uint32 FingerprintPolicyVersion = 2;
	constexpr int64 MaxCacheBytes = 256ll * 1024ll * 1024ll;
	constexpr int32 MaxCacheRecords = 4 * 1024 * 1024;

	void SerializeString(FArchive& Archive, FString& Value)
	{
		Archive << Value;
	}

	void SerializeName(FArchive& Archive, FName& Value)
	{
		FString Text = Archive.IsSaving() ? Value.ToString() : FString();
		Archive << Text;
		if (Archive.IsLoading())
		{
			Value = FName(*Text);
		}
	}

	template <typename ElementType, typename SerializerType>
	bool SerializeArray(FArchive& Archive, TArray<ElementType>& Values, SerializerType&& Serializer)
	{
		int32 Count = Values.Num();
		Archive << Count;
		if (Archive.IsLoading())
		{
			if (Count < 0 || Count > MaxCacheRecords)
			{
				Archive.SetError();
				return false;
			}
			Values.SetNum(Count);
		}
		for (ElementType& Value : Values)
		{
			Serializer(Archive, Value);
			if (Archive.IsError())
			{
				return false;
			}
		}
		return true;
	}

	void SerializeReference(FArchive& Archive, RPGIdReferencePrivate::FRPGIdReferenceRecord& Reference)
	{
		FString Id = Archive.IsSaving() ? Reference.Id.ToString() : FString();
		Archive << Id;
		Archive << Reference.Source;
		Archive << Reference.PropertyPath;
		if (Archive.IsLoading())
		{
			Reference.Id = FRPGId(FName(*Id));
		}
	}

	void SerializeIssue(FArchive& Archive, RPGIdReferencePrivate::FRPGIdReferenceIssue& Issue)
	{
		uint8 Code = static_cast<uint8>(Issue.Code);
		Archive << Code;
		Archive << Issue.Source;
		Archive << Issue.Detail;
		if (Archive.IsLoading())
		{
			if (Code > static_cast<uint8>(RPGIdReferencePrivate::ERPGIdReferenceIssueCode::MalformedBlueprintGraphPin))
			{
				Archive.SetError();
				return;
			}
			Issue.Code = static_cast<RPGIdReferencePrivate::ERPGIdReferenceIssueCode>(Code);
		}
	}

	void SerializeEntry(FArchive& Archive, RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexEntry& Entry)
	{
		SerializeName(Archive, Entry.PackageName);
		Archive << Entry.AssetPath;
		SerializeArray(Archive, Entry.Evidence.References, SerializeReference);
		SerializeArray(Archive, Entry.Evidence.Issues, SerializeIssue);
	}

	void SerializeCachePayload(FArchive& Archive, RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexCache& Cache)
	{
		Archive << Cache.WorkspaceFingerprint;
		int64 BuildTimeTicks = Archive.IsSaving() ? Cache.BuildTime.GetTicks() : 0;
		Archive << BuildTimeTicks;
		if (Archive.IsLoading())
		{
			Cache.BuildTime = FDateTime(BuildTimeTicks);
		}
		SerializeArray(Archive, Cache.NativeModules, [](FArchive& InnerArchive, FName& ModuleName)
		{
			SerializeName(InnerArchive, ModuleName);
		});
		SerializeArray(Archive, Cache.Entries, SerializeEntry);
	}

	void CanonicalizeEntries(TArray<RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexEntry>& Entries)
	{
		Entries.Sort([](const RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexEntry& A,
			const RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexEntry& B)
		{
			return A.PackageName.LexicalLess(B.PackageName);
		});
		for (RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexEntry& Entry : Entries)
		{
			Entry.Evidence.References.Sort([](const RPGIdReferencePrivate::FRPGIdReferenceRecord& A,
				const RPGIdReferencePrivate::FRPGIdReferenceRecord& B)
			{
				if (A.Source != B.Source)
				{
					return A.Source < B.Source;
				}
				if (A.PropertyPath != B.PropertyPath)
				{
					return A.PropertyPath < B.PropertyPath;
				}
				return A.Id.ToString() < B.Id.ToString();
			});
			Entry.Evidence.Issues.Sort([](const RPGIdReferencePrivate::FRPGIdReferenceIssue& A,
				const RPGIdReferencePrivate::FRPGIdReferenceIssue& B)
			{
				if (A.Source != B.Source)
				{
					return A.Source < B.Source;
				}
				if (A.Detail != B.Detail)
				{
					return A.Detail < B.Detail;
				}
				return static_cast<uint8>(A.Code) < static_cast<uint8>(B.Code);
			});
		}
	}

	void CanonicalizeCache(RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexCache& Cache)
	{
		Cache.NativeModules.Sort(FNameLexicalLess());
		Cache.NativeModules.SetNum(Algo::Unique(Cache.NativeModules));
		CanonicalizeEntries(Cache.Entries);
	}

	bool ReferencesEqual(const RPGIdReferencePrivate::FRPGIdReferenceRecord& A,
		const RPGIdReferencePrivate::FRPGIdReferenceRecord& B)
	{
		return A.Id == B.Id && A.Source == B.Source && A.PropertyPath == B.PropertyPath;
	}

	bool IssuesEqual(const RPGIdReferencePrivate::FRPGIdReferenceIssue& A, const RPGIdReferencePrivate::FRPGIdReferenceIssue& B)
	{
		return A.Code == B.Code && A.Source == B.Source && A.Detail == B.Detail;
	}

	bool BuildFileBytes(const RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexCache& SourceCache, TArray<uint8>& OutBytes,
		FString& OutError)
	{
		RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexCache Cache = SourceCache;
		CanonicalizeCache(Cache);
		TArray<uint8> Payload;
		FMemoryWriter PayloadWriter(Payload, true);
		SerializeCachePayload(PayloadWriter, Cache);
		PayloadWriter.Close();
		if (PayloadWriter.IsError())
		{
			OutError = TEXT("Could not serialize the RPG Id reference index payload.");
			return false;
		}

		uint32 Magic = CacheMagic;
		uint32 SchemaVersion = CacheSchemaVersion;
		int64 PayloadSize = Payload.Num();
		FString Checksum = LexToString(FIoHash::HashBuffer(Payload));
		FMemoryWriter Writer(OutBytes, true);
		Writer << Magic;
		Writer << SchemaVersion;
		Writer << PayloadSize;
		Writer << Checksum;
		Writer.Serialize(Payload.GetData(), Payload.Num());
		Writer.Close();
		if (Writer.IsError())
		{
			OutError = TEXT("Could not serialize the RPG Id reference index file.");
			return false;
		}
		return true;
	}

	struct FIndexBuildSnapshot
	{
		RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexFingerprintInput FingerprintInput;
		TArray<FAssetData> Assets;
		TArray<FName> NativeModuleNames;
		FString Fingerprint;
	};

	bool bIndexRegistered = false;
	bool bRefreshQueued = false;
	bool bRebuildQueued = false;
	bool bAsyncLoadInFlight = false;
	uint64 BuildGeneration = 0;
	uint64 LoadedOverlayGeneration = 0;
	FString NativeReloadIdentity;
	int32 ActiveAsyncRequestId = INDEX_NONE;
	RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexStateModel IndexState;
	RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexCache LoadedCache;
	TUniquePtr<RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexBuildAccumulator> BuildAccumulator;
	FIndexBuildSnapshot ActiveSnapshot;
	int32 NextAssetIndex = 0;
	FTSTicker::FDelegateHandle IndexTickerHandle;
	FDelegateHandle AssetAddedHandle;
	FDelegateHandle AssetRemovedHandle;
	FDelegateHandle AssetRenamedHandle;
	FDelegateHandle AssetUpdatedHandle;
	FDelegateHandle PackageSavedHandle;
	FDelegateHandle BlueprintCompiledHandle;
	FDelegateHandle ModulesChangedHandle;
	FDelegateHandle ReloadCompleteHandle;

	bool IsWorkspacePackage(const FName PackageName)
	{
		const FString Name = PackageName.ToString();
		return Name.StartsWith(TEXT("/Game/")) || Name.StartsWith(TEXT("/IronicRPG/"));
	}

	void QueueRegistryRefresh(const FAssetData& AssetData)
	{
		if (IsWorkspacePackage(AssetData.PackageName))
		{
			bRefreshQueued = true;
		}
	}

	bool CaptureWorkspaceSnapshot(FIndexBuildSnapshot& OutSnapshot, FString& OutError)
	{
		OutSnapshot = FIndexBuildSnapshot();
		IAssetRegistry& Registry = IAssetRegistry::GetChecked();
		if (Registry.IsLoadingAssets() || !Registry.IsSearchAllAssets())
		{
			OutError = TEXT("Mounted asset discovery is not complete; the RPG Id reference index cannot be built yet.");
			return false;
		}

		FARFilter Filter;
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		Filter.PackagePaths.Add(FName(TEXT("/Game")));
		Filter.PackagePaths.Add(FName(TEXT("/IronicRPG")));
		Filter.bRecursiveClasses = true;
		Filter.bRecursivePaths = true;
		Filter.bIncludeOnlyOnDiskAssets = true;
		if (!Registry.GetAssets(Filter, OutSnapshot.Assets, false))
		{
			OutError = TEXT("The saved Blueprint catalog query failed.");
			return false;
		}
		OutSnapshot.Assets.Sort([](const FAssetData& A, const FAssetData& B)
		{
			return A.PackageName.LexicalLess(B.PackageName);
		});

		TSet<FName> SeenPackages;
		TSet<FName> NativeModules;
		NativeModules.Add(TEXT("RPGEditor"));
		for (const FAssetData& Asset : OutSnapshot.Assets)
		{
			if (SeenPackages.Contains(Asset.PackageName))
			{
				OutError = FString::Printf(TEXT("Blueprint package %s contains multiple indexed Blueprint assets."),
					*Asset.PackageName.ToString());
				return false;
			}
			SeenPackages.Add(Asset.PackageName);
			const TOptional<FAssetPackageData> PackageData = Registry.GetAssetPackageDataCopy(Asset.PackageName);
			if (!PackageData.IsSet() || PackageData->GetPackageSavedHash().IsZero())
			{
				OutError = FString::Printf(TEXT("Blueprint package %s has no trustworthy saved package hash."),
					*Asset.PackageName.ToString());
				return false;
			}
			OutSnapshot.FingerprintInput.Catalog.Add({ Asset.PackageName, Asset.GetSoftObjectPath().ToString(),
				LexToString(PackageData->GetPackageSavedHash()) });
			if (const UObject* LoadedAsset = Asset.GetSoftObjectPath().ResolveObject())
			{
				if (LoadedAsset->GetOutermost()->GetSavedHash() != PackageData->GetPackageSavedHash())
				{
					OutError = FString::Printf(TEXT("Loaded Blueprint %s differs from its saved package. Reload it or restart the Editor before rebuilding."),
						*Asset.PackageName.ToString());
					return false;
				}
			}

			TArray<FName> Dependencies;
			if (!Registry.GetDependencies(Asset.PackageName, Dependencies, UE::AssetRegistry::EDependencyCategory::Package))
			{
				OutError = FString::Printf(TEXT("Native dependency discovery failed for Blueprint package %s."),
					*Asset.PackageName.ToString());
				return false;
			}
			for (const FName Dependency : Dependencies)
			{
				const FString DependencyName = Dependency.ToString();
				if (DependencyName.StartsWith(TEXT("/Script/")))
				{
					NativeModules.Add(FName(*DependencyName.RightChop(8)));
				}
			}
		}

		OutSnapshot.NativeModuleNames = NativeModules.Array();
		OutSnapshot.NativeModuleNames.Sort(FNameLexicalLess());
		for (const FName ModuleName : OutSnapshot.NativeModuleNames)
		{
			FString ModuleFilename;
			if (!FModuleManager::Get().ModuleExists(*ModuleName.ToString(), &ModuleFilename))
			{
				OutError = FString::Printf(TEXT("Native module %s has no resolvable binary."), *ModuleName.ToString());
				return false;
			}
			FString BinaryHash;
			if (!RPGIdReferenceIndexPrivate::HashNativeBinary(ModuleFilename, BinaryHash, OutError))
			{
				return false;
			}
			OutSnapshot.FingerprintInput.NativeModules.Add({ ModuleName, MoveTemp(BinaryHash) });
		}
		OutSnapshot.FingerprintInput.EngineIdentity = FString::Printf(TEXT("%s|%s|%s"),
			*FEngineVersion::Current().ToString(EVersionComponent::Changelist), FApp::GetBuildVersion(),
			ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName()));
		// Patched code cannot be identified by the base DLL. Never reuse its evidence in another process.
		OutSnapshot.FingerprintInput.EngineIdentity += NativeReloadIdentity;
		OutSnapshot.FingerprintInput.MountRoots = { TEXT("/Game"), TEXT("/IronicRPG") };
		OutSnapshot.Fingerprint = RPGIdReferenceIndexPrivate::BuildWorkspaceFingerprint(OutSnapshot.FingerprintInput);
		return true;
	}

	bool BuilderShouldPause()
	{
		return IsGarbageCollecting() || UE::IsSavingPackage() || (GEditor && GEditor->PlayWorld != nullptr);
	}

	void DiscardActiveBuild(const FString& Reason)
	{
		++BuildGeneration;
		BuildAccumulator.Reset();
		ActiveSnapshot = FIndexBuildSnapshot();
		NextAssetIndex = 0;
		IndexState.Invalidate(Reason);
		bRebuildQueued = true;
	}

	void FinalizeActiveBuild()
	{
		RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexCache Cache;
		FString Error;
		if (!BuildAccumulator || !BuildAccumulator->Finalize(Cache, Error))
		{
			IndexState.FailBuild(Error);
			UE_LOG(LogRPGIdReferenceIndex, Error, TEXT("Index build failed: %s"), *Error);
			BuildAccumulator.Reset();
			return;
		}
		FIndexBuildSnapshot CurrentSnapshot;
		if (!CaptureWorkspaceSnapshot(CurrentSnapshot, Error) || CurrentSnapshot.Fingerprint != ActiveSnapshot.Fingerprint)
		{
			DiscardActiveBuild(Error.IsEmpty()
				? TEXT("The Blueprint catalog or native environment changed while the RPG Id reference index was building.") : Error);
			return;
		}
		CanonicalizeCache(Cache);
		if (!RPGIdReferenceIndexPrivate::SaveCacheAtomically(FRPGIdBlueprintReferenceIndex::GetCachePath(), Cache, Error))
		{
			IndexState.FailBuild(Error);
			UE_LOG(LogRPGIdReferenceIndex, Error, TEXT("Index persistence failed: %s"), *Error);
			BuildAccumulator.Reset();
			return;
		}
		LoadedCache = MoveTemp(Cache);
		IndexState.CompleteBuild();
		UE_LOG(LogRPGIdReferenceIndex, Display, TEXT("Index Ready: %d Blueprint packages persisted to %s."),
			LoadedCache.Entries.Num(), *FRPGIdBlueprintReferenceIndex::GetCachePath());
		BuildAccumulator.Reset();
		ActiveSnapshot = FIndexBuildSnapshot();
		NextAssetIndex = 0;
	}

	void HandleAsyncBlueprintLoaded(const uint64 RequestedGeneration, const FAssetData Asset, const FName& PackageName,
		UPackage* LoadedPackage, const EAsyncLoadingResult::Type Result)
	{
		bAsyncLoadInFlight = false;
		ActiveAsyncRequestId = INDEX_NONE;
		if (!bIndexRegistered || RequestedGeneration != BuildGeneration)
		{
			return;
		}
		if (Result != EAsyncLoadingResult::Succeeded || !IsValid(LoadedPackage))
		{
			IndexState.FailBuild(FString::Printf(TEXT("Blueprint package %s could not be loaded for reference indexing."),
				*PackageName.ToString()));
			BuildAccumulator.Reset();
			return;
		}
		UBlueprint* Blueprint = FindObject<UBlueprint>(nullptr, *Asset.GetSoftObjectPath().ToString());
		if (!IsValid(Blueprint))
		{
			IndexState.FailBuild(FString::Printf(TEXT("Blueprint asset %s could not be resolved after its package loaded."),
				*Asset.GetSoftObjectPath().ToString()));
			BuildAccumulator.Reset();
			return;
		}
		const RPGIdReferencePrivate::FRPGIdBlueprintEvidence Evidence = RPGIdReferencePrivate::ExtractBlueprintEvidence(*Blueprint);
		const TOptional<FAssetPackageData> SavedData = IAssetRegistry::GetChecked().GetAssetPackageDataCopy(PackageName);
		if (!SavedData.IsSet() || LoadedPackage->GetSavedHash() != SavedData->GetPackageSavedHash())
		{
			IndexState.FailBuild(TEXT("Loaded Blueprint differs from its saved package; reload or restart before rebuilding: ") + PackageName.ToString());
			BuildAccumulator.Reset();
			return;
		}
		FString Error;
		if (!BuildAccumulator || !BuildAccumulator->AddEntry(PackageName, Asset.GetSoftObjectPath().ToString(), Evidence,
			LoadedPackage->IsDirty(), Error))
		{
			IndexState.FailBuild(Error);
			BuildAccumulator.Reset();
			return;
		}
		IndexState.RecordIndexedPackage();
	}

	void StartNextAsyncLoad()
	{
		if (!BuildAccumulator || NextAssetIndex >= ActiveSnapshot.Assets.Num())
		{
			FinalizeActiveBuild();
			return;
		}
		const FAssetData Asset = ActiveSnapshot.Assets[NextAssetIndex++];
		const uint64 RequestedGeneration = BuildGeneration;
		bAsyncLoadInFlight = true;
		ActiveAsyncRequestId = LoadPackageAsync(Asset.PackageName.ToString(),
			FLoadPackageAsyncDelegate::CreateLambda([RequestedGeneration, Asset](const FName& PackageName, UPackage* Package,
				const EAsyncLoadingResult::Type Result)
			{
				HandleAsyncBlueprintLoaded(RequestedGeneration, Asset, PackageName, Package, Result);
			}));
		if (ActiveAsyncRequestId == INDEX_NONE)
		{
			bAsyncLoadInFlight = false;
			IndexState.FailBuild(FString::Printf(TEXT("Blueprint package %s could not queue an asynchronous load."),
				*Asset.PackageName.ToString()));
			BuildAccumulator.Reset();
		}
	}

	void StartBuild()
	{
		FString Error;
		if (!CaptureWorkspaceSnapshot(ActiveSnapshot, Error))
		{
			IndexState.FailBuild(Error);
			UE_LOG(LogRPGIdReferenceIndex, Warning, TEXT("Index build could not start: %s"), *Error);
			return;
		}
		++BuildGeneration;
		NextAssetIndex = 0;
		BuildAccumulator = MakeUnique<RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexBuildAccumulator>(
			ActiveSnapshot.Fingerprint, ActiveSnapshot.FingerprintInput.Catalog, ActiveSnapshot.NativeModuleNames);
		IndexState.BeginBuild(ActiveSnapshot.Assets.Num());
		UE_LOG(LogRPGIdReferenceIndex, Display, TEXT("Index build started for %d Blueprint packages."), ActiveSnapshot.Assets.Num());
		bRebuildQueued = false;
	}

	void RefreshSavedState()
	{
		FIndexBuildSnapshot Snapshot;
		FString Error;
		if (!CaptureWorkspaceSnapshot(Snapshot, Error))
		{
			IndexState.FailBuild(Error);
			bRefreshQueued = true;
			return;
		}
		RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexCache Cache;
		const RPGIdReferenceIndexPrivate::ERPGIdReferenceIndexLoadResult LoadResult =
			RPGIdReferenceIndexPrivate::LoadCache(FRPGIdBlueprintReferenceIndex::GetCachePath(), Cache, Error);
		IndexState.Bootstrap(LoadResult, LoadResult == RPGIdReferenceIndexPrivate::ERPGIdReferenceIndexLoadResult::Ready ? &Cache : nullptr,
			Snapshot.Fingerprint, Snapshot.Assets.Num(), Error);
		if (IndexState.GetStatus().IsReady())
		{
			LoadedCache = MoveTemp(Cache);
			++BuildGeneration;
			UE_LOG(LogRPGIdReferenceIndex, Display, TEXT("Index Ready from persisted cache: %d Blueprint packages from %s."),
				LoadedCache.Entries.Num(), *LoadedCache.BuildTime.ToString());
		}
		else
		{
			bRebuildQueued = true;
		}
		bRefreshQueued = false;
	}

	bool TickReferenceIndex(float)
	{
		if (!bIndexRegistered)
		{
			return false;
		}
		if (BuilderShouldPause())
		{
			return true;
		}
		if (bRefreshQueued && !bAsyncLoadInFlight)
		{
			if (BuildAccumulator)
			{
				DiscardActiveBuild(TEXT("The workspace changed while the RPG Id reference index was building."));
			}
			RefreshSavedState();
		}
		if (bRebuildQueued && !BuildAccumulator && !bAsyncLoadInFlight && !BuilderShouldPause())
		{
			StartBuild();
		}
		if (BuildAccumulator && !bAsyncLoadInFlight && !BuilderShouldPause())
		{
			StartNextAsyncLoad();
		}
		return true;
	}
}

bool RPGIdReferenceIndexPrivate::HashNativeBinary(const FString& Filename, FString& OutHash, FString& OutError)
{
	OutHash.Reset();
	OutError.Reset();
	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Filename));
	if (!Reader || Reader->TotalSize() <= 0)
	{
		OutError = TEXT("Native binary could not be read: ") + Filename;
		return false;
	}
	FIoHashBuilder Hash;
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(1024 * 1024);
	int64 Remaining = Reader->TotalSize();
	while (Remaining > 0 && !Reader->IsError())
	{
		const int64 Count = FMath::Min<int64>(Remaining, Buffer.Num());
		Reader->Serialize(Buffer.GetData(), Count);
		Hash.Update(Buffer.GetData(), Count);
		Remaining -= Count;
	}
	const bool bReadSucceeded = !Reader->IsError() && Remaining == 0;
	if (!Reader->Close() || !bReadSucceeded)
	{
		OutError = TEXT("Native binary fingerprint read failed: ") + Filename;
		return false;
	}
	OutHash = LexToString(Hash.Finalize());
	return true;
}

FString RPGIdReferenceIndexPrivate::BuildWorkspaceFingerprint(const FRPGIdReferenceIndexFingerprintInput& Input)
{
	TArray<FString> MountRoots = Input.MountRoots;
	MountRoots.Sort();
	TArray<FRPGIdReferenceIndexCatalogItem> Catalog = Input.Catalog;
	Catalog.Sort([](const FRPGIdReferenceIndexCatalogItem& A, const FRPGIdReferenceIndexCatalogItem& B)
	{
		return A.PackageName.LexicalLess(B.PackageName);
	});
	TArray<FRPGIdReferenceIndexNativeModule> NativeModules = Input.NativeModules;
	NativeModules.Sort([](const FRPGIdReferenceIndexNativeModule& A, const FRPGIdReferenceIndexNativeModule& B)
	{
		return A.ModuleName.LexicalLess(B.ModuleName);
	});

	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes, true);
	uint32 PolicyVersion = FingerprintPolicyVersion;
	Writer << PolicyVersion;
	FString EngineIdentity = Input.EngineIdentity;
	Writer << EngineIdentity;
	SerializeArray(Writer, MountRoots, SerializeString);
	SerializeArray(Writer, Catalog, [](FArchive& Archive, FRPGIdReferenceIndexCatalogItem& Item)
	{
		SerializeName(Archive, Item.PackageName);
		Archive << Item.AssetPath;
		Archive << Item.PackageHash;
	});
	SerializeArray(Writer, NativeModules, [](FArchive& Archive, FRPGIdReferenceIndexNativeModule& Module)
	{
		SerializeName(Archive, Module.ModuleName);
		Archive << Module.BinaryHash;
	});
	Writer.Close();
	return LexToString(FIoHash::HashBuffer(Bytes));
}

RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexShadowComparison RPGIdReferenceIndexPrivate::CompareBlueprintEvidence(
	const TArray<FRPGIdReferenceIndexEntry>& IndexedEntries, const TArray<FRPGIdReferenceIndexEntry>& DirectEntries)
{
	FRPGIdReferenceIndexShadowComparison Result;
	TArray<FRPGIdReferenceIndexEntry> CanonicalIndexedEntries = IndexedEntries;
	CanonicalizeEntries(CanonicalIndexedEntries);
	for (int32 Index = 0; Index < IndexedEntries.Num(); ++Index)
	{
		if (IndexedEntries[Index].PackageName != CanonicalIndexedEntries[Index].PackageName)
		{
			Result.Difference = FString::Printf(TEXT("Persisted Blueprint catalog ordering is not canonical at entry %d."), Index);
			return Result;
		}
		const TArray<RPGIdReferencePrivate::FRPGIdReferenceRecord>& IndexedReferences = IndexedEntries[Index].Evidence.References;
		const TArray<RPGIdReferencePrivate::FRPGIdReferenceRecord>& CanonicalReferences = CanonicalIndexedEntries[Index].Evidence.References;
		for (int32 ReferenceIndex = 0; ReferenceIndex < IndexedReferences.Num(); ++ReferenceIndex)
		{
			if (!ReferencesEqual(IndexedReferences[ReferenceIndex], CanonicalReferences[ReferenceIndex]))
			{
				Result.Difference = FString::Printf(TEXT("Persisted Blueprint reference ordering is not canonical for %s."),
					*IndexedEntries[Index].PackageName.ToString());
				return Result;
			}
		}
		const TArray<RPGIdReferencePrivate::FRPGIdReferenceIssue>& IndexedIssues = IndexedEntries[Index].Evidence.Issues;
		const TArray<RPGIdReferencePrivate::FRPGIdReferenceIssue>& CanonicalIssues = CanonicalIndexedEntries[Index].Evidence.Issues;
		for (int32 IssueIndex = 0; IssueIndex < IndexedIssues.Num(); ++IssueIndex)
		{
			if (!IssuesEqual(IndexedIssues[IssueIndex], CanonicalIssues[IssueIndex]))
			{
				Result.Difference = FString::Printf(TEXT("Persisted Blueprint issue ordering is not canonical for %s."),
					*IndexedEntries[Index].PackageName.ToString());
				return Result;
			}
		}
	}

	TArray<FRPGIdReferenceIndexEntry> CanonicalDirectEntries = DirectEntries;
	CanonicalizeEntries(CanonicalDirectEntries);
	if (IndexedEntries.Num() != CanonicalDirectEntries.Num())
	{
		Result.Difference = FString::Printf(TEXT("Blueprint catalog mismatch: %d indexed package(s), %d direct package(s)."),
			IndexedEntries.Num(), CanonicalDirectEntries.Num());
		return Result;
	}
	for (int32 Index = 0; Index < IndexedEntries.Num(); ++Index)
	{
		const FRPGIdReferenceIndexEntry& Indexed = IndexedEntries[Index];
		const FRPGIdReferenceIndexEntry& Direct = CanonicalDirectEntries[Index];
		if (Indexed.PackageName != Direct.PackageName || Indexed.AssetPath != Direct.AssetPath)
		{
			Result.Difference = FString::Printf(TEXT("Blueprint catalog entry mismatch at %d: indexed %s (%s), direct %s (%s)."),
				Index, *Indexed.PackageName.ToString(), *Indexed.AssetPath, *Direct.PackageName.ToString(), *Direct.AssetPath);
			return Result;
		}
		if (Indexed.Evidence.References.Num() != Direct.Evidence.References.Num())
		{
			Result.Difference = FString::Printf(TEXT("Blueprint reference count mismatch for %s: %d indexed, %d direct."),
				*Indexed.PackageName.ToString(), Indexed.Evidence.References.Num(), Direct.Evidence.References.Num());
			return Result;
		}
		for (int32 ReferenceIndex = 0; ReferenceIndex < Indexed.Evidence.References.Num(); ++ReferenceIndex)
		{
			if (!ReferencesEqual(Indexed.Evidence.References[ReferenceIndex], Direct.Evidence.References[ReferenceIndex]))
			{
				Result.Difference = FString::Printf(TEXT("Blueprint reference mismatch for %s at record %d."),
					*Indexed.PackageName.ToString(), ReferenceIndex);
				return Result;
			}
		}
		if (Indexed.Evidence.Issues.Num() != Direct.Evidence.Issues.Num())
		{
			Result.Difference = FString::Printf(TEXT("Blueprint issue count mismatch for %s: %d indexed, %d direct."),
				*Indexed.PackageName.ToString(), Indexed.Evidence.Issues.Num(), Direct.Evidence.Issues.Num());
			return Result;
		}
		for (int32 IssueIndex = 0; IssueIndex < Indexed.Evidence.Issues.Num(); ++IssueIndex)
		{
			if (!IssuesEqual(Indexed.Evidence.Issues[IssueIndex], Direct.Evidence.Issues[IssueIndex]))
			{
				Result.Difference = FString::Printf(TEXT("Blueprint issue mismatch for %s at record %d."),
					*Indexed.PackageName.ToString(), IssueIndex);
				return Result;
			}
		}
	}
	Result.bExact = true;
	return Result;
}

bool RPGIdReferenceIndexPrivate::SaveCacheAtomically(const FString& CachePath, const FRPGIdReferenceIndexCache& Cache,
	FString& OutError)
{
	OutError.Reset();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CachePath), true);
	const FString TemporaryPath = CachePath + TEXT(".tmp");
	TArray<uint8> Bytes;
	if (!BuildFileBytes(Cache, Bytes, OutError) || !FFileHelper::SaveArrayToFile(Bytes, *TemporaryPath))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Could not write the temporary RPG Id reference index cache.");
		}
		return false;
	}

	FRPGIdReferenceIndexCache Verified;
	FString VerifyError;
	if (LoadCache(TemporaryPath, Verified, VerifyError) != ERPGIdReferenceIndexLoadResult::Ready)
	{
		OutError = FString::Printf(TEXT("Temporary RPG Id reference index verification failed: %s"), *VerifyError);
		IFileManager::Get().Delete(*TemporaryPath, false, true);
		return false;
	}
	if (!IFileManager::Get().Move(*CachePath, *TemporaryPath, true, false, false, true))
	{
		OutError = TEXT("Could not atomically replace the RPG Id reference index cache.");
		IFileManager::Get().Delete(*TemporaryPath, false, true);
		return false;
	}
	return true;
}

RPGIdReferenceIndexPrivate::ERPGIdReferenceIndexLoadResult RPGIdReferenceIndexPrivate::LoadCache(const FString& CachePath,
	FRPGIdReferenceIndexCache& OutCache, FString& OutError)
{
	OutCache = FRPGIdReferenceIndexCache();
	OutError.Reset();
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *CachePath))
	{
		OutError = TEXT("The RPG Id reference index cache does not exist.");
		return ERPGIdReferenceIndexLoadResult::Missing;
	}
	if (Bytes.Num() <= 0 || Bytes.Num() > MaxCacheBytes)
	{
		OutError = TEXT("The RPG Id reference index cache has an invalid size.");
		return ERPGIdReferenceIndexLoadResult::Corrupt;
	}

	FMemoryReader Reader(Bytes, true);
	uint32 Magic = 0;
	uint32 SchemaVersion = 0;
	int64 PayloadSize = 0;
	FString ExpectedChecksum;
	Reader << Magic;
	Reader << SchemaVersion;
	Reader << PayloadSize;
	Reader << ExpectedChecksum;
	if (Reader.IsError() || Magic != CacheMagic)
	{
		OutError = TEXT("The RPG Id reference index cache header is invalid.");
		return ERPGIdReferenceIndexLoadResult::Corrupt;
	}
	if (SchemaVersion != CacheSchemaVersion)
	{
		OutError = TEXT("The RPG Id reference index cache schema is not current.");
		return ERPGIdReferenceIndexLoadResult::SchemaMismatch;
	}
	if (PayloadSize < 0 || PayloadSize > MaxCacheBytes || Reader.Tell() + PayloadSize != Bytes.Num())
	{
		OutError = TEXT("The RPG Id reference index cache payload size is invalid.");
		return ERPGIdReferenceIndexLoadResult::Corrupt;
	}

	TArray<uint8> Payload;
	Payload.SetNumUninitialized(static_cast<int32>(PayloadSize));
	Reader.Serialize(Payload.GetData(), Payload.Num());
	if (Reader.IsError() || LexToString(FIoHash::HashBuffer(Payload)) != ExpectedChecksum)
	{
		OutError = TEXT("The RPG Id reference index cache checksum is invalid.");
		return ERPGIdReferenceIndexLoadResult::Corrupt;
	}

	FMemoryReader PayloadReader(Payload, true);
	SerializeCachePayload(PayloadReader, OutCache);
	if (PayloadReader.IsError() || PayloadReader.Tell() != Payload.Num())
	{
		OutCache = FRPGIdReferenceIndexCache();
		OutError = TEXT("The RPG Id reference index cache payload is malformed.");
		return ERPGIdReferenceIndexLoadResult::Corrupt;
	}
	return ERPGIdReferenceIndexLoadResult::Ready;
}

void RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexStateModel::Bootstrap(const ERPGIdReferenceIndexLoadResult LoadResult,
	const FRPGIdReferenceIndexCache* Cache, const FString& CurrentFingerprint, const int32 TotalPackageCount, const FString& LoadError)
{
	Status = FRPGIdReferenceIndexStatus();
	Status.TotalPackageCount = TotalPackageCount;
	if (LoadResult == ERPGIdReferenceIndexLoadResult::Ready && Cache && Cache->WorkspaceFingerprint == CurrentFingerprint)
	{
		Status.State = ERPGIdReferenceIndexState::Ready;
		Status.IndexedPackageCount = Cache->Entries.Num();
		Status.LastBuildTime = Cache->BuildTime;
		return;
	}
	if (LoadResult == ERPGIdReferenceIndexLoadResult::Ready)
	{
		Status.State = ERPGIdReferenceIndexState::Stale;
		Status.LastFailure = TEXT("The saved RPG Id reference index workspace fingerprint is stale.");
		return;
	}
	Status.State = ERPGIdReferenceIndexState::Unavailable;
	Status.LastFailure = LoadError.IsEmpty() ? TEXT("The RPG Id reference index is unavailable.") : LoadError;
}

void RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexStateModel::BeginBuild(const int32 TotalPackageCount)
{
	Status.State = ERPGIdReferenceIndexState::Building;
	Status.IndexedPackageCount = 0;
	Status.TotalPackageCount = TotalPackageCount;
	Status.SuccessfulShadowComparisonCount = 0;
	Status.LastShadowComparisonTime = FDateTime();
	Status.LastShadowTarget.Reset();
	Status.LastFailure.Reset();
}

void RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexStateModel::RecordIndexedPackage()
{
	if (Status.State == ERPGIdReferenceIndexState::Building)
	{
		++Status.IndexedPackageCount;
	}
}

void RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexStateModel::CompleteBuild()
{
	if (Status.State == ERPGIdReferenceIndexState::Building && Status.IndexedPackageCount == Status.TotalPackageCount)
	{
		Status.State = ERPGIdReferenceIndexState::Ready;
		Status.LastBuildTime = FDateTime::Now();
		Status.LastFailure.Reset();
	}
	else
	{
		FailBuild(TEXT("The RPG Id reference index build did not inspect the complete Blueprint catalog."));
	}
}

void RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexStateModel::FailBuild(const FString& Reason)
{
	Status.State = ERPGIdReferenceIndexState::Unavailable;
	Status.SuccessfulShadowComparisonCount = 0;
	Status.LastFailure = Reason;
}

void RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexStateModel::CancelBuild()
{
	FailBuild(TEXT("The RPG Id reference index build was cancelled."));
}

void RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexStateModel::Invalidate(const FString& Reason)
{
	Status.State = ERPGIdReferenceIndexState::Stale;
	Status.SuccessfulShadowComparisonCount = 0;
	Status.LastFailure = Reason;
}

void RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexStateModel::RecordShadowSuccess(const FRPGId& Target)
{
	if (Status.IsReady())
	{
		++Status.SuccessfulShadowComparisonCount;
		Status.LastShadowComparisonTime = FDateTime::UtcNow();
		Status.LastShadowTarget = Target.ToString();
	}
}

void RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexStateModel::RecordShadowFailure(const FRPGId& Target, const FString& Reason)
{
	Status.State = ERPGIdReferenceIndexState::Unavailable;
	Status.SuccessfulShadowComparisonCount = 0;
	Status.LastShadowComparisonTime = FDateTime::UtcNow();
	Status.LastShadowTarget = Target.ToString();
	Status.LastFailure = Reason;
}

const RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexStatus&
RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexStateModel::GetStatus() const
{
	return Status;
}

RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexBuildAccumulator::FRPGIdReferenceIndexBuildAccumulator(
	const FString& WorkspaceFingerprint, const TArray<FRPGIdReferenceIndexCatalogItem>& Catalog, const TArray<FName>& NativeModules)
{
	Cache.WorkspaceFingerprint = WorkspaceFingerprint;
	Cache.NativeModules = NativeModules;
	for (const FRPGIdReferenceIndexCatalogItem& Item : Catalog)
	{
		ExpectedAssets.Add(Item.PackageName, Item.AssetPath);
	}
}

bool RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexBuildAccumulator::AddEntry(const FName PackageName, const FString& AssetPath,
	const RPGIdReferencePrivate::FRPGIdBlueprintEvidence& Evidence, const bool bPackageDirty, FString& OutError)
{
	OutError.Reset();
	const FString* ExpectedAssetPath = ExpectedAssets.Find(PackageName);
	if (!ExpectedAssetPath || *ExpectedAssetPath != AssetPath)
	{
		OutError = FString::Printf(TEXT("Blueprint package %s is not part of the exact index catalog."), *PackageName.ToString());
		return false;
	}
	if (CompletedPackages.Contains(PackageName))
	{
		OutError = FString::Printf(TEXT("Blueprint package %s produced duplicate index evidence."), *PackageName.ToString());
		return false;
	}
	if (bPackageDirty)
	{
		OutError = FString::Printf(TEXT("Blueprint package %s became dirty while building the reference index."), *PackageName.ToString());
		return false;
	}

	FRPGIdReferenceIndexEntry& Entry = Cache.Entries.AddDefaulted_GetRef();
	Entry.PackageName = PackageName;
	Entry.AssetPath = AssetPath;
	Entry.Evidence = Evidence;
	CompletedPackages.Add(PackageName);
	return true;
}

bool RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexBuildAccumulator::Finalize(FRPGIdReferenceIndexCache& OutCache,
	FString& OutError) const
{
	if (CompletedPackages.Num() != ExpectedAssets.Num())
	{
		OutError = FString::Printf(TEXT("The reference index inspected %d of %d Blueprint packages."), CompletedPackages.Num(),
			ExpectedAssets.Num());
		return false;
	}
	OutCache = Cache;
	OutCache.BuildTime = FDateTime::UtcNow();
	OutError.Reset();
	return true;
}

void FRPGIdBlueprintReferenceIndex::Register()
{
	if (bIndexRegistered || IsRunningCommandlet())
	{
		return;
	}
	bIndexRegistered = true;
	bRefreshQueued = true;
	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	AssetAddedHandle = Registry.OnAssetAdded().AddStatic(&QueueRegistryRefresh);
	AssetRemovedHandle = Registry.OnAssetRemoved().AddStatic(&QueueRegistryRefresh);
	AssetUpdatedHandle = Registry.OnAssetUpdatedOnDisk().AddStatic(&QueueRegistryRefresh);
	AssetRenamedHandle = Registry.OnAssetRenamed().AddLambda([](const FAssetData& AssetData, const FString&)
	{
		QueueRegistryRefresh(AssetData);
	});
	PackageSavedHandle = UPackage::PackageSavedWithContextEvent.AddLambda([](const FString&, UPackage* Package, FObjectPostSaveContext)
	{
		if (Package && IsWorkspacePackage(Package->GetFName()))
		{
			bRefreshQueued = true;
		}
	});
	if (GEditor)
	{
		BlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddLambda([]()
		{
			++LoadedOverlayGeneration;
		});
	}
	ModulesChangedHandle = FModuleManager::Get().OnModulesChanged().AddLambda([](const FName ModuleName,
		const EModuleChangeReason Reason)
	{
		if (Reason == EModuleChangeReason::PluginDirectoryChanged)
		{
			return;
		}
		if (LoadedCache.NativeModules.Contains(ModuleName) || ActiveSnapshot.NativeModuleNames.Contains(ModuleName))
		{
			FRPGIdBlueprintReferenceIndex::NotifyNativeCodeChanged();
		}
	});
	ReloadCompleteHandle = FCoreUObjectDelegates::ReloadCompleteDelegate.AddLambda([](EReloadCompleteReason)
	{
		FRPGIdBlueprintReferenceIndex::NotifyNativeCodeChanged();
	});
	IndexTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickReferenceIndex));
}

void FRPGIdBlueprintReferenceIndex::Unregister()
{
	if (!bIndexRegistered)
	{
		return;
	}
	if (bAsyncLoadInFlight && ActiveAsyncRequestId != INDEX_NONE)
	{
		FlushAsyncLoading(ActiveAsyncRequestId);
	}
	bIndexRegistered = false;
	++BuildGeneration;
	if (IAssetRegistry* Registry = IAssetRegistry::Get())
	{
		Registry->OnAssetAdded().Remove(AssetAddedHandle);
		Registry->OnAssetRemoved().Remove(AssetRemovedHandle);
		Registry->OnAssetUpdatedOnDisk().Remove(AssetUpdatedHandle);
		Registry->OnAssetRenamed().Remove(AssetRenamedHandle);
	}
	UPackage::PackageSavedWithContextEvent.Remove(PackageSavedHandle);
	if (GEditor)
	{
		GEditor->OnBlueprintCompiled().Remove(BlueprintCompiledHandle);
	}
	FModuleManager::Get().OnModulesChanged().Remove(ModulesChangedHandle);
	FCoreUObjectDelegates::ReloadCompleteDelegate.Remove(ReloadCompleteHandle);
	if (IndexTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(IndexTickerHandle);
	}
	AssetAddedHandle.Reset();
	AssetRemovedHandle.Reset();
	AssetRenamedHandle.Reset();
	AssetUpdatedHandle.Reset();
	PackageSavedHandle.Reset();
	BlueprintCompiledHandle.Reset();
	ModulesChangedHandle.Reset();
	ReloadCompleteHandle.Reset();
	IndexTickerHandle.Reset();
	BuildAccumulator.Reset();
	LoadedCache = RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexCache();
	ActiveSnapshot = FIndexBuildSnapshot();
	bRefreshQueued = false;
	bRebuildQueued = false;
	bAsyncLoadInFlight = false;
	ActiveAsyncRequestId = INDEX_NONE;
}

RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexStatus FRPGIdBlueprintReferenceIndex::GetStatus()
{
	return IndexState.GetStatus();
}

void FRPGIdBlueprintReferenceIndex::NotifyNativeCodeChanged()
{
	NativeReloadIdentity = TEXT("|RuntimeReload:") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	RequestRebuild();
	IndexState.Invalidate(TEXT("Native code was reloaded. Rebuild the reference index and Check again."));
}

void FRPGIdBlueprintReferenceIndex::RequestRebuild()
{
	if (!bIndexRegistered)
	{
		return;
	}
	++BuildGeneration;
	BuildAccumulator.Reset();
	ActiveSnapshot = FIndexBuildSnapshot();
	NextAssetIndex = 0;
	IndexState.Invalidate(TEXT("A full RPG Id reference index rebuild was requested."));
	bRefreshQueued = false;
	bRebuildQueued = true;
}

void FRPGIdBlueprintReferenceIndex::CancelBuild()
{
	if (!bIndexRegistered || (!BuildAccumulator && !bAsyncLoadInFlight && !bRebuildQueued))
	{
		return;
	}
	++BuildGeneration;
	BuildAccumulator.Reset();
	ActiveSnapshot = FIndexBuildSnapshot();
	NextAssetIndex = 0;
	bRebuildQueued = false;
	bRefreshQueued = false;
	IndexState.CancelBuild();
}

bool FRPGIdBlueprintReferenceIndex::QueryExactSnapshot(RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexCache& OutCache,
	FString& OutError)
{
	OutCache = RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexCache();
	OutError.Reset();
	if (!bIndexRegistered)
	{
		OutError = TEXT("The RPG Id reference index is not registered in this process.");
		return false;
	}
	if (!IndexState.GetStatus().IsReady() || bRefreshQueued || bRebuildQueued || BuildAccumulator || bAsyncLoadInFlight)
	{
		if (!bRefreshQueued && !bRebuildQueued && !BuildAccumulator && !bAsyncLoadInFlight
			&& !IndexState.GetStatus().LastFailure.StartsWith(TEXT("Shadow comparison failed:")))
		{
			bRebuildQueued = true;
		}
		OutError = IndexState.GetStatus().LastFailure.IsEmpty()
			? TEXT("The RPG Id reference index is not ready for an exact query.") : IndexState.GetStatus().LastFailure;
		return false;
	}

	FIndexBuildSnapshot CurrentSnapshot;
	if (!CaptureWorkspaceSnapshot(CurrentSnapshot, OutError))
	{
		IndexState.FailBuild(OutError);
		return false;
	}
	if (LoadedCache.WorkspaceFingerprint != CurrentSnapshot.Fingerprint
		|| LoadedCache.Entries.Num() != CurrentSnapshot.Assets.Num())
	{
		OutError = TEXT("The RPG Id reference index no longer matches the exact Blueprint catalog and workspace fingerprint.");
		IndexState.Invalidate(OutError);
		bRebuildQueued = true;
		return false;
	}
	for (int32 Index = 0; Index < LoadedCache.Entries.Num(); ++Index)
	{
		const auto& Entry = LoadedCache.Entries[Index];
		const auto& Asset = CurrentSnapshot.Assets[Index];
		if (Entry.PackageName != Asset.PackageName || Entry.AssetPath != Asset.GetSoftObjectPath().ToString())
		{
			OutError = TEXT("The reference index catalog differs from the saved Blueprint catalog. Rebuild and check again.");
			IndexState.Invalidate(OutError);
			bRebuildQueued = true;
			return false;
		}
	}
	OutCache = LoadedCache;
	return true;
}

bool FRPGIdBlueprintReferenceIndex::RecordShadowComparison(const FRPGId& Target, const FString& ExpectedWorkspaceFingerprint,
	const RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexShadowComparison& Comparison)
{
	if (!bIndexRegistered || !IndexState.GetStatus().IsReady() || bRefreshQueued || bRebuildQueued || BuildAccumulator
		|| bAsyncLoadInFlight || LoadedCache.WorkspaceFingerprint != ExpectedWorkspaceFingerprint)
	{
		return false;
	}
	if (!Comparison.bExact)
	{
		IndexState.RecordShadowFailure(Target, FString::Printf(TEXT("Shadow comparison failed: %s"), *Comparison.Difference));
		bRebuildQueued = false;
		return true;
	}
	IndexState.RecordShadowSuccess(Target);
	return true;
}

uint64 FRPGIdBlueprintReferenceIndex::GetGeneration()
{
	return BuildGeneration;
}

bool FRPGIdBlueprintReferenceIndex::IsGenerationCurrent(const uint64 Generation)
{
	return Generation != 0 && Generation == BuildGeneration && bIndexRegistered && IndexState.GetStatus().IsReady()
		&& !bRefreshQueued && !bRebuildQueued && !BuildAccumulator && !bAsyncLoadInFlight;
}

FString FRPGIdBlueprintReferenceIndex::GetCachePath()
{
	return FPaths::ProjectSavedDir() / TEXT("IronicRPG/RPGIdReferenceIndex.bin");
}
