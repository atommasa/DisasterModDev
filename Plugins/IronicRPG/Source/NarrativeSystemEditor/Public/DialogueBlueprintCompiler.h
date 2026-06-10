#pragma once

#include "CoreMinimal.h"
#include "KismetCompiler.h"
#include "KismetCompilerModule.h"
#include "Blueprints/DialogueBlueprintGeneratedClass.h"
#include "EdGraphNode_Comment.h"
#include "EdGraph/EdGraphNode_Documentation.h"
#include "Nodes/NarrativeGraphNodeBase.h"

/**
 * Custom compiler context for Dialogue Blueprints
 */
class DialogueBlueprintCompiler : public FKismetCompilerContext
{
public:
    DialogueBlueprintCompiler(UDialogueBlueprint* SourceBP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& CompilerOptions)
        : FKismetCompilerContext(SourceBP, InMessageLog, CompilerOptions)
    {
    }

    virtual void SpawnNewClass(const FString& NewClassName) override
    {
        if (UDialogueBlueprint* DialogueBP = Cast<UDialogueBlueprint>(Blueprint))
        {
            NewClass = NewObject<UDialogueBlueprintGeneratedClass>(
                GetTransientPackage(),
                *NewClassName,
                RF_Public | RF_Transient
            );
        }
        else
        {
            FKismetCompilerContext::SpawnNewClass(NewClassName);
        }
    }

    virtual void OnNewClassSet(UBlueprintGeneratedClass* ClassToUse) override
    {
        FKismetCompilerContext::OnNewClassSet(ClassToUse);
    }

    virtual bool IsNodePure(const UEdGraphNode* Node) const override
    {
        if (const UK2Node* K2Node = Cast<const UK2Node>(Node))
        {
            return K2Node->IsNodePure();
        }

        // Only non-K2 nodes are comments, documentation, and narrative graph nodes, which are pure
        ensure(Node->IsA<UEdGraphNode_Comment>() ||
            Node->IsA<UEdGraphNode_Documentation>() ||
            Node->IsA<UNarrativeGraphNodeBase>());
        return true;
    }
};

/**
 * Compiler module for Dialogue Blueprints
 */
class FDialogueBlueprintCompilerModule : public IBlueprintCompiler
{
public:
    virtual bool CanCompile(const UBlueprint* Blueprint) override
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

    virtual void Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results) override
    {
        if (UDialogueBlueprint* DialogueBP = Cast<UDialogueBlueprint>(Blueprint))
        {
            TSharedPtr<FKismetCompilerContext> Compiler = MakeShareable(
                new DialogueBlueprintCompiler(DialogueBP, Results, CompileOptions)
            );
            Compiler->Compile();
        }
        else
        {
            Results.Error(TEXT("Tried to compile a non-Dialogue Blueprint with Dialogue compiler."));
        }
    }

    virtual bool GetBlueprintTypesForClass(
        UClass* ParentClass,
        UClass*& OutBlueprintClass,
        UClass*& OutBlueprintGeneratedClass
    ) const override
    {
        if (ParentClass && ParentClass->IsChildOf(UDialogue::StaticClass()))
        {
            OutBlueprintClass = UDialogueBlueprint::StaticClass();
            OutBlueprintGeneratedClass = UDialogueBlueprintGeneratedClass::StaticClass();
            return true;
        }

        return false;
    }
};
