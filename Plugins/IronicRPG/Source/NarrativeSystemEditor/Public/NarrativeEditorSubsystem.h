// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "DataTypes/RPGId.h"
#include "NarrativeEditorSubsystem.generated.h"

class UCharacterAsset;

/**
 * 
 */
UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativeEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()
	
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	TMap<FRPGId, UCharacterAsset*> TryGetSpeakerAssets();

protected:	
	UPROPERTY()
	TMap<FRPGId, UCharacterAsset*> SpeakerAssetMap;

	UFUNCTION()
	void OnAssetAdded(const FAssetData& AssetData);

	UFUNCTION()
	void OnAssetRemoved(const FAssetData& AssetData);

private:
	bool bHasInitializedSpeakerAssets = false;

};
