// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseSaveModule.generated.h"

UENUM(BlueprintType)
enum class ESaveModuleType : uint8
{
	Unknown = 0 UMETA(DisplayName = "Unknown"),
	Character UMETA(DisplayName = "Character"),
};

USTRUCT(BlueprintType)
struct RPGCORE_API FBaseSaveModule
{
	GENERATED_BODY()

public:
	~FBaseSaveModule() = default;

	UPROPERTY(SaveGame)
	ESaveModuleType ModuleType = ESaveModuleType::Unknown;

	// Empty structure, exists only as a base
};
