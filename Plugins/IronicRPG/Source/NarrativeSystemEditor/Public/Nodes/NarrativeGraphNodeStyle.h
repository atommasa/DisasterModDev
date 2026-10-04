// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/RPGGraphNodeStyle.h"
#include "Nodes/NarrativeNodeInfo.h"

/**
 * 
 */
class NARRATIVESYSTEMEDITOR_API SNarrativeGraphNodeBase : public SRPGGraphNodeBase
{
protected:
	virtual void BindNodeInfoChanged() override;
	virtual void UnbindNodeInfoChanged() override;

	UNarrativeNodeInfo* GetNarrativeNodeInfo() const;

private:
	TWeakObjectPtr<UNarrativeNodeInfo> BoundNodeInfo;
	FDelegateHandle NodeInfoPropertyChangedHandle;
};
