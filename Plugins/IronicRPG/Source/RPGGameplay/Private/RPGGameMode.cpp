// Copyright Ironic Studio. All Rights Reserved.


#include "RPGGameMode.h"
#include "EngineUtils.h"
#include "RPGSettings.h"

#include "SaveGame/RPGSaveGameMetadata.h"

#include "Widgets/MenuBase.h"

#include "Misc/SpawnablePoint.h"

DEFINE_LOG_CATEGORY(LogRPGGameMode);

void ARPGGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	SaveSubsystem = GetGameInstance()->GetSubsystem<USaveGameSubsystem>();
	check(SaveSubsystem);
	SaveSubsystem->OnSaveGameLoadCompleted.AddUObject(this, &ARPGGameMode::OnSaveGameLoaded);

	GameZoneSubsystem = GetGameInstance()->GetSubsystem<UGameZoneSubsystem>();
	check(GameZoneSubsystem);

	UISubsystem = GetGameInstance()->GetSubsystem<UUISubsystem>();
	check(UISubsystem);

	LoadingScreenSubsystem = ULoadingScreenSubsystem::Get(this);
	check(LoadingScreenSubsystem);
	// TODO: Maybe use more fliexible event binding (e.g., BlueprintImplementableEvent)
	LoadingScreenSubsystem->OnLoadingScreenStop.AddUniqueDynamic(this, &ARPGGameMode::OnStoppedLoading);

	CharacterSubsystem = GetGameInstance()->GetSubsystem<UCharacterSubsystem>();
	check(CharacterSubsystem);
	CharacterSubsystem->OnPartyConstructed.AddLambda([this]()
		{
			OnPartyReady();
		});
}

void ARPGGameMode::BeginPlay()
{
	Super::BeginPlay();

	CreateMainMenuWidget();
}

void ARPGGameMode::StartGameSession(const FString& SlotName)
{
	LoadingScreenSubsystem->StartLoadingScreen();

	SaveSubsystem->LoadGame(SlotName);

	UISubsystem->CloseAllUI();
}

void ARPGGameMode::TeleportTo(const FGameZoneContext& NewGameZoneContext)
{
	LoadingScreenSubsystem->StartLoadingScreen();
	
	GameZoneSubsystem->EnterGameZone(NewGameZoneContext, TDelegate<void()>::CreateLambda([this, NewGameZoneContext]()
		{
			CharacterSubsystem->SpawnPartyMembers(GameZoneSubsystem->GetSaveGameTransform(), true, ESpawnPartyMode::ByPlayerPartyIndex);
		}));
}

void ARPGGameMode::AwakenSpawnablePoints()
{
	for (TActorIterator<ASpawnablePoint> It(GetWorld()); It; ++It)
	{
		ASpawnablePoint* SpawnPoint = *It;
		if (SpawnPoint && SpawnPoint->CanSpawnOnLevelLoaded())
		{
			SpawnPoint->Awaken();
		}
	}
}

void ARPGGameMode::OnSaveGameLoaded_Implementation()
{
	// Awaken spawn points
	AwakenSpawnablePoints();

	// Spawn party members
	CharacterSubsystem->SpawnPartyMembers(GameZoneSubsystem->GetSaveGameTransform(), true, ESpawnPartyMode::ByPlayerPartyIndex);
}

void ARPGGameMode::CreateMainMenuWidget_Implementation()
{
	if (UISubsystem && MainMenuWidgetClass)
	{
		if (UMenuBase* MainMenu = Cast<UMenuBase>(UISubsystem->OpenUIByClass(MainMenuWidgetClass)))
		{
			MainMenuWidget = MainMenu;

			OnMainMenuInitialized();
		}
		else
		{
			UE_LOG(LogRPGGameMode, Warning, TEXT("Failed to create menu widget of class %s"), *MainMenuWidgetClass->GetName());
		}
	}
}

void ARPGGameMode::OnStoppedLoading_Implementation()
{
	// TODO: 未來採用更好的方式切換控制狀態
	if (ARPGPlayerController* PC = Cast<ARPGPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		PC->SetControlMode(ERPGControlMode::Gameplay);
	}
}

void ARPGGameMode::OnPartyReady()
{
	// Possess the player character
	if (ABaseCharacter* CurrentCharacter = CharacterSubsystem->GetPlayerCharacter())
	{
		if (ARPGPlayerController* PC = Cast<ARPGPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
		{
			PC->PossessCharacter(CurrentCharacter, ERPGControlMode::None);
		}
	}
	else
	{
		UE_LOG(LogRPGGameMode, Warning, TEXT("No player character found in CharacterSubsystem!"));
	}

	LoadingScreenSubsystem->StopLoadingScreen();
}
