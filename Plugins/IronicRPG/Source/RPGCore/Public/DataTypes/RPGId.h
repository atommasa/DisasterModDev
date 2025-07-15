#pragma once

#include "CoreMinimal.h"
#include "RPGId.generated.h"

UENUM(BlueprintType)
enum class EIdType : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	Character UMETA(DisplayName = "Character"),
	Item UMETA(DisplayName = "Item"),
	Quest UMETA(DisplayName = "Quest"),
	Skill UMETA(DisplayName = "Skill")
};

/**
 * This structure defines a unique identifier for various game entities.
 * It includes an Id of type FName and an IdType to categorize the entity.
 */
USTRUCT(BlueprintType)
struct RPGCORE_API FRPGId
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName Id;

	FRPGId() : Id(TEXT("")) {}

	FRPGId(const FName& InId) : Id(InId) {}

	bool IsValid() const { return Id.IsValid() && !Id.IsNone(); }

	FString ToString() const { return Id.ToString(); }

	bool operator==(const FRPGId& Other) const { return Id == Other.Id; }

	bool operator!=(const FRPGId& Other) const { return !(*this == Other); }

	EIdType GetIdType() const
	{
		if (!IsValid())
		{
			return EIdType::None;
		}

		FString IdStr = Id.ToString();
		if (IdStr.Len() <= 4)
		{
			return EIdType::None;
		}

		FString Prefix = IdStr.LeftChop(4).ToLower();

		if (Prefix == "c") return EIdType::Character;
		else if (Prefix == "i") return EIdType::Item;
		else if (Prefix == "q") return EIdType::Quest;
		else if (Prefix == "s") return EIdType::Skill;

		return EIdType::None;
	}

	FString GetIdTypeString() const
	{
		EIdType Type = GetIdType();
		UEnum* Enum = StaticEnum<EIdType>();
		if (Enum)
		{
			return Enum->GetDisplayNameTextByValue((int32)Type).ToString();
		}

		return TEXT("None");
	}
};

FORCEINLINE uint32 GetTypeHash(const FRPGId& Id)
{
	return HashCombine(GetTypeHash(Id.Id), GetTypeHash(static_cast<uint8>(Id.GetIdType())));
}