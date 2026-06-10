// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Helpers/RPGHelperMacros.h"

#include "DataTypes/RPGId.h"

#include "Abilities/AbilityDataTypes.h"
#include "Misc/RPGTeamAgentInterface.h"

#include "Levels/GameZoneContext.h"

#include "RPGSettings.generated.h"

class URichTextBlockDecorator;

/**
 * This class is used to manage all the settings in RPG plugin.
 */
UCLASS(config=IronicRPG, defaultconfig, meta=(DisplayName="Ironic RPG Settings"))
class RPGCORE_API URPGSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URPGSettings(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable)
	static const URPGSettings* GetRPGSettings() { return GetDefault<URPGSettings>(); }

public: // RPGId settings
	// The max length of prefix part for an Id.
	// Note: Please make sure there is enough length for your customized prefixes.
	UPROPERTY(EditDefaultsOnly, config, Category = "RPGId", meta=(ClampMin = "4", DisplayName = "Max Prefix Length"))
	int32 MaxPrefixLen = 4;

	// The length of numeric part for an Id.
	// Note: It is recommended not to modify this value when you have declared many Ids.
	UPROPERTY(EditDefaultsOnly, config, Category = "RPGId", meta=(ClampMin = "4", DisplayName = "Numeric Length"))
	int32 NumericLen = 4;

public: // Save system settings
	// The prefix for save slot names that can be used to identify save files.
	UPROPERTY(EditDefaultsOnly, config, Category = "Save System")
	FString SaveSlotPrefix = TEXT("save.");

	UPROPERTY(EditDefaultsOnly, config, Category = "Save System")
	FString MetaDataSaveSlotName = TEXT("meta");

	// Auto-save slot name.
	UPROPERTY(EditDefaultsOnly, config, Category = "Save System")
	FString AutoSaveSlotName = TEXT("auto");

	// The maximum number of save slots allowed.
	UPROPERTY(EditDefaultsOnly, config, Category = "Save System", meta=(ClampMin = "1"))
	int32 MaxSaveSlots = 10;

public: // Character system settings
	// Playable character that can be controlled by the player.
	UPROPERTY(EditDefaultsOnly, config, Category = "Character|Default", meta=(IdType = "Character"))
	TArray<FRPGId> PlayableCharacters;

	// Unsaveable party members that will be used when starting a new game.
	UPROPERTY(EditDefaultsOnly, config, Category = "Character|Default", meta=(IdType = "Character"))
	TArray<FRPGId> DefaultPartyMembers;

	// Unsaveable player index in the party. This is used to determine which character will be controlled by the player.
	UPROPERTY(EditDefaultsOnly, config, Category = "Character|Default", meta=(ClampMin = "0", ArrayClamp = "DefaultPartyMembers"))
	int32 DefaultPlayerIndex = 0;

	// Maximum number of party members allowed in the game.
	UPROPERTY(EditDefaultsOnly, config, Category = "Character|Party", meta=(ClampMin = "1"))
	int32 MaxPartyMembers = 3;

public: // Team settings
	UPROPERTY(EditDefaultsOnly, config, Category = "Team", meta=(Categories = "Team"))
	TMap<FGameplayTag, FTeamRelationInfo> TeamRelations = {
		{ Team_Ally, FTeamRelationInfo::AllyTeamRelationInfo },
		{ Team_Enemy, FTeamRelationInfo::EnemyTeamRelationInfo },
		{ Team_Neutral, FTeamRelationInfo() }
	};

public: // Ability system settings
	// The enum asset that defines the input bindings for abilities.
	UPROPERTY(EditDefaultsOnly, config, Category = "Ability|Control", meta=(AllowedClasses = "/Script/CoreUObject.Enum"))
	FSoftObjectPath AbilityInputEnum;

	UPROPERTY(EditDefaultsOnly, config, Category = "Ability|Effect")
	TMap<FGameplayTag, EMagnitudeRoundingMode> RoundingModes;
	
public: // Level settings
	UPROPERTY(EditDefaultsOnly, config, Category = "Level|GameZone")
	FGameZoneContext DefaultGameZoneContext;

public: // UI settings
	UPROPERTY(EditDefaultsOnly, config, Category = "UI|Text")
	TSoftObjectPtr<UDataTable> TextStyleSet;

	UPROPERTY(EditDefaultsOnly, config, Category = "UI|Text")
	TArray<TSubclassOf<URichTextBlockDecorator>> DecoratorClasses;

protected:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("IronicRPG"); }

#if WITH_EDITOR
protected:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};