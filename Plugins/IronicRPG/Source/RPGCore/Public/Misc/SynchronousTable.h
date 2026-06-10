// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StructUtils/InstancedStruct.h"
#include "SynchronousTable.generated.h"

class USynchronousTableMaster;

/**
 * The table that can get rows from master table, and can set specific data to rows without contaminated other sub-tables
 */
UCLASS(Blueprintable)
class RPGCORE_API USynchronousTable : public UDataAsset
{
	GENERATED_BODY()

public:
	USynchronousTable(const FObjectInitializer& ObjectInitializer);

protected:
	// The master table of this synchronous table
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<USynchronousTableMaster> TableMasterClass = nullptr;

	// The categories can be displayed from the master table
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(GetOptions = "GetMasterCategories"))
	TArray<FName> SupportedCategories;

	// The rows that defined in master table, you can set specific data here
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, EditFixedSize, meta=(ReadOnlyKeys, StructTypeConst))
	TMap<FName, FInstancedStruct> Table;

public:
	UFUNCTION(BlueprintCallable, Category = "SynchronousTable")
	TArray<FName> GetMasterCategories() const;

	UFUNCTION(BlueprintCallable, Category = "SynchronousTable")
	FInstancedStruct GetData(const FName& Key) const;

#if WITH_EDITOR
protected:
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	
	virtual void UpdateTableSchema();
	virtual void UpdateTableKeys();

	void OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event);
#endif // WITH_EDITOR
};
