// Copyright Ironic Studio. All Rights Reserved.

#include "Levels/GameZoneMapDataValidation.h"

#include "Engine/Texture2D.h"

#define LOCTEXT_NAMESPACE "GameZoneMapDataValidation"

namespace
{
    constexpr double TextureAspectRatioTolerance = 0.005;

    bool DoMappingsOverlap(
        const FGameZoneMapSheetMapping& First,
        const FGameZoneMapSheetMapping& Second)
    {
        const double FirstRadians = FMath::DegreesToRadians(static_cast<double>(First.WorldYaw));
        const double SecondRadians = FMath::DegreesToRadians(static_cast<double>(Second.WorldYaw));
        const FVector2D FirstX(FMath::Cos(FirstRadians), FMath::Sin(FirstRadians));
        const FVector2D FirstY(-FirstX.Y, FirstX.X);
        const FVector2D SecondX(FMath::Cos(SecondRadians), FMath::Sin(SecondRadians));
        const FVector2D SecondY(-SecondX.Y, SecondX.X);
        const FVector2D Delta(
            Second.WorldOrigin.X - First.WorldOrigin.X,
            Second.WorldOrigin.Y - First.WorldOrigin.Y);
        const FVector2D FirstExtent = First.WorldSize * 0.5;
        const FVector2D SecondExtent = Second.WorldSize * 0.5;
        const FVector2D Axes[]{FirstX, FirstY, SecondX, SecondY};

        for (const FVector2D& Axis : Axes)
        {
            const double CenterDistance = FMath::Abs(FVector2D::DotProduct(Delta, Axis));
            const double FirstRadius =
                FMath::Abs(FVector2D::DotProduct(FirstX, Axis)) * FirstExtent.X
                + FMath::Abs(FVector2D::DotProduct(FirstY, Axis)) * FirstExtent.Y;
            const double SecondRadius =
                FMath::Abs(FVector2D::DotProduct(SecondX, Axis)) * SecondExtent.X
                + FMath::Abs(FVector2D::DotProduct(SecondY, Axis)) * SecondExtent.Y;

            if (CenterDistance >= FirstRadius + SecondRadius - UE_KINDA_SMALL_NUMBER)
            {
                return false;
            }
        }

        return true;
    }
}

EDataValidationResult ValidateGameZoneMapAssetData(
    const TConstArrayView<FGameZoneMapLayer> Layers,
    const TConstArrayView<FGameZoneMapSheet> Sheets,
    const FGameZoneMapSheetId& DefaultSheetId,
    const TConstArrayView<FGameZoneMapSheetMapping> Mappings,
    const TConstArrayView<FGameZoneMapRegion> Regions,
    FDataValidationContext& Context)
{
    TSet<FGameZoneMapLayerId> LayerIds;

    for (const FGameZoneMapLayer& Layer : Layers)
    {
        if (!Layer.LayerId.IsValid())
        {
            Context.AddError(LOCTEXT("InvalidLayerId", "Map LayerId cannot be None."));
            continue;
        }

        if (LayerIds.Contains(Layer.LayerId))
        {
            Context.AddError(FText::Format(
                LOCTEXT("DuplicateLayerId", "Map LayerId '{0}' is duplicated."),
                FText::FromString(Layer.LayerId.ToString())));
            continue;
        }

        LayerIds.Add(Layer.LayerId);
    }

    TSet<FGameZoneMapSheetId> SheetIds;

    for (const FGameZoneMapSheet& Sheet : Sheets)
    {
        if (!Sheet.SheetId.IsValid())
        {
            Context.AddError(LOCTEXT("InvalidSheetId", "Map SheetId cannot be None."));
            continue;
        }

        if (SheetIds.Contains(Sheet.SheetId))
        {
            Context.AddError(FText::Format(
                LOCTEXT("DuplicateSheetId", "Map SheetId '{0}' is duplicated."),
                FText::FromString(Sheet.SheetId.ToString())));
            continue;
        }

        SheetIds.Add(Sheet.SheetId);

        if (!LayerIds.Contains(Sheet.LayerId))
        {
            Context.AddError(FText::Format(
                LOCTEXT("MissingSheetLayer", "Map Sheet '{0}' references missing LayerId '{1}'."),
                FText::FromString(Sheet.SheetId.ToString()),
                FText::FromString(Sheet.LayerId.ToString())));
        }
    }

    if ((!Sheets.IsEmpty() || DefaultSheetId.IsValid()) && !SheetIds.Contains(DefaultSheetId))
    {
        Context.AddError(FText::Format(
            LOCTEXT("MissingDefaultSheet", "DefaultSheetId '{0}' does not reference an existing Map Sheet."),
            FText::FromString(DefaultSheetId.ToString())));
    }

    TMap<FGameZoneMapSheetId, const FGameZoneMapSheetMapping*> MappingsBySheetId;

    for (const FGameZoneMapSheetMapping& Mapping : Mappings)
    {
        if (!Mapping.SheetId.IsValid())
        {
            Context.AddError(LOCTEXT("InvalidMappingSheetId", "A Map Sheet Mapping has a None SheetId."));
            continue;
        }

        if (!SheetIds.Contains(Mapping.SheetId))
        {
            Context.AddError(FText::Format(
                LOCTEXT("MissingMappingSheet", "A Map Sheet Mapping references missing SheetId '{0}'."),
                FText::FromString(Mapping.SheetId.ToString())));
            continue;
        }

        if (MappingsBySheetId.Contains(Mapping.SheetId))
        {
            Context.AddError(FText::Format(
                LOCTEXT("DuplicateSheetMapping", "Map Sheet '{0}' has multiple Bounds-derived Mappings."),
                FText::FromString(Mapping.SheetId.ToString())));
            continue;
        }

        if (Mapping.WorldSize.X <= UE_SMALL_NUMBER || Mapping.WorldSize.Y <= UE_SMALL_NUMBER)
        {
            Context.AddError(FText::Format(
                LOCTEXT("InvalidMappingSize", "Map Sheet '{0}' has a Mapping without a positive WorldSize."),
                FText::FromString(Mapping.SheetId.ToString())));
            continue;
        }

        MappingsBySheetId.Add(Mapping.SheetId, &Mapping);
    }

    for (const FGameZoneMapSheet& Sheet : Sheets)
    {
        if (Sheet.MapTexture.IsNull())
        {
            Context.AddWarning(FText::Format(
                LOCTEXT("MissingSheetTexture", "Map Sheet '{0}' has no dedicated Texture and will use the system fallback."),
                FText::FromString(Sheet.SheetId.ToString())));
            continue;
        }

        const FGameZoneMapSheetMapping* const* Mapping = MappingsBySheetId.Find(Sheet.SheetId);
        UTexture2D* Texture = Sheet.MapTexture.LoadSynchronous();

        if (Mapping == nullptr || !IsValid(Texture))
        {
            continue;
        }

        const double TextureAspectRatio = static_cast<double>(Texture->GetSizeX()) / Texture->GetSizeY();
        const double MappingAspectRatio = (*Mapping)->WorldSize.X / (*Mapping)->WorldSize.Y;
        const double RelativeError = FMath::Abs(TextureAspectRatio / MappingAspectRatio - 1.0);

        if (RelativeError > TextureAspectRatioTolerance)
        {
            Context.AddError(FText::Format(
                LOCTEXT(
                    "TextureAspectMismatch",
                    "Map Sheet '{0}' Texture aspect ratio does not match its Mapping within the 0.5% tolerance."),
                FText::FromString(Sheet.SheetId.ToString())));
        }
    }

    TSet<FGameZoneMapRegionId> RegionIds;

    for (const FGameZoneMapRegion& Region : Regions)
    {
        if (!Region.RegionId.IsValid())
        {
            Context.AddError(LOCTEXT("InvalidRegionId", "Map RegionId cannot be None."));
        }
        else if (RegionIds.Contains(Region.RegionId))
        {
            Context.AddError(FText::Format(
                LOCTEXT("DuplicateRegionId", "Map RegionId '{0}' is duplicated."),
                FText::FromString(Region.RegionId.ToString())));
        }
        else
        {
            RegionIds.Add(Region.RegionId);
        }

        if (!SheetIds.Contains(Region.SheetId))
        {
            Context.AddError(FText::Format(
                LOCTEXT("MissingRegionSheet", "Map Region '{0}' references missing SheetId '{1}'."),
                FText::FromString(Region.RegionId.ToString()),
                FText::FromString(Region.SheetId.ToString())));
        }
    }

    return Context.GetNumErrors() == 0
        ? EDataValidationResult::Valid
        : EDataValidationResult::Invalid;
}

EDataValidationResult ValidateGameZoneMapBakeData(
    const TConstArrayView<FGameZoneMapLayer> Layers,
    const TConstArrayView<FGameZoneMapSheet> Sheets,
    const FGameZoneMapSheetId& DefaultSheetId,
    const TConstArrayView<FGameZoneMapSheetMapping> Mappings,
    const TConstArrayView<FGameZoneMapRegion> Regions,
    FDataValidationContext& Context)
{
    ValidateGameZoneMapAssetData(
        Layers,
        Sheets,
        DefaultSheetId,
        Mappings,
        Regions,
        Context);

    TMap<FGameZoneMapSheetId, int32> MappingCounts;
    TMap<FGameZoneMapSheetId, int32> RegionCounts;
    TMap<FGameZoneMapSheetId, FGameZoneMapLayerId> LayerBySheetId;

    for (const FGameZoneMapSheet& Sheet : Sheets)
    {
        LayerBySheetId.Add(Sheet.SheetId, Sheet.LayerId);
    }

    for (const FGameZoneMapSheetMapping& Mapping : Mappings)
    {
        ++MappingCounts.FindOrAdd(Mapping.SheetId);
    }

    for (const FGameZoneMapRegion& Region : Regions)
    {
        ++RegionCounts.FindOrAdd(Region.SheetId);

        if (!Region.Bounds.IsValid || Region.Planes.IsEmpty())
        {
            Context.AddError(FText::Format(
                LOCTEXT("InvalidRegionGeometry", "Map Region '{0}' does not contain valid convex geometry."),
                FText::FromString(Region.RegionId.ToString())));
        }
    }

    for (const FGameZoneMapSheet& Sheet : Sheets)
    {
        const int32 MappingCount = MappingCounts.FindRef(Sheet.SheetId);
        if (MappingCount == 0)
        {
            Context.AddError(FText::Format(
                LOCTEXT("MissingSheetBounds", "Map Sheet '{0}' does not have a Sheet Bounds actor."),
                FText::FromString(Sheet.SheetId.ToString())));
        }

        if (RegionCounts.FindRef(Sheet.SheetId) == 0)
        {
            Context.AddWarning(FText::Format(
                LOCTEXT("MissingSheetRegion", "Map Sheet '{0}' has no Region and can only be selected by Default or override."),
                FText::FromString(Sheet.SheetId.ToString())));
        }
    }

    for (int32 FirstIndex = 0; FirstIndex < Mappings.Num(); ++FirstIndex)
    {
        const FGameZoneMapLayerId* FirstLayer = LayerBySheetId.Find(Mappings[FirstIndex].SheetId);
        if (!FirstLayer)
        {
            continue;
        }

        for (int32 SecondIndex = FirstIndex + 1; SecondIndex < Mappings.Num(); ++SecondIndex)
        {
            const FGameZoneMapLayerId* SecondLayer = LayerBySheetId.Find(Mappings[SecondIndex].SheetId);
            if (SecondLayer && *FirstLayer == *SecondLayer
                && DoMappingsOverlap(Mappings[FirstIndex], Mappings[SecondIndex]))
            {
                Context.AddError(FText::Format(
                    LOCTEXT(
                        "OverlappingSheetMappings",
                        "Map Sheets '{0}' and '{1}' overlap within Layer '{2}'."),
                    FText::FromString(Mappings[FirstIndex].SheetId.ToString()),
                    FText::FromString(Mappings[SecondIndex].SheetId.ToString()),
                    FText::FromString(FirstLayer->ToString())));
            }
        }
    }

    return Context.GetNumErrors() == 0
        ? EDataValidationResult::Valid
        : EDataValidationResult::Invalid;
}

#undef LOCTEXT_NAMESPACE
