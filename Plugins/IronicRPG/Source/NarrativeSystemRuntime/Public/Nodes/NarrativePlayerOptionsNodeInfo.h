

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Nodes/DialogueVariableParsable.h"
#include "NarrativeNodeInfo.h"
#include "Narrative/DialogueDataTypes.h"
#include "NarrativePlayerOptionsNodeInfo.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class NARRATIVESYSTEMRUNTIME_API UNarrativePlayerOptionsNodeInfo : public UNarrativeNodeInfo, public IDialogueVariableParsable
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FOptionLine> Options;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EOptionSelectedDisplayPolicy SelectedDisplayPolicy = EOptionSelectedDisplayPolicy::ShowAsSelected;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EOptionDisabledDisplayPolicy DisabledDisplayPolicy = EOptionDisabledDisplayPolicy::ShowDisabled;

public:
	UFUNCTION(BlueprintImplementableEvent)
	bool IsOptionSelectable(const UNarrativePlayerOptionsNodeInfo* NodeInfo, int32 OptionIndex) const;

public: // IDialogueVariableParsable interface
	virtual void GetDialogueLinesForVariableParsing(TArray<FDialogueLine>& OutDialogueLines) const override;

#if WITH_EDITOR
private:
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent) override;
#endif // WITH_EDITOR

};
