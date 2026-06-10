// Copyright Ironic Studio. All Rights Reserved.


#include "CharacterSubsystem.h"
#include "RPGSettings.h"
#include "SaveGameSubsystem.h"
#include "Kismet/GameplayStatics.h"

#include "Characters/Attributes/RPGAttributeSet.h"
#include "Characters/Components/CharacterAbilitySystemComponent.h"
#include "Characters/CharacterControlComponent.h"

#include "Camera/RPGPlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"

#include "CharacterSaveModule.h"
#include "SaveGame/RPGSaveGame.h"

#include "Helpers/RPGHelperMacros.h"
#include "Assets/RPGAssetLibrary.h"

DEFINE_LOG_CATEGORY(LogCharacterSubsystem);

void UCharacterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	// Load settings
	const URPGSettings* Settings = URPGSettings::GetRPGSettings();
	if (!Settings)
	{
		return;
	}

	MaxPartyMembers = Settings->MaxPartyMembers;
}

void UCharacterSubsystem::Deinitialize()
{
	// Clean up all character instances
	for (TPair<FGuid, TObjectPtr<ABaseCharacter>> Pair : InstanceCharacters)
	{
		if (Pair.Value)
		{
			UE_LOG(LogCharacterSubsystem, Log, TEXT("Destroying character instance: %s"), *Pair.Value->GetName());
			Pair.Value->Destroy();
		}
	}

	InstanceCharacters.Empty();
}

void UCharacterSubsystem::SaveCharacterDataToMap(ABaseCharacter* Instance)
{
	if (Instance && CharacterDataMap.Contains(Instance->GetId()))
	{
		FCharacterSaveData Data = Instance->GetCharacterData();
		Data.bIsSaveData = true; // Mark as save data

		CharacterDataMap[Instance->GetId()] = Data;
	}
}

void UCharacterSubsystem::SavePartyMembersDataToMap()
{
	for (const FGuid& Guid : PartyMemberGuids)
	{
		SaveCharacterDataToMap(InstanceCharacters.FindRef(Guid));
	}
}

void UCharacterSubsystem::ModifyCharactersData(const TArray<FRPGId>& Ids, UGameplayEffect* Effect, const float Level)
{
	if (!Effect)
	{
		return;
	}

	ABaseCharacter* TempOwner = nullptr;

	for (const FRPGId& Id : Ids)
	{
		if (!CharacterDataMap.Contains(Id))
		{
			continue;
		}

		int32 InstanceIndex = PartyMemberIds.IndexOfByKey(Id);
		if (InstanceIndex != INDEX_NONE)
		{
			if (ABaseCharacter* Instance = InstanceCharacters.FindRef(PartyMemberGuids[InstanceIndex]))
			{
				Instance->AbilitySystemComponent->ApplyGameplayEffectToSelf(Effect, Level, Instance->AbilitySystemComponent->MakeEffectContext());
				continue;
			}
		}

		if (!TempOwner)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			TempOwner = GetWorld()->SpawnActor<ABaseCharacter>(PlayableCharacterClass, Params);
			TempOwner->PrimaryActorTick.bCanEverTick = false; // Disable ticking for performance
			TempOwner->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Disable collision
		}

		FCharacterSaveData Data;
		Data.Attributes = CharacterDataMap[Id].Attributes;
		TempOwner->SetCharacterData(Data);

		TempOwner->AbilitySystemComponent->ApplyGameplayEffectToSelf(Effect, Level, TempOwner->AbilitySystemComponent->MakeEffectContext());

		// Update character data in subsystem
		for (UAttributeSet* Set : TempOwner->AbilitySystemComponent->GetSpawnedAttributes())
		{
			if (URPGAttributeSet* RPGSet = Cast<URPGAttributeSet>(Set))
			{
				CharacterDataMap[Id].Attributes = TempOwner->GetCharacterData().Attributes;
				RPGSet->SaveAttributesTo(CharacterDataMap[Id]);

				CharacterDataMap[Id].bIsSaveData = true; // Mark as save data
			}
		}
	}

	if (TempOwner)
	{
		TempOwner->Destroy();
	}
}

void UCharacterSubsystem::AddInstanceCharacter(ABaseCharacter* Character)
{
	if (InstanceCharacters.Contains(Character->GetInstanceId()))
	{
		return;
	}

	InstanceCharacters.Add(Character->GetInstanceId(), Character);
}

void UCharacterSubsystem::SetCharacterAvailability(const FRPGId& Id, const ECharacterAvailabilityStatus Status)
{
	if (CharacterDataMap.Contains(Id))
	{
		CharacterDataMap[Id].AvailabilityStatus = Status;
	}
}

bool UCharacterSubsystem::IsAvailable(const FRPGId& Id) const
{
	if (CharacterDataMap.Contains(Id))
	{
		return CharacterDataMap[Id].AvailabilityStatus == ECharacterAvailabilityStatus::Available;
	}

	return false;
}

ABaseCharacter* UCharacterSubsystem::SpawnCharacter(const FRPGId& Id, const TSubclassOf<ABaseCharacter> CharacterClass, const FVector Location, const FRotator Rotation, const bool bAsync)
{
	if (!Id.IsValid())
	{
		UE_LOG(LogCharacterSubsystem, Warning, TEXT("Spawn character with invalid Character Id!"));
		return nullptr;
	}

	if (bAsync)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		Params.OverrideLevel = GetWorld()->PersistentLevel;

		// Spawn the character actor immediately, but without data
		ABaseCharacter* NewCharacter = GetWorld()->SpawnActor<ABaseCharacter>(CharacterClass, Location, Rotation, Params);
		const FGuid CharacterGuid = NewCharacter->GetInstanceId();
		if (!InstanceCharacters.Contains(CharacterGuid))
		{
			UE_LOG(LogTemp, Display, TEXT("Spawning async character with Guid: %s"), *CharacterGuid.ToString());
			AddInstanceCharacter(NewCharacter);
		}

		// Apply data when asset is loaded
		URPGAssetLibrary::GetAssetByRPGIdAsync(Id, { "Character", "UI" }, [this, CharacterGuid](URPGPrimaryAsset* Asset)
			{
				OnCharacterToSpawnLoaded(Asset, CharacterGuid);
			});

		return NewCharacter;
	}
	else
	{
		URPGPrimaryAsset* Asset = URPGAssetLibrary::GetAssetByRPGId(Id);
		return SpawnCharacterByAsset(Cast<UCharacterAsset>(Asset), CharacterClass, Location, Rotation);
	}
}

ABaseCharacter* UCharacterSubsystem::SpawnCharacterByAsset(UCharacterAsset* Asset, const TSubclassOf<ABaseCharacter> CharacterClass, const FVector Location, const FRotator Rotation)
{
	if (!Asset)
	{
		UE_LOG(LogCharacterSubsystem, Warning, TEXT(__FUNCTION__"Invalid Character Asset!"));
		return nullptr;
	}

	TSubclassOf<ABaseCharacter> SpawnClass = CharacterClass;
	if (!SpawnClass)
	{
		SpawnClass = ABaseCharacter::StaticClass();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.OverrideLevel = GetWorld()->PersistentLevel;

	ABaseCharacter* NewCharacter = GetWorld()->SpawnActor<ABaseCharacter>(SpawnClass, Location, Rotation, Params);
	if (NewCharacter)
	{
		FCharacterSaveData* SavedData = CharacterDataMap.Find(Asset->GetId());
		if (SavedData)
		{
			NewCharacter->InitCharacterData(Asset, *SavedData);
		}
		else
		{
			NewCharacter->InitCharacterDataDefault(Asset);
		}

		AddInstanceCharacter(NewCharacter);

		return NewCharacter;
	}

	return nullptr;
}

void UCharacterSubsystem::DespawnCharacter(const FGuid& Guid)
{
	if (!Guid.IsValid() || !InstanceCharacters.Contains(Guid))
	{
		return;
	}

	if (ABaseCharacter* Character = InstanceCharacters[Guid])
	{
		// Save character data before destroying
		SaveCharacterDataToMap(Character);

		Character->Destroy();
	}

	InstanceCharacters.Remove(Guid);

	// Also remove form PartyMemberGuids
	if (PartyMemberGuids.Contains(Guid))
	{
		PartyMemberGuids.Remove(Guid);
	}
}

void UCharacterSubsystem::OnCharacterToSpawnLoaded(URPGPrimaryAsset* Asset, const FGuid InGuid)
{
	if (!Asset)
	{
		UE_LOG(LogCharacterSubsystem, Warning, TEXT("Failed to load character asset!"));
		return;
	}

	if (!InGuid.IsValid())
	{
		UE_LOG(LogCharacterSubsystem, Warning, TEXT("Invalid Guid provided to OnCharacterToSpawnLoaded!"));
		return;
	}

	if (TObjectPtr<ABaseCharacter> Instance = InstanceCharacters.FindRef(InGuid))
	{
		// Save character data before applying new data
		SaveCharacterDataToMap(Instance);

		if (const UCharacterAsset* CharacterAsset = Cast<UCharacterAsset>(Asset))
		{
			FCharacterSaveData* SavedData = CharacterDataMap.Find(Asset->GetId());
			if (SavedData)
			{
				Instance->InitCharacterData(CharacterAsset, *SavedData);
			}
			else
			{
				Instance->InitCharacterDataDefault(CharacterAsset);
			}

			Instance->OnCharacterDataInitialized.AddLambda([this](ABaseCharacter*)
				{
					PendingPartyInitCount--;
					TryBroadcastPartyReady();
				});
		}
		else
		{
			UE_LOG(LogCharacterSubsystem, Warning, TEXT("Loaded asset is not a Character Asset!"));
		}
	}
}

void UCharacterSubsystem::TryBroadcastPartyReady()
{
	if (bPartyTeleportDone && PendingPartyInitCount == 0)
	{
		bIsSpawningPartyMembers = false;
		OnPartyReady.Broadcast();
		OnPartyConstructed.Broadcast();
	}
}

TArray<APlayableCharacter*> UCharacterSubsystem::GetPartyMembers() const
{
	TArray<APlayableCharacter*> Result;
	for (const FGuid& Guid : PartyMemberGuids)
	{
		if (Guid.IsValid())
		{
			if (APlayableCharacter* Member = Cast<APlayableCharacter>(InstanceCharacters.FindRef(Guid)))
			{
				Result.Add(Member);
			}
		}
	}

	return Result;
}

ABaseCharacter* UCharacterSubsystem::GetPlayerCharacter() const
{
	TArray<ABaseCharacter*> PartyMemberInstances = GetPartyMemberInstances();
	
	return PartyMemberInstances.IsValidIndex(PlayerPartyIndex) ? PartyMemberInstances[PlayerPartyIndex] : nullptr;
}

TArray<ABaseCharacter*> UCharacterSubsystem::GetPartyMemberInstances() const
{
	TArray<ABaseCharacter*> PartyMemberInstances;
	for (const FGuid& Guid : PartyMemberGuids)
	{
		if (ABaseCharacter* Instance = InstanceCharacters.FindRef(Guid))
		{
			PartyMemberInstances.Add(Instance);
		}
	}

	return PartyMemberInstances;
}

ABaseCharacter* UCharacterSubsystem::GetPartyMemberInstanceById(const FRPGId Id) const
{
	TArray<ABaseCharacter*> PartyMemberInstances = GetPartyMemberInstances();
	int32 Index = PartyMemberIds.IndexOfByKey(Id);
	if (Index != INDEX_NONE)
	{
		return PartyMemberInstances[Index];
	}

	return nullptr;
}

void UCharacterSubsystem::SpawnPartyMembers(const FVector Location, const FRotator Rotation, bool bAsync, ESpawnPartyMode SpawnPartyMode)
{
	SpawnNewPartyMembers(PartyMemberIds, Location, Rotation, bAsync, SpawnPartyMode);
}

void UCharacterSubsystem::SpawnPartyMembers(const FTransform Transform, bool bAsync, ESpawnPartyMode SpawnPartyMode)
{
	SpawnNewPartyMembers(PartyMemberIds, Transform.GetLocation(), Transform.Rotator(), bAsync, SpawnPartyMode);
}

void UCharacterSubsystem::SpawnNewPartyMembers(TArray<FRPGId> NewParty, const FVector Location, const FRotator Rotation, bool bAsync, ESpawnPartyMode SpawnPartyMode)
{
	if (bIsSpawningPartyMembers)
	{
		UE_LOG(LogCharacterSubsystem, Warning, TEXT("Cannot spawn party members while another spawn is in progress!"));
		return;
	}

	bIsSpawningPartyMembers = true;
	PendingPartyInitCount = 0;
	bPartyTeleportDone = false;

	if (NewParty.IsEmpty())
	{
		UE_LOG(LogCharacterSubsystem, Warning, TEXT("NewParty is empty!"));
		return;
	}

	if (NewParty.Num() > MaxPartyMembers)
	{
		NewParty.SetNum(MaxPartyMembers);

		UE_LOG(LogCharacterSubsystem, Warning, TEXT("NewParty exceeds MaxPartyMembers. Truncated."));
	}

	NewParty.RemoveAll([](const FRPGId& MemberId) {
		return !MemberId.IsValid();
		});

	if (PartyMemberGuids.Num() < NewParty.Num())
	{
		PartyMemberGuids.SetNum(NewParty.Num());
	}

	switch(SpawnPartyMode)
	{
		case ESpawnPartyMode::KeepControlSameCharacter:
		{
			int32 NewPlayerIndex = NewParty.IndexOfByKey(PartyMemberIds[PlayerPartyIndex]);
			if (NewPlayerIndex == INDEX_NONE)
			{
				NewPlayerIndex = 0; // Unaveable to first member
			}
			
			PartyMemberGuids.Swap(PlayerPartyIndex, NewPlayerIndex);

			PlayerPartyIndex = NewPlayerIndex;

			break;
		}

		case ESpawnPartyMode::ByPlayerPartyIndex:
		{
			if (ABaseCharacter* CurrentCharacter = Cast<ABaseCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)))
			{
				if (const FGuid* CurrentGuid = InstanceCharacters.FindKey(CurrentCharacter))
				{
					int32 CurrentIndex = PartyMemberGuids.IndexOfByKey(*CurrentGuid);
					if (CurrentIndex != INDEX_NONE)
					{
						PartyMemberGuids.Swap(CurrentIndex, PlayerPartyIndex);
					}
				}
			}

			break;
		}
	}

	PartyMemberGuids.RemoveAll([](const FGuid& MemberGuid) {
		return !MemberGuid.IsValid();
		});

	TArray<FRPGId> ToSpawn;
	int32 Max = FMath::Max(PartyMemberGuids.Num(), NewParty.Num());
	for (int32 i = 0; i < Max; i++)
	{
		const bool bInstanceValid = PartyMemberGuids.IsValidIndex(i) && InstanceCharacters.Contains(PartyMemberGuids[i]);
		const bool bNewPartyValid = NewParty.IsValidIndex(i);
		
		// Should spawn new actor 
		if (!bInstanceValid && bNewPartyValid)
		{ 
			ToSpawn.Add(NewParty[i]);
		}
		
		// Should despawn old actor
		else if (bInstanceValid && !bNewPartyValid)
		{ 
			const FGuid RemoveGuid = PartyMemberGuids[i]; // We should not remove by refference, as the array may change during despawn
			DespawnCharacter(RemoveGuid);
		}
		
		// Set new Id to the current character instance
		else if (bInstanceValid && bNewPartyValid)
		{
			ABaseCharacter* Instance = InstanceCharacters[PartyMemberGuids[i]];

			// Save current character data before changing
			SaveCharacterDataToMap(Instance);

			const FRPGId& NewId = NewParty[i];

			if (CharacterDataMap.Contains(NewId))
			{
				Instance->InitCharacterDataById(NewId, CharacterDataMap[NewId]);
			}
			else
			{ 
				Instance->InitCharacterDataDefaultById(NewId);
			} 
		} 
	}

	PartyMemberIds = NewParty;

	TSubclassOf<ABaseCharacter> SpawnClass = PlayableCharacterClass;
	if (!SpawnClass)
	{
		SpawnClass = APlayableCharacter::StaticClass();
	}

	// Spawn each party member
	for (const FRPGId& SpawnId : ToSpawn)
	{
		if (!SpawnId.IsValid())
		{
			continue;
		}

		UE_LOG(LogCharacterSubsystem, Warning, TEXT("Spawn %s"), *SpawnId.ToString());

		ABaseCharacter* NewCharacter = SpawnCharacter(SpawnId, SpawnClass, Location, Rotation, bAsync);

		if (NewCharacter)
		{
			const FGuid& NewGuid = NewCharacter->GetInstanceId();
			PartyMemberGuids.Add(NewGuid);

			if (bAsync)
			{
				PendingPartyInitCount++;
			}
		}
		else
		{
			UE_LOG(LogCharacterSubsystem, Warning, TEXT("Failed to spawn party member for asset: %s"), *SpawnId.ToString());
		}
	}

	// After all members are spawned, teleport them to the location
	TeleportPartyMembers(Location, Rotation);

	bPartyTeleportDone = true;
	TryBroadcastPartyReady();
}

void UCharacterSubsystem::TeleportPartyMembers(const FVector Location, const FRotator Rotation)
{
	for (int32 i = 0; i < PartyMemberGuids.Num(); i++)
	{
		const FGuid& Guid = PartyMemberGuids[i];
		if (!Guid.IsValid() || !InstanceCharacters.Contains(Guid))
		{
			continue;
		}

		FVector SpawnLocation = Location;
		FRotator SpawnRotation = Rotation;

		if (i != PlayerPartyIndex)
		{
			SpawnLocation.X += FMath::RandRange(-SpawnLocationOffset, SpawnLocationOffset);
			SpawnLocation.Y += FMath::RandRange(-SpawnLocationOffset, SpawnLocationOffset);
			SpawnRotation.Yaw += FMath::RandRange(-SpawnRotationOffset, SpawnRotationOffset);
		}

		InstanceCharacters[Guid]->SetActorLocationAndRotation(SpawnLocation, SpawnRotation);

		// TODO: Reset character state (e.g., stop animations, reset physics, etc.)
	}
}

void UCharacterSubsystem::AddPartyMember(const FRPGId Id, const int32 Index)
{
	EPartyOperationResult Result;
	AddPartyMember(Result, Id, Index);
}

void UCharacterSubsystem::RemovePartyMember(const FRPGId Id)
{
	EPartyOperationResult Result;
	RemovePartyMember(Result, Id);
}

void UCharacterSubsystem::RemovePartyMemberByIndex(const int32 Index)
{
	EPartyOperationResult Result;
	RemovePartyMemberByIndex(Result, Index);
}

void UCharacterSubsystem::AddPartyMember(OUT EPartyOperationResult& Result, const FRPGId Id, const int32 Index)
{
	const auto* Data = CharacterDataMap.Find(Id);
	if (!Data)
	{
		Result = EPartyOperationResult::CharacterNotFound;
		return;
	}

	if (!IsAvailable(Id))
	{
		Result = EPartyOperationResult::CharacterUnavailable;
		return;
	}

	if (IsPartyMember(Id))
	{
		Result = EPartyOperationResult::AlreadyInParty;
		return;
	}

	if (PartyMemberIds.IsValidIndex(Index))
	{
		PartyMemberIds[Index] = Id;
	}
	else if (Index != INDEX_NONE)
	{
		Result = EPartyOperationResult::InvalidOperation;
		return;
	}

	for (FRPGId& MemberId : PartyMemberIds)
	{
		if (!MemberId.IsValid())
		{
			MemberId = Id;

			Result = EPartyOperationResult::Success;
			return;
		}
	}

	if (PartyMemberIds.Num() < MaxPartyMembers)
	{
		PartyMemberIds.Add(Id);

		Result = EPartyOperationResult::Success;
	}
	else
	{
		Result = EPartyOperationResult::PartyFull;
	}
}

void UCharacterSubsystem::RemovePartyMember(OUT EPartyOperationResult& Result, const FRPGId Id)
{
	const auto* Data = CharacterDataMap.Find(Id);
	if (!Data)
	{
		Result = EPartyOperationResult::CharacterNotFound;
		return;
	}

	int32 FoundIndex = PartyMemberIds.IndexOfByKey(Id);
	if (FoundIndex != INDEX_NONE)
	{
		PartyMemberIds[FoundIndex] = FRPGId();

		Result = EPartyOperationResult::Success;
	}
	else
	{
		Result = EPartyOperationResult::NotInParty;
	}
}

void UCharacterSubsystem::RemovePartyMemberByIndex(OUT EPartyOperationResult& Result, const int32 Index)
{
	if (PartyMemberIds.IsValidIndex(Index))
	{
		PartyMemberIds[Index] = FRPGId();

		Result = EPartyOperationResult::Success;
		return;
	}

	Result = EPartyOperationResult::InvalidOperation;
}

bool UCharacterSubsystem::IsPartyMember(const FRPGId Id) const
{
	return PartyMemberIds.Contains(Id);
}

bool UCharacterSubsystem::CanJoinParty(const FRPGId Id) const
{
	return IsAvailable(Id);
}

void UCharacterSubsystem::SwitchToNextCharacter(const float DurationOverridden)
{
	int32 Index = PlayerPartyIndex + 1;
	if (Index >= PartyMemberGuids.Num())
	{
		Index = 0;
	}

	SwitchPlayerCharacterByIndex(Index, DurationOverridden);
}

void UCharacterSubsystem::SwitchToPreviousCharacter(const float DurationOverridden)
{
	int32 Index = PlayerPartyIndex - 1;
	if (Index < 0)
	{
		Index = PartyMemberGuids.Num() - 1;
	}

	SwitchPlayerCharacterByIndex(Index, DurationOverridden);
}

void UCharacterSubsystem::SwitchPlayerCharacterByIndex(const int32 Index, const float DurationOverridden)
{
	if (!PartyMemberGuids.IsValidIndex(Index))
	{
		return;
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		ABaseCharacter* OldPlayerCharacter = GetPlayerCharacter();
		ABaseCharacter* NewPlayerCharacter = InstanceCharacters[PartyMemberGuids[Index]];

		if (OldPlayerCharacter && NewPlayerCharacter)
		{
			if (OldPlayerCharacter == NewPlayerCharacter)
			{
				return;
			}
			
			// We do not want character to move when switching
			if (UCharacterControlComponent* ControlComponent = PC->FindComponentByClass<UCharacterControlComponent>())
			{
				ControlComponent->DisableMoveContext();
			}

			if (ARPGPlayerCameraManager* CamMgr = Cast<ARPGPlayerCameraManager>(PC->PlayerCameraManager))
			{
				CamMgr->ActorCameraViewTransition(
					NewPlayerCharacter,
					DurationOverridden >= 0.0f ? DurationOverridden : SwitchCharacterDuration,
					nullptr,
					[this, Index, NewPlayerCharacter, OldPlayerCharacter](const FVector& Position, const FRotator& Rotation) {
						PlayerPartyIndex = Index;
						UpdatePossessedCharacter(OldPlayerCharacter, NewPlayerCharacter, Position, Rotation);
					}
				);
			}
		}
	}
}

void UCharacterSubsystem::SwitchPlayerCharacterById(const FRPGId& Id, const float DurationOverridden)
{
	SwitchPlayerCharacterByIndex(PartyMemberIds.IndexOfByKey(Id), DurationOverridden);
}

void UCharacterSubsystem::UpdatePossessedCharacter(ABaseCharacter* OldCharacter, ABaseCharacter* NewCharacter, const FVector& Position, const FRotator& Rotation)
{
	if (!NewCharacter)
	{
		return;
	}

	if (ARPGPlayerController* PC = Cast<ARPGPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		PC->Possess(NewCharacter);
		PC->SetControlRotation(Rotation);
	}
	else
	{
		UE_LOG(LogCharacterSubsystem, Warning, TEXT("PlayerController is not a RPGPlayerController!"));
	}
}

FName UCharacterSubsystem::GetSaveModuleType() const
{
	return FCharacterSaveModule::StaticStruct()->GetFName();
}

void UCharacterSubsystem::SaveDataTo(FInstancedStruct& SaveData)
{
	SaveData.Reset();

	FCharacterSaveModule CharacterSave;
	
	// Force save all party members' data to map before saving
	SavePartyMembersDataToMap();

	CharacterSave.CharacterData = CharacterDataMap;

	CharacterSave.PlayerPartyIndex = PlayerPartyIndex;
	CharacterSave.PartyMembers = PartyMemberIds;

	SaveData.InitializeAs<FCharacterSaveModule>(CharacterSave);
}

void UCharacterSubsystem::LoadDataFrom(const FInstancedStruct& SaveData)
{
	if (const FCharacterSaveModule* CharacterSave = SaveData.GetPtr<FCharacterSaveModule>())
	{
		CharacterDataMap = CharacterSave->CharacterData;

		PlayerPartyIndex = CharacterSave->PlayerPartyIndex;
		PartyMemberIds = CharacterSave->PartyMembers;

		// Load character data of all party members to map, so that they can be applied to character instances when spawned
		URPGAssetLibrary::GetAssetArrayByRPGIdsAsync(PartyMemberIds, { "Character", "UI" }, [this](TArray<URPGPrimaryAsset*> Result)
			{
				OnLoadComplete().Broadcast();
			});
	}
	// If the struct type does not match, we treat it as a new game
	else
	{
		const URPGSettings* Settings = URPGSettings::GetRPGSettings();
		if (!Settings)
		{
			return;
		}

		// If is new game, initialize default party members
		PartyMemberIds = Settings->DefaultPartyMembers;

		// Initialize player party index
		PlayerPartyIndex = Settings->DefaultPlayerIndex;

		// Initialize character data map
		URPGAssetLibrary::GetAssetArrayByRPGIdsAsync(Settings->PlayableCharacters, {}, [this, DefaultParty = Settings->DefaultPartyMembers](TArray<URPGPrimaryAsset*> Result)
			{
				for (URPGPrimaryAsset* Asset : Result)
				{
					if (UCharacterAsset* CharacterAsset = Cast<UCharacterAsset>(Asset))
					{
						CharacterDataMap.Add(Asset->GetId(), CharacterAsset->GetDefaultData());
					}
				}

				// Load default party members' data to map, so that they can be applied to character instances when spawned
				URPGAssetLibrary::GetAssetArrayByRPGIdsAsync(DefaultParty, { "Character", "UI"}, [this](TArray<URPGPrimaryAsset*> Result)
					{
						OnLoadComplete().Broadcast();
					});
			});
	}
}

FSimpleMulticastDelegate& UCharacterSubsystem::OnLoadComplete()
{
	return LoadCompleteDelegate;
}
