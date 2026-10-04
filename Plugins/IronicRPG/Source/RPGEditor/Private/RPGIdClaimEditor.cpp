// Copyright Ironic Studio. All Rights Reserved.

#include "RPGIdClaimEditor.h"
#include "RPGIdCategoryService.h"
#include "RPGIdBlueprintReferenceIndex.h"
#include "RPGIdClaimSnapshot.h"
#include "RPGIdReferenceAudit.h"
#include "RPGIdClaimAdapter.h"
#include "RPGIdPendingReservationCoordinator.h"
#include "RPGReleaseSealService.h"

#include "Assets/RPGAssetManager.h"
#include "Assets/RPGPrimaryAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "RPGSettings.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "RPGIdClaimEditor"

namespace
{
	bool bExecutingClaim = false;

	FRPGIdClaimResult Result(ERPGIdClaimResult Code, const FText& Message)
	{
		FRPGIdClaimResult Value;
		Value.Code = Code;
		Value.Message = Message;
		return Value;
	}

	URPGPrimaryAsset* GetOwner(const TArray<TWeakObjectPtr<UObject>>& Owners)
	{
		return Owners.Num() == 1 ? Cast<URPGPrimaryAsset>(Owners[0].Get()) : nullptr;
	}

	bool IsWorkspaceOwner(const URPGPrimaryAsset* Asset)
	{
		return IsValid(Asset) && Asset->IsAsset() && !Asset->IsTemplate() && !Asset->HasAnyFlags(RF_Transient)
			&& Asset->GetOutermost() != GetTransientPackage() && !Asset->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor);
	}

	FRPGIdClaimResult CheckOwner(const TArray<TWeakObjectPtr<UObject>>& Owners)
	{
		if (!IsInGameThread() || !GEditor || GEditor->PlayWorld || GIsPlayInEditorWorld || IsRunningCookCommandlet() || bExecutingClaim)
		{
			return Result(ERPGIdClaimResult::NotEditable,
				LOCTEXT("Environment", "Claim editing is unavailable during play, cook, or another Claim operation."));
		}
		URPGPrimaryAsset* Owner = GetOwner(Owners);
		if (!IsWorkspaceOwner(Owner))
		{
			return Result(ERPGIdClaimResult::UnsupportedOwner,
				LOCTEXT("Owner", "Select one top-level RPG asset. Templates, transient objects and multi-selection cannot claim an Id."));
		}
		UClass* Class = Owner->GetClass();
		if (!RPGIdClaimPrivate::FindAdapter(Class))
		{
			return Result(ERPGIdClaimResult::UnsupportedOwner, LOCTEXT("Class", "This owner class has no Claim authoring adapter. SubGameZone is frozen as a legacy owner."));
		}
		URPGAssetManager* Manager = GEngine ? Cast<URPGAssetManager>(GEngine->AssetManager) : nullptr;
		const URPGSettings* Settings = GetDefault<URPGSettings>();
		const FName Type = Owner->GetAssetType();
		const FName Prefix = Owner->GetAssetIdPrefix();
		if (!Manager || Settings->NumericLen < 1 || Settings->NumericLen > 9 || Settings->MaxPrefixLen < 1
			|| Prefix.IsNone() || Prefix.ToString().Len() > Settings->MaxPrefixLen || Manager->GetAssetTypeClass(Type) != Class
			|| Manager->GetIdPrefix(Type) != Prefix || Manager->GetIdTypeFromPrefix(Prefix) != Type)
		{
			return Result(ERPGIdClaimResult::InspectionIncomplete,
				LOCTEXT("Config", "Id format, declared type, prefix map or asset class settings are inconsistent (supported numeric length: 1-9)."));
		}
		TSet<FName> Types;
		for (const FName RegisteredType : Manager->GetAllAssetTypes())
		{
			if (RegisteredType.IsNone() || Types.Contains(RegisteredType))
			{
				return Result(ERPGIdClaimResult::InspectionIncomplete, LOCTEXT("PrefixCollision", "The prefix map must assign exactly one prefix to each type."));
			}
			Types.Add(RegisteredType);
		}
		FPrimaryAssetTypeInfo TypeInfo;
		const FString PackageName = Owner->GetOutermost()->GetName();
		const FString SupportedRoot = TEXT("/Game/DataAssets/") + Type.ToString() + TEXT("/");
		if (!Manager->GetPrimaryAssetTypeInfo(Type, TypeInfo) || TypeInfo.bHasBlueprintClasses || TypeInfo.AssetBaseClassLoaded != Class
			|| !PackageName.StartsWith(SupportedRoot) || !TypeInfo.AssetScanPaths.ContainsByPredicate([&PackageName](const FString& Path)
			{
				return PackageName == Path || PackageName.StartsWith(Path.EndsWith(TEXT("/")) ? Path : Path + TEXT("/"));
			}))
		{
			return Result(ERPGIdClaimResult::UnsupportedOwner,
				LOCTEXT("Path", "Claim editing requires a registered native type under /Game/DataAssets/<Type>/. "
					"Other paths still participate in collision checks."));
		}
		FString Filename;
		if (FPackageName::DoesPackageExist(PackageName, &Filename) && IFileManager::Get().IsReadOnly(*Filename))
		{
			return Result(ERPGIdClaimResult::NotEditable, LOCTEXT("ReadOnly", "The asset package is read-only. Make it writable before applying a Claim."));
		}
		return Result(ERPGIdClaimResult::Success, FText::GetEmpty());
	}

	bool ValidCandidate(const URPGPrimaryAsset& Owner, const FRPGId& Candidate)
	{
		const URPGSettings* Settings = GetDefault<URPGSettings>();
		const FString Value = Candidate.ToString();
		const FString Prefix = Owner.GetAssetIdPrefix().ToString();
		if (Value.Len() != Prefix.Len() + Settings->NumericLen || !Value.StartsWith(Prefix, ESearchCase::IgnoreCase))
		{
			return false;
		}
		for (const TCHAR Character : Value.Right(Settings->NumericLen))
		{
			if (Character < TEXT('0') || Character > TEXT('9'))
			{
				return false;
			}
		}
		return Candidate.GetRebuiltIdType() == Owner.GetAssetType();
	}

	FRPGIdClaimResult CheckCollision(const URPGPrimaryAsset& Owner, const FRPGId& Candidate)
	{
		TMap<FSoftObjectPath, FName> Owners;
		FRPGIdClaimResult Audit = RPGIdClaimPrivate::ReadOwners(Owners);
		if (!Audit.CanApply())
		{
			return Audit;
		}
		for (const auto& Entry : Owners)
		{
			if (Entry.Key != FSoftObjectPath(&Owner) && Entry.Value == Candidate.Id)
			{
				Audit.Conflicts.Add(Entry.Key);
			}
		}
		Audit.Conflicts.Sort([](const FSoftObjectPath& A, const FSoftObjectPath& B) { return A.ToString() < B.ToString(); });
		if (!Audit.Conflicts.IsEmpty())
		{
			Audit.Code = ERPGIdClaimResult::Collision;
			Audit.Message = LOCTEXT("Collision", "This Id is already claimed. No asset was changed. Conflicting owners:");
		}
		else
		{
			Audit.Message = LOCTEXT("Unique", "No collision in saved and loaded workspace owners. This is not a reference or saved-data validation.");
		}
		return Audit;
	}

	FRPGIdClaimResult PreviewChangeWithReferences(const URPGPrimaryAsset& Owner, const FRPGIdClaimAdapter& Adapter, FRPGIdClaimResult ClaimResult)
	{
		const FRPGIdReferenceAuditResult Audit = RPGIdReferencePrivate::AuditDevelopmentReferences(Owner.GetId());
		ClaimResult.IndexGeneration = Audit.IndexGeneration;
		FString Context = ClaimResult.Context.IsEmpty() ? Audit.Summary.ToString() : ClaimResult.Context.ToString() + TEXT("\n") + Audit.Summary.ToString();
		const int32 DisplayLimit = 16;
		int32 Displayed = 0;
		int32 ManagedCount = 0;
		for (const FRPGIdReferenceHit& Hit : Audit.Hits)
		{
			const FString Detail = Hit.Source + TEXT(" :: ") + Hit.PropertyPath;
			if (Adapter.ClassifyReference(Owner, Hit) == ERPGIdReferenceCapability::CoordinatorManaged)
			{
				++ManagedCount;
				if (Displayed++ < DisplayLimit)
				{
					Context += TEXT("\nCoordinator-managed reference: ") + Detail;
				}
				continue;
			}

			ClaimResult.References.Add(Detail);
			if (Displayed++ < DisplayLimit)
			{
				Context += TEXT("\nReference: ") + Detail;
			}
		}
		if (Displayed > DisplayLimit)
		{
			Context += FString::Printf(TEXT("\n... and %d more classified reference(s)."), Displayed - DisplayLimit);
		}
		for (const FText& Gap : Audit.CoverageGaps)
		{
			Context += TEXT("\nCoverage gap: ") + Gap.ToString();
		}
		ClaimResult.Context = FText::FromString(Context);

		const bool bBlocked = !ClaimResult.References.IsEmpty() || !Audit.CoverageGaps.IsEmpty();
		if (bBlocked)
		{
			ClaimResult.Code = ERPGIdClaimResult::ReferencesUnsupported;
		}
		ClaimResult.Message = Adapter.DescribeReferencePreflight(bBlocked, ManagedCount);
		return ClaimResult;
	}


}

FRPGIdClaimResult RPGIdClaimPrivate::ReadOwners(TMap<FSoftObjectPath, FName>& OutOwners)
{
	// Snapshot on explicit checks and validation only. Never cache across Apply or scan on Slate repaint.
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	if (Registry.IsLoadingAssets() || !Registry.IsSearchAllAssets())
	{
		return Result(ERPGIdClaimResult::InspectionIncomplete,
			LOCTEXT("Scanning", "Full mounted asset discovery has not completed. Check again after the editor finishes discovering assets."));
	}
	TMap<FSoftObjectPath, FName> Loaded;
	for (TObjectIterator<URPGPrimaryAsset> It; It; ++It)
	{
		if (IsWorkspaceOwner(*It))
		{
			Loaded.Add(FSoftObjectPath(*It), It->GetId().Id);
		}
	}
	FARFilter Filter;
	Filter.ClassPaths.Add(URPGPrimaryAsset::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Filter.bIncludeOnlyOnDiskAssets = true;
	TArray<FAssetData> Assets;
	if (!Registry.GetAssets(Filter, Assets, false))
	{
		return Result(ERPGIdClaimResult::InspectionIncomplete, LOCTEXT("QueryFailed", "The ownership registry query could not be completed."));
	}
	return MergeOwners(Assets, Loaded, OutOwners);
}

FRPGIdClaimResult RPGIdClaimPrivate::MergeOwners(const TArray<FAssetData>& Saved, const TMap<FSoftObjectPath, FName>& Loaded,
	TMap<FSoftObjectPath, FName>& OutOwners)
{
	TMap<FSoftObjectPath, FName> Merged = Loaded;
	for (const FAssetData& Data : Saved)
	{
		const FSoftObjectPath Path = Data.GetSoftObjectPath();
		if (Loaded.Contains(Path))
		{
			continue;
		}
		FString Id;
		if (!Data.IsValid() || !Data.GetTagValue(TEXT("RPGId"), Id) || Id.IsEmpty())
		{
			return Result(ERPGIdClaimResult::InspectionIncomplete,
				FText::Format(LOCTEXT("MissingTag", "Cannot inspect saved Claim for {0}. Load or resave that asset, then check again."),
					FText::FromString(Path.ToString())));
		}
		Merged.Add(Path, FName(*Id));
	}
	OutOwners = MoveTemp(Merged);
	return Result(ERPGIdClaimResult::Success, FText::GetEmpty());
}

FRPGIdClaimResult FRPGIdClaimEditor::Inspect(const TArray<TWeakObjectPtr<UObject>>& Owners)
{
	FRPGIdClaimResult Check = CheckOwner(Owners);
	if (!Check.CanApply())
	{
		return Check;
	}
	const URPGPrimaryAsset& Owner = *GetOwner(Owners);
	if (Owner.GetId().Id.IsNone())
	{
		return Result(ERPGIdClaimResult::Unclaimed, LOCTEXT("Unclaimed", "Unclaimed. Enter a candidate and Check; only Apply changes the asset."));
	}
	if (!ValidCandidate(Owner, Owner.GetId()))
	{
		return Result(ERPGIdClaimResult::InvalidFormatOrType,
			LOCTEXT("InvalidCurrent", "The existing Id has an invalid format or type. It has been preserved, not reset."));
	}
	FRPGIdClaimResult Collision = CheckCollision(Owner, Owner.GetId());
	if (!Collision.CanApply())
	{
		return Collision;
	}
	FRPGIdClaimResult Specific = RPGIdClaimPrivate::FindAdapter(Owner.GetClass())->PreviewOwner(Owner, Owner.GetId(), false);
	return Specific.Message.IsEmpty() && Specific.CanApply() ? Collision : Specific;
}

FRPGIdClaimResult FRPGIdClaimEditor::Preview(const FRPGIdClaimRequest& Request)
{
	FRPGIdClaimResult Check = CheckOwner(Request.Owners);
	if (!Check.CanApply())
	{
		return Check;
	}
	const URPGPrimaryAsset& Owner = *GetOwner(Request.Owners);
	if (Owner.GetId() != Request.ExpectedId)
	{
		return Result(ERPGIdClaimResult::StaleRequest, LOCTEXT("Stale", "The owner Id changed. Cancel and start a new draft."));
	}
	const FRPGIdClaimAdapter& Adapter = *RPGIdClaimPrivate::FindAdapter(Owner.GetClass());
	if (Request.Candidate == Request.ExpectedId)
	{
		FRPGIdClaimResult Current = Adapter.PreviewOwner(Owner, Request.Candidate, false);
		if (!Current.CanApply())
		{
			return Current;
		}
		Current.Code = ERPGIdClaimResult::NoChange;
		Current.Message = LOCTEXT("NoChange", "The Id is unchanged. No transaction or package modification is needed.");
		return Current;
	}
	FRPGIdClaimResult Operation = Adapter.CheckOperation(Request);
	FText ReleaseError;
	if (!FRPGReleaseSealService::CheckMutation(Request.ExpectedId, Request.Candidate, ReleaseError))
	{
		return Result(ERPGIdClaimResult::NotEditable, ReleaseError);
	}
	if (!Operation.CanApply())
	{
		return !Adapter.SupportsExistingChange() && !Request.ExpectedId.Id.IsNone()
			? PreviewChangeWithReferences(Owner, Adapter, MoveTemp(Operation)) : Operation;
	}
	const bool bExistingMutation = !Request.ExpectedId.Id.IsNone();
	if (bExistingMutation && Adapter.UsesPendingReservations())
	{
		FText CleanError;
		if (!FRPGIdPendingReservationCoordinator::CheckWorkspaceClean(CleanError))
		{
			return Result(ERPGIdClaimResult::NotEditable, CleanError);
		}
		if (!FRPGIdPendingReservationCoordinator::CheckOwnerReadyForMutation(Owner, CleanError))
		{
			return Result(ERPGIdClaimResult::NotEditable, CleanError);
		}
	}
	const bool bClearing = bExistingMutation && Request.Candidate.Id.IsNone();
	if (!bClearing && !ValidCandidate(Owner, Request.Candidate))
	{
		return Result(ERPGIdClaimResult::InvalidFormatOrType,
			FText::Format(LOCTEXT("Format", "Enter prefix {0} followed by exactly {1} digits. Free number-band selection is supported."),
				FText::FromName(Owner.GetAssetIdPrefix()), FText::AsNumber(GetDefault<URPGSettings>()->NumericLen)));
	}
	FRPGIdClaimResult Collision = Result(ERPGIdClaimResult::Success, FText::GetEmpty());
	if (!bClearing)
	{
		Collision = CheckCollision(Owner, Request.Candidate);
		if (!Collision.CanApply())
		{
			return Collision;
		}
		FText PendingError;
		if (!FRPGIdPendingReservationCoordinator::CheckCandidate(&Owner, Request.Candidate, PendingError))
		{
			return Result(ERPGIdClaimResult::Collision, PendingError);
		}
	}
	FRPGIdClaimResult Specific = Adapter.PreviewOwner(Owner, Request.Candidate, true);
	if (!Specific.CanApply())
	{
		return Specific;
	}
	if (Owner.GetId().IsValid())
	{
		return PreviewChangeWithReferences(Owner, Adapter, MoveTemp(Specific));
	}
	return Specific.Message.IsEmpty() ? Collision : Specific;
}

FRPGIdClaimResult FRPGIdClaimEditor::Execute(const FRPGIdClaimRequest& Request)
{
	if (Request.ExpectedIndexGeneration != 0 &&
		!FRPGIdBlueprintReferenceIndex::IsGenerationCurrent(Request.ExpectedIndexGeneration))
	{
		return Result(ERPGIdClaimResult::StaleRequest,
			LOCTEXT("IndexDraftStale", "The reference index changed since Check. Check again before Apply; nothing was modified."));
	}
	FRPGIdClaimResult Check = Preview(Request);
	if (Check.Code != ERPGIdClaimResult::Success)
	{
		return Check;
	}
	if (Request.ExpectedIndexGeneration != 0 && Check.IndexGeneration != Request.ExpectedIndexGeneration)
	{
		return Result(ERPGIdClaimResult::StaleRequest,
			LOCTEXT("IndexRecheckStale", "The reference index changed during Apply validation. Check again; nothing was modified."));
	}
	TGuardValue<bool> Guard(bExecutingClaim, true);
	URPGPrimaryAsset& Owner = *GetOwner(Request.Owners);
	FRPGIdClaimResult Applied = RPGIdClaimPrivate::FindAdapter(Owner.GetClass())->Apply(Owner, Request, [&]()
	{
		const bool bWasDirty = Owner.GetOutermost()->IsDirty();
		FScopedTransaction Transaction(LOCTEXT("Transaction", "Claim RPG Id"));
		Owner.Modify();
		Owner.Id = Request.Candidate;
		URPGAssetManager& Manager = URPGAssetManager::Get();
		Manager.RefreshAssetData(&Owner);
		const bool bMappingValid = Request.Candidate.IsValid()
			? Manager.GetPrimaryAssetPath(Owner.GetPrimaryAssetId()) == FSoftObjectPath(&Owner)
			: Manager.GetPrimaryAssetPath(Request.ExpectedId.ToPrimaryAssetId()).IsNull();
		if (!bMappingValid)
		{
			Owner.Id = Request.ExpectedId;
			Manager.RefreshAssetData(&Owner);
			Owner.GetOutermost()->SetDirtyFlag(bWasDirty);
			Transaction.Cancel();
			return Result(ERPGIdClaimResult::InspectionIncomplete,
				LOCTEXT("RefreshFailed", "AssetManager could not register this owner. The Claim was rolled back; nothing was saved."));
		}
		FPropertyChangedEvent Event(FindFProperty<FProperty>(URPGPrimaryAsset::StaticClass(), TEXT("Id")), EPropertyChangeType::ValueSet);
		Owner.PostEditChangeProperty(Event);
		return Result(ERPGIdClaimResult::Success,
			Request.ExpectedId.IsValid()
				? LOCTEXT("MutationApplied", "RPG Id changed in memory. Pending Save: save this asset normally; no references or other packages were modified.")
				: LOCTEXT("Applied", "Claim applied in memory. Save this asset normally; no other packages were saved."));
	});
	if (Applied.Code == ERPGIdClaimResult::Success)
	{
		FRPGIdPendingReservationCoordinator::RefreshOwner(Owner);
	}
	return Applied;
}

FRPGIdClaimResult FRPGIdClaimEditor::Suggest(const FRPGIdClaimRequest& Request, const FRPGIdCategorySelection* Selection)
{
	FRPGIdClaimResult Check = CheckOwner(Request.Owners);
	if (!Check.CanApply())
	{
		return Check;
	}
	const URPGPrimaryAsset& Owner = *GetOwner(Request.Owners);
	if (Owner.GetId() != Request.ExpectedId)
	{
		return Result(ERPGIdClaimResult::StaleRequest, LOCTEXT("SuggestStale", "The owner changed. Start a new draft."));
	}
	if (!RPGIdClaimPrivate::FindAdapter(Owner.GetClass())->SupportsExistingChange() && !Owner.GetId().Id.IsNone())
	{
		return Result(ERPGIdClaimResult::ReferencesUnsupported, LOCTEXT("SuggestExisting", "Suggest is available for initial Claim only on ordinary assets."));
	}
	FRPGIdCategoryScope Scope;
	FString CategoryError;
	const bool bCategories = GetDefault<URPGSettings>()->NumericLen == 4;
	if (bCategories && !FRPGIdCategoryStore::Get().ResolveSelection(Owner.GetAssetType(), Selection, Scope, CategoryError))
	{
		return Result(ERPGIdClaimResult::InspectionIncomplete, FText::FromString(CategoryError));
	}
	if (!bCategories && Selection)
	{
		return Result(ERPGIdClaimResult::InspectionIncomplete, LOCTEXT("CategoryFormat", "Category Suggest requires NumericLen=4."));
	}
	TMap<FSoftObjectPath, FName> Owners;
	Check = RPGIdClaimPrivate::ReadOwners(Owners);
	if (!Check.CanApply())
	{
		return Check;
	}
	TSet<FName> Used;
	FRPGReleaseHistory History;
	FText ReleaseError;
	if (!FRPGReleaseSealService::ReadHistory(History, ReleaseError))
	{
		return Result(ERPGIdClaimResult::InspectionIncomplete, ReleaseError);
	}
	Used.Append(History.ReservedIds);
	TSet<FName> PendingIds;
	FText PendingError;
	if (!FRPGIdPendingReservationCoordinator::ReadReservedIds(PendingIds, PendingError))
	{
		return Result(ERPGIdClaimResult::InspectionIncomplete, PendingError);
	}
	Used.Append(PendingIds);
	for (const auto& Entry : Owners)
	{
		Used.Add(Entry.Value);
	}
	if (bCategories)
	{
		FName Candidate;
		if (FRPGIdCategoryStore::FindAvailable(Scope, Owner.GetAssetIdPrefix(), Used, Candidate))
		{
			Check.SuggestedId = FRPGId(Candidate);
			Check.Message = LOCTEXT("CategorySuggested", "Candidate only, not reserved. Check and Apply recheck ownership.");
			return Check;
		}
		return Result(ERPGIdClaimResult::NoAvailableId, FText::FromString(Scope.DisplayName + TEXT(" is full. No available Id in this scope.")));
	}
	const int32 Digits = GetDefault<URPGSettings>()->NumericLen;
	int64 Capacity = 1;
	for (int32 Index = 0; Index < Digits; ++Index)
	{
		Capacity *= 10;
	}
	const int64 Start = ValidCandidate(Owner, Request.Candidate) ? Request.Candidate.GetNumeric() : 0;
	for (int64 Offset = 0; Offset <= Used.Num() && Start + Offset < Capacity; ++Offset)
	{
		const FString Number = FString::FromInt(static_cast<int32>(Start + Offset));
		const FName Candidate(*(Owner.GetAssetIdPrefix().ToString() + FString::ChrN(Digits - Number.Len(), TEXT('0')) + Number));
		if (!Used.Contains(Candidate))
		{
			Check.SuggestedId = FRPGId(Candidate);
			Check.Message = LOCTEXT("Suggested", "Candidate only, not reserved. Search starts at your draft number, never wraps. Apply rechecks ownership.");
			return Check;
		}
	}
	return Result(ERPGIdClaimResult::NoAvailableId,
		LOCTEXT("Exhausted", "No available Id at or above this starting number. Choose another number band; no wrap or overwrite was performed."));
}

bool FRPGIdClaimEditor::SupportsExistingChange(const TArray<TWeakObjectPtr<UObject>>& Owners)
{
	const URPGPrimaryAsset* Owner = GetOwner(Owners);
	const FRPGIdClaimAdapter* Adapter = Owner ? RPGIdClaimPrivate::FindAdapter(Owner->GetClass()) : nullptr;
	return Adapter && Adapter->SupportsExistingChange();
}

FText FRPGIdClaimEditor::GetPolicyText(const TArray<TWeakObjectPtr<UObject>>& Owners)
{
	const URPGPrimaryAsset* Owner = GetOwner(Owners);
	const FRPGIdClaimAdapter* Adapter = Owner ? RPGIdClaimPrivate::FindAdapter(Owner->GetClass()) : nullptr;
	return Adapter ? Adapter->GetPolicyText() : LOCTEXT("NoAdapter", "No Claim authoring adapter is registered for this owner.");
}

#undef LOCTEXT_NAMESPACE
