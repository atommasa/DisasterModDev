// Fill out your copyright notice in the Description page of Project Settings.


#include "CharacterSubsystem.h"
#include "Characters/BaseCharacter.h"
#include "SaveGame/CharacterSaveModule.h"
#include "Characters/Attributes/CharacterAttributeSet.h"
#include "SaveGame/RPGSaveGame.h"
#include "Characters/Components/CharacterAbilitySystemComponent.h"
#include "Characters/Attributes/CharacterAttributeSet.h"

#include "Engine/AssetManager.h"
#include "Characters/CharacterPrimaryAsset.h"

bool UCharacterSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (this->GetClass()->IsInBlueprint() && Super::ShouldCreateSubsystem(Outer))
	{
		return true;
	}

	return false;
}

void UCharacterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	GhostASC = NewObject<UAbilitySystemComponent>(this);
	GhostAttributes = NewObject<UCharacterAttributeSet>(GhostASC);

	LoadPlayableCharacters();

}

void UCharacterSubsystem::Deinitialize()
{
	// Clean up all character instances
	for (const auto& Pair : InstanceCharacters)
	{
		if (Pair.Value.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("Destroying character instance: %s"), *Pair.Key.ToString());
			Pair.Value->Destroy();
		}
	}

	InstanceCharacters.Empty();
}

void UCharacterSubsystem::ModifyCharacterData(const FRPGId Id, FCharacterInstanceData& Data, const UGameplayEffect* Effect, const float Level)
{
	if (!GhostASC || !GhostAttributes || !Effect)
	{
		return;
	}

	GhostAttributes->LoadAttributesFrom(Data);

	FGameplayEffectContextHandle ContextHandle = GhostASC->MakeEffectContext();
	GhostASC->ApplyGameplayEffectToSelf(Effect, Level, ContextHandle);

	GhostAttributes->SaveAttributesTo(Data);
}

void UCharacterSubsystem::LoadPlayableCharacters()
{
	TArray<FPrimaryAssetId> CharacterAssets;
	UAssetManager::Get().GetPrimaryAssetIdList(
		FPrimaryAssetType(UCharacterPrimaryAsset::CharacterAssetType),
		CharacterAssets
	);

	// Using the Asset Manager to preload all character assets
	TSharedPtr<FStreamableHandle> Handle = UAssetManager::Get().PreloadPrimaryAssets(
		CharacterAssets,
		{},
		true,  // bLoadRecursive to ensure all dependencies are loaded
		FStreamableDelegate(),  // Delegate to call when loading is complete
		0      // Priority
	);

	if (Handle.IsValid())
	{
		Handle->WaitUntilComplete();  // Wait for the assets to be loaded
	}

	// Iterate through the loaded character assets and populate the maps
	for (const FPrimaryAssetId& Id : CharacterAssets)
	{
		auto* Asset = Cast<UCharacterPrimaryAsset>(UAssetManager::Get().GetPrimaryAssetObject(Id));
		if (Asset)
		{
			CharacterAssetMap.Add(Asset->Id, Asset);
			CharacterDataMap.Add(Asset->Id, Asset->DefaultData);

			UE_LOG(LogTemp, Log, TEXT("Loaded Character Asset: %s"), *Asset->Id.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get loaded asset for ID: %s"), *Id.ToString());
		}
	}
}

bool UCharacterSubsystem::IsCharacterIdMatched(const FRPGId Id) const
{
	return Id.GetIdType() == EIdType::Character;
}

ABaseCharacter* UCharacterSubsystem::GetCharacterInstance(const FRPGId Id) const
{
	const auto* Found = InstanceCharacters.Find(Id);
	return Found && Found->IsValid() ? Found->Get() : nullptr;
}

void UCharacterSubsystem::SetCharacterAvailability(const FRPGId Id, const ECharacterAvailabilityStatus Status)
{
	if (CharacterDataMap.Contains(Id))
	{
		CharacterDataMap[Id].AvailabilityStatus = Status;
	}
}

bool UCharacterSubsystem::IsAvailable(const FRPGId Id) const
{
	if (CharacterDataMap.Contains(Id))
	{
		return CharacterDataMap[Id].AvailabilityStatus == ECharacterAvailabilityStatus::Available;
	}

	return false;
}

ABaseCharacter* UCharacterSubsystem::SpawnCharacter(const FRPGId Id, const FVector Location, const FRotator Rotation)
{
	if (!CharacterDataMap.Contains(Id))
	{
		UE_LOG(LogTemp, Warning, TEXT("Attempted to spawn character with invalid Id: %s"), *Id.ToString());
		return nullptr;
	}
	
	// Check if the character is already in the game
	ABaseCharacter* InstanceCharacter = GetCharacterInstance(Id);
	if (InstanceCharacter)
	{
		InstanceCharacter->TeleportTo(Location, Rotation);

		return InstanceCharacter;
	}

	if (!WaitForRemove.IsEmpty())
	{
		// If the character is in the wait list, replace it with the new one
		auto ReplaceId = WaitForRemove.Pop();
		auto* ReplaceCharacter = GetCharacterInstance(ReplaceId);
		if (ReplaceId.IsValid() && ReplaceCharacter)
		{
			ReplaceCharacter->SetCharacterData(CharacterDataMap[Id]);
			InstanceCharacters.Remove(ReplaceId);
			InstanceCharacters.Add({ Id, ReplaceCharacter });

			return ReplaceCharacter;
		}
	}

	TSoftClassPtr<ABaseCharacter> ClassToSpawn = CharacterAssetMap[Id]->CharacterClass;
	if (!ClassToSpawn.IsValid())
	{
		ClassToSpawn.LoadSynchronous();
	}

	// If the class is still invalid after loading, log a warning and return nullptr
	if (!ClassToSpawn.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Attempted to spawn character with invalid class: %s"), *Id.ToString());
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.OverrideLevel = GetWorld()->PersistentLevel;

	// If there are no available character in the pool, dynamically spawn a new character
	auto* NewCharacter = GetWorld()->SpawnActor<ABaseCharacter>(ClassToSpawn.Get(), Location, Rotation, Params);
	if (NewCharacter)
	{
		NewCharacter->InitDefaultData(CharacterAssetMap[Id]);
		InstanceCharacters.Add(Id, NewCharacter);
	}

	return NewCharacter;
}

void UCharacterSubsystem::SpawnPartyMembers(const FVector Location, const FRotator Rotation)
{
	SpawnNewPartyMembers(PartyMembers, Location, Rotation);
}

void UCharacterSubsystem::SpawnNewPartyMembers(TArray<FRPGId> NewParty, const FVector Location, const FRotator Rotation)
{
	if (NewParty.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("NewParty is empty!"));
		return;
	}

	if (NewParty.Num() > MaxPartyMembers)
	{
		NewParty.SetNum(MaxPartyMembers);

		UE_LOG(LogTemp, Warning, TEXT("NewParty exceeds MaxPartyMembers. Truncated."));
	}

	TSet<FRPGId> CurrentSet(PartyMembers);
	TSet<FRPGId> NewSet(NewParty);

	TSet<FRPGId> Intersection = CurrentSet.Intersect(NewSet);
	TSet<FRPGId> ToReplace = NewSet.Difference(CurrentSet);
	TSet<FRPGId> ToRemove = CurrentSet.Difference(NewSet);

	// Update CurrentCharacterId if needed
	if (!NewSet.Contains(CurrentCharacterId) && NewParty.Num() > 0)
	{
		CurrentCharacterId = NewParty[0];

		UE_LOG(LogTemp, Warning, TEXT("CurrentCharacterId not in NewParty, defaulting to first in NewParty."));
	}

	for (const FRPGId& Id : Intersection)
	{
		ABaseCharacter* Character = GetCharacterInstance(Id);
		if (!Character)
		{
			ToReplace.Add(Id);
		}
	}

	for (const FRPGId& Id : ToRemove)
	{
		WaitForRemove.Add(Id);
	}

	// Only spawn or update new members
	for (const FRPGId& Id : ToReplace)
	{
		FVector SpawnLocation = Location;
		FRotator SpawnRotation = Rotation;

		if (Id != CurrentCharacterId)
		{
			SpawnLocation.X += FMath::RandRange(-SpawnLocationOffset, SpawnLocationOffset);
			SpawnLocation.Y += FMath::RandRange(-SpawnLocationOffset, SpawnLocationOffset);
			SpawnRotation.Yaw += FMath::RandRange(-SpawnRotationOffset, SpawnRotationOffset);
		}

		SpawnCharacter(Id, SpawnLocation, SpawnRotation);
	}

	PartyMembers = NewParty;

	ClearWaitForRemove();
}

void UCharacterSubsystem::DespawnCharacter(const FRPGId Id)
{	
	ABaseCharacter* Character = GetCharacterInstance(Id);
	if (Character)
	{
		Character->Destroy();
	}

	InstanceCharacters.Remove(Id);
}

void UCharacterSubsystem::ClearWaitForRemove()
{
	for (const FRPGId& Id : WaitForRemove)
	{
		DespawnCharacter(Id);
	}

	WaitForRemove.Empty();
}

EPartyOperationResult UCharacterSubsystem::AddPartyMember(const FRPGId Id)
{
	const auto* Data = CharacterDataMap.Find(Id);
	if (!Data)
	{
		return EPartyOperationResult::CharacterNotFound;
	}

	if (!CanJoinParty(Id))
	{
		return EPartyOperationResult::CharacterUnavailable;
	}

	if (IsPartyMember(Id))
	{
		return EPartyOperationResult::AlreadyInParty;
	}

	if (PartyMembers.Num() >= MaxPartyMembers)
	{
		return EPartyOperationResult::PartyFull;
	}

	PartyMembers.Add(Id);
	return EPartyOperationResult::Success;
}

EPartyOperationResult UCharacterSubsystem::RemovePartyMember(const FRPGId Id)
{
	const auto* Data = CharacterDataMap.Find(Id);
	if (!Data)
	{
		return EPartyOperationResult::CharacterNotFound;
	}

	if (!IsPartyMember(Id))
	{
		return EPartyOperationResult::NotInParty;
	}

	PartyMembers.Remove(Id);
	return EPartyOperationResult::Success;
}

bool UCharacterSubsystem::IsPartyMember(const FRPGId Id) const
{
	return PartyMembers.Contains(Id);
}

bool UCharacterSubsystem::CanJoinParty(const FRPGId Id) const
{
	return IsAvailable(Id);
}

void UCharacterSubsystem::GetSaveData(FBaseSaveModule* SaveData) const
{
	if (!SaveData)
	{
		UE_LOG(LogTemp, Error, TEXT("SaveData is null"));
		return;
	}
	
	if (SaveData->ModuleType != ESaveModuleType::Character)
	{
		UE_LOG(LogTemp, Error, TEXT("SaveData type mismatch"));
		return;
	}

	FCharacterSaveModule* Out = static_cast<FCharacterSaveModule*>(SaveData);

	for (const auto& Pair : InstanceCharacters)
	{
		if (!Pair.Value.IsValid())
		{
			continue;
		}

		FCharacterInstanceData SaveDataEntry;
		SaveDataEntry.AvailabilityStatus = CharacterDataMap.Contains(Pair.Key)
			? CharacterDataMap[Pair.Key].AvailabilityStatus
			: ECharacterAvailabilityStatus::Unavailable;

		if (const ABaseCharacter* Character = Cast<ABaseCharacter>(Pair.Value))
		{
			if (UCharacterAttributeSet* AttrSet = Character->AttributeSet)
			{
				AttrSet->SaveAttributesTo(SaveDataEntry); // TODO: Use GetCharacterData
			}
		}

		Out->CharacterData.Add(Pair.Key, SaveDataEntry);
	}

	Out->CurrentCharacterId = CurrentCharacterId;
	Out->CurrentParty = PartyMembers;
}

void UCharacterSubsystem::ApplySaveData(const FBaseSaveModule* SaveData)
{
	if (!SaveData)
	{
		UE_LOG(LogTemp, Error, TEXT("SaveData is null"));
		return;
	}
	
	if (SaveData->ModuleType != ESaveModuleType::Character)
	{
		UE_LOG(LogTemp, Error, TEXT("SaveData type mismatch"));
		return;
	}

	const FCharacterSaveModule* SaveModule = static_cast<const FCharacterSaveModule*>(SaveData);
	
	for (const auto& Pair : SaveModule->CharacterData)
	{
		const FRPGId& Id = Pair.Key;
		const FCharacterInstanceData& SaveDataEntry = Pair.Value;

		if (!CharacterDataMap.Contains(Id))
		{
			continue;
		}

		SetCharacterAvailability(Id, SaveDataEntry.AvailabilityStatus);

		if (InstanceCharacters.Contains(Id) && InstanceCharacters[Id].IsValid())
		{
			ABaseCharacter* Character = InstanceCharacters[Id].Get();
			if (Character)
			{
				Character->SetCharacterData(SaveDataEntry);
			}
		}

		CharacterDataMap[Id] = SaveDataEntry; // Update the character data map with the saved data
	}

	CurrentCharacterId = SaveModule->CurrentCharacterId;
	PartyMembers = SaveModule->CurrentParty;
}
