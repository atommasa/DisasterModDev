// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGId.h"
#include "SaveGame/Saveable.h"
#include "SaveGameSubsystem.h"
#include "StructUtils/InstancedStruct.h"
#include "RPGSaveGameMigrationTestTypes.generated.h"

USTRUCT()
struct FRPGSaveGameMigrationNestedTestData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FRPGId DirectId;

	UPROPERTY()
	FRPGId TransientId;

	UPROPERTY(SaveGame)
	TArray<FRPGId> IdArray;

	UPROPERTY(SaveGame)
	TSet<FRPGId> IdSet;

	UPROPERTY(SaveGame)
	TMap<FRPGId, FRPGId> IdMap;
};

USTRUCT()
struct FRPGSaveGameMigrationTestModule
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FRPGId DirectId;

	UPROPERTY(SaveGame)
	FRPGSaveGameMigrationNestedTestData Nested;

	UPROPERTY(SaveGame)
	FInstancedStruct NestedInstance;
};

UCLASS()
class URPGSaveGameMigrationTestProvider : public UGameInstanceSubsystem, public ISaveable
{
	GENERATED_BODY()

public:
	virtual FName GetSaveModuleType() const override { return TEXT("MigrationTest"); }
	virtual void SaveDataTo(FInstancedStruct& SaveData) override {}
	virtual void LoadDataFrom(const FInstancedStruct& SaveData) override { ++LoadCallCount; }
	virtual FSimpleMulticastDelegate& OnLoadComplete() override { return LoadCompleteDelegate; }

public:
	int32 LoadCallCount = 0;
	FSimpleMulticastDelegate LoadCompleteDelegate;
};

UCLASS()
class URPGSaveGameMigrationTestSubsystem : public USaveGameSubsystem
{
	GENERATED_BODY()

public:
	void DispatchLoadedSaveForTest(URPGSaveGame& SaveGame)
	{
		OnGameLoaded(TEXT("TestSlot"), 0, &SaveGame);
	}

protected:
	virtual void UpdatePlayTimeBySlot(const FString& SlotName) override {}
};
