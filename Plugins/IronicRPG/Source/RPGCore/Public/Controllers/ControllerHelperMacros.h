// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputSubsystems.h"

#define DEFINE_INPUTMAPPING_FUNCTIONS(InputSubsystem, Context, Priority, ...) \
public: \
	void Enable##Context() { if (InputSubsystem) InputSubsystem->AddMappingContext(Context, Priority, ##__VA_ARGS__); } \
	void Disable##Context() { if (InputSubsystem) InputSubsystem->RemoveMappingContext(Context, ##__VA_ARGS__); }
