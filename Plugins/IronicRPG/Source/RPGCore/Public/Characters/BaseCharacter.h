// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DataTypes/RPGId.h"
#include "Characters/CharacterDataTypes.h"
#include "BaseCharacter.generated.h"

class UCharacterAsset;

class UInputComponent;

class URPGAbilitySystemComponent;
class URPGAttributeSet;

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

	virtual void PostInitializeComponents() override;

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

	// The save data of this character, but it is not "Authority Data", so we recommend that you only use it for loading and saving
	UPROPERTY(BlueprintReadOnly, Category = "Character Data")
	FCharacterSaveData CharacterSaveData;

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability System")
	URPGAbilitySystemComponent* AbilitySystemComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes")
	TArray<TSubclassOf<URPGAttributeSet>> AttributeSets;

public:
	UFUNCTION(BlueprintCallable, Category = "Character Data")
	virtual void InitCharacterData(const UCharacterAsset* Asset, const FCharacterSaveData& Data);

	UFUNCTION(BlueprintCallable, Category = "Character Data")
	void InitCharacterDataById(const FRPGId& InId, const FCharacterSaveData& Data);

	UFUNCTION(BlueprintCallable, Category = "Character Data")
	void InitCharacterDataDefault(const UCharacterAsset* Asset);

	UFUNCTION(BlueprintCallable, Category = "Character Data")
	void InitCharacterDataDefaultById(const FRPGId& InId);

	UFUNCTION(BlueprintCallable, Category = "Character Data")
	virtual void InitAttributeWithGrowthCurve(const UCurveTable* CurveTable);

	// Set the character data for this character
	virtual void SetCharacterData(const FCharacterSaveData& Data);

	// Get the character data for this character
	virtual FCharacterSaveData GetCharacterData();

};
