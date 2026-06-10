// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NativeGameplayTags.h"
#include "CombatComponent.generated.h"

class UAbilitySystemComponent;

struct FAttributeEventContext;

UCLASS( meta=(BlueprintSpawnableComponent) )
class RPGCORE_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public: // Death handling
    UFUNCTION(BlueprintCallable, Category = "Death")
    void RequestDeath(const FAttributeEventContext& DeathEventContext);

    UFUNCTION(BlueprintCallable, Category = "Death")
    void FinishDeath();

    UFUNCTION(BlueprintPure, Category = "Death")
    bool IsCharacterDead() const;

	UFUNCTION(BlueprintPure, Category = "Death")
    bool IsCharacterStateDying() const;

    UFUNCTION(BlueprintPure, Category = "Death")
    bool IsCharacterStateDead() const;

protected: // Death handling
    UPROPERTY(EditDefaultsOnly, Category = "Death|Tags")
    FGameplayTag DeadTag;

    UPROPERTY(EditDefaultsOnly, Category = "Death|Tags")
    FGameplayTag DyingTag;

    UPROPERTY(EditDefaultsOnly, Category = "Death|Events")
    FGameplayTag DeathEventTag;

private: // Death handling
    bool bDeathRequested = false;

public: // Revive handling
	UFUNCTION(BlueprintCallable, Category = "Revive")
    void RequestRevive(const FAttributeEventContext& ReviveEventContext);

    UFUNCTION(BlueprintCallable, Category = "Revive")
    void FinishRevive();

    UFUNCTION(BlueprintPure, Category = "Revive")
	bool IsCharacterReviving() const;

protected: // Revive handling
    UPROPERTY(EditDefaultsOnly, Category = "Revive|Tags")
    FGameplayTag RevivingTag;

    UPROPERTY(EditDefaultsOnly, Category = "Revive|Events")
	FGameplayTag ReviveEventTag;

private: // Revive handling
	bool bReviveRequested = false;

public: // Post death hit reaction handling
    UFUNCTION(BlueprintCallable, Category = "Death|Post Death HitReaction")
    void InitializePostDeathHitReaction();

    UFUNCTION(BlueprintCallable, Category = "Death|Post Death HitReaction")
    void EndPostDeathHitReaction();

    UFUNCTION(BlueprintCallable, Category = "Death|Post Death HitReaction")
	bool CanReceivePostDeathHitReaction() const;

protected: // Post death hit reaction handling
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death|Post Death HitReaction")
    bool bAllowPostDeathHitReaction = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death|Post Death HitReaction", meta = (EditCondition = "bAllowPostDeathHitReaction"))
    int32 MaxPostDeathHitReactCount = 5;

private: // Post death hit reaction handling
	bool bPostDeathHitReactActive = false;
    // int32 PostDeathHitReactCount = 0;

public: // Hit reaction handling
    UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
    void RequestHitReaction(const FAttributeEventContext& HitEventContext);

	UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
    void FinishHitReaction();

    UFUNCTION(BlueprintPure, Category = "Hit Reaction")
    bool IsHitReactionRequested() const { return bHitReactionRequested; }

protected: // Hit reaction handling
    UPROPERTY(EditDefaultsOnly, Category = "Hit Reaction|Events")
	FGameplayTag HitReactionEventTag;

private:
	bool bHitReactionRequested = false;

};
