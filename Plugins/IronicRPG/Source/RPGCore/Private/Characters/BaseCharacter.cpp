// Copyright Ironic Studio. All Rights Reserved.


#include "Characters/BaseCharacter.h"
#include "Characters/CharacterAsset.h"
#include "Characters/Components/RPGCharacterMovementComponent.h"
#include "Characters/Components/CharacterAbilitySystemComponent.h"
#include "Characters/Attributes/RPGAttributeSet.h"
#include "Characters/Attributes/LevelAttributeSet.h"

#include "Components/CapsuleComponent.h"

#include "Helpers/RPGHelperMacros.h"
#include "Assets/RPGAssetLibrary.h"

#include "AIController.h"
#include "NavigationInvokerComponent.h"

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

	
}

void ABaseCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this); // OwnerActor, AvatarActor

		// Create Attribute Sets
		for (TSubclassOf<URPGAttributeSet> AttributeClass : AttributeSets)
		{
			UAttributeSet* Attributes = NewObject<UAttributeSet>(this, AttributeClass);
			AbilitySystemComponent->AddSpawnedAttribute(Attributes);
		}
	}
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

	Id = Asset->GetId();

	GetCapsuleComponent()->SetCapsuleHalfHeight(Asset->GetDefaultCapsuleHalfHeight());
	GetCapsuleComponent()->SetCapsuleRadius(Asset->GetDefaultCapsuleRadius());
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -Asset->GetDefaultCapsuleHalfHeight()));

	BehaviorTree = Asset->GetBehaviorTree().LoadSynchronous();

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

		InitAttributeWithGrowthCurve(Asset->GetGrowthTable().LoadSynchronous()); // Then initialize attributes with growth curve
	}
}

void ABaseCharacter::InitCharacterDataById(const FRPGId& InId, const FCharacterSaveData& Data)
{
	URPGAssetLibrary::GetAssetByRPGIdAsync(InId, {}, [&](auto&& Result) {
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
	URPGAssetLibrary::GetAssetByRPGIdAsync(InId, {}, [&](auto&& Result) {
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

	CharacterSaveData = Data;
}

FCharacterSaveData ABaseCharacter::GetCharacterData()
{
	for (UAttributeSet* Set : AbilitySystemComponent->GetSpawnedAttributes())
	{
		if (URPGAttributeSet* RPGSet = Cast<URPGAttributeSet>(Set))
		{
			RPGSet->SaveAttributesTo(CharacterSaveData);
		}
	}

	return CharacterSaveData;
}
