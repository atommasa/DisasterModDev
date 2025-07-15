// Fill out your copyright notice in the Description page of Project Settings.


#include "Assets/NarrativeAssetFactory.h"
#include "Assets/NarrativeAssetAction.h"
#include "Narrative/NarrativeEventBlueprintBase.h"
#include "NarrativeAsset.h"
#include "Kismet2/KismetEditorUtilities.h"
#include <Kismet2/BlueprintEditorUtils.h>
#include <NarrativeGraphSchema.h>
#include <KismetCompilerModule.h>
#include "Blueprint/DialogueBlueprintGeneratedClass.h"
#include "NarrativeEventGraphSchema.h"

UNarrativeBlueprintFactory::UNarrativeBlueprintFactory()
{
    bCreateNew = true;
    bEditAfterNew = true;

    SupportedClass = UDialogueBlueprint::StaticClass();
    ParentClass = UDialogue::StaticClass();
}

bool UNarrativeBlueprintFactory::ConfigureProperties()
{
    ParentClass = UDialogue::StaticClass();
    return true;
}

UObject* UNarrativeBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
    UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(
        ParentClass,
        InParent,
        Name,
        BPTYPE_Normal,
        CallingContext
    );
    
    if (!NewBP)
    {
        UE_LOG(LogTemp, Error, TEXT("NarrativeBlueprintFactory: Failed to create Blueprint."));
        return nullptr;
    }

    UE_LOG(LogTemp, Warning, TEXT("Created BP Class: %s"), *NewBP->GetClass()->GetName());

    return NewBP;
}
