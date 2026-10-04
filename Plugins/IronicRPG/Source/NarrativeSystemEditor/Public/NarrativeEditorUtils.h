// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RPGGraphEditorUtils.h"

class NARRATIVESYSTEMEDITOR_API FNarrativeEditorUtils : public FRPGGraphEditorUtils
{
public:
    // Backward-compatible name for the dialogue event graph helper.
    static UEdGraph* GetOrCreateGraph(UBlueprint* InBlueprint, FName DialogueEventGraphName);
    static UEdGraph* GetOrCreateFunctionGraph(UBlueprint* InBlueprint, FName FunctionName, UFunction* SignatureFunction = nullptr);
};
