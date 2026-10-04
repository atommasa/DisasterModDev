// Copyright Ironic Studio. All Rights Reserved.


#include "Levels/GameZoneAsset.h"

#include "Levels/GameZoneMapDataValidation.h"

TSet<FRPGId> UGameZoneAsset::GetStaticPointMarkerTypes() const
{
    TSet<FRPGId> Result;

    for (const TPair<FGuid, FGameZonePointData>& Pair : BakedPoints)
    {
        Result.Add(Pair.Value.MarkerTypeId);
    }

    return Result;
}

#if WITH_EDITOR

#include "Components/BoxComponent.h"
#include "Components/BrushComponent.h"
#include "EngineUtils.h"
#include "Engine/WorldInitializationValues.h"
#include "IO/IoHash.h"
#include "Internationalization/Text.h"
#include "Model.h"
#include "Misc/DataValidation.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/SavePackage.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "Levels/GameZoneMapRegionVolume.h"
#include "Levels/GameZoneMapSheetBounds.h"
#include "Levels/GameZonePointComponent.h"
#include "Levels/RPGWorldSettings.h"

namespace
{
    constexpr int32 CurrentMapDataSchemaVersion = 2;

    class FScopedGameZoneAuditWorldInitialization
    {
    public:
        explicit FScopedGameZoneAuditWorldInitialization(UWorld& InWorld)
            : World(InWorld)
        {
            if (World.bIsWorldInitialized)
            {
                return;
            }

            FWorldInitializationValues InitializationValues;
            InitializationValues.AllowAudioPlayback(false);
            InitializationValues.RequiresHitProxies(false);
            InitializationValues.ShouldSimulatePhysics(false);
            InitializationValues.EnableTraceCollision(true);
            InitializationValues.SetTransactional(false);
            InitializationValues.CreateWorldPartition(true);
            InitializationValues.CreateAISystem(false);
            InitializationValues.CreateNavigation(false);
            World.InitWorld(InitializationValues);
            World.UpdateWorldComponents(true, false);
            bOwnsInitialization = true;
        }

        ~FScopedGameZoneAuditWorldInitialization()
        {
            if (!bOwnsInitialization)
            {
                return;
            }

            World.ClearWorldComponents();
            World.CleanupWorld();
            World.SetPhysicsScene(nullptr);
        }

    private:
        UWorld& World;
        bool bOwnsInitialization = false;
    };

    struct FGameZoneMapBakeCandidate
    {
        TMap<FGuid, FGameZonePointData> Points;
        TArray<FGameZoneMapSheetMapping> Mappings;
        TArray<FGameZoneMapRegion> Regions;
    };

    void AppendToken(FString& Canonical, const FString& Value)
    {
        Canonical.Appendf(TEXT("%d:"), Value.Len());
        Canonical.Append(Value);
        Canonical.AppendChar(TEXT('|'));
    }

    template <typename ValueType>
    void AppendValue(FString& Canonical, const ValueType& Value)
    {
        AppendToken(Canonical, LexToString(Value));
    }

    void AppendText(FString& Canonical, const FText& Value)
    {
        // Package saves may rewrite localization identity; source content is the stable authored value.
        FName StringTableId;
        FTextKey StringTableKey;
        if (FTextInspector::GetTableIdAndKey(Value, StringTableId, StringTableKey))
        {
            AppendToken(Canonical, TEXT("StringTable"));
            AppendToken(Canonical, StringTableId.ToString());
            AppendToken(Canonical, StringTableKey.ToString());
        }
        else
        {
            AppendToken(Canonical, TEXT("Source"));
            AppendValue(Canonical, FTextInspector::ShouldGatherForLocalization(Value));
        }

        const FString* SourceString = FTextInspector::GetSourceString(Value);
        check(SourceString);
        AppendToken(Canonical, *SourceString);
    }

    void AppendVector(FString& Canonical, const FVector& Value)
    {
        AppendValue(Canonical, Value.X);
        AppendValue(Canonical, Value.Y);
        AppendValue(Canonical, Value.Z);
    }

    void AppendVector2D(FString& Canonical, const FVector2D& Value)
    {
        AppendValue(Canonical, Value.X);
        AppendValue(Canonical, Value.Y);
    }

    void AppendTransform(FString& Canonical, const FTransform& Value)
    {
        AppendVector(Canonical, Value.GetTranslation());
        const FQuat Rotation = Value.GetRotation();
        AppendValue(Canonical, Rotation.X);
        AppendValue(Canonical, Rotation.Y);
        AppendValue(Canonical, Rotation.Z);
        AppendValue(Canonical, Rotation.W);
        AppendVector(Canonical, Value.GetScale3D());
    }

    FString MakePointRecord(const FGuid& PointId, const FGameZonePointData& Point)
    {
        FString Record;
        AppendToken(Record, PointId.ToString(EGuidFormats::Digits));
        AppendToken(Record, Point.ZoneId.ToString());
        AppendToken(Record, Point.MarkerTypeId.ToString());

        TArray<FGameplayTag> Tags;
        Point.MarkerTags.GetGameplayTagArray(Tags);
        Tags.Sort([](const FGameplayTag& First, const FGameplayTag& Second)
        {
            return First.GetTagName().LexicalLess(Second.GetTagName());
        });
        AppendValue(Record, Tags.Num());
        for (const FGameplayTag& Tag : Tags)
        {
            AppendToken(Record, Tag.ToString());
        }

        AppendValue(Record, static_cast<uint8>(Point.MarkerState));
        AppendValue(Record, Point.DisplayMode);
        AppendValue(Record, static_cast<uint8>(Point.SavePolicy));
        AppendText(Record, Point.DisplayName);
        AppendText(Record, Point.Description);
        AppendToken(Record, Point.ReferenceAssetId.ToString());
        AppendTransform(Record, Point.WorldTransform);
        return Record;
    }

    FString MakeMappingRecord(const FGameZoneMapSheetMapping& Mapping)
    {
        FString Record;
        AppendToken(Record, Mapping.SheetId.ToString());
        AppendVector(Record, Mapping.WorldOrigin);
        AppendVector2D(Record, Mapping.WorldSize);
        AppendValue(Record, Mapping.WorldYaw);
        return Record;
    }

    FString MakeRegionRecord(const FGameZoneMapRegion& Region)
    {
        FString Record;
        AppendToken(Record, Region.RegionId.ToString());
        AppendToken(Record, Region.SheetId.ToString());
        AppendValue(Record, Region.Priority);
        AppendValue(Record, Region.Bounds.IsValid != 0);
        AppendVector(Record, Region.Bounds.Min);
        AppendVector(Record, Region.Bounds.Max);

        TArray<FString> PlaneRecords;
        PlaneRecords.Reserve(Region.Planes.Num());
        for (const FPlane& Plane : Region.Planes)
        {
            FString PlaneRecord;
            AppendValue(PlaneRecord, Plane.X);
            AppendValue(PlaneRecord, Plane.Y);
            AppendValue(PlaneRecord, Plane.Z);
            AppendValue(PlaneRecord, Plane.W);
            PlaneRecords.Add(MoveTemp(PlaneRecord));
        }
        PlaneRecords.Sort();
        AppendValue(Record, PlaneRecords.Num());
        for (const FString& PlaneRecord : PlaneRecords)
        {
            AppendToken(Record, PlaneRecord);
        }
        return Record;
    }

    FString MakeOutputCanonical(
        const TMap<FGuid, FGameZonePointData>& Points,
        const TConstArrayView<FGameZoneMapSheetMapping> Mappings,
        const TConstArrayView<FGameZoneMapRegion> Regions)
    {
        TArray<FString> Records;
        Records.Reserve(Points.Num() + Mappings.Num() + Regions.Num());
        for (const TPair<FGuid, FGameZonePointData>& Pair : Points)
        {
            Records.Add(TEXT("P") + MakePointRecord(Pair.Key, Pair.Value));
        }
        for (const FGameZoneMapSheetMapping& Mapping : Mappings)
        {
            Records.Add(TEXT("M") + MakeMappingRecord(Mapping));
        }
        for (const FGameZoneMapRegion& Region : Regions)
        {
            Records.Add(TEXT("R") + MakeRegionRecord(Region));
        }
        Records.Sort();

        FString Canonical;
        for (const FString& Record : Records)
        {
            AppendToken(Canonical, Record);
        }
        return Canonical;
    }

    FString MakeInputCanonical(
        const UGameZoneAsset& ZoneAsset,
        const FString& OutputCanonical)
    {
        FString Canonical;
        AppendValue(Canonical, CurrentMapDataSchemaVersion);
        AppendToken(Canonical, ZoneAsset.GetId().ToString());
        AppendToken(Canonical, ZoneAsset.GetLevelToLoad().ToSoftObjectPath().ToString());
        AppendToken(Canonical, ZoneAsset.GetDefaultSheetId().ToString());

        TArray<FString> LayerRecords;
        for (const FGameZoneMapLayer& Layer : ZoneAsset.GetMapLayers())
        {
            FString Record;
            AppendToken(Record, Layer.LayerId.ToString());
            AppendText(Record, Layer.DisplayName);
            AppendValue(Record, Layer.SortOrder);
            AppendValue(Record, Layer.ElevationOrder);
            LayerRecords.Add(MoveTemp(Record));
        }
        LayerRecords.Sort();
        AppendToken(Canonical, TEXT("Layers"));
        AppendValue(Canonical, LayerRecords.Num());
        for (const FString& Record : LayerRecords)
        {
            AppendToken(Canonical, Record);
        }

        TArray<FString> SheetRecords;
        for (const FGameZoneMapSheet& Sheet : ZoneAsset.GetMapSheets())
        {
            FString Record;
            AppendToken(Record, Sheet.SheetId.ToString());
            AppendToken(Record, Sheet.LayerId.ToString());
            AppendText(Record, Sheet.DisplayName);
            AppendValue(Record, Sheet.SortOrder);
            AppendToken(Record, Sheet.MapTexture.ToSoftObjectPath().ToString());
            SheetRecords.Add(MoveTemp(Record));
        }
        SheetRecords.Sort();
        AppendToken(Canonical, TEXT("Sheets"));
        AppendValue(Canonical, SheetRecords.Num());
        for (const FString& Record : SheetRecords)
        {
            AppendToken(Canonical, Record);
        }

        AppendToken(Canonical, OutputCanonical);
        return Canonical;
    }

    FString HashCanonical(const FString& Canonical)
    {
        const FTCHARToUTF8 Utf8(*Canonical);
        return LexToString(FIoHash::HashBuffer(Utf8.Get(), Utf8.Length()));
    }

    void AddBakeError(FDataValidationContext& Context, const FString& Message)
    {
        Context.AddError(FText::FromString(Message));
    }

    bool IsBakeSourceActor(const AActor& Actor)
    {
        if (Actor.IsA<AGameZoneMapSheetBounds>() || Actor.IsA<AGameZoneMapRegionVolume>())
        {
            return true;
        }

        TInlineComponentArray<UGameZonePointComponent*> PointComponents;
        Actor.GetComponents(PointComponents);
        return PointComponents.ContainsByPredicate([](const UGameZonePointComponent* Component)
        {
            return IsValid(Component) && Component->ShouldBakeMarker();
        });
    }

    bool HasUnsavedBakeSourcePackage(const UWorld& World, FString& OutActorPath)
    {
        for (TActorIterator<AActor> ActorIt(&World); ActorIt; ++ActorIt)
        {
            const AActor* Actor = *ActorIt;
            if (!IsValid(Actor) || Actor->IsTemplate() || !IsBakeSourceActor(*Actor))
            {
                continue;
            }

            const UPackage* Package = Actor->GetOutermost();
            if (!Package || Package->IsDirty() || !FPackageName::DoesPackageExist(Package->GetName()))
            {
                OutActorPath = Actor->GetPathName();
                return true;
            }
        }
        return false;
    }

    void CollectMarkerCandidates(
        UWorld& World,
        const FRPGId& ZoneId,
        FGameZoneMapBakeCandidate& Candidate,
        FDataValidationContext& Context)
    {
        for (TActorIterator<AActor> ActorIt(&World); ActorIt; ++ActorIt)
        {
            AActor* Actor = *ActorIt;
            if (!IsValid(Actor) || Actor->IsTemplate())
            {
                continue;
            }

            TInlineComponentArray<UGameZonePointComponent*> PointComponents;
            Actor->GetComponents(PointComponents);

            if (PointComponents.ContainsByPredicate([](const UGameZonePointComponent* Component)
                {
                    return IsValid(Component) && Component->ShouldBakeMarker();
                })
                && Actor->GetExternalDataLayerAsset())
            {
                AddBakeError(
                    Context,
                    FString::Printf(
                        TEXT("Bakeable marker Actor '%s' cannot belong to an External Data Layer."),
                        *GetNameSafe(Actor)));
                continue;
            }

            for (UGameZonePointComponent* PointComponent : PointComponents)
            {
                if (!IsValid(PointComponent) || !PointComponent->ShouldBakeMarker())
                {
                    continue;
                }

                const FGuid PointId = PointComponent->GetPointId();
                if (!PointId.IsValid())
                {
                    AddBakeError(
                        Context,
                        FString::Printf(
                            TEXT("Marker on Actor '%s' has an invalid PointId."),
                            *GetNameSafe(Actor)));
                    continue;
                }

                if (Candidate.Points.Contains(PointId))
                {
                    AddBakeError(
                        Context,
                        FString::Printf(
                            TEXT("PointId '%s' is duplicated by Actor '%s'."),
                            *PointId.ToString(),
                            *GetNameSafe(Actor)));
                    continue;
                }

                FGameZonePointData MarkerData = PointComponent->MakePointSnapshot();
                MarkerData.ZoneId = ZoneId;
                MarkerData.WorldTransform = Actor->GetActorTransform();

                if (!MarkerData.MarkerTypeId.IsValid())
                {
                    AddBakeError(
                        Context,
                        FString::Printf(
                            TEXT("Marker on Actor '%s' has an invalid MarkerTypeId."),
                            *GetNameSafe(Actor)));
                    continue;
                }

                Candidate.Points.Add(PointId, MoveTemp(MarkerData));
            }
        }
    }

    void CollectMappingCandidates(
        UWorld& World,
        FGameZoneMapBakeCandidate& Candidate,
        FDataValidationContext& Context)
    {
        for (TActorIterator<AGameZoneMapSheetBounds> ActorIt(&World); ActorIt; ++ActorIt)
        {
            AGameZoneMapSheetBounds* BoundsActor = *ActorIt;
            if (!IsValid(BoundsActor) || BoundsActor->IsTemplate())
            {
                continue;
            }

            const FVector ActorScale = BoundsActor->GetActorScale3D();
            const FRotator ActorRotation = BoundsActor->GetActorRotation();
            const bool bHasPitchOrRoll =
                !FMath::IsNearlyZero(FRotator::NormalizeAxis(ActorRotation.Pitch), UE_KINDA_SMALL_NUMBER)
                || !FMath::IsNearlyZero(FRotator::NormalizeAxis(ActorRotation.Roll), UE_KINDA_SMALL_NUMBER);

            if (!ActorScale.Equals(FVector::OneVector, UE_KINDA_SMALL_NUMBER) || bHasPitchOrRoll)
            {
                AddBakeError(
                    Context,
                    FString::Printf(
                        TEXT("Sheet Bounds '%s' must use unit Scale and zero Pitch/Roll."),
                        *GetNameSafe(BoundsActor)));
                continue;
            }

            const UBoxComponent* BoundsComponent = BoundsActor->GetBoundsComponent();
            const FVector BoxExtent = IsValid(BoundsComponent)
                ? BoundsComponent->GetUnscaledBoxExtent()
                : FVector::ZeroVector;

            if (BoxExtent.X <= UE_SMALL_NUMBER || BoxExtent.Y <= UE_SMALL_NUMBER)
            {
                AddBakeError(
                    Context,
                    FString::Printf(
                        TEXT("Sheet Bounds '%s' must have positive X/Y Box Extents."),
                        *GetNameSafe(BoundsActor)));
                continue;
            }

            FGameZoneMapSheetMapping& Mapping = Candidate.Mappings.AddDefaulted_GetRef();
            Mapping.SheetId = BoundsActor->SheetId;
            Mapping.WorldOrigin = BoundsActor->GetActorLocation();
            Mapping.WorldSize = FVector2D(BoxExtent.X * 2.0, BoxExtent.Y * 2.0);
            Mapping.WorldYaw = FRotator::NormalizeAxis(ActorRotation.Yaw);
        }
    }

    const FGameZoneMapSheetMapping* FindMapping(
        const FGameZoneMapBakeCandidate& Candidate,
        const FGameZoneMapSheetId& SheetId)
    {
        return Candidate.Mappings.FindByPredicate(
            [&SheetId](const FGameZoneMapSheetMapping& Mapping)
            {
                return Mapping.SheetId == SheetId;
            });
    }

    void CollectRegionCandidates(
        UWorld& World,
        FGameZoneMapBakeCandidate& Candidate,
        FDataValidationContext& Context)
    {
        for (TActorIterator<AGameZoneMapRegionVolume> ActorIt(&World); ActorIt; ++ActorIt)
        {
            AGameZoneMapRegionVolume* RegionActor = *ActorIt;
            if (!IsValid(RegionActor) || RegionActor->IsTemplate())
            {
                continue;
            }

            UBrushComponent* BrushComponent = RegionActor->GetBrushComponent();
            UModel* BrushModel = IsValid(BrushComponent) ? BrushComponent->Brush : nullptr;
            const UBodySetup* BodySetup = IsValid(BrushComponent) ? BrushComponent->BrushBodySetup : nullptr;

            if (!IsValid(BrushModel) || !IsValid(BodySetup)
                || BodySetup->AggGeom.ConvexElems.Num() != 1)
            {
                AddBakeError(
                    Context,
                    FString::Printf(
                        TEXT("Map Region '%s' must use one convex Brush."),
                        *GetNameSafe(RegionActor)));
                continue;
            }

            FGameZoneMapRegion Region;
            Region.RegionId = RegionActor->RegionId;
            Region.SheetId = RegionActor->SheetId;
            Region.Priority = RegionActor->Priority;
            Region.Bounds = BrushComponent->CalcBounds(BrushComponent->GetComponentTransform()).GetBox();
            BrushModel->GetSurfacePlanes(RegionActor, Region.Planes);

            const FGameZoneMapSheetMapping* Mapping = FindMapping(Candidate, Region.SheetId);
            if (Mapping)
            {
                for (const FVector3f& LocalPoint : BrushModel->Points)
                {
                    const FVector WorldPoint = RegionActor->GetActorTransform().TransformPosition(FVector(LocalPoint));
                    FGameZoneMapProjection Projection;
                    if (!Mapping->ProjectWorldLocation(WorldPoint, Projection) || !Projection.bIsInsideSheet)
                    {
                        AddBakeError(
                            Context,
                            FString::Printf(
                                TEXT("Map Region '%s' extends outside Sheet Mapping '%s'."),
                                *GetNameSafe(RegionActor),
                                *Region.SheetId.ToString()));
                        break;
                    }
                }
            }

            Candidate.Regions.Add(MoveTemp(Region));
        }
    }

    void LogBakeIssues(const UGameZoneAsset& ZoneAsset, const FDataValidationContext& Context)
    {
        for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
        {
            if (Issue.Severity == EMessageSeverity::Error)
            {
                UE_LOG(LogTemp, Error, TEXT("[MapBake] Zone '%s': %s"), *ZoneAsset.GetName(), *Issue.Message.ToString());
            }
            else if (Issue.Severity == EMessageSeverity::Warning
                || Issue.Severity == EMessageSeverity::PerformanceWarning)
            {
                UE_LOG(LogTemp, Warning, TEXT("[MapBake] Zone '%s': %s"), *ZoneAsset.GetName(), *Issue.Message.ToString());
            }
        }
    }
}

FGameZoneMapBakeAuditResult UGameZoneAsset::AuditMapDataForRelease() const
{
    FGameZoneMapBakeAuditResult Result;
    const auto AddIssue = [&Result](const FString& Message)
    {
        Result.Issues.Add(FText::FromString(Message));
    };
    const auto FinalizeEarlyIssueFingerprint = [this, &Result]()
    {
        TArray<FString> IssueStrings;
        IssueStrings.Reserve(Result.Issues.Num());
        for (const FText& Issue : Result.Issues)
        {
            IssueStrings.Add(Issue.ToString());
        }
        IssueStrings.Sort();

        FString ProblemIdentity;
        AppendToken(ProblemIdentity, GetPathName());
        for (const FString& Issue : IssueStrings)
        {
            AppendToken(ProblemIdentity, Issue);
        }
        Result.IssueFingerprint = HashCanonical(ProblemIdentity);
    };

    if (!GetId().IsValid())
    {
        AddIssue(FString::Printf(TEXT("Zone Asset '%s' has no valid Id."), *GetPathName()));
    }
    if (LevelToLoad.IsNull())
    {
        AddIssue(FString::Printf(TEXT("Zone Asset '%s' has no LevelToLoad."), *GetPathName()));
    }
    if (!Result.Issues.IsEmpty())
    {
        FinalizeEarlyIssueFingerprint();
        return Result;
    }

    UWorld* TargetWorld = LevelToLoad.LoadSynchronous();
    if (!IsValid(TargetWorld))
    {
        AddIssue(FString::Printf(
            TEXT("Zone Asset '%s' could not load Level '%s'."),
            *GetPathName(),
            *LevelToLoad.ToSoftObjectPath().ToString()));
        FinalizeEarlyIssueFingerprint();
        return Result;
    }

    FScopedGameZoneAuditWorldInitialization AuditWorldInitialization(*TargetWorld);

    const FGameZoneBindingAuditResult BindingAudit = AuditGameZoneBindingAgainstWorld(*this, *TargetWorld);
    if (!BindingAudit.IsVerified())
    {
        AddIssue(FString::Printf(
            TEXT("Zone Asset '%s' has a non-exact Level binding: %s"),
            *GetPathName(),
            *BindingAudit.Message.ToString()));
    }

    const UPackage* SourcePackage = TargetWorld->GetOutermost();
    Result.bHasUnsavedLevelChanges = !SourcePackage
        || SourcePackage->IsDirty()
        || !FPackageName::DoesPackageExist(SourcePackage->GetName());

    bool bWorldSourceComplete = true;
    TArray<FWorldPartitionReference> LoadedActorReferences;
    if (UWorldPartition* WorldPartition = TargetWorld->GetWorldPartition())
    {
        if (!WorldPartition->IsInitialized())
        {
            bWorldSourceComplete = false;
            AddIssue(FString::Printf(
                TEXT("Level '%s' cannot be audited because World Partition is not initialized."),
                *TargetWorld->GetPathName()));
        }
        else
        {
            WorldPartition->LoadAllActors(LoadedActorReferences);
            if (LoadedActorReferences.ContainsByPredicate([](const FWorldPartitionReference& Reference)
                {
                    return !Reference.IsLoaded() || !IsValid(Reference.GetActor());
                }))
            {
                bWorldSourceComplete = false;
                AddIssue(FString::Printf(
                    TEXT("Level '%s' cannot be audited because a World Partition actor failed to load."),
                    *TargetWorld->GetPathName()));
            }
        }
    }

    FString UnsavedActorPath;
    if (HasUnsavedBakeSourcePackage(*TargetWorld, UnsavedActorPath))
    {
        Result.bHasUnsavedLevelChanges = true;
        AddIssue(FString::Printf(TEXT("Bake source Actor '%s' has unsaved changes."), *UnsavedActorPath));
    }

    FGameZoneMapBakeCandidate Candidate;
    FDataValidationContext ValidationContext;
    CollectMarkerCandidates(*TargetWorld, GetId(), Candidate, ValidationContext);
    CollectMappingCandidates(*TargetWorld, Candidate, ValidationContext);
    CollectRegionCandidates(*TargetWorld, Candidate, ValidationContext);
    const EDataValidationResult ValidationResult = ValidateGameZoneMapBakeData(
        MapLayers,
        MapSheets,
        DefaultSheetId,
        Candidate.Mappings,
        Candidate.Regions,
        ValidationContext);
    for (const FDataValidationContext::FIssue& Issue : ValidationContext.GetIssues())
    {
        if (Issue.Severity == EMessageSeverity::Error)
        {
            Result.Issues.Add(Issue.Message);
        }
    }

    const FString CandidateOutputCanonical = MakeOutputCanonical(
        Candidate.Points,
        Candidate.Mappings,
        Candidate.Regions);
    const FString CandidateOutputFingerprint = HashCanonical(CandidateOutputCanonical);
    const FString CandidateInputFingerprint = HashCanonical(MakeInputCanonical(*this, CandidateOutputCanonical));
    const FString ExistingOutputFingerprint = HashCanonical(MakeOutputCanonical(
        BakedPoints,
        BakedSheetMappings,
        BakedMapRegions));

#if WITH_EDITORONLY_DATA
    Result.bHasUnsavedAssetChanges = GetOutermost()->IsDirty();
    Result.bSnapshotMatches = BindingVerificationStatus == EGameZoneBindingVerificationStatus::Verified
        && MapDataSchemaVersion == CurrentMapDataSchemaVersion
        && MapBakeRevision.IsValid()
        && BakedSourceLevel == LevelToLoad.ToSoftObjectPath()
        && MapBakeInputFingerprint == CandidateInputFingerprint
        && MapBakeOutputFingerprint == CandidateOutputFingerprint
        && MapBakeOutputFingerprint == ExistingOutputFingerprint;
#endif
    Result.bCanEvaluate = BindingAudit.IsVerified()
        && ValidationResult != EDataValidationResult::Invalid
        && bWorldSourceComplete;

    if (Result.bHasUnsavedLevelChanges)
    {
        AddIssue(FString::Printf(TEXT("Level '%s' has unsaved in-memory authoring changes."), *TargetWorld->GetPathName()));
    }
    if (Result.bHasUnsavedAssetChanges)
    {
        AddIssue(FString::Printf(TEXT("Zone Asset '%s' has unsaved in-memory changes."), *GetPathName()));
    }
    if (!Result.bSnapshotMatches)
    {
        AddIssue(Result.bHasUnsavedLevelChanges || Result.bHasUnsavedAssetChanges
            ? FString::Printf(TEXT("The in-memory Bake for Zone Asset '%s' is not persistently verified."), *GetPathName())
            : FString::Printf(TEXT("The saved Bake for Zone Asset '%s' is stale or invalid."), *GetPathName()));
    }

    TArray<FString> IssueStrings;
    IssueStrings.Reserve(Result.Issues.Num());
    for (const FText& Issue : Result.Issues)
    {
        IssueStrings.Add(Issue.ToString());
    }
    IssueStrings.Sort();

    FString ProblemIdentity;
    AppendToken(ProblemIdentity, GetPathName());
    AppendToken(ProblemIdentity, CandidateInputFingerprint);
    AppendToken(ProblemIdentity, CandidateOutputFingerprint);
    AppendToken(ProblemIdentity, ExistingOutputFingerprint);
    AppendValue(ProblemIdentity, Result.bHasUnsavedLevelChanges);
    AppendValue(ProblemIdentity, Result.bHasUnsavedAssetChanges);
    for (const FString& Issue : IssueStrings)
    {
        AppendToken(ProblemIdentity, Issue);
    }
    Result.IssueFingerprint = HashCanonical(ProblemIdentity);
    return Result;
}

FGameZoneMapBakeAuditResult UGameZoneAsset::AuditMapDataForPIE() const
{
    return AuditMapDataForRelease();
}

EDataValidationResult UGameZoneAsset::IsDataValid(FDataValidationContext& Context) const
{
    const EDataValidationResult SuperResult = Super::IsDataValid(Context);
    const EDataValidationResult MapResult = ValidateGameZoneMapAssetData(
        MapLayers,
        MapSheets,
        DefaultSheetId,
        BakedSheetMappings,
        BakedMapRegions,
        Context);
    return CombineDataValidationResults(SuperResult, MapResult);
}

void UGameZoneAsset::BakeMapData()
{
    BakeMapDataWithResult();
}

EGameZoneMapBakeResult UGameZoneAsset::BakeMapDataWithResult(const bool bRequireSavedSource)
{
    const auto FailBake = [this](const FString& Message, const bool bLogAsError = true)
    {
        if (bLogAsError)
        {
            UE_LOG(LogTemp, Error, TEXT("[MapBake] %s"), *Message);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[MapBake] %s"), *Message);
        }
#if WITH_EDITORONLY_DATA
        if (BindingVerificationStatus != EGameZoneBindingVerificationStatus::BakeFailed)
        {
            Modify();
            BindingVerificationStatus = EGameZoneBindingVerificationStatus::BakeFailed;
            MarkPackageDirty();
        }
#endif
        return EGameZoneMapBakeResult::Failed;
    };

    if (!GetId().IsValid())
    {
        return FailBake(FString::Printf(TEXT("Zone Asset '%s' has no valid Id."), *GetName()));
    }

    if (LevelToLoad.IsNull())
    {
        return FailBake(FString::Printf(TEXT("Zone Asset '%s' has no LevelToLoad."), *GetName()));
    }

    UWorld* TargetWorld = LevelToLoad.LoadSynchronous();

    if (!IsValid(TargetWorld))
    {
        return FailBake(FString::Printf(
            TEXT("Failed to load world for ZoneAsset '%s': %s"),
            *GetName(),
            *LevelToLoad.ToSoftObjectPath().ToString()));
    }

    const FGameZoneBindingAuditResult BindingAudit = AuditGameZoneBindingAgainstWorld(*this, *TargetWorld);
    if (!BindingAudit.IsVerified())
    {
        return FailBake(FString::Printf(
            TEXT("Zone Asset '%s' cannot Bake from a non-exact Level binding: %s"),
            *GetName(),
            *BindingAudit.Message.ToString()));
    }

    UPackage* SourcePackage = TargetWorld->GetOutermost();
    if (bRequireSavedSource
        && (!SourcePackage
            || SourcePackage->IsDirty()
            || !FPackageName::DoesPackageExist(SourcePackage->GetName())))
    {
        return FailBake(FString::Printf(
            TEXT("Zone Asset '%s' cannot persist Bake data because Level '%s' is dirty or has never been saved."),
            *GetName(),
            *TargetWorld->GetPathName()));
    }

    TArray<FWorldPartitionReference> LoadedActorReferences;
    if (UWorldPartition* WorldPartition = TargetWorld->GetWorldPartition())
    {
        if (!WorldPartition->IsInitialized())
        {
            return FailBake(FString::Printf(
                TEXT("Zone Asset '%s' cannot Bake because World Partition is not initialized."),
                *GetName()));
        }
        WorldPartition->LoadAllActors(LoadedActorReferences);
        if (LoadedActorReferences.ContainsByPredicate([](const FWorldPartitionReference& Reference)
            {
                return !Reference.IsLoaded() || !IsValid(Reference.GetActor());
            }))
        {
            return FailBake(FString::Printf(
                TEXT("Zone Asset '%s' cannot Bake because at least one World Partition actor failed to load."),
                *GetName()));
        }
    }

    FString UnsavedActorPath;
    if (bRequireSavedSource && HasUnsavedBakeSourcePackage(*TargetWorld, UnsavedActorPath))
    {
        return FailBake(FString::Printf(
            TEXT("Zone Asset '%s' cannot persist Bake data because source Actor '%s' is unsaved."),
            *GetName(),
            *UnsavedActorPath));
    }

    FGameZoneMapBakeCandidate Candidate;
    FDataValidationContext ValidationContext;
    CollectMarkerCandidates(*TargetWorld, GetId(), Candidate, ValidationContext);
    CollectMappingCandidates(*TargetWorld, Candidate, ValidationContext);
    CollectRegionCandidates(*TargetWorld, Candidate, ValidationContext);

    const EDataValidationResult ValidationResult = ValidateGameZoneMapBakeData(
        MapLayers,
        MapSheets,
        DefaultSheetId,
        Candidate.Mappings,
        Candidate.Regions,
        ValidationContext);
    LogBakeIssues(*this, ValidationContext);

    if (ValidationResult == EDataValidationResult::Invalid)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MapBake] Zone '%s' was not modified because validation failed."), *GetName());
        return FailBake(FString::Printf(
            TEXT("Zone '%s' kept its last valid runtime snapshot because validation failed."),
            *GetName()), false);
    }

    const FString CandidateOutputCanonical = MakeOutputCanonical(
        Candidate.Points,
        Candidate.Mappings,
        Candidate.Regions);
    const FString CandidateOutputFingerprint = HashCanonical(CandidateOutputCanonical);
    const FString CandidateInputFingerprint = HashCanonical(MakeInputCanonical(*this, CandidateOutputCanonical));
    const FString ExistingOutputFingerprint = HashCanonical(MakeOutputCanonical(
        BakedPoints,
        BakedSheetMappings,
        BakedMapRegions));

#if WITH_EDITORONLY_DATA
    const bool bSnapshotMatches = MapDataSchemaVersion == CurrentMapDataSchemaVersion
        && MapBakeRevision.IsValid()
        && BakedSourceLevel == LevelToLoad.ToSoftObjectPath()
        && MapBakeInputFingerprint == CandidateInputFingerprint
        && MapBakeOutputFingerprint == CandidateOutputFingerprint
        && MapBakeOutputFingerprint == ExistingOutputFingerprint;
    if (bSnapshotMatches)
    {
        if (BindingVerificationStatus != EGameZoneBindingVerificationStatus::Verified)
        {
            Modify();
            BindingVerificationStatus = EGameZoneBindingVerificationStatus::Verified;
            MarkPackageDirty();
        }
        UE_LOG(LogTemp, Verbose, TEXT("[MapBake] Zone '%s' is already current."), *GetName());
        return EGameZoneMapBakeResult::NoChange;
    }
#endif

    Modify();
    BakedPoints = MoveTemp(Candidate.Points);
    BakedSheetMappings = MoveTemp(Candidate.Mappings);
    BakedMapRegions = MoveTemp(Candidate.Regions);
    MapDataSchemaVersion = CurrentMapDataSchemaVersion;
    MapBakeRevision = FGuid::NewGuid();
#if WITH_EDITORONLY_DATA
    BakedSourceLevel = LevelToLoad.ToSoftObjectPath();
    MapBakeInputFingerprint = CandidateInputFingerprint;
    MapBakeOutputFingerprint = CandidateOutputFingerprint;
    BindingVerificationStatus = EGameZoneBindingVerificationStatus::Verified;
#endif

    MarkPackageDirty();

    UE_LOG(
        LogTemp,
        Log,
        TEXT("[MapBake] Zone '%s' baked %d Markers, %d Mappings, and %d Regions."),
        *GetName(),
        BakedPoints.Num(),
        BakedSheetMappings.Num(),
        BakedMapRegions.Num());
    return EGameZoneMapBakeResult::Succeeded;
}

void UGameZoneAsset::MigrateLegacyBinding()
{
    FGameZoneBindingPropertyChangeRequest Request;
    Request.ZoneAsset = this;
    Request.Change = EGameZoneBindingPropertyChange::LevelToLoad;
    Request.PreviousLevel = LevelToLoad.ToSoftObjectPath();
    Request.PreviousZoneId = Id;
    Request.PreviousBindingId = GameZoneBindingId;
#if WITH_EDITORONLY_DATA
    Request.PreviousVerificationStatus = BindingVerificationStatus;
#endif

    FApplyGameZoneBindingPropertyChange& Delegate = GetApplyGameZoneBindingPropertyChangeDelegate();
    if (!Delegate.IsBound())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Legacy binding migration for '%s' requires the RPGEditor coordinator."),
            *GetPathName());
        return;
    }
    Delegate.Execute(Request);
}

void UGameZoneAsset::PostLoad()
{
    Super::PostLoad();

#if WITH_EDITORONLY_DATA
    if (MapBakeRevision.IsValid()
        && MapBakeInputFingerprint.IsEmpty()
        && BindingVerificationStatus == EGameZoneBindingVerificationStatus::Verified)
    {
        BindingVerificationStatus = EGameZoneBindingVerificationStatus::LegacyUnverified;
    }
#endif

    UWorld* LoadedWorld = LevelToLoad.Get();
    if (!LoadedWorld)
    {
        return;
    }

    const FGameZoneBindingAuditResult Audit = AuditGameZoneBindingAgainstWorld(*this, *LoadedWorld);
    if (!Audit.IsVerified())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("GameZone binding PostLoad audit for '%s': %s"),
            *GetPathName(),
            *Audit.Message.ToString());
    }
}

void UGameZoneAsset::PostDuplicate(const EDuplicateMode::Type DuplicateMode)
{
    Super::PostDuplicate(DuplicateMode);

    if (DuplicateMode != EDuplicateMode::Normal || HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
    {
        return;
    }

    Id = {};
    LevelToLoad.Reset();
    GameZoneBindingId.Invalidate();
    BakedPoints.Reset();
    BakedSheetMappings.Reset();
    BakedMapRegions.Reset();
    MapDataSchemaVersion = 0;
    MapBakeRevision.Invalidate();
#if WITH_EDITORONLY_DATA
    BindingVerificationStatus = EGameZoneBindingVerificationStatus::LegacyUnverified;
    BakedSourceLevel.Reset();
    MapBakeInputFingerprint.Reset();
    MapBakeOutputFingerprint.Reset();
#endif
    MarkPackageDirty();
}

void UGameZoneAsset::PreSave(FObjectPreSaveContext SaveContext)
{
    if (!SaveContext.IsCooking()
        && !SaveContext.IsProceduralSave()
        && !SaveContext.IsFromAutoSave()
        && !IsRunningCommandlet()
        && !LevelToLoad.IsNull()
        && GameZoneBindingId.IsValid())
    {
        BakeMapDataWithResult(true);
    }

    Super::PreSave(SaveContext);
}

void UGameZoneAsset::PreEditChange(FProperty* PropertyAboutToChange)
{
    const FName PropertyName = PropertyAboutToChange ? PropertyAboutToChange->GetFName() : NAME_None;

    if (PropertyName == GET_MEMBER_NAME_CHECKED(UGameZoneAsset, LevelToLoad)
        || PropertyName == TEXT("Id"))
    {
        CachedLevelToLoad = LevelToLoad.ToSoftObjectPath();
        CachedZoneId = Id;
        CachedGameZoneBindingId = GameZoneBindingId;
#if WITH_EDITORONLY_DATA
        CachedBindingVerificationStatus = BindingVerificationStatus;
#endif
    }

    Super::PreEditChange(PropertyAboutToChange);
}

void UGameZoneAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    const FName MemberPropertyName = PropertyChangedEvent.GetMemberPropertyName();
    const bool bLevelChanged = PropertyName == GET_MEMBER_NAME_CHECKED(UGameZoneAsset, LevelToLoad);
    const bool bZoneIdChanged = PropertyName == TEXT("Id");
    bool bBindingChangeSucceeded = false;

    if (bLevelChanged || bZoneIdChanged)
    {
        FGameZoneBindingPropertyChangeRequest Request;
        Request.ZoneAsset = this;
        Request.Change = bLevelChanged
            ? EGameZoneBindingPropertyChange::LevelToLoad
            : EGameZoneBindingPropertyChange::ZoneId;
        Request.PreviousLevel = CachedLevelToLoad;
        Request.PreviousZoneId = CachedZoneId;
        Request.PreviousBindingId = CachedGameZoneBindingId;
#if WITH_EDITORONLY_DATA
        Request.PreviousVerificationStatus = CachedBindingVerificationStatus;
#endif

        FApplyGameZoneBindingPropertyChange& Delegate = GetApplyGameZoneBindingPropertyChangeDelegate();
        if (Delegate.IsBound())
        {
            bBindingChangeSucceeded = Delegate.Execute(Request);
        }
        else if (bLevelChanged || !CachedLevelToLoad.IsNull() || CachedGameZoneBindingId.IsValid())
        {
            LevelToLoad = TSoftObjectPtr<UWorld>(CachedLevelToLoad);
            Id = CachedZoneId;
            GameZoneBindingId = CachedGameZoneBindingId;
#if WITH_EDITORONLY_DATA
            BindingVerificationStatus = CachedBindingVerificationStatus;
#endif
            UE_LOG(
                LogTemp,
                Error,
                TEXT("GameZone binding change for '%s' was reverted because RPGEditor coordinator is unavailable."),
                *GetPathName());
        }
    }

#if WITH_EDITORONLY_DATA
    const bool bMapAuthoringChanged = MemberPropertyName == GET_MEMBER_NAME_CHECKED(UGameZoneAsset, MapLayers)
        || MemberPropertyName == GET_MEMBER_NAME_CHECKED(UGameZoneAsset, MapSheets)
        || MemberPropertyName == GET_MEMBER_NAME_CHECKED(UGameZoneAsset, DefaultSheetId);
    if (bMapAuthoringChanged && BindingVerificationStatus == EGameZoneBindingVerificationStatus::Verified)
    {
        BindingVerificationStatus = EGameZoneBindingVerificationStatus::SourceStale;
    }
#endif

    if (bLevelChanged && bBindingChangeSucceeded && !LevelToLoad.IsNull())
    {
        BakeMapDataWithResult();
    }

    Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
