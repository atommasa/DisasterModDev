// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KismetCompiler.h"
#include "KismetCompilerModule.h"

class RPGGRAPHEDITOR_API FRPGGraphTerminationValidator
{
public:
    using FRPGGraphCyclePolicy = TFunction<bool(const TSet<UEdGraphNode*>& CycleNodes)>;
    using FRPGOnNodeVisited = TFunction<void(const UEdGraphNode* CurrentNode)>;

    explicit FRPGGraphTerminationValidator(FCompilerResultsLog& InMessageLog, FRPGGraphCyclePolicy InCyclePolicy = nullptr, FRPGOnNodeVisited InOnNodeVisited = nullptr)
        : MessageLog(InMessageLog)
        , CyclePolicy(MoveTemp(InCyclePolicy))
        , OnNodeVisited(MoveTemp(InOnNodeVisited))
    {
    }

    bool Validate(UEdGraph* InGraph, UEdGraphNode* StartNode);

private:
    void BuildReachableGraph(UEdGraphNode* StartNode);
    void GetNextNodes(UEdGraphNode* Node, TArray<UEdGraphNode*>& OutNextNodes) const;

    void StrongConnect(UEdGraphNode* Node);
    bool IsCycleComponent(const TSet<UEdGraphNode*>& Component) const;
    bool HandleCycleComponent(const TSet<UEdGraphNode*>& Component);

private:
    FCompilerResultsLog& MessageLog;
    FRPGGraphCyclePolicy CyclePolicy;
    FRPGOnNodeVisited OnNodeVisited;

    TMap<UEdGraphNode*, TArray<UEdGraphNode*>> NextNodeMap;
    TSet<UEdGraphNode*> ReachableNodes;

    int32 IndexCounter = 0;
    TMap<UEdGraphNode*, int32> NodeIndexMap;
    TMap<UEdGraphNode*, int32> LowLinkMap;
    TArray<UEdGraphNode*> Stack;
    TSet<UEdGraphNode*> NodesOnStack;

    bool bValid = true;
};

class RPGGRAPHEDITOR_API FRPGBlueprintCompilerContext : public FKismetCompilerContext
{
public:
    FRPGBlueprintCompilerContext(UBlueprint* SourceBP, TSubclassOf<UBlueprintGeneratedClass> InGeneratedClassClass,  FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& CompilerOptions)
        : FKismetCompilerContext(SourceBP, InMessageLog, CompilerOptions)
        , GeneratedClassClass(InGeneratedClassClass)
    {
    }

    virtual void SpawnNewClass(const FString& NewClassName) override;

    virtual bool IsNodePure(const UEdGraphNode* Node) const override;

protected:
    TSubclassOf<UBlueprintGeneratedClass> GeneratedClassClass;

};