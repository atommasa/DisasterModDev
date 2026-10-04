// Copyright Ironic Studio. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RPGIdCategorySettings.generated.h"

USTRUCT()
struct FRPGIdCategoryRange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Range", meta=(ClampMin="0", ClampMax="9999"))
	int32 Start = 0;

	UPROPERTY(EditAnywhere, Category="Range", meta=(ClampMin="0", ClampMax="9999"))
	int32 End = 0;
};

USTRUCT()
struct FRPGIdCategoryDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Category")
	FName CategoryKey;

	UPROPERTY(EditAnywhere, Category="Category")
	FString DisplayName;

	UPROPERTY(EditAnywhere, Category="Category", meta=(TitleProperty="Start"))
	TArray<FRPGIdCategoryRange> Ranges;
};

USTRUCT()
struct FRPGIdCategoryTypeRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="RPG Id Categories", meta=(TitleProperty="DisplayName"))
	TArray<FRPGIdCategoryDefinition> Categories;
};

USTRUCT()
struct FRPGIdCategoryRules
{
	GENERATED_BODY()

	/** Only child Categories are exposed by the settings customization. */
	UPROPERTY(EditAnywhere, EditFixedSize, Category="RPG Id Categories", meta=(ReadOnlyKeys))
	TMap<FName, FRPGIdCategoryTypeRules> AssetTypes;
};

/** Read-only schema 1 import types. Never used by the authoring UI or written to disk. */
USTRUCT()
struct FRPGIdCategoryLegacyDefinition : public FRPGIdCategoryDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	FName AssetType;
};

USTRUCT()
struct FRPGIdCategoryLegacyRules
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FRPGIdCategoryLegacyDefinition> Categories;
};

/** Transient Details working copy. Persistence is exclusively owned by the checked category store. */
UCLASS(Config=Editor, DefaultConfig)
class URPGIdCategorySettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="RPG Id Categories")
	FRPGIdCategoryRules Rules;
};
