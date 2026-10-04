// Copyright Ironic Studio. All Rights Reserved.


#include "Levels/MapMarkerTypeAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if WITH_EDITOR
EDataValidationResult UMapMarkerTypeAsset::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    TSet<FGameplayTag> ActionTags;

    for (const FGameZoneMarkerActionDefinition& Action : Actions)
    {
        if (!Action.ActionTag.IsValid())
        {
            Context.AddError(NSLOCTEXT("MapMarkerTypeAsset", "InvalidActionTag", "Marker action tags must be valid."));
            Result = EDataValidationResult::Invalid;
        }
        else if (ActionTags.Contains(Action.ActionTag))
        {
            Context.AddError(FText::Format(
                NSLOCTEXT("MapMarkerTypeAsset", "DuplicateActionTag", "Marker action tag '{0}' is duplicated."),
                FText::FromString(Action.ActionTag.ToString())));
            Result = EDataValidationResult::Invalid;
        }
        else
        {
            ActionTags.Add(Action.ActionTag);
        }

        if (!Action.ActionClass)
        {
            Context.AddError(NSLOCTEXT("MapMarkerTypeAsset", "MissingActionClass", "Marker actions must define an action class."));
            Result = EDataValidationResult::Invalid;
        }
        else if (Action.ActionClass->HasAnyClassFlags(CLASS_Abstract))
        {
            Context.AddError(NSLOCTEXT("MapMarkerTypeAsset", "AbstractActionClass", "Marker action classes must be concrete."));
            Result = EDataValidationResult::Invalid;
        }

        if (Action.Confirmation == EGameZoneMarkerActionConfirmation::Required && Action.ConfirmationText.IsEmpty())
        {
            Context.AddError(NSLOCTEXT("MapMarkerTypeAsset", "MissingConfirmationText", "Marker actions requiring confirmation must define confirmation text."));
            Result = EDataValidationResult::Invalid;
        }
    }

    return Result;
}
#endif
