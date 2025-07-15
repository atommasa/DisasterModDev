// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/BaseCharacter.h"
#include "Characters/CharacterPrimaryAsset.h"
#include "Characters/Components/RPGCharacterMovementComponent.h"
#include "Characters/Components/CharacterAbilitySystemComponent.h"
#include "Characters/Attributes/CharacterAttributeSet.h"

#include "AIController.h"
#include "NavigationInvokerComponent.h"

// Sets default values
ABaseCharacter::ABaseCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<URPGCharacterMovementComponent>(CharacterMovementComponentName))
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create Navigation Invoker Component
	NavigationInvoker = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavigationInvokerComp"));

	// Create Abilities
	Abilities = CreateDefaultSubobject<UCharacterAbilitySystemComponent>(TEXT("AbilitySystemComp"));

	// Create Attribute Set
	AttributeSet = CreateDefaultSubobject<UCharacterAttributeSet>(TEXT("AttributeSet"));

	AutoPossessAI = EAutoPossessAI::Disabled;
}

// Called when the game starts or when spawned
void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (Abilities)
	{
		Abilities->InitAbilityActorInfo(this, this); // OwnerActor, AvatarActor
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

void ABaseCharacter::InitDefaultData(UCharacterPrimaryAsset* Asset)
{
	if (!Abilities || !Asset)
	{
		return;
	}

	if (Asset->BehaviorTree.IsValid())
	{
		BehaviorTree = Asset->BehaviorTree.Get();
	}
	else
	{
		BehaviorTree = Asset->BehaviorTree.LoadSynchronous();
		if (!BehaviorTree)
		{
			UE_LOG(LogTemp, Warning, TEXT("BehaviorTree is not set in CharacterDefaultData for %s"), *GetName());
		}
	}

	/*if ()*/
	{
		SpawnDefaultController();
	}

	TSubclassOf<UGameplayEffect> AttributeEffectClass = Asset->AttributeEffectClass.LoadSynchronous();
	int32 DefaultLevel = Asset->DefaultLevel;
	if (!AttributeEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("AttributeEffectClass is not set in CharacterDefaultData for %s"), *GetName());
		return;
	}

	FGameplayEffectContextHandle EffectContext = Abilities->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	UGameplayEffect* GameplayEffect = AttributeEffectClass->GetDefaultObject<UGameplayEffect>();

	Abilities->ApplyGameplayEffectToSelf(GameplayEffect, DefaultLevel, EffectContext);

	SetCharacterData(Asset->DefaultData);
}

void ABaseCharacter::SetCharacterData(const FCharacterInstanceData& Data)
{
	if (auto* CharacterMesh = Data.CharacterMesh.LoadSynchronous())
	{
		GetMesh()->SetSkeletalMesh(CharacterMesh);
	}

	AttributeSet->LoadAttributesFrom(Data);

	CharacterData = Data;
}
