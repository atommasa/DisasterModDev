// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Assets/RPGPrimaryAsset.h"
#include "Characters/CharacterDataTypes.h"
#include "InstancedStruct.h"
#include "BehaviorTree/BehaviorTree.h"
#include "CharacterAsset.generated.h"

/**
 * The Character Asset class represents a character in the RPG game, including its properties, behavior, and default data.
 */
UCLASS()
class RPGCORE_API UCharacterAsset : public URPGPrimaryAsset
{
	GENERATED_BODY()
	DEFINE_ASSET_TYPE(Character, c)
	
protected: // General Properties
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capsule")
	float DefaultCapsuleHalfHeight = 88.f;
	ASSET_PROP_GETTER(float, DefaultCapsuleHalfHeight)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capsule")
	float DefaultCapsuleRadius = 34.f;
	ASSET_PROP_GETTER(float, DefaultCapsuleRadius)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	TSoftObjectPtr<UBehaviorTree> BehaviorTree = nullptr;
	ASSET_PROP_GETTER(TSoftObjectPtr<UBehaviorTree>, BehaviorTree)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TSoftObjectPtr<UCurveTable> GrowthTable = nullptr;
	ASSET_PROP_GETTER(TSoftObjectPtr<UCurveTable>, GrowthTable)
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TSoftObjectPtr<UTexture2D> Portrait = nullptr;
	ASSET_PROP_GETTER(TSoftObjectPtr<UTexture2D>, Portrait)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	FCharacterSaveData DefaultData;
	ASSET_PROP_GETTER(FCharacterSaveData, DefaultData)
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Profile", meta = (BaseStruct = "/Script/RPGCore.CharacterProfileBase", ExcludeBaseStruct))
	TArray<FInstancedStruct> Profiles;
	ASSET_PROP_GETTER(TArray<FInstancedStruct>, Profiles)

public:
	FCharacterSaveData GetAttributesAtLevel(int32 Level) const;
	FCharacterSaveData GetDefaultLevelAttributes() const;

	// Template function to get a specific character profile struct
	template<typename T>
	T* GetCharacterProfile() const;

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent) override;
#endif // WITH_EDITOR
};
