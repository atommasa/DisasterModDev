// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Compiler/RPGBlueprintCompiler.h"
#include "Blueprints/DialogueBlueprintGeneratedClass.h"
#include "NarrativeAsset.h"

/**
 * Custom compiler context for Dialogue Blueprints
 */
class FDialogueCompilerContext : public FRPGBlueprintCompilerContext
{
public:
    FDialogueCompilerContext(UDialogueBlueprint* SourceBP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& CompilerOptions)
        : FRPGBlueprintCompilerContext(SourceBP, UDialogueBlueprintGeneratedClass::StaticClass(), InMessageLog, CompilerOptions)
    {}

    virtual void PreCompile() override;

protected:
    bool EvaluateDialogueCycle(const TSet<UEdGraphNode*>& CycleNodes) const;
    bool DoesNodeHaveExitFromCycle(UEdGraphNode* PlayerOptionsNode, const TSet<UEdGraphNode*>& CycleNodes) const;

    void ValidateVariableParsingFunctionFullyMatch(const TSet<FName>& VariableNames) const;

};

/**
 * Compiler for Dialogue Blueprints
 */
class FDialogueCompiler : public IBlueprintCompiler
{
public:
    virtual bool CanCompile(const UBlueprint* Blueprint) override;
    virtual void Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results) override;
    virtual bool GetBlueprintTypesForClass(UClass* ParentClass, UClass*& OutBlueprintClass, UClass*& OutBlueprintGeneratedClass) const override;

};
