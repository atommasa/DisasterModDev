// Copyright Ironic Studio. All Rights Reserved.


#include "CharacterCheatExtension.h"
#include "CharacterSubsystem.h"

void UCharacterCheatExtension::SetPlayerCharacter(const FName& Id)
{
	auto* CharacterSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UCharacterSubsystem>();
	if (CharacterSubsystem)
	{
		auto* PlayerCharacter = CharacterSubsystem->GetPlayerCharacter();
		if (PlayerCharacter)
		{
			PlayerCharacter->InitCharacterDataById(Id, CharacterSubsystem->CharacterDataMap.FindRef(Id));
		}
	}
}
