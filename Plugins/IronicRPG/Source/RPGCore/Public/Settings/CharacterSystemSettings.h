// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DataTypes/RPGId.h"
#include "Misc/RPGTeamAgentInterface.h"
#include "CharacterSystemSettings.generated.h"

/**
 * 
 */
UCLASS(config = IronicRPG, defaultconfig)
class RPGCORE_API UCharacterSystemSettings : public UObject
{
	GENERATED_BODY()
	
public: // Character system settings
	// Playable character that can be controlled by the player.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category = "Character|Default", meta = (IdType = "Character"))
	TArray<FRPGId> PlayableCharacters;

	// Unsaveable party members that will be used when starting a new game.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category = "Character|Default", meta = (IdType = "Character"))
	TArray<FRPGId> DefaultPartyMembers;

	// Unsaveable player index in the party. This is used to determine which character will be controlled by the player.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category = "Character|Default", meta = (ClampMin = "0", ArrayClamp = "DefaultPartyMembers"))
	int32 DefaultPlayerIndex = 0;

	// Maximum number of party members allowed in the game.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category = "Character|Party", meta = (ClampMin = "1"))
	int32 MaxPartyMembers = 3;

public: // Team settings
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category = "Team", meta = (Categories = "Team"))
	TMap<FGameplayTag, FTeamRelationInfo> TeamRelations = {
		{ Team_Ally, FTeamRelationInfo::AllyTeamRelationInfo },
		{ Team_Enemy, FTeamRelationInfo::EnemyTeamRelationInfo },
		{ Team_Neutral, FTeamRelationInfo() }
	};

#if WITH_EDITOR
protected:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

};
