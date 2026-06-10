// Copyright Ironic Studio. All Rights Reserved.


#include "Narrative/NarrativeDialoguePlayer.h"
#include "NarrativeAsset.h"
#include "Nodes/NarrativeNodeInfo.h"
#include "Nodes/NarrativeDialogueNodeInfo.h"
#include "Nodes/NarrativePlayerOptionsNodeInfo.h"

DEFINE_LOG_CATEGORY_STATIC(NarrativePlayerRuntime, Log, All)

void UNarrativeDialoguePlayer::PlayDialogue(UDialogueBlueprint* NarrativeAsset, APlayerController* PC)
{
	_PlayingAsset = NarrativeAsset;
	UNarrativeRuntimeGraph* Graph = NarrativeAsset->Graph;

	// Find strat node
	for (auto* Node : Graph->Nodes)
	{
		if (Node->NodeType == ENarrativeNodeType::StartNode)
		{
			_CurrentNode = Node;
			break;
		}
	}

	if (!_CurrentNode)
	{
		UE_LOG(NarrativePlayerRuntime, Display, TEXT("Can not find start node!"));
		return;
	}

	_CurrentNode->NodeInfo->ExecuteNode();
}

void UNarrativeDialoguePlayer::ContinueToNextLine()
{
	if (!_CurrentNode)
	{
		UE_LOG(NarrativePlayerRuntime, Display, TEXT("Can not find start node!"));
		return;
	}

	switch (_CurrentNode->NodeType)
	{
	case ENarrativeNodeType::DialogueNode:
		// To next node
		if (_CurrentNode->OutputPins[0]->Connection)
		{
			_CurrentNode = _CurrentNode->OutputPins[0]->Connection->Parent;
		}

		UpdateFromDialogueNode();
		break;

	case ENarrativeNodeType::PlayerOptionsNode:
		// TODO: Show Option UI
		break;

	case ENarrativeNodeType::CutsceneNode:

		break;

	default:
		_CurrentNode = nullptr;
	}

	if (!_CurrentNode)
	{
		EndDialogue();
		return;
	}
}

void UNarrativeDialoguePlayer::ChoseOptionAtIndex(int Index)
{
	if (_CurrentNode->NodeType != ENarrativeNodeType::PlayerOptionsNode)
	{
		return;
	}

	UNarrativePlayerOptionsNodeInfo* NodeInfo = Cast<UNarrativePlayerOptionsNodeInfo>(_CurrentNode->NodeInfo);
	if (NodeInfo && NodeInfo->Options.IsValidIndex(Index))
	{
		if (_CurrentNode->OutputPins.IsValidIndex(Index) &&
			_CurrentNode->OutputPins[Index]->Connection)
		{
			_CurrentNode = _CurrentNode->OutputPins[Index]->Connection->Parent;
			ContinueToNextLine();
		}
		else
		{
			EndDialogue();
		}
	}
}

void UNarrativeDialoguePlayer::EndDialogue()
{

}

void UNarrativeDialoguePlayer::UpdateFromDialogueNode()
{
	if (_CurrentNode && _CurrentNode->NodeType == ENarrativeNodeType::DialogueNode)
	{
		UNarrativeDialogueNodeInfo* NodeInfo = Cast<UNarrativeDialogueNodeInfo>(_CurrentNode->NodeInfo);
		if (!NodeInfo)
		{
			return;
		}

		// TODO: Update dialogue
	}
}
