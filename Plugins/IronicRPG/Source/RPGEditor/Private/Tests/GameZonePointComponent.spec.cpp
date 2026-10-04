// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Levels/GameZonePointComponent.h"

#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZonePointComponentBlueprintInstanceIdTest,
    "IronicRPG.GameZone.PointComponent.BlueprintInstancesHaveUniqueIds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZonePointComponentBlueprintInstanceIdTest::RunTest(const FString& Parameters)
{
    const FName BlueprintName = MakeUniqueObjectName(
        GetTransientPackage(),
        UBlueprint::StaticClass(),
        TEXT("BP_GameZonePointIdTest"));

    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        AActor::StaticClass(),
        GetTransientPackage(),
        BlueprintName,
        BPTYPE_Normal,
        UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass(),
        FName(TEXT("GameZonePointComponentBlueprintInstanceIdTest")));

    TestNotNull(TEXT("Temporary Blueprint is created"), Blueprint);
    if (!Blueprint)
    {
        return false;
    }

    USCS_Node* PointNode = Blueprint->SimpleConstructionScript->CreateNode(
        UGameZonePointComponent::StaticClass(),
        TEXT("GameZonePoint"));
    Blueprint->SimpleConstructionScript->AddNode(PointNode);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    TestNotNull(TEXT("Test world is created"), World);
    if (!World || !Blueprint->GeneratedClass)
    {
        return false;
    }

    AActor* FirstActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass);
    AActor* SecondActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass);
    TestNotNull(TEXT("First Blueprint instance is spawned"), FirstActor);
    TestNotNull(TEXT("Second Blueprint instance is spawned"), SecondActor);
    if (!FirstActor || !SecondActor)
    {
        return false;
    }

    const UGameZonePointComponent* First =
        FirstActor->FindComponentByClass<UGameZonePointComponent>();
    const UGameZonePointComponent* Second =
        SecondActor->FindComponentByClass<UGameZonePointComponent>();
    TestNotNull(TEXT("First instance owns the point component"), First);
    TestNotNull(TEXT("Second instance owns the point component"), Second);
    if (!First || !Second)
    {
        return false;
    }

    TestTrue(TEXT("First instance has a valid id"), First->GetPointId().IsValid());
    TestTrue(TEXT("Second instance has a valid id"), Second->GetPointId().IsValid());
    TestNotEqual(
        TEXT("Blueprint instances receive different point ids"),
        First->GetPointId(),
        Second->GetPointId());

    const FGuid FirstPointId = First->GetPointId();
    FirstActor->RerunConstructionScripts();

    const UGameZonePointComponent* ReconstructedFirst =
        FirstActor->FindComponentByClass<UGameZonePointComponent>();
    TestNotNull(
        TEXT("Point component survives construction rerun"),
        ReconstructedFirst);
    if (ReconstructedFirst)
    {
        TestEqual(
            TEXT("Construction rerun preserves the instance point id"),
            ReconstructedFirst->GetPointId(),
            FirstPointId);
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
