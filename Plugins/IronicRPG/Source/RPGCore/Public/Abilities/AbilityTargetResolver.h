// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayEffectTypes.h"
#include "AbilityTargetResolver.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTargetResolved, class UAbilityTargetResolver*, Resolver, const FGameplayAbilityTargetDataHandle&, TargetDataHandle);

/**
 * Target resolver base class for resolving ability targets.
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew)
class RPGCORE_API UAbilityTargetResolver : public UObject
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;

	UFUNCTION(BlueprintCallable, Category = "TargetResolver")
	void StartResolveTargets(AActor* SourceActor);

	UFUNCTION(BlueprintCallable, Category = "TargetResolver")
	void EndResolveTargets();

public:
	UPROPERTY(BlueprintCallable, BlueprintAssignable)
	FOnTargetResolved OnTargetResolved;
	
protected:
	UFUNCTION(BlueprintImplementableEvent)
	void StartResolveTargetsEvent();

	UFUNCTION(BlueprintImplementableEvent)
	void EndResolveTargetsEvent();

protected:
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> Source = nullptr;

}; 
