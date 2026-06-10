// Copyright Ironic Studio. All Rights Reserved.


#include "NarrativeAsset.h"
#include "UObject/ObjectSaveContext.h"

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
