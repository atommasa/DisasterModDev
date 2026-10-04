// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Levels/GameZoneAsset.h"
#include "Levels/GameZoneBinding.h"
#include "Levels/RPGWorldSettings.h"

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"

namespace
{
    template <typename ValueType>
    ValueType& GetPropertyValue(UObject& Object, const FName PropertyName)
    {
        FProperty* Property = FindFProperty<FProperty>(Object.GetClass(), PropertyName);
        check(Property);
        return *Property->ContainerPtrToValuePtr<ValueType>(&Object);
    }

    struct FGameZoneBindingFixture
    {
        FGameZoneBindingFixture()
        {
            World = FAutomationEditorCommonUtils::CreateNewMap();
            ZoneAsset = NewObject<UGameZoneAsset>();
            WorldSettings = World ? Cast<ARPGWorldSettings>(World->GetWorldSettings()) : nullptr;
        }

        bool IsValid() const
        {
            return World && ZoneAsset && WorldSettings;
        }

        void SetBinding(const FRPGId& ZoneId, const FGuid& BindingId) const
        {
            GetPropertyValue<FRPGId>(*ZoneAsset, TEXT("Id")) = ZoneId;
            GetPropertyValue<TSoftObjectPtr<UWorld>>(*ZoneAsset, TEXT("LevelToLoad")) = World;
            GetPropertyValue<FGuid>(*ZoneAsset, TEXT("GameZoneBindingId")) = BindingId;
            GetPropertyValue<FRPGId>(*WorldSettings, TEXT("GameZoneId")) = ZoneId;
            GetPropertyValue<FGuid>(*WorldSettings, TEXT("GameZoneBindingId")) = BindingId;
        }

        UWorld* World = nullptr;
        UGameZoneAsset* ZoneAsset = nullptr;
        ARPGWorldSettings* WorldSettings = nullptr;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneBindingAuditVerifiedTest,
    "IronicRPG.GameZone.Binding.Audit.MatchingIdentityIsVerified",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneBindingAuditVerifiedTest::RunTest(const FString& Parameters)
{
    FGameZoneBindingFixture Fixture;
    TestTrue(TEXT("The binding fixture uses RPG World Settings"), Fixture.IsValid());
    if (!Fixture.IsValid())
    {
        return false;
    }

    Fixture.SetBinding(FRPGId(TEXT("z.BindingVerified")), FGuid::NewGuid());

    const FGameZoneBindingAuditResult Result = AuditGameZoneBindingAgainstWorld(*Fixture.ZoneAsset, *Fixture.World);
    TestEqual(TEXT("A matching binding has no issue"), Result.Issue, EGameZoneBindingIssue::None);
    TestTrue(TEXT("A matching binding is verified"), Result.IsVerified());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneBindingAuditZoneIdMismatchTest,
    "IronicRPG.GameZone.Binding.Audit.ZoneIdMismatchIsRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneBindingAuditZoneIdMismatchTest::RunTest(const FString& Parameters)
{
    FGameZoneBindingFixture Fixture;
    TestTrue(TEXT("The binding fixture uses RPG World Settings"), Fixture.IsValid());
    if (!Fixture.IsValid())
    {
        return false;
    }

    const FGuid BindingId = FGuid::NewGuid();
    Fixture.SetBinding(FRPGId(TEXT("z.BindingAsset")), BindingId);
    GetPropertyValue<FRPGId>(*Fixture.WorldSettings, TEXT("GameZoneId")) = FRPGId(TEXT("z.BindingForeign"));

    const FGameZoneBindingAuditResult Result = AuditGameZoneBindingAgainstWorld(*Fixture.ZoneAsset, *Fixture.World);
    TestEqual(TEXT("A foreign Level claim is identified"), Result.Issue, EGameZoneBindingIssue::ZoneIdMismatch);
    TestFalse(TEXT("A foreign Level claim is not verified"), Result.IsVerified());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneBindingAuditBindingIdMismatchTest,
    "IronicRPG.GameZone.Binding.Audit.BindingGenerationMismatchIsRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneBindingAuditBindingIdMismatchTest::RunTest(const FString& Parameters)
{
    FGameZoneBindingFixture Fixture;
    TestTrue(TEXT("The binding fixture uses RPG World Settings"), Fixture.IsValid());
    if (!Fixture.IsValid())
    {
        return false;
    }

    Fixture.SetBinding(FRPGId(TEXT("z.BindingGeneration")), FGuid::NewGuid());
    GetPropertyValue<FGuid>(*Fixture.WorldSettings, TEXT("GameZoneBindingId")) = FGuid::NewGuid();

    const FGameZoneBindingAuditResult Result = AuditGameZoneBindingAgainstWorld(*Fixture.ZoneAsset, *Fixture.World);
    TestEqual(TEXT("A torn binding generation is identified"), Result.Issue, EGameZoneBindingIssue::BindingIdMismatch);
    TestFalse(TEXT("A torn binding generation is not verified"), Result.IsVerified());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneBindingAuditLegacyTest,
    "IronicRPG.GameZone.Binding.Audit.LegacyBindingRequiresMigration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneBindingAuditLegacyTest::RunTest(const FString& Parameters)
{
    FGameZoneBindingFixture Fixture;
    TestTrue(TEXT("The binding fixture uses RPG World Settings"), Fixture.IsValid());
    if (!Fixture.IsValid())
    {
        return false;
    }

    Fixture.SetBinding(FRPGId(TEXT("z.BindingLegacy")), FGuid());

    const FGameZoneBindingAuditResult Result = AuditGameZoneBindingAgainstWorld(*Fixture.ZoneAsset, *Fixture.World);
    TestEqual(TEXT("A binding without a generation is legacy"), Result.Issue, EGameZoneBindingIssue::MissingBindingId);
    TestFalse(TEXT("A legacy binding is not silently verified"), Result.IsVerified());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneBindingAuditLevelPathMismatchTest,
    "IronicRPG.GameZone.Binding.Audit.LevelPathMismatchIsRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneBindingAuditLevelPathMismatchTest::RunTest(const FString& Parameters)
{
    FGameZoneBindingFixture Fixture;
    TestTrue(TEXT("The binding fixture uses RPG World Settings"), Fixture.IsValid());
    if (!Fixture.IsValid())
    {
        return false;
    }

    const FGuid BindingId = FGuid::NewGuid();
    Fixture.SetBinding(FRPGId(TEXT("z.BindingPath")), BindingId);
    GetPropertyValue<TSoftObjectPtr<UWorld>>(*Fixture.ZoneAsset, TEXT("LevelToLoad")) =
        TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Binding/OtherWorld.OtherWorld")));

    const FGameZoneBindingAuditResult Result = AuditGameZoneBindingAgainstWorld(*Fixture.ZoneAsset, *Fixture.World);
    TestEqual(TEXT("A different Level package is identified"), Result.Issue, EGameZoneBindingIssue::LevelPathMismatch);
    TestFalse(TEXT("A different Level package is not verified"), Result.IsVerified());

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
