// Copyright Ironic Studio. All Rights Reserved.


#include "Nodes/NarrativeGraphNodeStyle.h"
#include "Nodes/NarrativeGraphNodeBase.h"

void SNarrativeGraphNodeBase::BindNodeInfoChanged()
{
	UNarrativeNodeInfo* NodeInfo = GetNarrativeNodeInfo();
	if (!NodeInfo)
	{
		return;
	}

	if (BoundNodeInfo.Get() == NodeInfo && NodeInfoPropertyChangedHandle.IsValid())
	{
		return;
	}

	UnbindNodeInfoChanged();

	BoundNodeInfo = NodeInfo;
	
	NodeInfoPropertyChangedHandle =
		NodeInfo->OnNodeInfoPropertyChanged.AddSP(
			SharedThis(this),
			&SNarrativeGraphNodeBase::HandleNodeInfoPropertyChanged
		);
}

void SNarrativeGraphNodeBase::UnbindNodeInfoChanged()
{
	if (BoundNodeInfo.IsValid() && NodeInfoPropertyChangedHandle.IsValid())
	{
		BoundNodeInfo->OnNodeInfoPropertyChanged.Remove(NodeInfoPropertyChangedHandle);
	}
}

UNarrativeNodeInfo* SNarrativeGraphNodeBase::GetNarrativeNodeInfo() const
{
	return GraphNode ? Cast<UNarrativeGraphNodeBase>(GraphNode)->GetNarrativeNodeInfo() : nullptr;
}