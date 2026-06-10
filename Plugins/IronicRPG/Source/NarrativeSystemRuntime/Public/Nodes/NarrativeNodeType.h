// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NarrativeNodeType.generated.h"

/**
 * 
 */
UENUM()
enum class ENarrativeNodeType : uint8
{
	UnknownNode,
	StartNode,
	DialogueNode,
	PlayerOptionsNode,
	CutsceneNode,
	BranchNode,
	SetVariablesNode,
};
