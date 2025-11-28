// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "RPGPrimaryAssetFactory.generated.h"

/**
 * 
 */
UCLASS()
class URPGPrimaryAssetFactory : public UFactory
{
	GENERATED_BODY()
	
public:
	URPGPrimaryAssetFactory(const FObjectInitializer& ObjectInitializer);

public:
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool CanCreateNew() const override;
};
