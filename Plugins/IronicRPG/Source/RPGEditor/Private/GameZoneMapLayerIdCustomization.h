// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "GameZoneMapIdCustomization.h"

/** Provides inline Layer definitions and Zone-scoped Layer references. */
class FGameZoneMapLayerIdCustomization final : public FGameZoneMapIdCustomization
{
public:
    FGameZoneMapLayerIdCustomization();

    static TSharedRef<IPropertyTypeCustomization> MakeInstance();
};
