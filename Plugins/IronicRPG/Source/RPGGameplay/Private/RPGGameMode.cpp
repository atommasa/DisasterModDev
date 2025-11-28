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
	LoadingScreenSubsystem->OnLoadingScreenStop.AddUniqueDynamic(this, &ARPGGameMode::OnLoadingScreenStopped);

	CharacterSubsystem = GetGameInstance()->GetSubsystem<UCharacterSubsystem>();
	check(CharacterSubsystem);
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
	FVector SpawnLocation = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;

	const FGameZoneContext& CurrentContext = GameZoneSubsystem->GetCurrentContext();

	// If we have a saved transform, use that for spawning
	if (CurrentContext.bUseSavedTransform)
	{
		SpawnLocation = CurrentContext.SavedTransform.GetLocation();
		SpawnRotation = CurrentContext.SavedTransform.GetRotation().Rotator();
	}
	// Otherwise, find a PlayerStart with a matching tag
	else
	{
		for (TActorIterator<ARPGPlayerStart> It(GetWorld()); It; ++It)
		{
			ARPGPlayerStart* Start = *It;
			if (!Start)
			{
				continue;
			}

			UE_LOG(LogRPGGameMode, Display, TEXT(
				"Found PlayerStart [%s] in World [%s], Tag: [%s], Level: [%s]"
			),
				*It->GetName(),
				*It->GetWorld()->GetName(),
				*It->PlayerStartTag.ToString(),
				*GetNameSafe(It->GetLevel()));

			if (Start->PlayerStartTag == CurrentContext.EntryPointTag)
			{
				SpawnLocation = Start->GetActorLocation();
				SpawnRotation = Start->GetActorRotation();
				break;
			}
		}
	}

	// Awaken spawn points
	AwakenSpawnablePoints();

	// Spawn party members
	CharacterSubsystem->SpawnPartyMembers(SpawnLocation, SpawnRotation, true, ESpawnPartyMode::ByPlayerPartyIndex);

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

	// TODO: Maybe wait for character spawning to complete before stopping the loading screen
	LoadingScreenSubsystem->StopLoadingScreen();
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

void ARPGGameMode::OnLoadingScreenStopped()
{
	if (ARPGPlayerController* PC = Cast<ARPGPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		PC->SetControlMode(ERPGControlMode::Gameplay);
	}
}
