// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/GameZoneSubsystemTestTypes.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Levels/GameZonePointComponent.h"
#include "Levels/MapMarkerTypeAsset.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerActionNameTest,
    "IronicRPG.GameZone.MarkerAction.OptionsResolveDynamicNamesWithoutChangingAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerActionNameTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UMapMarkerTypeAsset> MarkerType(
        LoadObject<UMapMarkerTypeAsset>(nullptr, TEXT("/Game/DataAssets/MapMarker/DA_WayPoint.DA_WayPoint")));
    if (!TestNotNull(TEXT("The test marker type is available"), MarkerType.Get()))
    {
        return false;
    }

    FProperty* ActionsProperty = FindFProperty<FProperty>(MarkerType->GetClass(), TEXT("Actions"));
    check(ActionsProperty);
    auto& Actions = *ActionsProperty->ContainerPtrToValuePtr<TArray<FGameZoneMarkerActionDefinition>>(MarkerType.Get());
    TGuardValue<TArray<FGameZoneMarkerActionDefinition>> RestoreActions(Actions, {});
    FGameZoneMarkerActionDefinition Definition;
    Definition.ActionTag = FGameplayTag::RequestGameplayTag(TEXT("Map.Marker.Action.FastTravel"));
    Definition.DisplayName = NSLOCTEXT("MarkerActionNameTest", "Authored", "Authored");
    Definition.ActionClass = UGameZoneActionNameDefaultTestAction::StaticClass();
    Actions.Add(Definition);

    TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>(GEngine));
    GameInstance->InitializeStandalone(TEXT("MarkerActionNameTestWorld"));
    UWorld* World = GameInstance->GetWorld();
    if (!TestNotNull(TEXT("A standalone world is available"), World))
    {
        GameInstance->Shutdown();
        return false;
    }

    TStrongObjectPtr<UGameZoneSubsystem> Subsystem(NewObject<UGameZoneSubsystem>(GameInstance.Get()));
    APlayerController* Player = World->SpawnActor<APlayerController>();
    AActor* Owner = World->SpawnActor<AActor>();
    if (!Player || !Owner)
    {
        AddError(TEXT("Could not spawn the action context actors."));
        GameInstance->Shutdown();
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
        return false;
    }

    TStrongObjectPtr<UGameZonePointComponent> Point(NewObject<UGameZonePointComponent>(Owner));
    FGameZonePointData Data = Point->MakePointSnapshot();
    Data.MarkerTypeId = MarkerType->GetId();
    Data.MarkerState = EGameZonePointState::Deactivated;
    Data.DisplayMode = static_cast<int32>(EMapMarkerDisplayMode::WorldMap) | static_cast<int32>(EMapMarkerDisplayMode::MiniMap);
    Point->ApplyPointDataInitialization(Data);
    TestEqual(TEXT("The live point registers"), Subsystem->RegisterPoint(*Point), EGameZonePointRegistrationResult::Registered);

    auto CheckName = [this, &Subsystem, &Point, Player](EMapMarkerDisplayMode Mode, const TCHAR* Expected)
    {
        TArray<FGameZoneMarkerActionOption> Options;
        TestTrue(TEXT("Action options resolve"), Subsystem->GetMapMarkerActionOptions(Point->GetPointId(), Mode, Player, Options));
        TestEqual(TEXT("One action is returned"), Options.Num(), 1);
        if (Options.Num() == 1)
        {
            TestEqual(TEXT("The option contains the resolved display name"), Options[0].Definition.DisplayName.ToString(), FString(Expected));
        }
    };

    CheckName(EMapMarkerDisplayMode::WorldMap, TEXT("Authored"));
    Actions[0].ActionClass = UGameZoneActionNameDynamicTestAction::StaticClass();
    CheckName(EMapMarkerDisplayMode::WorldMap, TEXT("Authored: Activate"));
    Point->SetMarkerState(EGameZonePointState::Activated);
    Subsystem->NotifyPointChanged(*Point);
    CheckName(EMapMarkerDisplayMode::WorldMap, TEXT("Authored: Deactivate"));
    CheckName(EMapMarkerDisplayMode::MiniMap, TEXT("Authored"));
    TestTrue(TEXT("Queries do not overwrite the asset name"), Actions[0].DisplayName.EqualTo(Definition.DisplayName));
    TestEqual(TEXT("The action identity remains stable"), Actions[0].ActionTag, Definition.ActionTag);

    Subsystem->UnregisterPoint(*Point);
    Point.Reset();
    Subsystem.Reset();
    GameInstance->Shutdown();
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
