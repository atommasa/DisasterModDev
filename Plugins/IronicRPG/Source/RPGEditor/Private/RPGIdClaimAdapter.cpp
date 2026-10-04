// Copyright Ironic Studio. All Rights Reserved.

#include "RPGIdClaimAdapter.h"
#include "RPGIdReferenceAudit.h"
#include "GameZoneBindingCoordinator.h"
#include "Abilities/AbilityAsset.h"
#include "Characters/CharacterAsset.h"
#include "Items/ItemAsset.h"
#include "Levels/GameZoneAsset.h"
#include "Levels/MapMarkerTypeAsset.h"
#include "Engine/World.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "RPGIdClaimEditor"

namespace
{
	FRPGIdClaimResult Result(ERPGIdClaimResult Code, const FText& Message)
	{
		FRPGIdClaimResult Value;
		Value.Code = Code;
		Value.Message = Message;
		return Value;
	}

	ERPGIdClaimResult ToClaimCode(const EGameZoneBindingIssue Issue)
	{
		if (Issue == EGameZoneBindingIssue::DuplicateAssetId || Issue == EGameZoneBindingIssue::DuplicateLevelTarget)
		{
			return ERPGIdClaimResult::Collision;
		}
		if (Issue == EGameZoneBindingIssue::InvalidAssetId)
		{
			return ERPGIdClaimResult::InvalidFormatOrType;
		}
		return ERPGIdClaimResult::InspectionIncomplete;
	}

	FRPGIdClaimResult ZoneFailure(const FGameZoneBindingMutationResult& Failure)
	{
		return Result(ToClaimCode(Failure.Issue), Failure.Message);
	}

	void AddZoneContext(FRPGIdClaimResult& ClaimResult, const UGameZoneAsset& ZoneAsset, const bool bApplyingChange)
	{
		const FSoftObjectPath LevelPath = ZoneAsset.GetLevelToLoad().ToSoftObjectPath();
		if (LevelPath.IsNull())
		{
			ClaimResult.Context = FText::Format(
				LOCTEXT("ZoneUnboundContext", "GameZone adapter | Unbound | Zone Asset save: {0} | Level save: not applicable | Bake: not applicable"),
				bApplyingChange || ZoneAsset.GetOutermost()->IsDirty() ? LOCTEXT("Pending", "pending") : LOCTEXT("Clean", "clean"));
			return;
		}

		const UWorld* World = Cast<UWorld>(LevelPath.ResolveObject());
		const bool bLevelDirty = bApplyingChange || (World && World->GetOutermost()->IsDirty());
		const bool bBakePending = bApplyingChange || ZoneAsset.GetBindingVerificationStatus() != EGameZoneBindingVerificationStatus::Verified;
		ClaimResult.Context = FText::Format(
			LOCTEXT("ZoneBoundContext", "GameZone adapter | Exact pair: {0} | Zone Asset save: {1} | Level save: {2} | Bake: {3}"),
			FText::FromString(LevelPath.GetLongPackageName()),
			bApplyingChange || ZoneAsset.GetOutermost()->IsDirty() ? LOCTEXT("Pending", "pending") : LOCTEXT("Clean", "clean"),
			bLevelDirty ? LOCTEXT("Pending", "pending") : LOCTEXT("Clean", "clean"),
			bBakePending ? LOCTEXT("Required", "required") : LOCTEXT("Current", "current"));
	}

	FRPGIdClaimResult PreviewZone(const UGameZoneAsset& ZoneAsset, const FRPGId& Candidate, const bool bApplyingChange)
	{
		const FGameZoneBindingMutationResult Binding = FGameZoneBindingCoordinator::PreviewZoneIdChange(ZoneAsset, Candidate);
		if (!Binding.IsSuccess())
		{
			return ZoneFailure(Binding);
		}
		FRPGIdClaimResult ClaimResult = Result(ERPGIdClaimResult::Success,
			bApplyingChange
				? LOCTEXT("ZoneReady", "Exact GameZone binding verified. Apply will update both sides in one transaction; save the Level and Zone Asset to converge Bake data.")
				: LOCTEXT("ZoneCurrent", "GameZone Id and binding are valid in the current workspace."));
		AddZoneContext(ClaimResult, ZoneAsset, bApplyingChange);
		return ClaimResult;
	}

	class FOrdinaryClaimAdapter final : public FRPGIdClaimAdapter
	{
	public:
		virtual bool SupportsExistingChange() const override { return true; }

		virtual FText GetPolicyText() const override
		{
			return LOCTEXT("OrdinaryPolicy", "Change / Clear requires an unsealed Id, a clean authored workspace and a complete zero-reference audit. "
				"Apply changes only this asset in memory and leaves Pending Save. Connected runtime values and old player saves are outside this development guarantee.");
		}

		virtual FText DescribeReferencePreflight(bool bBlocked, int32 ManagedCount) const override
		{
			return bBlocked
				? LOCTEXT("OrdinaryReferencesBlocked", "Change / Clear is blocked because stored development references or coverage gaps remain. No asset was changed.")
				: LOCTEXT("OrdinaryReferencesClear", "No stored development references were found. Apply will change only this asset in memory and leave Pending Save.");
		}

		virtual FRPGIdClaimResult CheckOperation(const FRPGIdClaimRequest& Request) const override
		{
			return Result(ERPGIdClaimResult::Success, FText::GetEmpty());
		}
	};

	class FGameZoneClaimAdapter final : public FRPGIdClaimAdapter
	{
	public:
		virtual bool SupportsExistingChange() const override { return true; }
		virtual bool UsesPendingReservations() const override { return false; }

		virtual FText GetPolicyText() const override
		{
			return LOCTEXT("ZonePolicy", "GameZone Change uses the exact binding adapter. Clear remains disabled; Legacy migration stays a separate operation.");
		}

		virtual FText DescribeReferencePreflight(bool bBlocked, int32 ManagedCount) const override
		{
			return bBlocked ? LOCTEXT("ZoneReferences",
				"Exact GameZone binding verified, but Change remains disabled because development references or coverage gaps cannot be migrated safely.")
				: FText::Format(LOCTEXT("ZoneReferencesComplete",
					"Exact GameZone binding and reference preflight verified. Apply will update both sides and {0} Bake-owned reference(s)."),
					FText::AsNumber(ManagedCount));
		}

		virtual FRPGIdClaimResult CheckOperation(const FRPGIdClaimRequest& Request) const override
		{
			return Request.Candidate.Id.IsNone()
				? Result(ERPGIdClaimResult::ReferencesUnsupported,
					LOCTEXT("ZoneClear", "Clearing a GameZone Id is disabled. Unbind or repair the Level pair through the dedicated binding workflow."))
				: Result(ERPGIdClaimResult::Success, FText::GetEmpty());
		}

		virtual FRPGIdClaimResult PreviewOwner(const URPGPrimaryAsset& Owner, const FRPGId& Candidate, bool bChanging) const override
		{
			return PreviewZone(*CastChecked<UGameZoneAsset>(&Owner), Candidate, bChanging);
		}

		virtual ERPGIdReferenceCapability ClassifyReference(const URPGPrimaryAsset& Owner, const FRPGIdReferenceHit& Hit) const override
		{
			return RPGIdClaimPrivate::IsCoordinatorManagedZoneReference(*CastChecked<UGameZoneAsset>(&Owner), Hit)
				? ERPGIdReferenceCapability::CoordinatorManaged : ERPGIdReferenceCapability::Blocking;
		}

		virtual FRPGIdClaimResult Apply(URPGPrimaryAsset& Owner, const FRPGIdClaimRequest& Request,
			TFunctionRef<FRPGIdClaimResult()> ApplyOwnerClaim) const override
		{
			UGameZoneAsset* ZoneAsset = CastChecked<UGameZoneAsset>(&Owner);
			FGameZoneBindingPropertyChangeRequest BindingRequest;
			BindingRequest.ZoneAsset = ZoneAsset;
			BindingRequest.Change = EGameZoneBindingPropertyChange::ChangeZoneIdCommand;
			BindingRequest.PreviousLevel = ZoneAsset->GetLevelToLoad().ToSoftObjectPath();
			BindingRequest.PreviousZoneId = Request.ExpectedId;
			BindingRequest.PreviousBindingId = ZoneAsset->GetGameZoneBindingId();
			BindingRequest.ProposedZoneId = Request.Candidate;
			BindingRequest.PreviousVerificationStatus = ZoneAsset->GetBindingVerificationStatus();
			const FGameZoneBindingMutationResult BindingResult = FGameZoneBindingCoordinator::ApplyPropertyChange(BindingRequest);
			if (!BindingResult.IsSuccess())
			{
				return ZoneFailure(BindingResult);
			}
			FRPGIdClaimResult Applied = Result(ERPGIdClaimResult::Success,
				LOCTEXT("ZoneApplied", "GameZone Id changed on the Asset and exact Level pair. Save the Level; its save pipeline will Bake and save the Zone Asset."));
			AddZoneContext(Applied, *ZoneAsset, true);
			return Applied;
		}
	};
}

bool RPGIdClaimPrivate::IsCoordinatorManagedZoneReference(const UGameZoneAsset& ZoneAsset, const FRPGIdReferenceHit& Hit)
{
	const FString ZoneAssetSource = FSoftObjectPath(&ZoneAsset).ToString();
	if (Hit.Source == ZoneAssetSource && Hit.PropertyPath.StartsWith(TEXT("BakedPoints{"))
		&& Hit.PropertyPath.EndsWith(TEXT("}.Value.ZoneId")))
	{
		return true;
	}

	const UWorld* World = Cast<UWorld>(ZoneAsset.GetLevelToLoad().ToSoftObjectPath().ResolveObject());
	const AWorldSettings* WorldSettings = World ? World->GetWorldSettings() : nullptr;
	return WorldSettings && Hit.Source == FSoftObjectPath(WorldSettings).ToString() && Hit.PropertyPath == TEXT("GameZoneId");
}


FRPGIdClaimResult FRPGIdClaimAdapter::CheckOperation(const FRPGIdClaimRequest& Request) const
{
	return Result(Request.ExpectedId.Id.IsNone() ? ERPGIdClaimResult::Success : ERPGIdClaimResult::ReferencesUnsupported,
		Request.ExpectedId.Id.IsNone() ? FText::GetEmpty()
			: LOCTEXT("UnsupportedMutation", "Existing Id mutation is not supported by this Claim adapter."));
}

FText FRPGIdClaimAdapter::GetPolicyText() const
{
	return LOCTEXT("UnsupportedPolicy", "This Claim adapter does not support changing an existing Id.");
}

FText FRPGIdClaimAdapter::DescribeReferencePreflight(bool bBlocked, int32 ManagedCount) const
{
	return LOCTEXT("UnsupportedReferences", "Existing Id mutation is not supported by this Claim adapter.");
}

FRPGIdClaimResult FRPGIdClaimAdapter::PreviewOwner(const URPGPrimaryAsset& Owner, const FRPGId& Candidate, bool bChanging) const
{
	return Result(ERPGIdClaimResult::Success, FText::GetEmpty());
}

ERPGIdReferenceCapability FRPGIdClaimAdapter::ClassifyReference(const URPGPrimaryAsset& Owner, const FRPGIdReferenceHit& Hit) const
{
	return ERPGIdReferenceCapability::Blocking;
}

FRPGIdClaimResult FRPGIdClaimAdapter::Apply(URPGPrimaryAsset& Owner, const FRPGIdClaimRequest& Request,
	TFunctionRef<FRPGIdClaimResult()> ApplyOwnerClaim) const
{
	return ApplyOwnerClaim();
}

const FRPGIdClaimAdapter* RPGIdClaimPrivate::FindAdapter(const UClass* OwnerClass)
{
	static const FOrdinaryClaimAdapter Ordinary;
	static const FGameZoneClaimAdapter GameZone;
	// Immutable registrations avoid module startup ordering and accidental broad class fallback.
	const TPair<const UClass*, const FRPGIdClaimAdapter*> Registrations[] = {
		{ UItemAsset::StaticClass(), &Ordinary },
		{ UCharacterAsset::StaticClass(), &Ordinary },
		{ UAbilityAsset::StaticClass(), &Ordinary },
		{ UMapMarkerTypeAsset::StaticClass(), &Ordinary },
		{ UGameZoneAsset::StaticClass(), &GameZone }
	};
	for (const auto& Registration : Registrations)
	{
		if (Registration.Key == OwnerClass)
		{
			return Registration.Value;
		}
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
