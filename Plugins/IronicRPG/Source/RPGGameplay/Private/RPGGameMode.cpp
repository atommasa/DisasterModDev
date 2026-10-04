// Copyright Ironic Studio. All Rights Reserved.


#include "RPGGameMode.h"
#include "EngineUtils.h"
#include "RPGSettings.h"
#include "Assets/RPGAssetLibrary.h"

#include "SaveGame/RPGSaveGameMetadata.h"

#include "Widgets/WidgetBase.h"
#include "Components/UIControlComponent.h"

DEFINE_LOG_CATEGORY(LogRPGGameMode);

ARPGGameMode::ARPGGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseSeamlessTravel = true;

	
}

void ARPGGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	SaveSubsystem = GetGameInstance()->GetSubsystem<USaveGameSubsystem>();
	check(SaveSubsystem);

	GameZoneSubsystem = GetGameInstance()->GetSubsystem<UGameZoneSubsystem>();
	check(GameZoneSubsystem);

	CharacterSubsystem = GetGameInstance()->GetSubsystem<UCharacterSubsystem>();
	check(CharacterSubsystem);

#if WITH_EDITOR
	GameZoneSubsystem->OnStartGameInstantly.AddWeakLambda(this, [this]()
		{
			StartGameSession();
		});
#endif // WITH_EDITOR

}

void ARPGGameMode::StartPlay()
{
	Super::StartPlay();

}

void ARPGGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	

	Super::EndPlay(EndPlayReason);
}

void ARPGGameMode::GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList)
{
	Super::GetSeamlessTravelActorList(bToTransition, ActorList);

}

void ARPGGameMode::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();
	
	GameZoneSubsystem->HandlePostSeamlessTravel();
}

AActor* ARPGGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	return Super::ChoosePlayerStart_Implementation(Player);
}

ARPGGameMode* ARPGGameMode::GetRPGGameMode(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	return Cast<ARPGGameMode>(UGameplayStatics::GetGameMode(WorldContextObject));
}

void ARPGGameMode::StartGameSession(const FString& SlotName)
{
	SaveSubsystem->LoadGame(SlotName);

	PostStartGameSession();
}

void ARPGGameMode::RequestTeleportTo(const FGameZoneContext& NewContext)
{
	if (!GameZoneSubsystem)
    {
        return;
    }

    GameZoneSubsystem->EnterGameZone(NewContext);
}

void ARPGGameMode::AwakenSpawnablePoints()
{
	/*for (TActorIterator<ASpawnablePoint> It(GetWorld()); It; ++It)
	{
		ASpawnablePoint* SpawnPoint = *It;
		if (SpawnPoint && SpawnPoint->CanSpawnOnLevelLoaded())
		{
			SpawnPoint->Awaken();
		}
	}*/
}
