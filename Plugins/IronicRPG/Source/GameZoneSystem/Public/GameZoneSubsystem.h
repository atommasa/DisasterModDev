// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "Levels/GameZoneContext.h"

#include "SaveGame/Saveable.h"

#include "GameZoneSubsystem.generated.h"

class ARPGPlayerStart;

/**
 * 
 */
UCLASS(Abstract, Blueprintable)
class GAMEZONESYSTEM_API UGameZoneSubsystem : public UGameInstanceSubsystem, public ISaveable
{
	GENERATED_BODY()
	
protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	// Enters a game zone with the specified context.
	void EnterGameZone(const FGameZoneContext& NewGameZoneContext, TDelegate<void()> DelegateToCall = TDelegate<void()>());

	void LoadZone(const FGameZoneContext& LoadContext, TDelegate<void()> DelegateToCall = TDelegate<void()>());
	void LoadSubZone(const FGameZoneContext& LoadContext, TDelegate<void()> DelegateToCall = TDelegate<void()>());

	void CreateStreamInstance(UWorld* World, const FString& LongPackageName, const FVector& Location = FVector::ZeroVector, const FRotator& Rotation = FRotator::ZeroRotator);

	UFUNCTION(BlueprintCallable, Category = "GameZone")
	FGameZoneContext GetCurrentContext() const { return CurrentContext; }

	// Only used for loading the player start point when loading a game, it will find the player start point based on the current context and return its transform
	UFUNCTION(BlueprintCallable, Category = "GameZone")
	FTransform GetSaveGameTransform() const; // TODO: API 要不要改名字，或者嘗試新增 API

	UFUNCTION(BlueprintCallable, Category = "GameZone")
	class UGameZoneAsset* FindZoneAssetForCurrentMap() const;

private:
	UFUNCTION()
	void WaitUntilLevelActorInitialized();

	ULevelStreaming* CurrentStreaming;

	FGameZoneContext CurrentContext;

	FGameZoneContext PendingContext;

public: // ISaveable
	virtual FName GetSaveModuleType() const override;
	virtual void SaveDataTo(FInstancedStruct& SaveData) override;
	virtual void LoadDataFrom(const FInstancedStruct& SaveData) override;

	virtual FSimpleMulticastDelegate& OnLoadComplete() override;
	FSimpleMulticastDelegate LoadCompleteDelegate;
};
