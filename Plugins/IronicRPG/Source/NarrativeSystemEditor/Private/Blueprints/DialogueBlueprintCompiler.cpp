// Copyright Ironic Studio. All Rights Reserved.


#include "Blueprints/DialogueBlueprintCompiler.h"
#include "RPGGraphEditorUtils.h"
#include "Nodes/NarrativeStartGraphNode.h"
#include "Nodes/NarrativePlayerOptionsNode.h"
#include "Nodes/NarrativeBranchNode.h"

#include "Nodes/DialogueVariableParsable.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_SwitchName.h"

void FDialogueCompilerContext::PreCompile()
{
    FKismetCompilerContext::PreCompile();

    UDialogueBlueprint* DialogueBP = Cast<UDialogueBlueprint>(Blueprint);
    if (!DialogueBP)
    {
        return;
    }

    UEdGraph* DialogueGraph = nullptr;

    for (UEdGraph* Graph : DialogueBP->UbergraphPages)
    {
        if (Graph && Graph->GetFName() == UDialogueBlueprint::DialogueGraphName)
        {
            DialogueGraph = Graph;
            break;
        }
    }

    if (!DialogueGraph)
    {
        return;
    }

    UNarrativeStartGraphNode* StartNode = nullptr;

    for (UEdGraphNode* Node : DialogueGraph->Nodes)
    {
        if (StartNode)
        {
            break;
        }

        StartNode = Cast<UNarrativeStartGraphNode>(Node);
    }

    if (!StartNode)
    {
        MessageLog.Error(TEXT("This graph is missing a starting node."));
        return;
    }

    TArray<FDialogueLine> DialogueLines;

    FRPGGraphTerminationValidator Validator(
        MessageLog,
        [this](const TSet<UEdGraphNode*>& CycleNodes)
        {
            return EvaluateDialogueCycle(CycleNodes);
        },
        [&](const UEdGraphNode* CurrentNode)
        {
            const UNarrativeGraphNodeBase* NarrativeNode = Cast<UNarrativeGraphNodeBase>(CurrentNode);
            if (!NarrativeNode)
            {
                return;
            }

            if (IDialogueVariableParsable* Parsable = Cast<IDialogueVariableParsable>(NarrativeNode->GetNarrativeNodeInfo()))
            {
                Parsable->GetDialogueLinesForVariableParsing(DialogueLines);
            }
        });

    Validator.Validate(DialogueGraph, StartNode);

    TSet<FName> VariableNames;

    for (const FDialogueLine& Line : DialogueLines)
    {
        FDialogueLine::ExtractDialogueVariableNames(Line.DialogueText, VariableNames);
    }

    ValidateVariableParsingFunctionFullyMatch(VariableNames);
}

bool FDialogueCompilerContext::EvaluateDialogueCycle(const TSet<UEdGraphNode*>& CycleNodes) const
{
    bool bHasBranchNode = false;
    bool bHasPlayerOptionsNode = false;
    bool bHasExit = false;

    for (UEdGraphNode* Node : CycleNodes)
    {
        if (!Node)
        {
            continue;
        }

        if (Node->IsA<UNarrativeBranchNode>())
        {
            bHasBranchNode = true;
        }

        if (Node->IsA<UNarrativePlayerOptionsNode>())
        {
            bHasPlayerOptionsNode = true;
        }

        if (bHasPlayerOptionsNode || bHasBranchNode)
        {
            if (DoesNodeHaveExitFromCycle(Node, CycleNodes))
            {
                bHasExit = true;
            }
        }
    }

    return bHasExit;
}

bool FDialogueCompilerContext::DoesNodeHaveExitFromCycle(UEdGraphNode* PlayerOptionsNode, const TSet<UEdGraphNode*>& CycleNodes) const
{
    if (!PlayerOptionsNode)
    {
        return false;
    }

    for (UEdGraphPin* Pin : PlayerOptionsNode->Pins)
    {
        if (Pin->Direction != EGPD_Output)
        {
            continue;
        }

        if (Pin->LinkedTo.IsEmpty())
        {
            return true;
        }

        for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
        {
            TArray<UEdGraphNode*> OutNextNodes;
            FRPGGraphEditorUtils::ResolveLinkedNodesIgnoringKnot(LinkedPin, OutNextNodes); 
            if (!OutNextNodes.IsEmpty() && !CycleNodes.Contains(OutNextNodes.Top()))
            {
                return true;
            }
        }
    }

    return false;
}

void FDialogueCompilerContext::ValidateVariableParsingFunctionFullyMatch(const TSet<FName>& VariableNames) const
{
    if (VariableNames.IsEmpty())
    {
        return;
    }
    
    FName FunctionName = GET_FUNCTION_NAME_CHECKED(UDialogue, GetDialogueVariable);
    UEdGraph* FunctionGraph = FRPGGraphEditorUtils::GetGraphByName(Blueprint, FunctionName);
    if (!FunctionGraph)
    {
        MessageLog.Error(*FString::Printf(TEXT("This dialogue defines the dialogue variables in the dialogue lines, need to override %s."), *FunctionName.ToString()));
        return;
    }

    UEdGraphPin* VarNamePin = nullptr;
    UK2Node_SwitchName* SwitchNode = nullptr;

    for (UEdGraphNode* Node : FunctionGraph->Nodes)
    {
        UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node);
        if (!EntryNode)
        {
            continue;
        }
        
        VarNamePin = EntryNode->FindPinByPredicate([](UEdGraphPin* InPin)
            {
                return InPin && InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name;
            });

        if (!VarNamePin)
        {
            MessageLog.Error(TEXT("@@ is missing variable name pin."), EntryNode);
            return;
        }
        
        TArray<UEdGraphNode*> RealLinkedNodes;
        FRPGGraphEditorUtils::GetRealLinkedNodes(VarNamePin, RealLinkedNodes);
        for (UEdGraphNode* LinkedNode : RealLinkedNodes)
        {
            SwitchNode = Cast<UK2Node_SwitchName>(LinkedNode);
            if (SwitchNode)
            {
                break;
            }
        }
    }

    if (!SwitchNode)
    {
        MessageLog.Error(TEXT("@@ should link to a switch node."), VarNamePin);
        return;
    }

    TSet<FName> Cases;

    for (UEdGraphPin* Pin : SwitchNode->Pins)
    {
        if (!Pin || Pin->Direction != EGPD_Output || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
        {
            continue;
        }

        if (Pin->PinName == UEdGraphSchema_K2::PN_Then ||
            Pin->PinName == UEdGraphSchema_K2::PN_Execute ||
            Pin->PinName == TEXT("Default"))
        {
            continue;
        }

        FName CaseName = Pin->PinName;

        if (CaseName.IsNone() && !Pin->PinFriendlyName.IsEmpty())
        {
            CaseName = FName(*Pin->PinFriendlyName.ToString());
        }

        if (!CaseName.IsNone())
        {
            Cases.Add(CaseName);
        }
    }

    TSet<FName> MissingCases = VariableNames.Difference(Cases);
    for (const FName& Missing : MissingCases)
    {
        MessageLog.Error(*FString::Printf(TEXT("@@ is missing case: %s"), *Missing.ToString()), SwitchNode);
    }
}

bool FDialogueCompiler::CanCompile(const UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return false;
    }

    if (Blueprint->ParentClass && Blueprint->ParentClass->IsChildOf(UDialogue::StaticClass()))
    {
        return true;
    }

    return false;
}

void FDialogueCompiler::Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results)
{
    if (UDialogueBlueprint* DialogueBP = Cast<UDialogueBlueprint>(Blueprint))
    {
        TSharedPtr<FKismetCompilerContext> Compiler = MakeShareable(
            new FDialogueCompilerContext(DialogueBP, Results, CompileOptions)
        );

        Compiler->Compile();
    }
    else
    {
        Results.Error(TEXT("Tried to compile a non-Dialogue Blueprint with Dialogue compiler."));
    }
}

bool FDialogueCompiler::GetBlueprintTypesForClass(UClass* ParentClass, UClass*& OutBlueprintClass, UClass*& OutBlueprintGeneratedClass) const
{
    if (ParentClass && ParentClass->IsChildOf(UDialogue::StaticClass()))
    {
        OutBlueprintClass = UDialogueBlueprint::StaticClass();
        OutBlueprintGeneratedClass = UDialogueBlueprintGeneratedClass::StaticClass();
        return true;
    }

    return false;
}
