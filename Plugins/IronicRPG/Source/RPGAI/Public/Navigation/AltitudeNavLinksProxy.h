// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Navigation/GeneratedNavLinksProxy.h"
#include "AltitudeNavLinksProxy.generated.h"

/**
 * 
 */
UCLASS()
class RPGAI_API UAltitudeNavLinksProxy : public UGeneratedNavLinksProxy
{
	GENERATED_BODY()

	virtual UWorld* GetWorld() const override;

	virtual bool OnLinkMoveStarted(class UObject* PathComp, const FVector& DestPoint) override;

	void StartLinkMovement(AActor* Agent, const FVector Destination);
};
