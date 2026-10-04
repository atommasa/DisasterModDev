// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Narrative/DialogueDataTypes.h"
#include "DialogueVariableParsable.generated.h"

UINTERFACE(MinimalAPI)
class UDialogueVariableParsable : public UInterface
{
	GENERATED_BODY()
};

class NARRATIVESYSTEMRUNTIME_API IDialogueVariableParsable
{
	GENERATED_BODY()

public:
	virtual void GetDialogueLinesForVariableParsing(TArray<FDialogueLine>& OutDialogueLines) const = 0;
};
