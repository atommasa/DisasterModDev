// Copyright Ironic Studio. All Rights Reserved.


#include "NarrativeEditorUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"

UEdGraph* FNarrativeEditorUtils::GetOrCreateGraph(UBlueprint* InBlueprint, FName EventGraphName)
{
    for (UEdGraph* Graph : InBlueprint->UbergraphPages)
    {
        if (Graph && Graph->GetFName() == EventGraphName)
        {
            return Graph;
        }
    }

    InBlueprint->Modify();

    UEdGraph* EventGraph = FBlueprintEditorUtils::CreateNewGraph(
        InBlueprint,
        EventGraphName,
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass()
    );

    if (!EventGraph)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create NarrativeEventGraph."));
        return nullptr;
    }

    EventGraph->Modify();
    EventGraph->SetFlags(RF_Transactional);

    FBlueprintEditorUtils::AddUbergraphPage(InBlueprint, EventGraph);
    InBlueprint->LastEditedDocuments.AddUnique(EventGraph);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InBlueprint);
    InBlueprint->MarkPackageDirty();

    return EventGraph;
}

UEdGraph* FNarrativeEditorUtils::GetOrCreateFunctionGraph(UBlueprint* InBlueprint, FName FunctionName, UFunction* SignatureFunction)
{
	if (!InBlueprint || FunctionName.IsNone() || !SignatureFunction)
	{
		return nullptr;
	}

	if (UEdGraph* ExistingGraph = FindFunctionGraphByName(InBlueprint, FunctionName))
	{
		return ExistingGraph;
	}

	const FScopedTransaction Transaction(
		NSLOCTEXT("Narrative", "CreateConditionFunction", "Create Condition Function")
	);

	InBlueprint->Modify();

	UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(
		InBlueprint,
		FunctionName,
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

    FunctionGraph->Modify();
    FunctionGraph->SetFlags(RF_Transactional);

    FBlueprintEditorUtils::AddFunctionGraph(
		InBlueprint,
		FunctionGraph,
		true,
        SignatureFunction
	);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InBlueprint);
    InBlueprint->BroadcastChanged();
    InBlueprint->MarkPackageDirty();

	return FunctionGraph;
}

FVector2D FNarrativeEditorUtils::FindLocationForNewNode(const UEdGraph* Graph, const FVector2D& DefaultLocation)
{
    if (!Graph || Graph->Nodes.Num() == 0)
    {
        return DefaultLocation;
    }

    int32 MaxY = TNumericLimits<int32>::Lowest();
    int32 MinX = TNumericLimits<int32>::Max();

    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node->IsA<UK2Node_CustomEvent>())
        {
            continue;
        }

        MinX = FMath::Min(MinX, Node->NodePosX);
        MaxY = FMath::Max(MaxY, Node->NodePosY);
    }

    return FVector2D(
        MinX,
        MaxY + 250.0f
    );
}

UEdGraph* FNarrativeEditorUtils::FindFunctionGraphByName(UBlueprint* Blueprint, FName FunctionName)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == FunctionName)
		{
			return Graph;
		}
	}

	return nullptr;
}

UEdGraphPin* FNarrativeEditorUtils::CreateUserDefinedPinFor(UK2Node_EditablePinBase* CustomEventNode, FName PinName, const FEdGraphPinType& PinType, EEdGraphPinDirection Direction)
{
    if (!CustomEventNode || !CanCreateUserDefinedPinFor(CustomEventNode, PinName))
    {
        return nullptr;
	}

    return CustomEventNode->CreateUserDefinedPin(PinName, PinType, Direction);
}

bool FNarrativeEditorUtils::CanCreateUserDefinedPinFor(UK2Node_EditablePinBase* CustomEventNode, FName PinName)
{
    if (!CustomEventNode || PinName.IsNone())
    {
        return false;
    }

    return !CustomEventNode->UserDefinedPins.ContainsByPredicate([&PinName](const TSharedPtr<FUserPinInfo>& UDPin)
        {
            return UDPin.IsValid() && (UDPin->PinName == PinName);
        });
}

