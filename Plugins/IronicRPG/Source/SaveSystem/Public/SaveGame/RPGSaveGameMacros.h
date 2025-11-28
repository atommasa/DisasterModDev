// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#define GET_SAVEMODULE_BY_NAME(SaveGameInstance, ModuleName) \
	SaveGameInstance->##ModuleName##Module