// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Helpers/RPGHelperMacros.h"

#include "DataTypes/RPGId.h"

#include "Abilities/AbilityDataTypes.h"

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

	UFUNCTION(BlueprintPure)
	static const URPGSettings* GetRPGSettings() { return GetDefault<URPGSettings>(); }

public: // RPGId settings
	// The max length of prefix part for an Id.
	// Note: Please make sure there is enough length for your customized prefixes.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category = "RPGId", meta=(ClampMin = "4", DisplayName = "Max Prefix Length"))
	int32 MaxPrefixLen = 4;

	// The length of numeric part for an Id.
	// Note: It is recommended not to modify this value when you have declared many Ids.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category = "RPGId", meta=(ClampMin = "4", DisplayName = "Numeric Length"))
	int32 NumericLen = 4;

public: // Ability system settings
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category = "Ability|Effect")
	TMap<FGameplayTag, EMagnitudeRoundingMode> RoundingModes;

public: // UI settings
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category = "UI|Text")
	TSoftObjectPtr<UDataTable> TextStyleSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category = "UI|Text")
	TArray<TSubclassOf<URichTextBlockDecorator>> DecoratorClasses;

protected:
	virtual FName GetCategoryName() const override { return TEXT("Ironic"); }
	virtual FName GetSectionName() const override { return TEXT("IronicRPG"); }

#if WITH_EDITOR
protected:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};