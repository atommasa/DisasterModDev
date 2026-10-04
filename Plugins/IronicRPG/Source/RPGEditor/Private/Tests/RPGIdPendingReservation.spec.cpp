// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "RPGIdPendingReservationCoordinator.h"
#include "RPGIdClaimEditor.h"
#include "RPGIdClaimSnapshot.h"

#include "Abilities/AbilityAsset.h"
#include "Assets/RPGAssetManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Characters/CharacterAsset.h"
#include "Characters/CharacterDataTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "UObject/Package.h"
#include "UObject/PropertyAccessUtil.h"
#include "UObject/SavePackage.h"

namespace
{
	template <typename ValueType>
	ValueType& GetPropertyValue(UObject& Object, const FName PropertyName)
	{
		FProperty* Property = FindFProperty<FProperty>(Object.GetClass(), PropertyName);
		check(Property);
		return *Property->ContainerPtrToValuePtr<ValueType>(&Object);
	}

	FRPGId FindFreeAbilityId()
	{
		TMap<FSoftObjectPath, FName> Owners;
		if (!RPGIdClaimPrivate::ReadOwners(Owners).CanApply())
		{
			return {};
		}
		TSet<FName> Used;
		for (const TPair<FSoftObjectPath, FName>& Entry : Owners)
		{
			Used.Add(Entry.Value);
		}
		for (int32 Number = 9800; Number < 9899; ++Number)
		{
			const FName Candidate(*FString::Printf(TEXT("a%04d"), Number));
			const FName Next(*FString::Printf(TEXT("a%04d"), Number + 1));
			if (!Used.Contains(Candidate) && !Used.Contains(Next))
			{
				return FRPGId(Candidate);
			}
		}
		return {};
	}

	bool SaveAsset(UAbilityAsset& Asset, const FString& Filename)
	{
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Asset.GetOutermost(), &Asset, *Filename, Args);
	}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdPendingReservationTest,
	"IronicRPG.RPGId.PendingReservation.SavedBaselineUndoAndConvergence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdPendingReservationTest::RunTest(const FString&)
{
	FRPGIdPendingReservationCoordinator::ResetForTests();
	const FRPGId SavedId = FindFreeAbilityId();
	if (!TestTrue(TEXT("A free saved Id exists"), SavedId.IsValid()))
	{
		return false;
	}
	const FRPGId NewId(FName(*FString::Printf(TEXT("a%04d"), SavedId.GetNumeric() + 1)));
	const FString Token = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	const FString PackageName = TEXT("/Game/DataAssets/Ability/automation_pending_") + Token;
	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	UPackage* Package = CreatePackage(*PackageName);
	UAbilityAsset* Asset = NewObject<UAbilityAsset>(Package, *FPackageName::GetLongPackageAssetName(PackageName),
		RF_Public | RF_Standalone | RF_Transactional);
	GetPropertyValue<FRPGId>(*Asset, TEXT("Id")) = SavedId;
	FAssetRegistryModule::AssetCreated(Asset);
	if (!TestTrue(TEXT("Saved baseline fixture"), SaveAsset(*Asset, Filename)))
	{
		FAssetRegistryModule::AssetDeleted(Asset);
		return false;
	}
	IAssetRegistry::GetChecked().ScanModifiedAssetFiles({ Filename });
	Package->SetDirtyFlag(false);

	GetPropertyValue<FRPGId>(*Asset, TEXT("Id")) = NewId;
	URPGAssetManager::Get().RefreshAssetData(Asset);
	Package->MarkPackageDirty();
	FRPGIdPendingReservationCoordinator::RefreshOwner(*Asset);
	TSet<FName> Reserved;
	FText Error;
	TestTrue(TEXT("Pending reservations are readable"), FRPGIdPendingReservationCoordinator::ReadReservedIds(Reserved, Error));
	TestTrue(TEXT("The saved old Id remains reserved"), Reserved.Contains(SavedId.Id));
	TestTrue(TEXT("The same owner may revert to its saved Id"), FRPGIdPendingReservationCoordinator::CheckCandidate(Asset, SavedId, Error));
	const FString OtherName = TEXT("pending_candidate_") + Token;
	UPackage* OtherPackage = CreatePackage(*(TEXT("/Game/DataAssets/Ability/") + OtherName));
	UAbilityAsset* Other = NewObject<UAbilityAsset>(OtherPackage, *OtherName, RF_Public | RF_Standalone | RF_Transactional);
	FAssetRegistryModule::AssetCreated(Other);
	OtherPackage->SetDirtyFlag(false);
	TestFalse(TEXT("Another Claim cannot take the pending old Id"),
		FRPGIdPendingReservationCoordinator::CheckCandidate(Other, SavedId, Error));
	const FRPGIdClaimRequest CandidateRequest{ { Other }, {}, SavedId };
	TestEqual(TEXT("The common Claim pipeline rejects a pending Id"), FRPGIdClaimEditor::Preview(CandidateRequest).Code,
		ERPGIdClaimResult::Collision);
	TestNotEqual(TEXT("Suggest skips the pending old Id"), FRPGIdClaimEditor::Suggest(CandidateRequest).SuggestedId, SavedId);
	TestFalse(TEXT("Dirty authored workspace blocks existing mutation"), FRPGIdPendingReservationCoordinator::CheckWorkspaceClean(Error));

	GetPropertyValue<FRPGId>(*Asset, TEXT("Id")) = SavedId;
	URPGAssetManager::Get().RefreshAssetData(Asset);
	FRPGIdPendingReservationCoordinator::RefreshOwner(*Asset);
	TestTrue(TEXT("Reservations remain readable after an Undo-shaped revert"),
		FRPGIdPendingReservationCoordinator::ReadReservedIds(Reserved, Error));
	TestFalse(TEXT("Reverting to the saved Id removes the redundant reservation"), Reserved.Contains(SavedId.Id));

	GetPropertyValue<FRPGId>(*Asset, TEXT("Id")) = NewId;
	URPGAssetManager::Get().RefreshAssetData(Asset);
	FRPGIdPendingReservationCoordinator::RefreshOwner(*Asset);
	TestTrue(TEXT("Redo-shaped divergence restores the reservation"),
		FRPGIdPendingReservationCoordinator::ReadReservedIds(Reserved, Error) && Reserved.Contains(SavedId.Id));
	const FString ReferenceName = TEXT("pending_reference_") + Token;
	UPackage* ReferencePackage = CreatePackage(*(TEXT("/Game/DataAssets/Character/") + ReferenceName));
	UCharacterAsset* ReferenceAsset = NewObject<UCharacterAsset>(ReferencePackage, *ReferenceName,
		RF_Public | RF_Standalone | RF_Transactional);
	GetPropertyValue<FCharacterSaveData>(*ReferenceAsset, TEXT("DefaultData")).EquippedAbilities.Add(FGameplayTag(), SavedId);
	FAssetRegistryModule::AssetCreated(ReferenceAsset);
	ReferencePackage->SetDirtyFlag(false);
	TestTrue(TEXT("Saved changed identity"), SaveAsset(*Asset, Filename));
	IAssetRegistry::GetChecked().ScanModifiedAssetFiles({ Filename });
	Package->SetDirtyFlag(false);
	FRPGIdPendingReservationCoordinator::RefreshOwner(*Asset);
	TestTrue(TEXT("Reservations remain readable after save convergence"),
		FRPGIdPendingReservationCoordinator::ReadReservedIds(Reserved, Error));
	TestTrue(TEXT("A saved owner retains the old Id while a stored reference remains"), Reserved.Contains(SavedId.Id));
	const FRPGId ThirdId(FName(*FString::Printf(TEXT("a%04d"), SavedId.GetNumeric() + 2)));
	const FRPGIdClaimRequest RepeatRequest{ { Asset }, NewId, ThirdId };
	const FRPGIdClaimResult RepeatPreview = FRPGIdClaimEditor::Preview(RepeatRequest);
	TestEqual(TEXT("An unresolved owner reservation blocks a second identity mutation"), RepeatPreview.Code, ERPGIdClaimResult::NotEditable);
	TestTrue(TEXT("The unresolved previous Id is identified"), RepeatPreview.Message.ToString().Contains(SavedId.ToString()));
	TestEqual(TEXT("Blocked repeat mutation preserves the current Id"), Asset->GetId(), NewId);
	GetPropertyValue<FCharacterSaveData>(*ReferenceAsset, TEXT("DefaultData")).EquippedAbilities.Reset();
	ReferencePackage->SetDirtyFlag(false);
	FRPGIdPendingReservationCoordinator::RefreshOwner(*Asset);
	TestTrue(TEXT("Reservations remain readable after the final reference is removed"),
		FRPGIdPendingReservationCoordinator::ReadReservedIds(Reserved, Error));
	TestFalse(TEXT("A clean saved owner releases the old Id after a complete zero-reference audit"), Reserved.Contains(SavedId.Id));
	TestTrue(TEXT("The released old Id can be claimed again"), FRPGIdPendingReservationCoordinator::CheckCandidate(Other, SavedId, Error));

	FAssetRegistryModule::AssetDeleted(ReferenceAsset);
	ReferenceAsset->ClearFlags(RF_Public | RF_Standalone);
	ReferenceAsset->SetFlags(RF_Transient);
	ReferencePackage->SetDirtyFlag(false);
	FAssetRegistryModule::AssetDeleted(Other);
	Other->ClearFlags(RF_Public | RF_Standalone);
	Other->SetFlags(RF_Transient);
	OtherPackage->SetDirtyFlag(false);
	ObjectTools::DeleteObjectsUnchecked({ Asset });
	FRPGIdPendingReservationCoordinator::ResetForTests();
	TestFalse(TEXT("Fixture package was removed"), IFileManager::Get().FileExists(*Filename));
	return true;
}

#endif
