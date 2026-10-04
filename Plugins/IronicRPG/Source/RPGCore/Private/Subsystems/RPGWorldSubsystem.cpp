// Copyright Ironic Studio. All Rights Reserved.


#include "Subsystems/RPGWorldSubsystem.h"

bool URPGWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    if (!Super::ShouldCreateSubsystem(Outer))
    {
        return false;
    }

    UClass* ThisClass = GetClass();

    if (!ThisClass)
    {
        return false;
    }

    if (ThisClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
    {
        return true;
    }

    TArray<UClass*> DerivedClasses;
    GetDerivedClasses(ThisClass, DerivedClasses, true);

    for (UClass* DerivedClass : DerivedClasses)
    {
        if (!DerivedClass)
        {
            continue;
        }

        if (DerivedClass->HasAnyClassFlags(
            CLASS_Abstract |
            CLASS_Deprecated |
            CLASS_NewerVersionExists))
        {
            continue;
        }

        const bool bIsBlueprintClass = DerivedClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint);

        if (!bIsBlueprintClass)
        {
            continue;
        }

        UE_LOG(
            LogTemp,
            Log,
            TEXT("[%s] Native subsystem suppressed by Blueprint override: %s"),
            *ThisClass->GetName(),
            *DerivedClass->GetPathName()
        );

        return false;
    }

    return true;
}
