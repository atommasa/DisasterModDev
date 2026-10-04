// Copyright Ironic Studio. All Rights Reserved.

#include "RPGPrimaryAssetDataValidator.h"

#include "RPGIdClaimSnapshot.h"

#include "Abilities/AbilityAsset.h"
#include "Assets/RPGAssetManager.h"
#include "Assets/RPGPrimaryAsset.h"
#include "Characters/CharacterAsset.h"
#include "Items/ItemAsset.h"
#include "Levels/GameZoneAsset.h"
#include "Levels/MapMarkerTypeAsset.h"
#include "Misc/DataValidation.h"
#include "RPGSettings.h"

#define LOCTEXT_NAMESPACE "RPGPrimaryAssetDataValidator"

namespace
{
	bool IsSupportedNativeOwner(const URPGPrimaryAsset& Asset)
	{
		const UClass* Class = Asset.GetClass();
		return Class == UGameZoneAsset::StaticClass() || Class == UCharacterAsset::StaticClass() || Class == UItemAsset::StaticClass()
			|| Class == UAbilityAsset::StaticClass() || Class == USubGameZoneAsset::StaticClass() || Class == UMapMarkerTypeAsset::StaticClass();
	}

	bool HasValidConfiguration(const URPGPrimaryAsset& Asset)
	{
		const URPGAssetManager* Manager = GEngine ? Cast<URPGAssetManager>(GEngine->AssetManager) : nullptr;
		const URPGSettings* Settings = GetDefault<URPGSettings>();
		const FName Type = Asset.GetAssetType();
		const FName Prefix = Asset.GetAssetIdPrefix();
		if (!Manager || Settings->NumericLen < 1 || Settings->NumericLen > 9 || Settings->MaxPrefixLen < 1 || Prefix.IsNone()
			|| Prefix.ToString().Len() > Settings->MaxPrefixLen || Manager->GetAssetTypeClass(Type) != Asset.GetClass()
			|| Manager->GetIdPrefix(Type) != Prefix || Manager->GetIdTypeFromPrefix(Prefix) != Type)
		{
			return false;
		}

		FPrimaryAssetTypeInfo TypeInfo;
		const FString PackageName = Asset.GetOutermost()->GetName();
		const FString SupportedRoot = TEXT("/Game/DataAssets/") + Type.ToString() + TEXT("/");
		return Manager->GetPrimaryAssetTypeInfo(Type, TypeInfo) && !TypeInfo.bHasBlueprintClasses && TypeInfo.AssetBaseClassLoaded == Asset.GetClass()
			&& PackageName.StartsWith(SupportedRoot) && TypeInfo.AssetScanPaths.ContainsByPredicate([&PackageName](const FString& Path)
			{
				return PackageName == Path || PackageName.StartsWith(Path.EndsWith(TEXT("/")) ? Path : Path + TEXT("/"));
			});
	}

	bool HasValidFormat(const URPGPrimaryAsset& Asset)
	{
		if (!Asset.GetId().IsValid())
		{
			return false;
		}
		const FString Value = Asset.GetId().ToString();
		const FString Prefix = Asset.GetAssetIdPrefix().ToString();
		const int32 NumericLength = GetDefault<URPGSettings>()->NumericLen;
		if (Value.Len() != Prefix.Len() + NumericLength || !Value.StartsWith(Prefix, ESearchCase::IgnoreCase))
		{
			return false;
		}
		for (const TCHAR Character : Value.Right(NumericLength))
		{
			if (Character < TEXT('0') || Character > TEXT('9'))
			{
				return false;
			}
		}
		return Asset.GetId().GetRebuiltIdType() == Asset.GetAssetType();
	}

	FRPGPrimaryAssetValidationSubject MakeSubject(const URPGPrimaryAsset& Asset)
	{
		FRPGPrimaryAssetValidationSubject Subject;
		Subject.AssetPath = FSoftObjectPath(&Asset);
		Subject.ClaimedId = Asset.GetId().Id;
		Subject.ExpectedPrefix = Asset.GetAssetIdPrefix();
		Subject.NumericLength = GetDefault<URPGSettings>()->NumericLen;
		Subject.bSupportedOwner = IsSupportedNativeOwner(Asset);
		Subject.bConfigurationValid = HasValidConfiguration(Asset);
		Subject.bFormatValid = HasValidFormat(Asset);
		return Subject;
	}
}

URPGPrimaryAssetDataValidator::URPGPrimaryAssetDataValidator()
{
	bOnlyPrintCustomMessage = true;
}

FRPGPrimaryAssetValidationResult URPGPrimaryAssetDataValidator::BuildValidationResult(
	const FRPGPrimaryAssetValidationSubject& Subject, const TMap<FSoftObjectPath, FName>& WorkspaceOwners,
	const FText& SnapshotFailure, const bool bRequireClaim)
{
	FRPGPrimaryAssetValidationResult Result;
	if (!SnapshotFailure.IsEmpty())
	{
		Result.Errors.Add(FText::Format(LOCTEXT("SnapshotFailure", "RPG Id ownership could not be verified: {0}"), SnapshotFailure));
	}
	if (!Subject.bSupportedOwner)
	{
		Result.Errors.Add(LOCTEXT("UnsupportedOwner", "This RPGPrimaryAsset owner class is not supported by the native Claim lifecycle."));
	}
	if (!Subject.bConfigurationValid)
	{
		Result.Errors.Add(LOCTEXT("InvalidConfiguration", "The owner class, asset path, type or prefix registration is inconsistent."));
	}

	if (Subject.ClaimedId.IsNone())
	{
		const FText Message = LOCTEXT("MissingClaim", "This authored RPGPrimaryAsset has no RPG Id Claim.");
		(bRequireClaim ? Result.Errors : Result.Warnings).Add(Message);
		return Result;
	}
	if (!Subject.bFormatValid)
	{
		Result.Errors.Add(FText::Format(LOCTEXT("InvalidFormat", "RPG Id must use prefix {0} followed by exactly {1} digits and rebuild to the owner's type."),
			FText::FromName(Subject.ExpectedPrefix), FText::AsNumber(Subject.NumericLength)));
	}

	TArray<FSoftObjectPath> Conflicts;
	for (const auto& Entry : WorkspaceOwners)
	{
		if (Entry.Key != Subject.AssetPath && Entry.Value == Subject.ClaimedId)
		{
			Conflicts.Add(Entry.Key);
		}
	}
	Conflicts.Sort([](const FSoftObjectPath& A, const FSoftObjectPath& B) { return A.ToString() < B.ToString(); });
	for (const FSoftObjectPath& Conflict : Conflicts)
	{
		Result.Errors.Add(FText::Format(LOCTEXT("DuplicateClaim", "RPG Id {0} is also claimed by {1}. Neither owner was modified."),
			FText::FromName(Subject.ClaimedId), FText::FromString(Conflict.ToString())));
	}
	return Result;
}

bool URPGPrimaryAssetDataValidator::ShouldRequireClaim(const EDataValidationUsecase ValidationUsecase, const bool bIsCookCommandlet)
{
	return ValidationUsecase != EDataValidationUsecase::Save || bIsCookCommandlet;
}

bool URPGPrimaryAssetDataValidator::CanValidateAsset_Implementation(const FAssetData&, UObject* InObject, FDataValidationContext&) const
{
	const URPGPrimaryAsset* Asset = Cast<URPGPrimaryAsset>(InObject);
	return Asset && Asset->IsAsset() && !Asset->IsTemplate() && !Asset->HasAnyFlags(RF_Transient)
		&& !Asset->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor);
}

EDataValidationResult URPGPrimaryAssetDataValidator::ValidateLoadedAsset_Implementation(const FAssetData&, UObject* InAsset,
	FDataValidationContext& Context)
{
	URPGPrimaryAsset* Asset = CastChecked<URPGPrimaryAsset>(InAsset);
	TMap<FSoftObjectPath, FName> WorkspaceOwners;
	const FRPGIdClaimResult Snapshot = RPGIdClaimPrivate::ReadOwners(WorkspaceOwners);
	const FRPGPrimaryAssetValidationResult Result = BuildValidationResult(MakeSubject(*Asset), WorkspaceOwners,
		Snapshot.CanApply() ? FText::GetEmpty() : Snapshot.Message, ShouldRequireClaim(Context.GetValidationUsecase(), IsRunningCookCommandlet()));

	for (const FText& Warning : Result.Warnings)
	{
		AssetWarning(Asset, Warning);
	}
	if (Result.IsValid())
	{
		AssetPasses(Asset);
		return EDataValidationResult::Valid;
	}
	for (const FText& Error : Result.Errors)
	{
		AssetFails(Asset, Error);
	}
	return EDataValidationResult::Invalid;
}

#undef LOCTEXT_NAMESPACE
