// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Helpers/RPGHelperMacros.h"
#include "DataTypes/RPGId.h"
#include "RPGSettings.generated.h"

class URichTextBlockDecorator;

/**
 *	
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Ironic RPG Settings"))
class RPGCORE_API UNarrativeSystemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UNarrativeSystemSettings(const FObjectInitializer& ObjectInitializer);

public: // Character system settings
	UPROPERTY(EditDefaultsOnly, Category = "Character|Default")
	TArray<FRPGId> DefaultPartyMembers;

	UPROPERTY(EditDefaultsOnly, Category = "Character|Default", meta=(ClampMin="0", ArrayClamp="DefaultPartyMembers"))
	int32 DefaultPlayerIndex = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Character|Party", meta = (ClampMin = "1"))
	int32 MaxPartyMembers = 3;

	PROP_CHANGE_COMMIT()
	{
		CLAMPED_ARRAY(DefaultPartyMembers, MaxPartyMembers)
	}
	
public: // UI settings
	UPROPERTY(EditAnywhere, config, Category = "UI|Text")
	TSoftObjectPtr<UDataTable> TextStyleSet;

	UPROPERTY(EditAnywhere, config, Category = "UI|Text")
	TArray<TSubclassOf<URichTextBlockDecorator>> DecoratorClasses;

public:
	UFUNCTION(BlueprintCallable)
	static const UNarrativeSystemSettings* GetNarrativeSettings() { return GetDefault<UNarrativeSystemSettings>(); }

protected:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("IronicRPG"); }

};