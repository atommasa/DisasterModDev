// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Factories/BlueprintFactory.h"
#include "NarrativeAssetFactory.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativeBlueprintFactory : public UBlueprintFactory
{
	GENERATED_BODY()

public:
    UNarrativeBlueprintFactory();

    virtual bool ConfigureProperties() override;
    virtual bool ShouldShowInNewMenu() const override { return true; }

    virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;

};
