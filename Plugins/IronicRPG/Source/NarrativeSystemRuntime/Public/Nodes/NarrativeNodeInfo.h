// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NarrativeNodeInfo.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnNodeInfoPropertyChanged);

UENUM(BlueprintType)
enum class EDialogueEventType : uint8
{
	// When the dialogue line starts being displayed / played
	Starts UMETA(DisplayName = "Start Dialogue Line"),

	// When the dialogue line commits, as player chooses an option
	Commits UMETA(DisplayName = "Commit Dialogue Line"),

	// When the dialogue line finishes displaying / playing
	Finishes UMETA(DisplayName = "Finish Dialogue Line"),
};

/**
 * This class is used to store information about a narrative node.
 */
UCLASS(BlueprintType)
class NARRATIVESYSTEMRUNTIME_API UNarrativeNodeInfo : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FGuid NodeGuid = FGuid::NewGuid();

	UPROPERTY()
	FName CallableBindingName;
	
public: // Node Execution
	virtual bool ExecuteNode() { return true; }
	virtual bool StopNode() { return true; }

#if WITH_EDITOR
protected:
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent) override;

public:
	FOnNodeInfoPropertyChanged OnNodeInfoPropertyChanged;
#endif // WITH_EDITOR
};