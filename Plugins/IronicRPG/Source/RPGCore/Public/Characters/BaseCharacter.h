// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DataTypes/RPGId.h"
#include "Characters/CharacterDataTypes.h"
#include "BaseCharacter.generated.h"

class UCharacterPrimaryAsset;

class UInputComponent;

class UCharacterAbilitySystemComponent;
class UCharacterAttributeSet;

class AAIController;
class UNavigationInvokerComponent;
class UBehaviorTree;

UCLASS()
class RPGCORE_API ABaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABaseCharacter(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

public: // Character Data
	UFUNCTION(BlueprintCallable)
	FRPGId GetId() const { return Id; }

protected: // Character Data
	UPROPERTY(BlueprintReadOnly, Category = "Character Data")
	FRPGId Id;

	UPROPERTY(BlueprintReadOnly, Category = "Character Data")
	FCharacterInstanceData CharacterData;

public: // AI Control
	UFUNCTION(BlueprintCallable, Category = "AI Control")
	virtual void SetAIControl(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "AI Control")
	bool IsAIControlled() const;

	UFUNCTION(BlueprintCallable, Category = "AI Control")
	AAIController* GetAIController() const { return AIController; }

	UFUNCTION(BlueprintCallable, Category = "AI Control")
	AAIController* GetAIControllerSafe() const { return IsAIControlled() ? AIController : nullptr; }

	UFUNCTION(BlueprintCallable, Category = "AI Control")
	UBehaviorTree* GetBehaviorTree() const { return BehaviorTree; }

private: // AI Control
	// AI Controller for this character, but use Getter to access it instead of this directly.
	UPROPERTY()
	TObjectPtr<AAIController> AIController;

	UPROPERTY()
	TObjectPtr<UBehaviorTree> BehaviorTree;

public: // Navigation
	UPROPERTY(BlueprintReadOnly, Category = "Navigation")
	TObjectPtr<UNavigationInvokerComponent> NavigationInvoker;

public: // Gameplay Ability System
	UPROPERTY(BlueprintReadOnly, Category = "Ability System")
	UCharacterAbilitySystemComponent* Abilities;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	UCharacterAttributeSet* AttributeSet;

public:
	void InitDefaultData(UCharacterPrimaryAsset* Asset);
	void SetCharacterData(const FCharacterInstanceData& Data);

};
