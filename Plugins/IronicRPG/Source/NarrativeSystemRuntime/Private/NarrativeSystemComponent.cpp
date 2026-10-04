// Copyright Ironic Studio. All Rights Reserved.


#include "NarrativeSystemComponent.h"
#include "Narrative/Dialogue.h"

void UNarrativeSystemComponent::BeginPlay()
{
	Super::BeginPlay();

}

bool UNarrativeSystemComponent::BeginDialogue(TSubclassOf<UDialogue> DialogueClass)
{
	if (!DialogueClass)
	{
		return false;
	}

	const UDialogue* Dialogue = DialogueClass->GetDefaultObject<UDialogue>();
	if (!Dialogue)
	{
		return false;
	}

	return true;
}

