// Copyright Ironic Studio. All Rights Reserved.

#include "InteractionNativeTags.h"

namespace InteractionNativeTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		ErrorUnhandledAction,
		"Interaction.Error.UnhandledAction",
		"No execution handler is bound for the selected interaction action.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(ErrorTargetBusy, "Interaction.Error.TargetBusy", "The selected target is already executing an interaction.");
}
