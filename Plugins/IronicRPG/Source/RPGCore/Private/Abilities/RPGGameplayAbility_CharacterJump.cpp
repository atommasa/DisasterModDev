// Copyright Ironic Studio. All Rights Reserved.


#include "Abilities/RPGGameplayAbility_CharacterJump.h"
#include "Characters/BaseCharacter.h"
#include "Characters/Components/RPGCharacterMovementComponent.h"

void URPGGameplayAbility_CharacterJump::DoCharacterJump()
{
	Character = Cast<ABaseCharacter>(GetOwningActorFromActorInfo());
	if (Character.IsValid())
	{
		Character->LandedDelegate.AddUniqueDynamic(this, &URPGGameplayAbility_CharacterJump::OnCharacterJumpEnd);

		if (URPGCharacterMovementComponent* CharacterMovement = Cast<URPGCharacterMovementComponent>(Character->GetCharacterMovement()))
		{
			CharacterMovement->CharacterJump();
		}
	}
}

