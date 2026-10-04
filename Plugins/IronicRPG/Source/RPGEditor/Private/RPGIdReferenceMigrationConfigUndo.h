// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RPGIdReferenceMigrationConfigUndo.generated.h"

/** Transaction participant that keeps an external config file in lockstep with UObject Undo/Redo. */
UCLASS(Transient)
class URPGIdReferenceMigrationConfigUndo final : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(const FString& InFilename, const FString& InContents);
	void SetCommittedContents(const FString& InContents);

protected:
	virtual void PostEditUndo() override;

private:
	UPROPERTY()
	FString Filename;

	UPROPERTY()
	FString Contents;
};
