// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CharacterAsset.h"
#include "DataTypes/RPGId.h"
#include "Characters/CharacterDataTypes.h"
#include "Abilities/AbilityDataTypes.h"

#include "Misc/RPGTeamAgentInterface.h"

#include "BaseCharacter.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCharacterDataInitialized, ABaseCharacter* /*Character*/);

class UInputComponent;

class UCombatComponent;

class URPGAbilitySystemComponent;
class URPGAttributeSet;

class AAIController;
class UNavigationInvokerComponent;
class UBehaviorTree;

UCLASS()
class RPGCORE_API ABaseCharacter : public ACharacter, public IRPGTeamAgentInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABaseCharacter(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void PostInitializeComponents() override;

	virtual void PossessedBy(AController* NewController) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

public: // Character Data
	UFUNCTION(BlueprintCallable)
	const UCharacterAsset* GetCharacterAsset() const { return CharacterAsset; }

	UFUNCTION(BlueprintCallable)
	FRPGId GetId() const { return CharacterAsset ? CharacterAsset->GetId() : FRPGId(); }

	UFUNCTION(BlueprintCallable)
	FGuid GetInstanceId() const { return InstanceId; }

	FOnCharacterDataInitialized OnCharacterDataInitialized;

protected: // Character Data
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Character Data")
	TObjectPtr<const UCharacterAsset> CharacterAsset = nullptr;

	// The unique instance ID for this character
	UPROPERTY(BlueprintReadOnly, Category = "Character Data")
	FGuid InstanceId = FGuid::NewGuid();

	// The save data of this character, but it is not "Authority Data", so we recommend that you only use it for loading and saving
	UPROPERTY(BlueprintReadOnly, Category = "Character Data")
	mutable FCharacterSaveData CharacterSaveData;

public: // Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCombatComponent> CombatComponent = nullptr;

public: // Animation
	virtual void PreloadAnimData(TFunction<void()> Callback);

	virtual void GetCharacterAnimEntry(FCharacterAnimInput& InputStruct, FCharacterAnimEntry& OutEntry) const;

protected: // Animation
	void RecursivePreloadAnimData(const FInstancedStruct& ResultsStruct, TArray<TSharedPtr<FStreamableHandle>>& OutHandles, TFunction<void()> Callback) const;

	TArray<TSharedPtr<FStreamableHandle>> PreloadedAnimHandles;

	mutable int32 PreloadedAnimDataCount = 0;

public: // AI Control
	UFUNCTION(BlueprintCallable, Category = "AI|Control")
	virtual void SetAIControl(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "AI|Control")
	bool IsAIControlled() const;

	UFUNCTION(BlueprintCallable, Category = "AI|Control")
	AAIController* GetAIController() const { return AIController; }

	UFUNCTION(BlueprintCallable, Category = "AI|Control")
	AAIController* GetAIControllerSafe() const { return IsAIControlled() ? AIController : nullptr; }

	UFUNCTION(BlueprintCallable, Category = "AI|Control")
	UBehaviorTree* GetBehaviorTree() const { return CharacterAsset ? CharacterAsset->GetBehaviorTree().Get() : nullptr; }

private: // AI Control
	// AI Controller for this character, but use Getter to access it instead of this directly.
	UPROPERTY()
	TObjectPtr<AAIController> AIController = nullptr;

public: // Team
	UFUNCTION(BlueprintCallable, Category = "Team")
	virtual const FGameplayTag& GetDefaultTeamTag() const override { return DefaultTeamTag; }
	
private: // Team
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team", meta=(AllowPrivateAccess = "true", Categories = "Team"))
	FGameplayTag DefaultTeamTag;

public: // Navigation
	UPROPERTY(BlueprintReadOnly, Category = "Navigation")
	TObjectPtr<UNavigationInvokerComponent> NavigationInvoker = nullptr;

public: // Gameplay Ability System
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability System")
	TObjectPtr<URPGAbilitySystemComponent> AbilitySystemComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes")
	TArray<TSubclassOf<URPGAttributeSet>> AttributeSets;

	UFUNCTION(BlueprintCallable, Category = "Ability System")
	const TArray<FAbilityData>& GetDefaultAbilities() const { return DefaultAbilities; }

	UFUNCTION(BlueprintCallable, Category = "Ability System")
	FRPGId GetJumpAbilityId() const { return JumpAbilityId; }

protected: // Gameplay Ability System
	// The default abilities to give to this character, these abilities will not be saved.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability System")
	TArray<FAbilityData> DefaultAbilities;

	// The Id of the jump ability, used for input binding, this ability will not be saved.
	// Remember to set this Id in DefaultAbilities, otherwise this Id won't learn by this character.
	UPROPERTY(EditDefaultsOnly, Category = "Ability System", meta=(IdType = "Ability"))
	FRPGId JumpAbilityId;

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
	virtual const FCharacterSaveData& GetCharacterData() const;

#if WITH_EDITOR
protected:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

};
