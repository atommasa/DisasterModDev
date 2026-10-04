// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/RPGGraphNodeKnot.h"
#include "NarrativeNodeKnot.generated.h"

/**
 * Backward-compatible Narrative reroute node.
 * The implementation lives in RPGGraphEditor so Quest/Dialogue graphs can share it.
 */
UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativeNodeKnot : public URPGGraphNodeKnot
{
    GENERATED_BODY()
};

using SNarrativeNodeKnot = SRPGNodeKnot;
using SNarrativeNodeKnotPin = SRPGNodeKnotPin;
