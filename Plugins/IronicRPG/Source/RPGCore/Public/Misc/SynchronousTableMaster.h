// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SynchronousTableMaster.generated.h"

/**
 * The struct that defines key and category for synchronous table
 */
USTRUCT(BlueprintType)
struct FSyncTableEntry
{
	GENERATED_BODY()

	FSyncTableEntry(FName InKey = NAME_None, FName InCategory = NAME_None)
		: Key(InKey)
		, Category(InCategory)
	{
	}

	UPROPERTY(EditDefaultsOnly)
	FName Key;

	UPROPERTY(EditDefaultsOnly, meta=(GetOptions = "GetMasterCategories"))
	FName Category;

	bool operator==(const FSyncTableEntry& Other) const
	{
		return Key == Other.Key && Category == Other.Category;
	}

	bool operator!=(const FSyncTableEntry& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT(BlueprintType)
struct FSyncKey
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName KeyName;

};

/**
 * This table defines rows for sub-tables. All the sub-tables chosen this table as master table will get the same rows as the master table.
 * But all the sub-tables' rows is independent with other sub-tables, we can set different data in the same row in different sub-tables.
 */
UCLASS(Blueprintable)
class RPGCORE_API USynchronousTableMaster : public UObject
{
	GENERATED_BODY()
	
public:
	USynchronousTableMaster(const FObjectInitializer& ObjectInitializer);

	// The schema struct of this table
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UScriptStruct* SchemaStruct;

	// The catrgories that the master keys can set
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FName> Categories;

	// MasterKeys can define rows for specific data in the synchronization table.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FSyncTableEntry> MasterKeys;

public:
	UFUNCTION(BlueprintCallable, Category = "SynchronousTable")
	TArray<FName> GetMasterKeys() const;

	UFUNCTION(BlueprintCallable, Category = "SynchronousTable")
	TArray<FName> GetMasterCategories() const;

#if WITH_EDITOR
protected:
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent) override;
#endif // WITH_EDITOR

};
