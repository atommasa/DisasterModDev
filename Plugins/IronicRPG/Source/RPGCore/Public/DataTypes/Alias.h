// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Alias.generated.h"

/**
 * Alias structure for handling names with gameplay tag variations.
 */
USTRUCT(BlueprintType)
struct RPGCORE_API FAlias
{
	GENERATED_BODY()

	// Authoritative name
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DefaultName;

	// Aliases mapped to gameplay tags
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FGameplayTag, FText> Aliases;
	// TODO: 考慮未來把FGameplayTag換成其他可配合敘事模組的類型

	FAlias() = default;

public:
	FAlias& operator=(const FAlias& Other)
	{
		DefaultName = Other.DefaultName;
		Aliases = Other.Aliases;
		return *this;
	}

	FText& operator[](const FGameplayTag& Tag)
	{
		return Aliases[Tag];
	}

public:
	FText GetDefaultName() const { return DefaultName; }

	bool IsValid() const { return !DefaultName.IsEmpty(); }

};
