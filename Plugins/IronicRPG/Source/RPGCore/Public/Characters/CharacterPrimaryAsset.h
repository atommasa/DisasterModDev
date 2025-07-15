// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DataTypes/RPGId.h"
#include "CharacterDataTypes.h"
#include "BehaviorTree/BehaviorTree.h"
#include "CharacterPrimaryAsset.generated.h"

class ABaseCharacter;

class UGameplayEffect;
class UGameplayAbility;

/**
 * 
 */
UCLASS()
class RPGCORE_API UCharacterPrimaryAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FRPGId Id;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftClassPtr<ABaseCharacter> CharacterClass = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	TSoftObjectPtr<UBehaviorTree> BehaviorTree;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftClassPtr<UGameplayEffect> AttributeEffectClass = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Portrait = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 DefaultLevel = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FCharacterInstanceData DefaultData;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType(CharacterAssetType), Id.Id);
	}

	static FName CharacterAssetType;

public:
	FText GetDisplayName() const { return DefaultData.DisplayName; }
};