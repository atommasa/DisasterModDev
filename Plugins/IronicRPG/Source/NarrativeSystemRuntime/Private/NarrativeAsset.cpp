// Fill out your copyright notice in the Description page of Project Settings.


#include "NarrativeAsset.h"
#include "UObject/ObjectSaveContext.h"
#include "BlueprintActionDatabase.h"
#include "K2Node_CallFunction.h"
#include "BlueprintNodeSpawner.h"

void UNarrativeBlueprintBase::PreSave(FObjectPreSaveContext SaveContext)
{
	if (_OnPreSaveListener)
	{
		_OnPreSaveListener();
	}
}

void UNarrativeBlueprintBase::GetTypeActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	Super::GetTypeActions(ActionRegistrar);

}
