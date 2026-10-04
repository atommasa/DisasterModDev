// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"

class UBlueprint;
class UEdGraph;
class UK2Node_EditablePinBase;

class RPGGRAPHEDITOR_API FRPGGraphEditorUtils
{
public:
    static UEdGraph* GetGraphByName(UBlueprint* InBlueprint, FName GraphName);

    static UEdGraph* GetOrCreateUbergraphPage(UBlueprint* InBlueprint, FName GraphName);
    static UEdGraph* GetOrCreateFunctionGraph(UBlueprint* InBlueprint, FName FunctionName, UFunction* SignatureFunction = nullptr);
    static UEdGraph* FindFunctionGraphByName(UBlueprint* InBlueprint, FName FunctionName);

    static FVector2D FindLocationForNewNode(const UEdGraph* Graph, const FVector2D& DefaultLocation = FVector2D::ZeroVector);

    static UEdGraphPin* CreateUserDefinedPinFor(UK2Node_EditablePinBase* Node, FName PinName, const FEdGraphPinType& PinType, EEdGraphPinDirection Direction);
    static bool CanCreateUserDefinedPinFor(UK2Node_EditablePinBase* Node, FName PinName);

    static void GetRealLinkedNodes(UEdGraphPin* SourcePin, TArray<UEdGraphNode*>& OutRealLinkedNodes);

	static void ResolveLinkedNodesIgnoringKnot(UEdGraphPin* LinkedPin, TArray<UEdGraphNode*>& OutNodes);
	static void ResolveLinkedNodesIgnoringKnot(UEdGraphPin* LinkedPin, TArray<UEdGraphNode*>& OutNodes, TSet<UEdGraphNode*>& VisitedKnotNodes);

};
