// Copyright Ironic Studio. All Rights Reserved.


#include "ContentBrowser/RPGPrimaryAssetFactory.h"

#include "Assets/RPGPrimaryAsset.h"

URPGPrimaryAssetFactory::URPGPrimaryAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = URPGPrimaryAsset::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* URPGPrimaryAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<URPGPrimaryAsset>(InParent, Class, Name, Flags);
}

bool URPGPrimaryAssetFactory::CanCreateNew() const
{
	return true;
}
