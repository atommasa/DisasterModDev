// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGIdMigration.h"
#include "UObject/Object.h"
#include "UObject/StrongObjectPtr.h"
#include "RPGIdReferenceMigrationHandoff.generated.h"

/** Transaction participant for persisted redirects waiting for the next Release Seal. */
UCLASS(Transient)
class URPGIdReferenceMigrationHandoffState final : public UObject
{
	GENERATED_BODY()

	friend class FRPGIdReferenceMigrationHandoffStore;

protected:
	virtual void PostEditUndo() override;

private:
	bool Save(FText& OutError) const;

private:
	UPROPERTY()
	TArray<FRPGIdRedirect> Redirects;

	FString StoragePath;
};

/** Owns persisted pending redirects and keeps them in lockstep with Editor Undo/Redo. */
class FRPGIdReferenceMigrationHandoffStore
{
public:
	explicit FRPGIdReferenceMigrationHandoffStore(FString InStoragePath);

	bool ReadPending(TArray<FRPGIdRedirect>& OutRedirects, FText& OutError) const;
	bool Stage(const FRPGIdRedirect& Redirect, bool& OutAdded, FText& OutError);
	bool RollbackStage(const FRPGIdRedirect& Redirect, bool bWasAdded, FText& OutError);
	bool Consume(TConstArrayView<FRPGIdRedirect> Redirects, FText& OutError);

private:
	bool Load(FText& OutError);

private:
	TStrongObjectPtr<URPGIdReferenceMigrationHandoffState> State;
	FText InitializationError;
};

/** Process-wide production handoff used by migration Apply and Release Seal. */
class FRPGIdReferenceMigrationHandoff
{
public:
	static void Register();
	static void Unregister();

	static bool ReadPending(TArray<FRPGIdRedirect>& OutRedirects, FText& OutError);
	static bool Stage(const FRPGIdRedirect& Redirect, bool& OutAdded, FText& OutError);
	static bool RollbackStage(const FRPGIdRedirect& Redirect, bool bWasAdded, FText& OutError);
	static bool Consume(TConstArrayView<FRPGIdRedirect> Redirects, FText& OutError);
	static FString GetStoragePath();
};
