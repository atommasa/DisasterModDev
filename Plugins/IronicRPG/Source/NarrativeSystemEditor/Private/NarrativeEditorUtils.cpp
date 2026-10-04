// Copyright Ironic Studio. All Rights Reserved.

#include "NarrativeEditorUtils.h"

UEdGraph* FNarrativeEditorUtils::GetOrCreateGraph(UBlueprint* InBlueprint, FName DialogueEventGraphName)
{
    return FRPGGraphEditorUtils::GetOrCreateUbergraphPage(InBlueprint, DialogueEventGraphName);
}

UEdGraph* FNarrativeEditorUtils::GetOrCreateFunctionGraph(UBlueprint* InBlueprint, FName FunctionName, UFunction* SignatureFunction)
{
    return FRPGGraphEditorUtils::GetOrCreateFunctionGraph(InBlueprint, FunctionName, SignatureFunction);
}
