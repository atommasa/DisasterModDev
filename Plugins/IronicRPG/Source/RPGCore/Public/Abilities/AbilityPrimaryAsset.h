// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DataTypes/RPGId.h"
#include "Abilities/GameplayAbility.h"
#include "AbilityDataTypes.h"
#include "AbilityPrimaryAsset.generated.h"

/**
 * 
 */
UCLASS()
class RPGCORE_API UAbilityPrimaryAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FRPGId Id;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftClassPtr<UGameplayAbility> AbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 DefaultLevel = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FAbilityInstanceData DefaultData;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType(GetClass()->GetFName()), Id.Id);
	}

	static FName AbilityAssetType;
};