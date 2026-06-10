// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UK2Node_CustomEvent;

class NARRATIVESYSTEMEDITOR_API FNarrativeEditorUtils
{
public:
	static UEdGraph* GetOrCreateGraph(UBlueprint* InBlueprint, FName EventGraphName);
	static UEdGraph* GetOrCreateFunctionGraph(UBlueprint* InBlueprint, FName FunctionName, UFunction* SignatureFunction = nullptr);

public:
	static FVector2D FindLocationForNewNode(const UEdGraph* Graph, const FVector2D& DefaultLocation = FVector2D::ZeroVector);

	static UEdGraph* FindFunctionGraphByName(UBlueprint* Blueprint, FName FunctionName);

public:
	static UEdGraphPin* CreateUserDefinedPinFor(UK2Node_EditablePinBase* CustomEventNode, FName PinName, const FEdGraphPinType& PinType, EEdGraphPinDirection Direction);
	static bool CanCreateUserDefinedPinFor(UK2Node_EditablePinBase* CustomEventNode, FName PinName);

};