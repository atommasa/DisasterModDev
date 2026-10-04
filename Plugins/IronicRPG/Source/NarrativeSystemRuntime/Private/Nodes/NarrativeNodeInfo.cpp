// Copyright Ironic Studio. All Rights Reserved.


#include "Nodes/NarrativeNodeInfo.h"

#if WITH_EDITOR
void UNarrativeNodeInfo::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent)
{
	Super::PostEditChangeProperty(PropertyChangedChainEvent);

	OnNodeInfoPropertyChanged.Broadcast();
}
#endif // WITH_EDITOR
