// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RPGId.generated.h"

#define ID_TYPE_TAG TEXT("IdType") // A metadata tag used to specify the type of the Id.
#define ID_CLAIM_TAG TEXT("IdClaim") // A metadata tag used to claim the Id for a specific purpose, most likely to use in the primary assets.

#define ID_None NAME_None

/**
 * This structure defines a unique identifier for various game entities.
 */
USTRUCT(BlueprintType)
struct RPGCORE_API FRPGId
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName Id = NAME_None;

	FRPGId() = default;

	FRPGId(const FName& InId) : Id(InId) {}

	bool IsValid() const { return Id.IsValid() && !Id.IsNone(); }

	FString ToString() const { return Id.ToString(); }

	FPrimaryAssetId ToPrimaryAssetId() const { return FPrimaryAssetId(FName(GetIdTypeString()), Id); }
	
	int32 GetNumeric() const;
	
	bool operator==(const FRPGId& Other) const { return Id == Other.Id; }

	bool operator!=(const FRPGId& Other) const { return !(*this == Other); }

	FName GetRebuiltIdType() const;

	FName GetIdType() const;
	FString GetIdTypeString() const;

	mutable FName CachedType = ID_None;
	mutable FName CachedId = ID_None;
};

FORCEINLINE uint32 GetTypeHash(const FRPGId& Id)
{
	return HashCombine(GetTypeHash(Id.Id), GetTypeHash(Id.GetIdType()));
}

/**
 * Macro to check if an FRPGId is valid and matches a specific Id type.
 * @param Id The FRPGId to check.
 * @param Type The type name to compare against.
 */
#define CHECK_ID_TYPE(Id, Type) \
	(Id.IsValid() && Id.GetIdTypeString() == #Type)