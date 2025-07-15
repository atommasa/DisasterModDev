// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "KismetCompiler.h"
#include "NarrativeAsset.h"
#include "KismetCompilerModule.h"
#include "Blueprint/DialogueBlueprintGeneratedClass.h"
#include "EdGraphNode_Comment.h"
#include "EdGraph/EdGraphNode_Documentation.h"
#include "Nodes/NarrativeGraphNodeBase.h"

/**
 * 
 */
class DialogueBlueprintCompiler : public FKismetCompilerContext
{
public:
    DialogueBlueprintCompiler(UDialogueBlueprint* SourceBP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& CompilerOptions)
        : FKismetCompilerContext(SourceBP, InMessageLog, CompilerOptions)
    {}

    virtual void SpawnNewClass(const FString& NewClassName) override
    {
        NewClass = NewObject<UDialogueBlueprintGeneratedClass>(GetTransientPackage(), *NewClassName, RF_Public | RF_Transient);
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
        // Only non K2Nodes are comments, documentation and narrative graph nodes, which are pure
        ensure(Node->IsA(UEdGraphNode_Comment::StaticClass()) || Node->IsA(UEdGraphNode_Documentation::StaticClass()) || Node->IsA(UNarrativeGraphNodeBase::StaticClass()));
        return true;
    }
};

class FDialogueBlueprintCompilerModule : public IBlueprintCompiler
{
public:
	virtual bool CanCompile(const UBlueprint* Blueprint) override
	{
		return Blueprint->GeneratedClass->IsChildOf(UDialogueBlueprintGeneratedClass::StaticClass());
	}

	virtual void Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results)
	{
		TSharedPtr<FKismetCompilerContext> Compiler = MakeShareable(new DialogueBlueprintCompiler(
			CastChecked<UDialogueBlueprint>(Blueprint), Results, CompileOptions));
		Compiler->Compile();
	}
    
    virtual bool GetBlueprintTypesForClass(UClass* ParentClass, UClass*& OutBlueprintClass, UClass*& OutBlueprintGeneratedClass) const
    {
		if (ParentClass->IsChildOf(UDialogue::StaticClass()))
        {
            OutBlueprintClass = UDialogueBlueprint::StaticClass();
            OutBlueprintGeneratedClass = UDialogueBlueprintGeneratedClass::StaticClass();
        }

        return true;
    }
};