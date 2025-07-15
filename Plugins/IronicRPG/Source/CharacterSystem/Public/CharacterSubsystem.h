// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SaveGame/Saveable.h"
#include "SaveGame/CharacterSaveModule.h"
#include "Characters/CharacterDataTypes.h"
#include "DataTypes/RPGId.h"
#include "CharacterSubsystem.generated.h"

UENUM(BlueprintType)
enum class EPartyOperationResult : uint8
{
	Success UMETA(DisplayName = "Success"),
	CharacterNotFound UMETA(DisplayName = "Character Not Found"),
	CharacterUnavailable UMETA(DisplayName = "Character Unavailable"),
	PartyFull UMETA(DisplayName = "Party Full"),
	AlreadyInParty UMETA(DisplayName = "Already In Party"),
	NotInParty UMETA(DisplayName = "Not In Party")
};

/**
 * 
 */
UCLASS(Blueprintable)
class CHARACTERSYSTEM_API UCharacterSubsystem : public UGameInstanceSubsystem, public ISaveable
{
	GENERATED_BODY()

protected: // Subsystem Interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	void Initialize(FSubsystemCollectionBase& Collection) override;
	void Deinitialize() override;

public: // Data Layer
	// Map of character primary assets, keyed by character Id
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Character")
	TMap<FRPGId, class UCharacterPrimaryAsset*> CharacterAssetMap;

	// Map of character data instances, keyed by character Id
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Character")
	TMap<FRPGId, FCharacterInstanceData> CharacterDataMap;

	// Modify the character data for a specific character Id
	UFUNCTION(BlueprintCallable)
	void ModifyCharacterData(const FRPGId Id, FCharacterInstanceData& Data, const UGameplayEffect* Effect, const float Level = 1.0f);

private: // Data Layer
	// Read the character data from the DataTable and populate the CharacterDataMap
	void LoadPlayableCharacters();

	// Check if a character Id patterns match the specified format cXXXX (where X is a digit)
	bool IsCharacterIdMatched(const FRPGId Id) const;

	UPROPERTY()
	TObjectPtr<class UAbilitySystemComponent> GhostASC;

	UPROPERTY()
	TObjectPtr<class UCharacterAttributeSet> GhostAttributes;

public: // Instance Layer
	// Map of instances of characters currently in the game
	TMap<FRPGId, TWeakObjectPtr<ABaseCharacter>> InstanceCharacters;

	// Get a character instance by its Id
	UFUNCTION(BlueprintCallable)
	ABaseCharacter* GetCharacterInstance(const FRPGId Id) const;

	// Set the availability status of a character
	UFUNCTION(BlueprintCallable)
	void SetCharacterAvailability(const FRPGId Id, const ECharacterAvailabilityStatus Status);

	UFUNCTION(BlueprintCallable)
	bool IsAvailable(const FRPGId Id) const;

	UFUNCTION(BlueprintCallable)
	class ABaseCharacter* SpawnCharacter(const FRPGId Id, const FVector Location, const FRotator Rotation);

	UFUNCTION(BlueprintCallable)
	void SpawnPartyMembers(const FVector Location, const FRotator Rotation);

	UFUNCTION(BlueprintCallable)
	void SpawnNewPartyMembers(TArray<FRPGId> NewParty, const FVector Location, const FRotator Rotation);

	UFUNCTION(BlueprintCallable)
	void DespawnCharacter(const FRPGId Id);

private: // Instance Layer
	UPROPERTY()
	TArray<FRPGId> WaitForRemove;

	void ClearWaitForRemove();

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, SaveGame, Category = "Character|Party")
	FRPGId CurrentCharacterId;

	UFUNCTION(BlueprintCallable)
	ABaseCharacter* GetCurrentCharacter() const { return GetCharacterInstance(CurrentCharacterId); }

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, SaveGame, Category = "Character|Party")
	TArray<FRPGId> PartyMembers;

	UFUNCTION(BlueprintCallable)
	EPartyOperationResult AddPartyMember(const FRPGId Id);

	UFUNCTION(BlueprintCallable)
	EPartyOperationResult RemovePartyMember(const FRPGId Id);

	UFUNCTION(BlueprintCallable)
	bool IsPartyMember(const FRPGId Id) const;

	UFUNCTION(BlueprintCallable)
	bool CanJoinParty(const FRPGId Id) const;

	UFUNCTION(BlueprintCallable)
	int32 GetMaxPartyMembers() const { return MaxPartyMembers; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Character|Party")
	int32 MaxPartyMembers = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Character|Party")
	float SpawnLocationOffset = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Character|Party")
	float SpawnRotationOffset = 60.0f;

public: // ISaveable
	virtual ESaveModuleType GetSaveModuleType() const override { return ESaveModuleType::Character; }
	virtual void GetSaveData(FBaseSaveModule* SaveData) const override;
	virtual void ApplySaveData(const FBaseSaveModule* SaveData) override;

};
