// Copyright Ironic Studio. All Rights Reserved.


#include "Characters/BaseCharacter.h"
#include "Characters/Components/RPGCharacterMovementComponent.h"
#include "Characters/Components/CombatComponent.h"
#include "Characters/Components/CharacterAbilitySystemComponent.h"
#include "Characters/Attributes/RPGAttributeSet.h"
#include "Characters/Attributes/CombatAttributeSet.h"
#include "Characters/Attributes/LevelAttributeSet.h"

#include "Abilities/RPGGameplayAbility.h"

#include "Components/CapsuleComponent.h"

#include "Helpers/RPGHelperMacros.h"
#include "Assets/RPGAssetLibrary.h"

#include "AIController.h"
#include "NavigationInvokerComponent.h"

#include "Chooser.h"
#include "ChooserFunctionLibrary.h"
#include "ObjectChooser_Asset.h"
#include "GameplayTagColumn.h"

// Sets default values
ABaseCharacter::ABaseCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<URPGCharacterMovementComponent>(CharacterMovementComponentName))
{
 	// We don't need to call Tick every frame
	PrimaryActorTick.bCanEverTick = false;

	// We do not use network replication by default
	bReplicates = false;
	SetReplicateMovement(false);

	bAlwaysRelevant = false;
	bOnlyRelevantToOwner = false;
	bNetUseOwnerRelevancy = false;

	NetDormancy = DORM_Never;
	NetUpdateFrequency = 0.0f;
	MinNetUpdateFrequency = 0.0f;

	// Controller settings
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetRelativeRotation(FRotator(0.f, 270.f, 0.f));
	}

	// Create Combat Component
	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComp"));

	// Create Navigation Invoker Component
	NavigationInvoker = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavigationInvokerComp"));

	// Create AbilitySystemComponent
	AbilitySystemComponent = CreateDefaultSubobject<URPGAbilitySystemComponent>(TEXT("AbilitySystemComp"));

	AutoPossessAI = EAutoPossessAI::Disabled;

}

// Called when the game starts or when spawned
void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Initialize team tag
	ResetDefaultTeamTag();
}

void ABaseCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Do not allow an old animation preload request to complete after this
	// character has entered EndPlay or after a world travel has completed.
	CancelActiveAnimPreload();

	if (AIController)
	{
		if (UBehaviorTreeComponent* BTComponent = Cast<UBehaviorTreeComponent>(AIController->GetBrainComponent()))
		{
			BTComponent->StopTree(EBTStopMode::Safe);
		}

		AIController->UnPossess();
	}

	RPGFlow::NotifyObjectEndingPlay(this);

	Super::EndPlay(EndPlayReason);
}

void ABaseCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this); // OwnerActor, AvatarActor

	if (!HasAuthority())
	{
		return;
	}
	
	// Create Attribute Sets
	for (TSubclassOf<URPGAttributeSet> AttributeClass : AttributeSets)
	{
		URPGAttributeSet* Attributes = NewObject<URPGAttributeSet>(this, AttributeClass);
		AbilitySystemComponent->AddSpawnedAttribute(Attributes);
		Attributes->BindAttributeChangedDelegates(AbilitySystemComponent);

		if (UCombatAttributeSet* CombatAttributes = Cast<UCombatAttributeSet>(Attributes))
		{
			CombatAttributes->OnOutOfHealth.AddUObject(CombatComponent, &UCombatComponent::RequestDeath);
			CombatAttributes->OnRevive.AddUObject(CombatComponent, &UCombatComponent::RequestRevive);
			CombatAttributes->OnTakeDamage.AddUObject(CombatComponent, &UCombatComponent::RequestHitReaction);
			// CombatAttributes->OnHeal.AddUObject(CombatComponent, &UCombatComponent::HandleHeal);
		}
	}
}

void ABaseCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	
}

// Called every frame
void ABaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void ABaseCharacter::GetCharacterAnimEntry(FCharacterAnimInput& InputStruct, OUT FCharacterAnimEntry& OutEntry) const
{
	if (!CharacterAsset)
	{
		return;
	}

	UChooserTable* AnimationTable = CharacterAsset->GetAnimationTable().Get();
	if (!AnimationTable)
	{
		UE_LOG(LogTemp, Display, TEXT("[ABaseCharacter::GetCharacterAnimEntry] No animation table found for character %s"), *GetName());

		return;
	}

	const FInstancedStruct& ChooserInstance = UChooserFunctionLibrary::MakeEvaluateChooser(AnimationTable);
	if (!ChooserInstance.IsValid())
	{
		return;
	}

	FChooserEvaluationContext Ctx = UChooserFunctionLibrary::MakeChooserEvaluationContext();
	Ctx.AddStructParam(InputStruct);
	Ctx.AddStructParam(OutEntry);

	TSoftObjectPtr<UObject> ResultObj = UChooserFunctionLibrary::EvaluateObjectChooserBaseSoft(
		Ctx, ChooserInstance, UAnimMontage::StaticClass()
	);

	if (ResultObj.IsNull())
	{
		UE_LOG(LogTemp, Display, TEXT("[ABaseCharacter::GetCharacterAnimEntry] Failed to get montage from animation table for character %s"), *GetName());
		return;
	}

	// Return the found montage
	OutEntry.Montage = ResultObj;
}

void ABaseCharacter::PreloadAnimData(TFunction<void()> Callback)
{
	check(IsInGameThread());

	CancelActiveAnimPreload();

	const TSharedRef<FAnimPreloadRequestState, ESPMode::ThreadSafe> RequestState =
		MakeShared<FAnimPreloadRequestState, ESPMode::ThreadSafe>();

	RequestState->Generation = ++AnimPreloadGeneration;
	RequestState->CompletionCallback = MoveTemp(Callback);
	ActiveAnimPreloadRequest = RequestState;

	if (!AbilitySystemComponent || !CharacterAsset)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ABaseCharacter::PreloadAnimData] Missing ability system or character asset for %s; skipping animation preload. Generation=%d"),
			*GetName(), RequestState->Generation);

		RequestState->bEnumerationFinished = true;
		TryFinishAnimPreload(RequestState);
		return;
	}

	UChooserTable* AnimationTable = CharacterAsset->GetAnimationTable().Get();
	if (!AnimationTable)
	{
		UE_LOG(LogTemp, Display,
			TEXT("[ABaseCharacter::PreloadAnimData] No animation table found for character %s; skipping animation preload. Generation=%d"),
			*GetName(), RequestState->Generation);

		RequestState->bEnumerationFinished = true;
		TryFinishAnimPreload(RequestState);
		return;
	}

	TArray<FGameplayTagContainer> AbilityTagsSet;
	for (const auto& AbilityDataPair : AbilitySystemComponent->GetLearnedAbilities())
	{
		if (const UAbilityAsset* AbilityData = AbilityDataPair.Value.AbilityAsset)
		{
			for (const FGameplayTagContainer& Container : AbilityData->GetRequireAnimTagContainers())
			{
				AbilityTagsSet.AddUnique(Container);
			}
		}
	}

	for (FInstancedStruct& Struct : AnimationTable->ColumnsStructs)
	{
		FGameplayTagColumn* Column = Struct.GetMutablePtr<FGameplayTagColumn>();
		if (!Column)
		{
			continue;
		}

		const FChooserParameterGameplayTagBase* Param =
			Column->InputValue.GetPtr<FChooserParameterGameplayTagBase>();
		if (!Param)
		{
			continue;
		}

#if WITH_EDITORONLY_DATA
		const TArray<FInstancedStruct>* ResultsStructsPtr =
			AnimationTable->IsCookedData() ? &AnimationTable->CookedResults : &AnimationTable->ResultsStructs;
#else
		const TArray<FInstancedStruct>* ResultsStructsPtr = &AnimationTable->CookedResults;
#endif

		if (!ResultsStructsPtr)
		{
			UE_LOG(LogTemp, Display,
				TEXT("[ABaseCharacter::PreloadAnimData] No results found for character %s; skipping animation preload. Generation=%d"),
				*GetName(), RequestState->Generation);

			RequestState->bEnumerationFinished = true;
			TryFinishAnimPreload(RequestState);
			return;
		}

		const TArray<FInstancedStruct>& ResultsStructs = *ResultsStructsPtr;
		UE_LOG(LogTemp, Display,
			TEXT("[ABaseCharacter::PreloadAnimData] Found %d rows for character %s. Generation=%d"),
			ResultsStructs.Num(), *GetName(), RequestState->Generation);
		
		bool bHasMatchingTag = false;
		for (int32 Index = 0; Index < Column->RowValues.Num(); ++Index)
		{
			if (!AbilityTagsSet.Contains(Column->RowValues[Index]) || !ResultsStructs.IsValidIndex(Index))
			{
				continue;
			}

			UE_LOG(LogTemp, Display,
				TEXT("[ABaseCharacter::PreloadAnimData] Container=%s Generation=%d"),
				*Column->RowValues[Index].ToString(), RequestState->Generation);

			RecursivePreloadAnimData(ResultsStructs[Index], RequestState);
			bHasMatchingTag = true;
		}

		RequestState->bEnumerationFinished = true;
		TryFinishAnimPreload(RequestState);

		if (bHasMatchingTag)
		{
			return;
		}
	}

	RequestState->bEnumerationFinished = true;
	TryFinishAnimPreload(RequestState);
}

void ABaseCharacter::RecursivePreloadAnimData(
	const FInstancedStruct& ResultsStruct,
	const TSharedRef<FAnimPreloadRequestState, ESPMode::ThreadSafe>& RequestState)
{
	check(IsInGameThread());

	if (RequestState->bCanceled || RequestState->bFinished)
	{
		return;
	}

	if (const FSoftAssetChooser* SoftAssetChooser = ResultsStruct.GetPtr<FSoftAssetChooser>())
	{
		if (SoftAssetChooser->Asset.IsNull())
		{
			return;
		}

		++RequestState->PendingCount;

		const TWeakObjectPtr<ABaseCharacter> WeakThis(this);
		const TWeakPtr<FAnimPreloadRequestState, ESPMode::ThreadSafe> WeakRequest = RequestState;

		TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
			SoftAssetChooser->Asset.ToSoftObjectPath(),
			FStreamableDelegate::CreateLambda([WeakThis, WeakRequest]()
			{
				ABaseCharacter* StrongThis = WeakThis.Get();
				const TSharedPtr<FAnimPreloadRequestState, ESPMode::ThreadSafe> StrongRequest = WeakRequest.Pin();
				if (!StrongThis || !StrongRequest)
				{
					return;
				}

				StrongRequest->PendingCount = FMath::Max(0, StrongRequest->PendingCount - 1);

				if (StrongRequest->bCanceled || StrongRequest->bFinished)
				{
					return;
				}

				StrongThis->TryFinishAnimPreload(StrongRequest.ToSharedRef());
			}));

		if (Handle.IsValid())
		{
			RequestState->Handles.Add(Handle);
		}
		else
		{
			RequestState->PendingCount = FMath::Max(0, RequestState->PendingCount - 1);
			UE_LOG(LogTemp, Warning,
				TEXT("[ABaseCharacter::RecursivePreloadAnimData] Failed to create preload handle for character %s. Generation=%d"),
				*GetName(), RequestState->Generation);
		}

		return;
	}

	if (const FNestedChooser* NestedChooser = ResultsStruct.GetPtr<FNestedChooser>())
	{
		UChooserTable* NestedTable = Cast<UChooserTable>(NestedChooser->Chooser);
		if (!NestedTable)
		{
			return;
		}

		const TArray<FInstancedStruct>* ResultsStructsPtr = nullptr;
#if WITH_EDITORONLY_DATA
		ResultsStructsPtr = NestedTable->IsCookedData() ? &NestedTable->CookedResults : &NestedTable->ResultsStructs;
#else
		ResultsStructsPtr = &NestedTable->CookedResults;
#endif

		if (!ResultsStructsPtr)
		{
			return;
		}

		for (const FInstancedStruct& NestedResultStruct : *ResultsStructsPtr)
		{
			RecursivePreloadAnimData(NestedResultStruct, RequestState);
		}
	}
}

void ABaseCharacter::TryFinishAnimPreload(
	const TSharedRef<FAnimPreloadRequestState, ESPMode::ThreadSafe>& RequestState)
{
	check(IsInGameThread());

	if (RequestState->bCanceled || RequestState->bFinished ||
		!RequestState->bEnumerationFinished || RequestState->PendingCount > 0)
	{
		return;
	}

	if (!ActiveAnimPreloadRequest.IsValid() ||
		ActiveAnimPreloadRequest.Get() != &RequestState.Get())
	{
		return;
	}

	RequestState->bFinished = true;
	RequestState->Handles.Empty();

	UE_LOG(LogTemp, Display,
		TEXT("[ABaseCharacter::PreloadAnimData] Finished animation preload for character %s. Generation=%d"),
		*GetName(), RequestState->Generation);

	TFunction<void()> CompletionCallback = MoveTemp(RequestState->CompletionCallback);
	ActiveAnimPreloadRequest.Reset();

	if (CompletionCallback)
	{
		CompletionCallback();
	}
}

void ABaseCharacter::CancelActiveAnimPreload()
{
	check(IsInGameThread());

	if (!ActiveAnimPreloadRequest.IsValid())
	{
		return;
	}

	TSharedPtr<FAnimPreloadRequestState, ESPMode::ThreadSafe> PreviousRequest = MoveTemp(ActiveAnimPreloadRequest);
	PreviousRequest->bCanceled = true;
	PreviousRequest->CompletionCallback = nullptr;

	UE_LOG(LogTemp, Display,
		TEXT("[ABaseCharacter::PreloadAnimData] Canceling generation %d with %d handles for character %s"),
		PreviousRequest->Generation, PreviousRequest->Handles.Num(), *GetName());

	for (const TSharedPtr<FStreamableHandle>& Handle : PreviousRequest->Handles)
	{
		if (Handle.IsValid())
		{
			Handle->CancelHandle();
		}
	}

	PreviousRequest->Handles.Empty();
}

void ABaseCharacter::SetAIControl(bool bEnable)
{
	if (bEnable)
	{
		if (!AIController)
		{
			SpawnDefaultController();
			AIController = Cast<AAIController>(GetController());
		}
		else if (!AIController->GetPawn())
		{
			AIController->Possess(this);
		}
	}
	else
	{
		if (AIController && AIController->GetPawn() == this)
		{
			AIController->UnPossess();
		}
	}
}

bool ABaseCharacter::IsAIControlled() const
{
	return Controller ? Controller->IsA(AAIController::StaticClass()) : false;
}

void ABaseCharacter::InitCharacterData(const UCharacterAsset* Asset, const FCharacterSaveData& Data)
{
	if (!Asset)
	{
		return;
	}

	CharacterAsset = Asset;

	GetCapsuleComponent()->SetCapsuleHalfHeight(Asset->GetDefaultCapsuleHalfHeight());
	GetCapsuleComponent()->SetCapsuleRadius(Asset->GetDefaultCapsuleRadius());
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -Asset->GetDefaultCapsuleHalfHeight()));

	// Check Behavior Tree
	if (!Asset->GetBehaviorTree().IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ABaseCharacter::InitCharacterData] No behavior tree found for character %s"), *GetName());
	}

	// Check Animation Table
	if (!Asset->GetAnimationTable().IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ABaseCharacter::InitCharacterData] No animation table found for character %s"), *GetName());
	}

	/*if ()*/
	{
		SpawnDefaultController();
	}

	// If the data is from save file, use it directly; otherwise, use the default level attributes from the asset
	if (Data.bIsSaveData)
	{
		SetCharacterData(Data);
	}
	else
	{
		FCharacterSaveData NewData = Asset->GetDefaultData();
		NewData.bIsSaveData = true; // Mark as save data

		SetCharacterData(NewData); // Set default data first

		// Then initialize attributes with growth curve
		if (UCurveTable* GrowthTable = Asset->GetGrowthTable().Get())
		{
			InitAttributeWithGrowthCurve(GrowthTable);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ABaseCharacter::InitCharacterData] No growth table found for character %s"), *GetName());
		}
	}
}

void ABaseCharacter::InitCharacterDataById(const FRPGId& InId, const FCharacterSaveData& Data)
{
	const FCharacterSaveData DataCopy = Data;
	TWeakObjectPtr<ABaseCharacter> WeakThis(this);

	URPGAssetLibrary::LoadAssetByRPGIdAsync(InId, { "Character", "UI" }, [WeakThis, DataCopy](URPGPrimaryAsset* Result)
		{
			if (ABaseCharacter* Character = WeakThis.Get())
			{
				Character->InitCharacterData(Cast<UCharacterAsset>(Result), DataCopy);
			}
		});
}

void ABaseCharacter::InitCharacterDataDefault(const UCharacterAsset* Asset)
{
	if (!Asset)
	{
		return;
	}

	InitCharacterData(Asset, Asset->GetDefaultData());
}

void ABaseCharacter::InitCharacterDataDefaultById(const FRPGId& InId)
{
	TWeakObjectPtr<ABaseCharacter> WeakThis(this);

	URPGAssetLibrary::LoadAssetByRPGIdAsync(InId, { "Character", "UI" }, [WeakThis](URPGPrimaryAsset* Result)
		{
			if (ABaseCharacter* Character = WeakThis.Get())
			{
				Character->InitCharacterDataDefault(Cast<UCharacterAsset>(Result));
			}
		});
}

void ABaseCharacter::InitAttributeWithGrowthCurve(const UCurveTable* CurveTable)
{
	if (!CurveTable)
	{
		return;
	}

	UGameplayEffect* InitGE = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("InitAttributesGE"));
	InitGE->DurationPolicy = EGameplayEffectDurationType::Instant;

	for (const auto& Pair : CharacterSaveData.Attributes)
	{ 
		if (!CurveTable->FindCurve(FName(Pair.Key.AttributeName), ""))
		{
			continue;
		}

		FScalableFloat ScalableFloat(Pair.Value);
		ScalableFloat.Curve.CurveTable = CurveTable;
		ScalableFloat.Curve.RowName = FName(Pair.Key.AttributeName);

		FGameplayModifierInfo Mod;
		Mod.Attribute = Pair.Key;
		Mod.ModifierOp = EGameplayModOp::Override;

		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(ScalableFloat);
		InitGE->Modifiers.Add(Mod); 
	}

	float FinalLevel = 1.0f;
	if (const float* FoundLevel = CharacterSaveData.Attributes.Find(ULevelAttributeSet::GetLevelAttribute()))
	{
		FinalLevel = *FoundLevel;
	}

	AbilitySystemComponent->ApplyGameplayEffectToSelf(InitGE, FinalLevel, AbilitySystemComponent->MakeEffectContext());
}

void ABaseCharacter::SetCharacterData(const FCharacterSaveData& Data)
{
	if (Data.CharacterMesh)
	{
		GetMesh()->SetSkeletalMesh(Data.CharacterMesh);
	}

	for (UAttributeSet* Set : AbilitySystemComponent->GetSpawnedAttributes())
	{
		if (URPGAttributeSet* RPGSet = Cast<URPGAttributeSet>(Set))
		{
			RPGSet->LoadAttributesFrom(Data);
		}
	}

	// Unlearn all current abilities
	AbilitySystemComponent->UnlearnAllAbilities();

	// Learn default and saved abilities
	TArray<FAbilityData> AbilitiesToLearn;
	AbilitiesToLearn.Append(DefaultAbilities);
	AbilitiesToLearn.Append(Data.LearnedAbilities);
	
	AbilitySystemComponent->LearnAbilities(AbilitiesToLearn, [this, Data](const TArray<FAbilityData>& LearnedAbilities)
		{
			for (TPair<FGameplayTag, FRPGId> Pair : Data.EquippedAbilities)
			{
				if (!Pair.Key.IsValid() || !Pair.Value.IsValid())
				{
					continue;
				}

				AbilitySystemComponent->EquipAbilityById(Pair.Value, Pair.Key);
			}

			PreloadAnimData([WeakThis = TWeakObjectPtr<const ABaseCharacter>(this)]()
				{
					if (ABaseCharacter* StrongThis = const_cast<ABaseCharacter*>(WeakThis.Get()))
					{
						StrongThis->OnCharacterDataInitialized.Broadcast(StrongThis);
					}
				});
		});

	CharacterSaveData = Data;
}

const FCharacterSaveData& ABaseCharacter::GetCharacterData() const
{
	for (UAttributeSet* Set : AbilitySystemComponent->GetSpawnedAttributes())
	{
		if (URPGAttributeSet* RPGSet = Cast<URPGAttributeSet>(Set))
		{
			RPGSet->SaveAttributesTo(CharacterSaveData);
		}
	}

	CharacterSaveData.LearnedAbilities.Empty();

	for (const auto& AbilityDataPair : AbilitySystemComponent->GetLearnedAbilities())
	{
		const FAbilityData& AbilityData = AbilityDataPair.Value;

		// Only save learned abilities, skip default abilities
		if (AbilityData.AbilityGrantType == EAbilitySavePolicy::NonSavable || AbilityData.AbilityGrantType == EAbilitySavePolicy::Default)
		{
			continue;
		}

		CharacterSaveData.LearnedAbilities.Add(AbilityData);
	}

	return CharacterSaveData;
}

#if WITH_EDITOR
void ABaseCharacter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName& PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABaseCharacter, DefaultAbilities))
	{
		for (FAbilityData& AbilityData : DefaultAbilities)
		{
			AbilityData.AbilityGrantType = EAbilitySavePolicy::NonSavable;

#if WITH_EDITORONLY_DATA
			AbilityData.bSavePolicyLock = true;
#endif // WITH_EDITORONLY_DATA
		}
	}
}
#endif // WITH_EDITOR
