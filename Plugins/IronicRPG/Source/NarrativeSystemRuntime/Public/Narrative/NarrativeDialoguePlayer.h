// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NarrativeDialoguePlayer.generated.h"

/**
* 
 */
UCLASS()
class NARRATIVESYSTEMRUNTIME_API UNarrativeDialoguePlayer : public UObject
{
	GENERATED_BODY()
	
public:
	void PlayDialogue(class UDialogueBlueprint* NarrativeAsset, APlayerController* PC);
	void ContinueToNextLine();
	void ChoseOptionAtIndex(int Index);
	void EndDialogue();

protected:
	void UpdateFromDialogueNode();

private:
	UPROPERTY()
	class UDialogueBlueprint* _PlayingAsset = nullptr;

	UPROPERTY()
	class UNarrativeRuntimeNode* _CurrentNode = nullptr;

	/* TODO: We need a player controller class for narrative
	UPROPERTY()
	class UNarrativeController* _
	*/
};
