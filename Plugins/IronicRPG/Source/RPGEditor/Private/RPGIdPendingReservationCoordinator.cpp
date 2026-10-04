// Copyright Ironic Studio. All Rights Reserved.

#include "RPGIdPendingReservationCoordinator.h"

#include "RPGIdClaimAdapter.h"
#include "RPGIdClaimSnapshot.h"
#include "RPGIdReferenceAudit.h"
#include "Assets/RPGPrimaryAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "RPGIdPendingReservationCoordinator"

namespace
{
	struct FPendingReservation
	{
		TWeakObjectPtr<const URPGPrimaryAsset> Owner;
		FName SavedId;
	};

	TArray<FPendingReservation> PendingReservations;
	FDelegateHandle ObjectTransactedHandle;
	FDelegateHandle PackageSavedHandle;
	FDelegateHandle AssetUpdatedOnDiskHandle;

	bool IsOrdinaryOwner(const URPGPrimaryAsset& Owner)
	{
		const FRPGIdClaimAdapter* Adapter = RPGIdClaimPrivate::FindAdapter(Owner.GetClass());
		return Adapter && Adapter->UsesPendingReservations();
	}

	int32 FindReservation(const URPGPrimaryAsset& Owner)
	{
		return PendingReservations.IndexOfByPredicate([&Owner](const FPendingReservation& Entry)
		{
			return Entry.Owner.Get() == &Owner;
		});
	}

	bool ReadSavedId(const URPGPrimaryAsset& Owner, FName& OutSavedId, bool& bOutHasSavedAsset, FText& OutError)
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		if (Registry.IsLoadingAssets() || !Registry.IsSearchAllAssets())
		{
			OutError = LOCTEXT("Scanning", "Full mounted asset discovery has not completed. Pending Id reservations cannot be verified yet.");
			return false;
		}

		const FAssetData Saved = Registry.GetAssetByObjectPath(FSoftObjectPath(&Owner), true);
		bOutHasSavedAsset = Saved.IsValid();
		OutSavedId = NAME_None;
		if (!bOutHasSavedAsset)
		{
			return true;
		}

		FString SavedValue;
		if (!Saved.GetTagValue(TEXT("RPGId"), SavedValue))
		{
			OutError = FText::Format(LOCTEXT("MissingTag", "Cannot read the saved RPGId tag for {0}. Resave the asset before changing identity."),
				FText::FromString(FSoftObjectPath(&Owner).ToString()));
			return false;
		}
		OutSavedId = SavedValue.IsEmpty() ? NAME_None : FName(*SavedValue);
		return true;
	}

	bool RefreshOwnerInternal(const URPGPrimaryAsset& Owner, const bool bAllowRelease, FText& OutError)
	{
		if (!IsOrdinaryOwner(Owner) || Owner.HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient)
			|| !Owner.IsAsset() || Owner.GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
		{
			return true;
		}

		FName SavedId;
		bool bHasSavedAsset = false;
		if (!ReadSavedId(Owner, SavedId, bHasSavedAsset, OutError))
		{
			return false;
		}

		const FName CurrentId = Owner.GetId().Id;
		const int32 ExistingIndex = FindReservation(Owner);
		if (ExistingIndex != INDEX_NONE && CurrentId == PendingReservations[ExistingIndex].SavedId)
		{
			PendingReservations.RemoveAtSwap(ExistingIndex);
			return true;
		}

		if (bHasSavedAsset && !SavedId.IsNone() && SavedId != CurrentId)
		{
			FPendingReservation& Entry = ExistingIndex == INDEX_NONE
				? PendingReservations.AddDefaulted_GetRef() : PendingReservations[ExistingIndex];
			Entry.Owner = &Owner;
			Entry.SavedId = SavedId;
			return true;
		}

		if (ExistingIndex == INDEX_NONE || !bAllowRelease || Owner.GetOutermost()->IsDirty() || !bHasSavedAsset || SavedId != CurrentId)
		{
			return true;
		}

		TMap<FSoftObjectPath, FName> Owners;
		const FRPGIdClaimResult Ownership = RPGIdClaimPrivate::ReadOwners(Owners);
		if (!Ownership.CanApply())
		{
			OutError = Ownership.Message;
			return false;
		}
		const FName ReservedId = PendingReservations[ExistingIndex].SavedId;
		bool bStillOwned = false;
		for (const TPair<FSoftObjectPath, FName>& Entry : Owners)
		{
			if (Entry.Value == ReservedId)
			{
				bStillOwned = true;
				break;
			}
		}
		if (!bStillOwned)
		{
			// Releasing an identity reservation is authoritative, including when called by Release Seal.
			const FRPGIdReferenceAuditResult References = RPGIdReferencePrivate::AuditDevelopmentReferences(
				FRPGId(ReservedId), RPGIdReferencePrivate::ERPGIdAuditMode::Direct);
			if (References.Hits.IsEmpty() && References.CoverageGaps.IsEmpty())
			{
				PendingReservations.RemoveAtSwap(ExistingIndex);
			}
		}
		return true;
	}

	bool RefreshAll(FText& OutError)
	{
		PendingReservations.RemoveAll([](const FPendingReservation& Entry) { return !Entry.Owner.IsValid(); });
		for (TObjectIterator<URPGPrimaryAsset> It; It; ++It)
		{
			if (!RefreshOwnerInternal(**It, true, OutError))
			{
				return false;
			}
		}
		return true;
	}

	void HandleObjectTransacted(UObject* Object, const FTransactionObjectEvent&)
	{
		if (const URPGPrimaryAsset* Owner = Cast<URPGPrimaryAsset>(Object))
		{
			FText Ignored;
			RefreshOwnerInternal(*Owner, false, Ignored);
		}
	}

	void HandlePackageSaved(const FString&, UPackage* Package, FObjectPostSaveContext)
	{
		if (!Package)
		{
			return;
		}
		for (TObjectIterator<URPGPrimaryAsset> It; It; ++It)
		{
			if (It->GetOutermost() == Package)
			{
				FText Ignored;
				RefreshOwnerInternal(**It, false, Ignored);
			}
		}
	}

	void HandleAssetUpdatedOnDisk(const FAssetData& Data)
	{
		if (const URPGPrimaryAsset* Owner = Cast<URPGPrimaryAsset>(Data.GetSoftObjectPath().ResolveObject()))
		{
			FText Ignored;
			RefreshOwnerInternal(*Owner, false, Ignored);
		}
	}
}

void FRPGIdPendingReservationCoordinator::Register()
{
	if (ObjectTransactedHandle.IsValid())
	{
		return;
	}
	ObjectTransactedHandle = FCoreUObjectDelegates::OnObjectTransacted.AddStatic(&HandleObjectTransacted);
	PackageSavedHandle = UPackage::PackageSavedWithContextEvent.AddStatic(&HandlePackageSaved);
	AssetUpdatedOnDiskHandle = IAssetRegistry::GetChecked().OnAssetUpdatedOnDisk().AddStatic(&HandleAssetUpdatedOnDisk);
}

void FRPGIdPendingReservationCoordinator::Unregister()
{
	FCoreUObjectDelegates::OnObjectTransacted.Remove(ObjectTransactedHandle);
	UPackage::PackageSavedWithContextEvent.Remove(PackageSavedHandle);
	if (IAssetRegistry* Registry = IAssetRegistry::Get())
	{
		Registry->OnAssetUpdatedOnDisk().Remove(AssetUpdatedOnDiskHandle);
	}
	ObjectTransactedHandle.Reset();
	PackageSavedHandle.Reset();
	AssetUpdatedOnDiskHandle.Reset();
	PendingReservations.Reset();
}

void FRPGIdPendingReservationCoordinator::RefreshOwner(const URPGPrimaryAsset& Owner)
{
	FText Ignored;
	RefreshOwnerInternal(Owner, true, Ignored);
}

bool FRPGIdPendingReservationCoordinator::ReadReservedIds(TSet<FName>& OutIds, FText& OutError)
{
	OutIds.Reset();
	if (!RefreshAll(OutError))
	{
		return false;
	}
	for (const FPendingReservation& Entry : PendingReservations)
	{
		OutIds.Add(Entry.SavedId);
	}
	return true;
}

bool FRPGIdPendingReservationCoordinator::CheckCandidate(const URPGPrimaryAsset* RequestOwner, const FRPGId& Candidate, FText& OutError)
{
	if (!Candidate.IsValid())
	{
		return true;
	}
	TSet<FName> Reserved;
	if (!ReadReservedIds(Reserved, OutError))
	{
		return false;
	}
	const bool bReservedByAnotherOwner = PendingReservations.ContainsByPredicate([RequestOwner, &Candidate](const FPendingReservation& Entry)
	{
		return Entry.SavedId == Candidate.Id && Entry.Owner.Get() != RequestOwner;
	});
	if (bReservedByAnotherOwner)
	{
		OutError = FText::Format(LOCTEXT("Reserved", "Id {0} is reserved by an ordinary identity change that has not finished saving."),
			FText::FromName(Candidate.Id));
		return false;
	}
	return true;
}

bool FRPGIdPendingReservationCoordinator::CheckOwnerReadyForMutation(const URPGPrimaryAsset& Owner, FText& OutError)
{
	if (!RefreshAll(OutError))
	{
		return false;
	}
	const int32 ExistingIndex = FindReservation(Owner);
	if (ExistingIndex != INDEX_NONE)
	{
		OutError = FText::Format(LOCTEXT("OwnerPending", "Finish the pending RPGId transition before changing this owner again. "
			"The previous Id {0} remains reserved until save convergence and a complete zero-reference audit succeed."),
			FText::FromName(PendingReservations[ExistingIndex].SavedId));
		return false;
	}
	return true;
}

bool FRPGIdPendingReservationCoordinator::CheckWorkspaceClean(FText& OutError)
{
	for (TObjectIterator<UPackage> It; It; ++It)
	{
		const FString Name = It->GetName();
		if ((Name.StartsWith(TEXT("/Game/")) || Name.StartsWith(TEXT("/IronicRPG/"))) && It->IsDirty())
		{
			OutError = FText::Format(LOCTEXT("Dirty", "Save or revert the dirty package before changing an existing RPGId: {0}"),
				FText::FromString(Name));
			return false;
		}
	}
	return true;
}

void FRPGIdPendingReservationCoordinator::ResetForTests()
{
	PendingReservations.Reset();
}

#undef LOCTEXT_NAMESPACE
