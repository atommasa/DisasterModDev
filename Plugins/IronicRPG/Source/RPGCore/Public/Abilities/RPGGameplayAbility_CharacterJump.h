// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/RPGGameplayAbility.h"
#include "RPGGameplayAbility_CharacterJump.generated.h"

/**
 * The ability to make the character jump.
 */
UCLASS()
class RPGCORE_API URPGGameplayAbility_CharacterJump : public URPGGameplayAbility
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Ability|Character")
	virtual void DoCharacterJump();

	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|Character")
	void OnCharacterJumpEnd(const FHitResult& Hit);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Ability|Character")
	TWeakObjectPtr<class ABaseCharacter> Character;

};
