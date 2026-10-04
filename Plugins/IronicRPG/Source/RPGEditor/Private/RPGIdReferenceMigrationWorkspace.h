// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "RPGIdReferenceMigrationApply.h"

namespace RPGIdReferenceMigrationPrivate
{
	TUniquePtr<IRPGIdReferenceMigrationApplyWorkspace> CreateProductionWorkspace(const FString& ConfigFilenameOverride = FString());
}
