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
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

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

	// Learn default abilities
	for (const FAbilityData& AbilityData : DefaultAbilities)
	{
		AbilitySystemComponent->LearnAbility(AbilityData);
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
	if (!AbilitySystemComponent || !CharacterAsset)
	{
		return;
	}

	UChooserTable* AnimationTable = CharacterAsset->GetAnimationTable().Get();
	if (!AnimationTable)
	{
		UE_LOG(LogTemp, Display, TEXT("[ABaseCharacter::PreLoadAnimData] No animation table found for character %s"), *GetName());

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
		if (FGameplayTagColumn* Column = Struct.GetMutablePtr<FGameplayTagColumn>())
		{
			const FChooserParameterGameplayTagBase* Param = Column->InputValue.GetPtr<FChooserParameterGameplayTagBase>();
			if (!Param)
			{
				continue;
			}
#if WITH_EDITORONLY_DATA
			const TArray<FInstancedStruct>* ResultsStructsPtr = AnimationTable->IsCookedData() ? &AnimationTable->CookedResults : &AnimationTable->ResultsStructs;
#else // WITH_EDITORONLY_DATA
			const TArray<FInstancedStruct>* ResultsStructsPtr = &AnimationTable->CookedResults;
#endif // WITH_EDITORONLY_DATA
			if (!ResultsStructsPtr)
			{
				UE_LOG(LogTemp, Display, TEXT("[ABaseCharacter::PreLoadAnimData] No results found in animation table for character %s"), *GetName());
				return;
			}
			
			const TArray<FInstancedStruct>& ResultsStructs = *ResultsStructsPtr;
			UE_LOG(LogTemp, Display, TEXT("Found %d rows in animation table for character %s"), ResultsStructs.Num(), *GetName());

			if (!PreloadedAnimHandles.IsEmpty())
			{
				UE_LOG(LogTemp, Display, TEXT("[ABaseCharacter::PreLoadAnimData] Canceling %d previous preload handles for character %s"), PreloadedAnimHandles.Num(), *GetName());

				// Cancel previous handles before starting new preload to avoid unnecessary memory usage
				for (const TSharedPtr<FStreamableHandle>& Handle : PreloadedAnimHandles)
				{
					if (Handle.IsValid())
					{
						Handle->CancelHandle();
					}
				}

				PreloadedAnimHandles.Empty();
				PreloadedAnimDataCount = 0;
			}

			bool bHasMatchingTag = false;
			
			for (int32 i = 0; i < Column->RowValues.Num(); i++)
			{
				if (AbilityTagsSet.Contains(Column->RowValues[i]) && ResultsStructs.IsValidIndex(i))
				{
					UE_LOG(LogTemp, Display, TEXT("Container: %s"), *Column->RowValues[i].ToString());
					RecursivePreloadAnimData(ResultsStructs[i], PreloadedAnimHandles, Callback);

					bHasMatchingTag = true;
				}
			}

			if (bHasMatchingTag)
			{
				return;
			}
		}
	}
}

void ABaseCharacter::RecursivePreloadAnimData(const FInstancedStruct& ResultsStruct, TArray<TSharedPtr<FStreamableHandle>>& OutHandles, TFunction<void()> Callback) const
{
	if (const FSoftAssetChooser* SoftAssetChooser = ResultsStruct.GetPtr<FSoftAssetChooser>())
	{
		if (SoftAssetChooser->Asset.IsNull())
		{
			return;
		}

		// Load asset
		TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
			SoftAssetChooser->Asset.ToSoftObjectPath(),
			FStreamableDelegate::CreateLambda([WeakThis = TWeakObjectPtr<const ABaseCharacter>(this), Callback]()
				{
					ABaseCharacter* StrongThis = const_cast<ABaseCharacter*>(WeakThis.Get());
					if (!StrongThis)
					{
						return;
					}

					StrongThis->PreloadedAnimDataCount--;

					if (StrongThis->PreloadedAnimDataCount <= 0)
					{
						UE_LOG(LogTemp, Display, TEXT("[ABaseCharacter::RecursivePreloadAnimData] Finished preloading animation data for character %s"), *StrongThis->GetName());
						StrongThis->PreloadedAnimHandles.Empty();

						Callback();
					}
				})
		);
		
		OutHandles.Add(Handle);

		PreloadedAnimDataCount++;
	}
	else if (const FNestedChooser* NestedChooser = ResultsStruct.GetPtr<FNestedChooser>())
	{
		UChooserTable* NestedTable = Cast<UChooserTable>(NestedChooser->Chooser);
		if (!NestedTable)
		{
			return;
		}

		const TArray<FInstancedStruct>* ResultsStructsPtr = nullptr;

#if WITH_EDITORONLY_DATA
		ResultsStructsPtr = NestedTable->IsCookedData() ? &NestedTable->CookedResults : &NestedTable->ResultsStructs;
#else // WITH_EDITORONLY_DATA
		ResultsStructsPtr = &NestedTable->CookedResults;
#endif // WITH_EDITORONLY_DATA

		if (!ResultsStructsPtr)
		{
			return;
		}

		const TArray<FInstancedStruct>& ResultsStructs = *ResultsStructsPtr;

		for (const FInstancedStruct& NestedResultStruct : ResultsStructs)
		{
			// Load nested chooser assets
			RecursivePreloadAnimData(NestedResultStruct, OutHandles, Callback);
		}
	}
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
	URPGAssetLibrary::GetAssetByRPGIdAsync(InId, { "Character", "UI" }, [&](auto&& Result)
		{
			this->InitCharacterData(Cast<UCharacterAsset>(Result), Data);
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
	URPGAssetLibrary::GetAssetByRPGIdAsync(InId, { "Character", "UI" }, [&](auto&& Result)
		{
			this->InitCharacterDataDefault(Cast<UCharacterAsset>(Result));
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
	// AbilitySystemComponent->UnlearnAllAbilities();
	// TODO: 只刪除角色的學習技能，保留預設技能，這樣就不會因為切換角色而丟失預設技能了

	// Learn saved abilities
	AbilitySystemComponent->LearnAbilities(Data.LearnedAbilities, [this, Data](const TArray<FAbilityData>& LearnedAbilities)
		{
			for (int32 i = 0; i < Data.EquippedAbilities.Num(); i++)
			{
				AbilitySystemComponent->EquipAbilityById(Data.EquippedAbilities[i], i);
			}
		});

	CharacterSaveData = Data;

	PreloadAnimData([WeakThis = TWeakObjectPtr<const ABaseCharacter>(this)]()
	{
		if (ABaseCharacter* StrongThis = const_cast<ABaseCharacter*>(WeakThis.Get()))
		{
			StrongThis->OnCharacterDataInitialized.Broadcast(StrongThis);
		}
	});
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
