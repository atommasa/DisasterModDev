// Copyright Ironic Studio. All Rights Reserved.


#include "Levels/RPGWorldSettings.h"

#if WITH_EDITOR

void ARPGWorldSettings::PostDuplicate(const EDuplicateMode::Type DuplicateMode)
{
    Super::PostDuplicate(DuplicateMode);

    if (DuplicateMode != EDuplicateMode::Normal || HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
    {
        return;
    }

    GameZoneId = {};
    GameZoneBindingId.Invalidate();
    MarkPackageDirty();
}

#endif

