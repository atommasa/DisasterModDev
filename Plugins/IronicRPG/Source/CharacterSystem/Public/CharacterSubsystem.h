// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RPGFlow.h"

#include "Subsystems/RPGGameInstanceSubsystem.h"
#include "Characters/BaseCharacter.h"
#include "Controllers/RPGPlayerController.h"
#include "Characters/CharacterAsset.h"
#include "SaveGame/Saveable.h"
#include "Characters/CharacterDataTypes.h"
#include "DataTypes/RPGId.h"
#include "CharacterSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCharacterSubsystem, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPartyReady);
DECLARE_MULTICAST_DELEGATE(FOnPartyConstructed);

class URPGPrimaryAsset;
class UCharacterAsset;

UENUM(BlueprintType)
enum class EPartyOperationResult : uint8
{
	Success UMETA(DisplayName = "Success"),
	InvalidOperation UMETA(DisplayName = "Invalid Operation"),
	CharacterNotFound UMETA(DisplayName = "Character Not Found"),
	CharacterUnavailable UMETA(DisplayName = "Character Unavailable"),
	PartyFull UMETA(DisplayName = "Party Full"),
	AlreadyInParty UMETA(DisplayName = "Already In Party"),
	NotInParty UMETA(DisplayName = "Not In Party")
};

UENUM(BlueprintType)
enum class ESwitchCharacterPolicy : uint8
{
	ToNext,
	ToPrevious,
};

UENUM(BlueprintType)
enum class ESpawnPartyMode : uint8
{
	KeepControlSameCharacter UMETA(DisplayName = "Keep Control Same Character"),
	ByPlayerPartyIndex UMETA(DisplayName = "By Player Party Index")
};

/**
 * Character Subsystem to manage character data and instances.
 */
UCLASS(Blueprintable)
class CHARACTERSYSTEM_API UCharacterSubsystem : public URPGGameInstanceSubsystem, public ISaveable
{
	GENERATED_BODY()

protected: // Subsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ============
	//  Data Layer
	// ============

public:
	// Map of character data, keyed by character Id, but only stores CHARACTER DATA YOU WANT TO SAVE
	// And it is not "Authority Data", so we recommend that you only use it for loading and saving
	// The actual runtime data is owned by the character itself
	// Or you should call SavePartyMembersDataToMap() to sync the data with all character assets in the game
	UPROPERTY(BlueprintReadWrite, Category = "Character")
	TMap<FRPGId, FCharacterSaveData> CharacterDataMap;

	// Save a single character's data to CharacterDataMap
	UFUNCTION(BlueprintCallable)
	virtual void SaveCharacterDataToMap(ABaseCharacter* Instance);

	// Save all party members' data to CharacterDataMap
	UFUNCTION(BlueprintCallable)
	virtual void SavePartyMembersDataToMap();

	// Modify the character data of multiple characters by applying a gameplay effect
	// Only modifies the data in CharacterDataMap, does not affect the actual character instances
	// If you want to modify the actual character instances, you should apply the effect to them directly
	UFUNCTION(BlueprintCallable)
	virtual void ModifyCharactersData(const TArray<FRPGId>& Ids, UGameplayEffect* Effect, const float Level = 1.0f);

	// =================
	//  Instance Layer
	// =================

public:
	// Instances of characters currently in the game
	UPROPERTY(BlueprintReadOnly, Category = "Character")
	TMap<FGuid, TObjectPtr<ABaseCharacter>> InstanceCharacters;

	UFUNCTION(BlueprintCallable)
	void AddInstanceCharacter(ABaseCharacter* Character);

	// Set the availability status of a character
	UFUNCTION(BlueprintCallable)
	void SetCharacterAvailability(const FRPGId& Id, const ECharacterAvailabilityStatus Status);

	UFUNCTION(BlueprintCallable)
	bool IsAvailable(const FRPGId& Id) const;

	/**
	 * Use a RPGId to spawn a character
	 *
	 * @param Id The RPGId of a character that you want to spawn.
	 * @param CharacterClass The character class of the character instance. If it is nullptr, use ABaseCharacter as default.
	 * @param Location Location to spawn.
	 * @param Rotation Rotation to spawn.
	 * @param bAsync Whether to load the character asset asynchronously. If true, the character will spawn synchronously, but data will be loaded asynchronously.
	 */
	UFUNCTION(BlueprintCallable, Category = "RPG|Character", meta = (Latent, LatentInfo = "LatentInfo", InternalUseParam = "ReturnValue",
		DeterminesOutputType = "CharacterClass", DynamicOutputParam = "OutCharacter"))
	FRPGVoidCoroutine SpawnCharacterAsync(
		FRPGId Id,
		TSubclassOf<ABaseCharacter> CharacterClass,
		FVector Location,
		FRotator Rotation,
		ABaseCharacter*& OutCharacter,
		FLatentActionInfo LatentInfo
	);

private:
	// C++ core coroutine. Returns only after the actor has been fully initialized.
	TRPGCoroutine<TRPGAsyncResult<ABaseCharacter*>> SpawnCharacterCoreAsync(
		FRPGId Id,
		TSubclassOf<ABaseCharacter> CharacterClass,
		FVector Location,
		FRotator Rotation
	);

public:
	/**
	 * Use a character asset to spawn a character
	 *
	 * @param Asset The asset of a character that you want to spawn.
	 * @param CharacterClass The character class of the character instance. If it is nullptr, use ABaseCharacter as default.
	 * @param Location Location to spawn.
	 * @param Rotation Rotation to spawn.
	 */
	UFUNCTION(BlueprintCallable)
	ABaseCharacter* SpawnCharacterByAsset(
		UCharacterAsset* Asset,
		const TSubclassOf<ABaseCharacter> CharacterClass = nullptr,
		const FVector Location = FVector::ZeroVector,
		const FRotator Rotation = FRotator::ZeroRotator
	);

	UFUNCTION(BlueprintCallable)
	void DespawnCharacter(const FGuid& Guid);

	// On party ready, broadcast when all party members are spawned and ready, and the player character is possessed
	UPROPERTY(BlueprintAssignable)
	FOnPartyReady OnPartyReady;

	// On party constructed, for C++ use, broadcast when all party members are spawned
	FOnPartyConstructed OnPartyConstructed;

public:
	virtual void PartyTravelStart();
	virtual void PartyTravelEnd();

private:
	TRPGCoroutine<TRPGAsyncResult<>> InitializeCharacterAndWait(UCharacterAsset* Asset, FGuid InGuid);
	void DespawnCharacterInstance(const FGuid& Guid);

	bool bIsSpawningPartyMembers = false;

	bool bPartyTeleportDone = false;

	// Reject delayed preload callbacks from an older LoadGame request.
	uint32 LoadRequestSerial = 0;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Party")
	TSubclassOf<ABaseCharacter> PlayableCharacterClass = ABaseCharacter::StaticClass();

	// Offset distance between spawned party members.
	UPROPERTY(EditDefaultsOnly, Category = "Character|Party")
	float SpawnLocationOffset = 200.0f;

	// Offset angle between spawned party members.
	UPROPERTY(EditDefaultsOnly, Category = "Character|Party")
	float SpawnRotationOffset = 60.0f;

public: // Party Functions
	UPROPERTY(BlueprintReadOnly, Category = "Character|Party")
	int32 PlayerPartyIndex;

	UPROPERTY(BlueprintReadOnly, Category = "Character|Party")
	TArray<FRPGId> PartyMemberIds;

	UPROPERTY(BlueprintReadOnly, Category = "Character|Party")
	TArray<FGuid> PartyMemberGuids;

	UPROPERTY(BlueprintReadOnly, Category = "Character|Party")
	int32 MaxPartyMembers;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Party", meta = (ClampMin = 0.0f))
	float SwitchCharacterDuration = 1.f;

	UFUNCTION(BlueprintPure, Category = "RPG|Character")
	TArray<FRPGId> GetPartyMembers() const;

	UFUNCTION(BlueprintPure, Category = "RPG|Character")
	ABaseCharacter* GetPlayerCharacter() const;

	UFUNCTION(BlueprintPure, Category = "RPG|Character")
	TArray<ABaseCharacter*> GetPartyMemberInstances() const;

	UFUNCTION(BlueprintPure, Category = "RPG|Character")
	ABaseCharacter* GetPartyMemberInstanceById(const FRPGId& Id) const;

	UFUNCTION(BlueprintCallable, Category = "RPG|Character", meta=(Latent, LatentInfo = "LatentInfo", InternalUseParam = "ReturnValue"))
	FRPGVoidCoroutine SpawnPartyMembersAsync(FVector Location, FRotator Rotation, ESpawnPartyMode SpawnPartyMode, FLatentActionInfo LatentInfo);

	UFUNCTION(BlueprintCallable, Category = "RPG|Character", meta=(Latent, LatentInfo = "LatentInfo", InternalUseParam = "ReturnValue"))
	FRPGVoidCoroutine SpawnNewPartyMembersAsync(TArray<FRPGId> NewParty, FVector Location, FRotator Rotation, ESpawnPartyMode SpawnPartyMode, FLatentActionInfo LatentInfo);

	// C++ core coroutines. These retain typed results so failures are not lost by WhenAll.
	TRPGCoroutine<TRPGAsyncResult<>> SpawnPartyMembersCoreAsync(
		FVector Location,
		FRotator Rotation,
		ESpawnPartyMode SpawnPartyMode
	);

	TRPGCoroutine<TRPGAsyncResult<>> SpawnNewPartyMembersCoreAsync(
		TArray<FRPGId> NewParty,
		FVector Location,
		FRotator Rotation,
		ESpawnPartyMode SpawnPartyMode
	);

	UFUNCTION(BlueprintCallable, Category = "RPG|Character")
	void DespawnPartyMembers();

	UFUNCTION(BlueprintCallable, Category = "RPG|Character")
	void TeleportPartyMembers(const FVector Location, const FRotator Rotation);

	void AddPartyMember(const FRPGId& Id, const int32 Index = -1);

	void RemovePartyMember(const FRPGId& Id);

	void RemovePartyMemberByIndex(const int32 Index);

	UFUNCTION(BlueprintCallable, Category = "RPG|Character", meta = (ExpandEnumAsExecs = Result, AdvancedDisplay = 3))
	void AddPartyMember(EPartyOperationResult& Result, const FRPGId& Id, const int32 Index = -1);

	UFUNCTION(BlueprintCallable, Category = "RPG|Character", meta = (ExpandEnumAsExecs = Result, AdvancedDisplay = 3))
	void RemovePartyMember(EPartyOperationResult& Result, const FRPGId& Id);

	UFUNCTION(BlueprintCallable, Category = "RPG|Character", meta = (ExpandEnumAsExecs = Result, AdvancedDisplay = 3))
	void RemovePartyMemberByIndex(EPartyOperationResult& Result, const int32 Index);

	UFUNCTION(BlueprintPure, Category = "RPG|Character")
	bool IsPartyMember(const FRPGId& Id) const;

	UFUNCTION(BlueprintPure, Category = "RPG|Character")
	int32 GetPartyIndexById(const FRPGId& Id) const;

	UFUNCTION(BlueprintPure, Category = "RPG|Character")
	bool CanJoinParty(const FRPGId& Id) const;

	UFUNCTION(BlueprintPure, Category = "RPG|Character")
	bool IsPartyEmpty() const;

	UFUNCTION(BlueprintPure, Category = "RPG|Character")
	int32 GetMaxPartyMembers() const { return MaxPartyMembers; }

	UFUNCTION(BlueprintCallable, Category = "RPG|Character")
	void PossessParty();

	UFUNCTION(BlueprintCallable, Category = "RPG|Character")
	void SwitchToCharacter(ESwitchCharacterPolicy SwitchPolicy, float DurationOverridden = -1.0f, bool bCanSwitchToDead = false);

	UFUNCTION(BlueprintCallable, Category = "RPG|Character")
	void SwitchPlayerCharacterByIndex(const int32 Index, const float DurationOverridden = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "RPG|Character")
	void SwitchPlayerCharacterById(const FRPGId& Id, const float DurationOverridden = -1.0f);

private:
	void UpdatePossessedCharacter(ABaseCharacter* NewCharacter, const FVector& Position, const FRotator& Rotation);

public: // ISaveable
	virtual FName GetSaveModuleType() const override;
	virtual void SaveDataTo(FInstancedStruct& SaveData) override;
	virtual void LoadDataFrom(const FInstancedStruct& SaveData) override;

	virtual FSimpleMulticastDelegate& OnLoadComplete() override;
	FSimpleMulticastDelegate LoadCompleteDelegate;

};
