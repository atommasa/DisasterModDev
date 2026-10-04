// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "GameZoneMapIdCustomization.h"

/** Provides inline Sheet definitions and Zone-scoped Sheet references. */
class FGameZoneMapSheetIdCustomization final : public FGameZoneMapIdCustomization
{
public:
    FGameZoneMapSheetIdCustomization();

    static TSharedRef<IPropertyTypeCustomization> MakeInstance();
};
