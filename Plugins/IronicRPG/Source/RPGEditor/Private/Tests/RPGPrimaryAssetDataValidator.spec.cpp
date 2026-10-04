// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "RPGPrimaryAssetDataValidator.h"

#include "Assets/RPGPrimaryAsset.h"
#include "Items/ItemAsset.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	void SetClaim(URPGPrimaryAsset& Asset, const FName Id)
	{
		FProperty* Property = FindFProperty<FProperty>(URPGPrimaryAsset::StaticClass(), TEXT("Id"));
		*Property->ContainerPtrToValuePtr<FRPGId>(&Asset) = FRPGId(Id);
	}

	FRPGPrimaryAssetValidationSubject ValidSubject(const FSoftObjectPath& Path, const FName Id = TEXT("i6001"))
	{
		FRPGPrimaryAssetValidationSubject Subject;
		Subject.AssetPath = Path;
		Subject.ClaimedId = Id;
		Subject.ExpectedPrefix = TEXT("i");
		Subject.NumericLength = 4;
		Subject.bSupportedOwner = true;
		Subject.bConfigurationValid = true;
		Subject.bFormatValid = true;
		return Subject;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGPrimaryAssetDuplicateIdentityTest,
	"IronicRPG.RPGId.Lifecycle.NormalDuplicateClearsClaimAndPIEPreserves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGPrimaryAssetDuplicateIdentityTest::RunTest(const FString& Parameters)
{
	UItemAsset* Source = NewObject<UItemAsset>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UItemAsset::StaticClass()));
	SetClaim(*Source, TEXT("i6001"));

	UItemAsset* NormalDuplicate = Cast<UItemAsset>(StaticDuplicateObject(Source, GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), UItemAsset::StaticClass()), RF_AllFlags, nullptr, EDuplicateMode::Normal));
	TestNotNull(TEXT("Normal duplicate is created"), NormalDuplicate);
	if (NormalDuplicate)
	{
		TestFalse(TEXT("Normal duplicate becomes an unclaimed authored owner"), NormalDuplicate->GetId().IsValid());
	}

	UItemAsset* PIEDuplicate = Cast<UItemAsset>(StaticDuplicateObject(Source, GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), UItemAsset::StaticClass()), RF_AllFlags, nullptr, EDuplicateMode::PIE));
	TestNotNull(TEXT("PIE duplicate is created"), PIEDuplicate);
	if (PIEDuplicate)
	{
		TestEqual(TEXT("PIE duplicate preserves runtime identity"), PIEDuplicate->GetId(), Source->GetId());
	}
	TestEqual(TEXT("Source identity is never changed"), Source->GetId(), FRPGId(TEXT("i6001")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGPrimaryAssetReleaseValidationTest,
	"IronicRPG.RPGId.Validation.ReleaseOwnershipRejectsInvalidDuplicateAndIncomplete",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGPrimaryAssetReleaseValidationTest::RunTest(const FString& Parameters)
{
	const FSoftObjectPath SubjectPath(TEXT("/Game/DataAssets/Item/Subject.Subject"));
	FRPGPrimaryAssetValidationSubject Subject = ValidSubject(SubjectPath);
	TMap<FSoftObjectPath, FName> Owners;
	Owners.Add(SubjectPath, Subject.ClaimedId);
	Owners.Add(FSoftObjectPath(TEXT("/Game/DataAssets/Item/Other.Other")), Subject.ClaimedId);
	const FRPGPrimaryAssetValidationResult Duplicate = URPGPrimaryAssetDataValidator::BuildValidationResult(
		Subject, Owners, FText::GetEmpty(), true);
	TestFalse(TEXT("Both duplicate owners are release-invalid"), Duplicate.IsValid());
	TestEqual(TEXT("The other owner is identified once"), Duplicate.Errors.Num(), 1);

	Subject.bFormatValid = false;
	Owners.Remove(FSoftObjectPath(TEXT("/Game/DataAssets/Item/Other.Other")));
	const FRPGPrimaryAssetValidationResult Invalid = URPGPrimaryAssetDataValidator::BuildValidationResult(
		Subject, Owners, FText::GetEmpty(), true);
	TestFalse(TEXT("Invalid format is rejected"), Invalid.IsValid());
	TestEqual(TEXT("Invalid format has one focused error"), Invalid.Errors.Num(), 1);

	Subject = ValidSubject(SubjectPath, NAME_None);
	Owners[SubjectPath] = NAME_None;
	const FRPGPrimaryAssetValidationResult SaveDraft = URPGPrimaryAssetDataValidator::BuildValidationResult(
		Subject, Owners, FText::GetEmpty(), false);
	TestTrue(TEXT("Interactive Save permits an unclaimed draft"), SaveDraft.IsValid());
	TestEqual(TEXT("Interactive Save still exposes the missing Claim"), SaveDraft.Warnings.Num(), 1);
	const FRPGPrimaryAssetValidationResult ReleaseDraft = URPGPrimaryAssetDataValidator::BuildValidationResult(
		Subject, Owners, FText::GetEmpty(), true);
	TestFalse(TEXT("Release validation rejects an unclaimed authored owner"), ReleaseDraft.IsValid());

	Subject = ValidSubject(SubjectPath);
	Owners[SubjectPath] = Subject.ClaimedId;
	const FRPGPrimaryAssetValidationResult Incomplete = URPGPrimaryAssetDataValidator::BuildValidationResult(
		Subject, Owners, FText::FromString(TEXT("Registry discovery incomplete.")), true);
	TestFalse(TEXT("Incomplete ownership discovery fails closed"), Incomplete.IsValid());
	TestTrue(TEXT("Incomplete discovery reason is retained"), Incomplete.Errors[0].ToString().Contains(TEXT("Registry discovery incomplete")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGPrimaryAssetValidationUsecaseTest,
	"IronicRPG.RPGId.Validation.UnclaimedDraftIsSaveOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGPrimaryAssetValidationUsecaseTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Interactive Save allows a None draft with a warning"),
		URPGPrimaryAssetDataValidator::ShouldRequireClaim(EDataValidationUsecase::Save, false));
	TestTrue(TEXT("Cook keeps the hard gate even when validation use case is Save"),
		URPGPrimaryAssetDataValidator::ShouldRequireClaim(EDataValidationUsecase::Save, true));
	TestTrue(TEXT("Manual validation requires a Claim"),
		URPGPrimaryAssetDataValidator::ShouldRequireClaim(EDataValidationUsecase::Manual, false));
	TestTrue(TEXT("Commandlet validation requires a Claim"),
		URPGPrimaryAssetDataValidator::ShouldRequireClaim(EDataValidationUsecase::Commandlet, false));
	TestTrue(TEXT("Pre-submit validation requires a Claim"),
		URPGPrimaryAssetDataValidator::ShouldRequireClaim(EDataValidationUsecase::PreSubmit, false));
	return true;
}

#endif
