// Fill out your copyright notice in the Description page of Project Settings.


#include "RPGGameMode.h"
#include "Kismet/GameplayStatics.h"

#include "CharacterSubsystem.h"
#include "Characters/BaseCharacter.h"

#include "SaveGameSubsystem.h"
#include "SaveGame/RPGSaveGame.h"

void ARPGGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

}

APawn* ARPGGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* SpawnTransform)
{
	if (auto* CharacterSubsystem = GetGameInstance()->GetSubsystem<UCharacterSubsystem>())
	{
		CharacterSubsystem->SpawnPartyMembers(SpawnTransform->GetActorLocation(), SpawnTransform->GetActorRotation());

		if (auto* CurrentCharacter = CharacterSubsystem->GetCurrentCharacter())
		{
			return CurrentCharacter;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("No current character found in CharacterSubsystem!"));
		}
	}

	return Super::SpawnDefaultPawnFor_Implementation(NewPlayer, SpawnTransform);
}
