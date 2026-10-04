// Copyright Ironic Studio. All Rights Reserved.


#include "Nodes/NarrativePlayerOptionsNodeInfo.h"

void UNarrativePlayerOptionsNodeInfo::GetDialogueLinesForVariableParsing(TArray<FDialogueLine>& OutDialogueLines) const
{
	for (const FOptionLine& OptionLine : Options)
	{
		OutDialogueLines.Add(OptionLine.DialogueLine);
	}
}

#if WITH_EDITOR
void UNarrativePlayerOptionsNodeInfo::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedChainEvent);

	const FName PropertyName = (PropertyChangedChainEvent.Property != nullptr) ? PropertyChangedChainEvent.Property->GetFName() : NAME_None;
	
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UNarrativePlayerOptionsNodeInfo, Options))
	{
		if (Options.IsEmpty())
		{
			Options.Add({});
		}
	}
}
#endif // WITH_EDITOR