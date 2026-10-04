// Copyright Ironic Studio. All Rights Reserved.


#include "CharacterSubsystem.h"
#include "Settings/CharacterSystemSettings.h"
#include "SaveGameSubsystem.h"
#include "Kismet/GameplayStatics.h"

#include "Characters/Attributes/RPGAttributeSet.h"
#include "Characters/Components/CharacterAbilitySystemComponent.h"
#include "Characters/Components/RPGCharacterMovementComponent.h"
#include "Characters/CharacterControlComponent.h"
#include "Characters/Components/CombatComponent.h"

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
	const UCharacterSystemSettings* Settings = GetDefault<UCharacterSystemSettings>();
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
		if (PartyMemberGuids.IsValidIndex(InstanceIndex))
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

FRPGVoidCoroutine UCharacterSubsystem::SpawnCharacterAsync(
	FRPGId Id,
	TSubclassOf<ABaseCharacter> CharacterClass,
	FVector Location,
	FRotator Rotation,
	ABaseCharacter*& OutCharacter,
	FLatentActionInfo LatentInfo)
{
	OutCharacter = nullptr;

	const TRPGAsyncResult<ABaseCharacter*> Result = co_await SpawnCharacterCoreAsync(Id, CharacterClass, Location, Rotation);

	if (Result.IsSuccess() && IsValid(Result.Value))
	{
		OutCharacter = Result.Value;
	}
	else
	{
		UE_LOG(
			LogCharacterSubsystem,
			Error,
			TEXT("SpawnCharacterAsync failed for Id: %s"),
			*Id.ToString());
	}

	co_return;
}

TRPGCoroutine<TRPGAsyncResult<ABaseCharacter*>> UCharacterSubsystem::SpawnCharacterCoreAsync(
	FRPGId Id,
	TSubclassOf<ABaseCharacter> CharacterClass,
	FVector Location,
	FRotator Rotation)
{
	if (!Id.IsValid())
	{
		co_return TRPGAsyncResult<ABaseCharacter*>::Failure(
			TEXT("Character.InvalidId"),
			TEXT("Cannot spawn a character with an invalid character Id."));
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		co_return TRPGAsyncResult<ABaseCharacter*>::Failure(
			TEXT("Character.InvalidWorld"),
			TEXT("Character subsystem does not have a valid world."));
	}

	TSubclassOf<ABaseCharacter> SpawnClass = CharacterClass;
	if (!SpawnClass)
	{
		SpawnClass = PlayableCharacterClass;
	}
	if (!SpawnClass)
	{
		SpawnClass = ABaseCharacter::StaticClass();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.OverrideLevel = World->PersistentLevel;

	// Spawn the shell immediately. The coroutine does not return success until
	// its asset and character data are fully initialized.
	ABaseCharacter* NewCharacter = World->SpawnActor<ABaseCharacter>(SpawnClass, Location, Rotation, Params);

	if (!IsValid(NewCharacter))
	{
		co_return TRPGAsyncResult<ABaseCharacter*>::Failure(
			TEXT("Character.SpawnFailed"),
			TEXT("Failed to spawn the character actor."));
	}

	const FGuid CharacterGuid = NewCharacter->GetInstanceId();
	AddInstanceCharacter(NewCharacter);

	const auto LoadResult = co_await URPGAssetLibrary::LoadAssetByRPGIdAsync<UCharacterAsset>(
			Id,
			{ "Character", "UI" });

	if (!LoadResult.IsSuccess() || !IsValid(LoadResult.Value))
	{
		DespawnCharacterInstance(CharacterGuid);

		co_return TRPGAsyncResult<ABaseCharacter*>::Failure(
			TEXT("Character.AssetLoadFailed"),
			TEXT("Failed to load the character asset."));
	}

	const TRPGAsyncResult<> InitResult = co_await InitializeCharacterAndWait(LoadResult.Value, CharacterGuid);

	if (!InitResult.IsSuccess())
	{
		DespawnCharacterInstance(CharacterGuid);

		co_return TRPGAsyncResult<ABaseCharacter*>::Failure(
			TEXT("Character.InitializationFailed"),
			TEXT("Failed to initialize the character."));
	}

	ABaseCharacter* InitializedCharacter = InstanceCharacters.FindRef(CharacterGuid);

	if (!IsValid(InitializedCharacter))
	{
		co_return TRPGAsyncResult<ABaseCharacter*>::Failure(
			TEXT("Character.Destroyed"),
			TEXT("Character was destroyed during initialization."));
	}

	co_return TRPGAsyncResult<ABaseCharacter*>::Success(
		InitializedCharacter);
}

ABaseCharacter* UCharacterSubsystem::SpawnCharacterByAsset(UCharacterAsset* Asset, const TSubclassOf<ABaseCharacter> CharacterClass, const FVector Location, const FRotator Rotation)
{
	if (!Asset)
	{
		UE_LOG(LogCharacterSubsystem, Warning, TEXT(__FUNCTION__ "Invalid Character Asset!"));
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
	if (!IsValid(NewCharacter))
	{
		UE_LOG(LogCharacterSubsystem, Warning, TEXT("Failed to spawn character for asset: %s"), *GetNameSafe(Asset));
		return nullptr;
	}

	AddInstanceCharacter(NewCharacter);

	if (FCharacterSaveData* SavedData = CharacterDataMap.Find(Asset->GetId()))
	{
		NewCharacter->InitCharacterData(Asset, *SavedData);
	}
	else
	{
		NewCharacter->InitCharacterDataDefault(Asset);
	}

	return NewCharacter;
}

void UCharacterSubsystem::DespawnCharacterInstance(const FGuid& Guid)
{
	if (!Guid.IsValid())
	{
		return;
	}

	if (ABaseCharacter* Character = InstanceCharacters.FindRef(Guid))
	{
		SaveCharacterDataToMap(Character);
		Character->Destroy();
	}

	InstanceCharacters.Remove(Guid);
}

void UCharacterSubsystem::DespawnCharacter(const FGuid& Guid)
{
	if (!Guid.IsValid())
	{
		return;
	}

	DespawnCharacterInstance(Guid);

	const int32 PartyIndex = PartyMemberGuids.IndexOfByKey(Guid);
	if (PartyIndex != INDEX_NONE)
	{
		PartyMemberGuids.RemoveAt(PartyIndex);
		if (PartyMemberIds.IsValidIndex(PartyIndex))
		{
			PartyMemberIds.RemoveAt(PartyIndex);
		}

		PlayerPartyIndex = PartyMemberIds.IsEmpty()
			? INDEX_NONE
			: FMath::Clamp(PlayerPartyIndex, 0, PartyMemberIds.Num() - 1);
	}
}

void UCharacterSubsystem::PartyTravelStart()
{
	for (ABaseCharacter* Character : GetPartyMemberInstances())
	{
		if (!IsValid(Character))
		{
			continue;
		}

		Character->SetActorTickEnabled(false);
		Character->SetActorEnableCollision(false);

		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
	}
}

void UCharacterSubsystem::PartyTravelEnd()
{
	const TArray<ABaseCharacter*>& Instances = GetPartyMemberInstances();

	if (Instances.IsEmpty())
	{
		// Core coroutines are eager. Party construction will continue asynchronously
		// and broadcast OnPartyReady only after every member is initialized.
		SpawnPartyMembersCoreAsync(
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			ESpawnPartyMode::KeepControlSameCharacter);
		return;
	}

	for (ABaseCharacter* Character : Instances)
	{
		if (!Character)
		{
			continue;
		}

		Character->SetActorEnableCollision(true);
		Character->SetActorTickEnabled(true);

		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}

	if (ARPGPlayerController* PlayerController = Cast<ARPGPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		ABaseCharacter* PlayerCharacter = GetPlayerCharacter();

		if (PlayerCharacter && PlayerController->GetPawn() != PlayerCharacter)
		{
			PlayerController->PossessCharacterWithMode(PlayerCharacter, ERPGControlMode::None);
		}
	}
}

TRPGCoroutine<TRPGAsyncResult<>> UCharacterSubsystem::InitializeCharacterAndWait(UCharacterAsset* Asset, FGuid InGuid)
{
	if (!IsValid(Asset))
	{
		co_return TRPGAsyncResult<>::Failure(
			TEXT("Character.InvalidAsset"),
			TEXT("Character asset could not be loaded."));
	}

	ABaseCharacter* Instance = InstanceCharacters.FindRef(InGuid);
	if (!IsValid(Instance))
	{
		co_return TRPGAsyncResult<>::Failure(
			TEXT("Character.InvalidCharacter"),
			TEXT("Character instance no longer exists."));
	}

	// This child coroutine is eager, so WaitDelegate is bound before InitCharacterData
	// can synchronously broadcast OnCharacterDataInitialized.
	TRPGCoroutine<TRPGAsyncResult<>> InitializationWait =
		[Instance]() -> TRPGCoroutine<TRPGAsyncResult<>>
		{
			const auto WaitResult =
				co_await RPGFlow::WaitDelegate(
					Instance,
					Instance->OnCharacterDataInitialized);

			if (WaitResult.WasOwnerEndedPlay())
			{
				co_return TRPGAsyncResult<>::Failure(
					TEXT("Character.EndedPlay"),
					TEXT("Character ended play during initialization."));
			}

			if (WaitResult.WasOwnerDestroyed())
			{
				co_return TRPGAsyncResult<>::Failure(
					TEXT("Character.Destroyed"),
					TEXT("Character was destroyed during initialization."));
			}

			if (WaitResult.WasCanceled())
			{
				co_return TRPGAsyncResult<>::Failure(
					TEXT("Character.Canceled"),
					TEXT("Character initialization was canceled."));
			}

			if (!WaitResult.WasCompleted())
			{
				co_return TRPGAsyncResult<>::Failure(
					TEXT("Character.InitializationFailed"),
					TEXT("Character initialization did not complete."));
			}

			co_return TRPGAsyncResult<>::Success();
		}();

	SaveCharacterDataToMap(Instance);

	if (const FCharacterSaveData* SavedData =
		CharacterDataMap.Find(Asset->GetId()))
	{
		Instance->InitCharacterData(Asset, *SavedData);
	}
	else
	{
		Instance->InitCharacterDataDefault(Asset);
	}

	co_return co_await InitializationWait;
}

TArray<FRPGId> UCharacterSubsystem::GetPartyMembers() const
{
	return PartyMemberIds;
}

ABaseCharacter* UCharacterSubsystem::GetPlayerCharacter() const
{
	if (!PartyMemberGuids.IsValidIndex(PlayerPartyIndex))
	{
		return nullptr;
	}

	return InstanceCharacters.FindRef(PartyMemberGuids[PlayerPartyIndex]);
}

TArray<ABaseCharacter*> UCharacterSubsystem::GetPartyMemberInstances() const
{
	TArray<ABaseCharacter*> Result;
	for (const FGuid& Guid : PartyMemberGuids)
	{
		if (Guid.IsValid())
		{
			if (ABaseCharacter* Member = Cast<ABaseCharacter>(InstanceCharacters.FindRef(Guid)))
			{
				Result.Add(Member);
			}
		}
	}

	return Result;
}

ABaseCharacter* UCharacterSubsystem::GetPartyMemberInstanceById(const FRPGId& Id) const
{
	const int32 Index = PartyMemberIds.IndexOfByKey(Id);
	if (!PartyMemberGuids.IsValidIndex(Index))
	{
		return nullptr;
	}

	return InstanceCharacters.FindRef(PartyMemberGuids[Index]);
}

FRPGVoidCoroutine UCharacterSubsystem::SpawnPartyMembersAsync(
	FVector Location,
	FRotator Rotation,
	ESpawnPartyMode SpawnPartyMode,
	FLatentActionInfo LatentInfo)
{
	const TRPGAsyncResult<> Result = co_await SpawnPartyMembersCoreAsync(
			Location,
			Rotation,
			SpawnPartyMode);

	if (!Result.IsSuccess())
	{
		UE_LOG(
			LogCharacterSubsystem,
			Error,
			TEXT("SpawnPartyMembersAsync failed."));
	}

	co_return;
}

TRPGCoroutine<TRPGAsyncResult<>> UCharacterSubsystem::SpawnPartyMembersCoreAsync(
	FVector Location,
	FRotator Rotation,
	ESpawnPartyMode SpawnPartyMode)
{
	co_return co_await SpawnNewPartyMembersCoreAsync(
		PartyMemberIds,
		Location,
		Rotation,
		SpawnPartyMode);
}

FRPGVoidCoroutine UCharacterSubsystem::SpawnNewPartyMembersAsync(
	TArray<FRPGId> NewParty,
	FVector Location,
	FRotator Rotation,
	ESpawnPartyMode SpawnPartyMode,
	FLatentActionInfo LatentInfo)
{
	const TRPGAsyncResult<> Result =
		co_await SpawnNewPartyMembersCoreAsync(
			MoveTemp(NewParty),
			Location,
			Rotation,
			SpawnPartyMode);

	if (!Result.IsSuccess())
	{
		UE_LOG(
			LogCharacterSubsystem,
			Error,
			TEXT("SpawnNewPartyMembersAsync failed."));
	}

	co_return;
}

TRPGCoroutine<TRPGAsyncResult<>> UCharacterSubsystem::SpawnNewPartyMembersCoreAsync(
	TArray<FRPGId> NewParty,
	FVector Location,
	FRotator Rotation,
	ESpawnPartyMode SpawnPartyMode)
{
	if (bIsSpawningPartyMembers)
	{
		co_return TRPGAsyncResult<>::Failure(
			TEXT("Party.SpawnInProgress"),
			TEXT("Another party spawn operation is already in progress."));
	}

	if (!PlayableCharacterClass)
	{
		co_return TRPGAsyncResult<>::Failure(
			TEXT("Party.InvalidCharacterClass"),
			TEXT("PlayableCharacterClass is null."));
	}

	NewParty.RemoveAll(
		[](const FRPGId& MemberId)
		{
			return !MemberId.IsValid();
		});

	if (NewParty.Num() > MaxPartyMembers)
	{
		NewParty.SetNum(MaxPartyMembers);

		UE_LOG(
			LogCharacterSubsystem,
			Warning,
			TEXT("NewParty exceeds MaxPartyMembers. Truncated."));
	}

	if (NewParty.IsEmpty())
	{
		co_return TRPGAsyncResult<>::Failure(
			TEXT("Party.Empty"),
			TEXT("Cannot construct an empty party."));
	}

	bIsSpawningPartyMembers = true;
	bPartyTeleportDone = false;

	const TArray<FRPGId> OldPartyIds = PartyMemberIds;
	const TArray<FGuid> OldPartyGuids = PartyMemberGuids;
	const int32 OldPlayerPartyIndex = PlayerPartyIndex;

	FRPGId PreviouslyControlledId;
	if (SpawnPartyMode == ESpawnPartyMode::KeepControlSameCharacter && OldPartyIds.IsValidIndex(OldPlayerPartyIndex))
	{
		PreviouslyControlledId = OldPartyIds[OldPlayerPartyIndex];
	}

	// Candidate arrays are not exposed until every new character is ready.
	TArray<FGuid> CandidatePartyGuids;
	CandidatePartyGuids.SetNum(NewParty.Num());

	TSet<FGuid> ReusedGuids;

	for (int32 NewIndex = 0; NewIndex < NewParty.Num(); ++NewIndex)
	{
		for (int32 OldIndex = 0; OldIndex < OldPartyIds.Num(); ++OldIndex)
		{
			if (OldPartyIds[OldIndex] != NewParty[NewIndex] || !OldPartyGuids.IsValidIndex(OldIndex))
			{
				continue;
			}

			const FGuid CandidateGuid = OldPartyGuids[OldIndex];
			if (!CandidateGuid.IsValid() || ReusedGuids.Contains(CandidateGuid))
			{
				continue;
			}

			ABaseCharacter* Candidate = InstanceCharacters.FindRef(CandidateGuid);

			if (IsValid(Candidate) && Candidate->GetId() == NewParty[NewIndex])
			{
				CandidatePartyGuids[NewIndex] = CandidateGuid;
				ReusedGuids.Add(CandidateGuid);
				break;
			}
		}
	}

	struct FPendingPartySpawn
	{
		int32 PartyIndex = INDEX_NONE;
		TRPGCoroutine<TRPGAsyncResult<ABaseCharacter*>> Task;
	};

	TArray<FPendingPartySpawn> PendingSpawns;
	PendingSpawns.Reserve(NewParty.Num());

	// SpawnCharacterCoreAsync is eager. Every task starts here before we await
	// any individual result, so asset loading still proceeds concurrently.
	for (int32 Index = 0; Index < NewParty.Num(); ++Index)
	{
		if (CandidatePartyGuids[Index].IsValid())
		{
			continue;
		}

		FPendingPartySpawn& Pending = PendingSpawns.AddDefaulted_GetRef();

		Pending.PartyIndex = Index;
		Pending.Task = SpawnCharacterCoreAsync(
			NewParty[Index],
			PlayableCharacterClass,
			Location,
			Rotation);
	}

	TArray<FGuid> NewlySpawnedGuids;
	NewlySpawnedGuids.Reserve(PendingSpawns.Num());

	for (FPendingPartySpawn& Pending : PendingSpawns)
	{
		const TRPGAsyncResult<ABaseCharacter*> SpawnResult = co_await Pending.Task;

		if (!SpawnResult.IsSuccess() || !IsValid(SpawnResult.Value))
		{
			for (const FGuid& SpawnedGuid : NewlySpawnedGuids)
			{
				DespawnCharacterInstance(SpawnedGuid);
			}

			bIsSpawningPartyMembers = false;
			bPartyTeleportDone = false;

			co_return TRPGAsyncResult<>::Failure(
				TEXT("Party.MemberSpawnFailed"),
				TEXT("One or more party members failed to spawn."));
		}

		const FGuid SpawnedGuid = SpawnResult.Value->GetInstanceId();

		CandidatePartyGuids[Pending.PartyIndex] = SpawnedGuid;
		NewlySpawnedGuids.Add(SpawnedGuid);
	}

	for (const FGuid& Guid : CandidatePartyGuids)
	{
		if (!Guid.IsValid() || !IsValid(InstanceCharacters.FindRef(Guid)))
		{
			for (const FGuid& SpawnedGuid : NewlySpawnedGuids)
			{
				DespawnCharacterInstance(SpawnedGuid);
			}

			bIsSpawningPartyMembers = false;
			bPartyTeleportDone = false;

			co_return TRPGAsyncResult<>::Failure(
				TEXT("Party.InvalidCandidate"),
				TEXT("The constructed party contains an invalid character."));
		}
	}

	// Commit atomically only after every new member is initialized.
	PartyMemberIds = NewParty;
	PartyMemberGuids = CandidatePartyGuids;

	if (SpawnPartyMode == ESpawnPartyMode::KeepControlSameCharacter)
	{
		const int32 NewPlayerIndex = NewParty.IndexOfByKey(PreviouslyControlledId);

		PlayerPartyIndex =
			NewPlayerIndex != INDEX_NONE
			? NewPlayerIndex
			: 0;
	}
	else
	{
		PlayerPartyIndex =
			FMath::Clamp(
				OldPlayerPartyIndex,
				0,
				NewParty.Num() - 1);
	}

	// Old actors are destroyed only after the new party state has committed.
	for (const FGuid& OldGuid : OldPartyGuids)
	{
		if (OldGuid.IsValid() && !ReusedGuids.Contains(OldGuid))
		{
			DespawnCharacterInstance(OldGuid);
		}
	}

	TeleportPartyMembers(Location, Rotation);
	bPartyTeleportDone = true;
	bIsSpawningPartyMembers = false;

	PossessParty();

	OnPartyConstructed.Broadcast();
	OnPartyReady.Broadcast();

	co_return TRPGAsyncResult<>::Success();
}

void UCharacterSubsystem::DespawnPartyMembers()
{
	for (const FGuid& Guid : PartyMemberGuids)
	{
		DespawnCharacterInstance(Guid);
	}
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

void UCharacterSubsystem::AddPartyMember(const FRPGId& Id, const int32 Index)
{
	EPartyOperationResult Result;
	AddPartyMember(Result, Id, Index);
}

void UCharacterSubsystem::RemovePartyMember(const FRPGId& Id)
{
	EPartyOperationResult Result;
	RemovePartyMember(Result, Id);
}

void UCharacterSubsystem::RemovePartyMemberByIndex(const int32 Index)
{
	EPartyOperationResult Result;
	RemovePartyMemberByIndex(Result, Index);
}

void UCharacterSubsystem::AddPartyMember(OUT EPartyOperationResult& Result, const FRPGId& Id, const int32 Index)
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

void UCharacterSubsystem::RemovePartyMember(OUT EPartyOperationResult& Result, const FRPGId& Id)
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

bool UCharacterSubsystem::IsPartyMember(const FRPGId& Id) const
{
	return PartyMemberIds.Contains(Id);
}

int32 UCharacterSubsystem::GetPartyIndexById(const FRPGId& Id) const
{
	return PartyMemberIds.Find(Id);
}

bool UCharacterSubsystem::CanJoinParty(const FRPGId& Id) const
{
	return IsAvailable(Id);
}

bool UCharacterSubsystem::IsPartyEmpty() const
{
	return PartyMemberIds.IsEmpty() || !PartyMemberIds.FindByPredicate([](FRPGId Item)
		{
			return Item.IsValid();
		});
}

void UCharacterSubsystem::PossessParty()
{
	if (ARPGPlayerController* PC = Cast<ARPGPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		PC->PossessCharacter(GetPartyMemberInstances()[PlayerPartyIndex]);
	}
}

void UCharacterSubsystem::SwitchToCharacter(ESwitchCharacterPolicy SwitchPolicy, float DurationOverridden, bool bCanSwitchToDead)
{
	auto GetNextIndex = [](ESwitchCharacterPolicy InSwitchPolicy)
		{
			return InSwitchPolicy == ESwitchCharacterPolicy::ToNext ? 1 : -1;
		};

	int32 Index = PlayerPartyIndex + GetNextIndex(SwitchPolicy);

	while (Index != PlayerPartyIndex)
	{
		if (Index >= PartyMemberGuids.Num())
		{
			Index = 0;
		}
		else if (Index < 0)
		{
			Index = PartyMemberGuids.Num() - 1;
		}

		ABaseCharacter* Current = InstanceCharacters.FindRef(PartyMemberGuids[Index]);
		check(Current);

		if (bCanSwitchToDead || !(Current->CombatComponent && Current->CombatComponent->IsCharacterDead()))
		{
			SwitchPlayerCharacterByIndex(Index, DurationOverridden);
			return;
		}

		Index += GetNextIndex(SwitchPolicy);
	}
}

void UCharacterSubsystem::SwitchPlayerCharacterByIndex(const int32 Index, const float DurationOverridden)
{
	if (!PartyMemberGuids.IsValidIndex(Index))
	{
		return;
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		ABaseCharacter* NewPlayerCharacter = InstanceCharacters.FindRef(PartyMemberGuids[Index]);

		if (NewPlayerCharacter)
		{
			if (GetPlayerCharacter() == NewPlayerCharacter)
			{
				return;
			}

			// We do not want character to move when switching
			if (UCharacterControlComponent* ControlComponent = PC->FindComponentByClass<UCharacterControlComponent>())
			{
				ControlComponent->DisableAllInputs();
			}

			if (ARPGPlayerCameraManager* CamMgr = Cast<ARPGPlayerCameraManager>(PC->PlayerCameraManager))
			{
				CamMgr->ActorCameraViewTransition(
					NewPlayerCharacter,
					DurationOverridden >= 0.0f ? DurationOverridden : SwitchCharacterDuration,
					nullptr,
					[this, Index, NewPlayerCharacter](const FVector& Position, const FRotator& Rotation) {
						PlayerPartyIndex = Index;
						UpdatePossessedCharacter(NewPlayerCharacter, Position, Rotation);
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

void UCharacterSubsystem::UpdatePossessedCharacter(ABaseCharacter* NewCharacter, const FVector& Position, const FRotator& Rotation)
{
	if (!NewCharacter)
	{
		return;
	}

	if (ARPGPlayerController* PC = Cast<ARPGPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		// A camera transition can theoretically complete after another possession
		// path has already selected this pawn. Avoid a redundant direct Possess;
		// ARPGPlayerController::PossessCharacter also protects this invariant for
		// the party-ready path.
		if (PC->GetPawn() != NewCharacter)
		{
			PC->PossessCharacter(NewCharacter);
		}

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
	// GameInstanceSubsystem survives map changes and repeated loads. Clear the
	// previous runtime layer before accepting the new saved data.
	const uint32 ThisLoadRequest = LoadRequestSerial;
	TWeakObjectPtr<UCharacterSubsystem> WeakThis(this);

	if (const FCharacterSaveModule* CharacterSave = SaveData.GetPtr<FCharacterSaveModule>())
	{
		CharacterDataMap = CharacterSave->CharacterData;
		PlayerPartyIndex = CharacterSave->PlayerPartyIndex;
		PartyMemberIds = CharacterSave->PartyMembers;

		OnLoadComplete().Broadcast();

		return;
	}

	const UCharacterSystemSettings* Settings = GetDefault<UCharacterSystemSettings>();
	if (!Settings)
	{
		OnLoadComplete().Broadcast();
		return;
	}

	PartyMemberIds = Settings->DefaultPartyMembers;
	PlayerPartyIndex = Settings->DefaultPlayerIndex;
	const TArray<FRPGId> DefaultParty = Settings->DefaultPartyMembers;
	
	URPGAssetLibrary::LoadAssetArrayByRPGIdsAsync(Settings->PlayableCharacters, {},
		[WeakThis, ThisLoadRequest, DefaultParty](TArray<URPGPrimaryAsset*> Result)
		{
			UCharacterSubsystem* StrongThis = WeakThis.Get();
			if (!StrongThis || StrongThis->LoadRequestSerial != ThisLoadRequest)
			{
				return;
			}

			for (URPGPrimaryAsset* Asset : Result)
			{
				if (UCharacterAsset* CharacterAsset = Cast<UCharacterAsset>(Asset))
				{
					StrongThis->CharacterDataMap.Add(Asset->GetId(), CharacterAsset->GetDefaultData());
				}
			}

			StrongThis->OnLoadComplete().Broadcast();
		});
}

FSimpleMulticastDelegate& UCharacterSubsystem::OnLoadComplete()
{
	return LoadCompleteDelegate;
}

