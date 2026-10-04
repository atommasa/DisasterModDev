// Copyright Ironic Studio. All Rights Reserved.

#include "Widgets/NavigationWidget.h"

#include "Components/PanelWidget.h"
#include "Components/UniformGridSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Components/UIControlComponent.h"
#include "Widgets/Contents/InteractiveWidget.h"


void UNavigationWidget::NativeConstruct()
{
    Super::NativeConstruct();

    RebuildNavigation();
    ResetNavigationFocus();
}

void UNavigationWidget::RegisterNavigationRegion(
    FName RegionId,
    UPanelWidget* SourceContainer,
    ERPGNavigationRegionLayout Layout,
    bool bRegionAllowWrap,
    bool bRequireNewPressForWrap)
{
    if (!SourceContainer)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s could not register Navigation Region '%s': SourceContainer is null."), *GetName(), *RegionId.ToString());
        return;
    }

    FRPGRuntimeNavigationRegion Region;
    Region.RegionId = RegionId.IsNone() ? SourceContainer->GetFName() : RegionId;
    Region.SourceContainer = SourceContainer;
    Region.Layout = Layout;
    Region.bAllowWrap = bRegionAllowWrap;
    Region.bRequireNewPressForWrap = bRequireNewPressForWrap;

    // The Blueprint Configure event is intentionally declarative: its entries exist
    // only for the current rebuild. Direct runtime registrations persist until removed.
    TArray<FRPGRuntimeNavigationRegion>& TargetRegions = bIsConfiguringNavigationRegions
        ? ConfiguredNavigationRegions
        : DynamicNavigationRegions;

    AddOrReplaceRuntimeNavigationRegion(TargetRegions, Region);
}

bool UNavigationWidget::UnregisterNavigationRegion(FName RegionId)
{
    if (RegionId.IsNone())
    {
        return false;
    }

    const int32 RemovedCount = DynamicNavigationRegions.RemoveAll(
        [RegionId](const FRPGRuntimeNavigationRegion& Existing)
        {
            return Existing.RegionId == RegionId;
        }
    );

    return RemovedCount > 0;
}

void UNavigationWidget::ClearDynamicNavigationRegions()
{
    DynamicNavigationRegions.Reset();
}

void UNavigationWidget::RebuildNavigation()
{
    const FName PreviousFocusNavigationId = CurrentFocusNavigationId;

    ButtonCoordMap.Empty();
    NavigationNodes.Empty();
    RuntimeNavigationIdAliases.Empty();
    CurrentFocusNavigationId = NAME_None;
    MaxRow = 0;
    MaxCol = 0;

    // ConfigureNavigationRegions is a per-build declaration. Entries from an earlier
    // page state are discarded before this event runs, while manual runtime registrations
    // remain in DynamicNavigationRegions until explicitly removed.
    ConfiguredNavigationRegions.Reset();
    bIsConfiguringNavigationRegions = true;
    ConfigureNavigationRegions();
    bIsConfiguringNavigationRegions = false;

    BuildRegisteredNavigationRegions();

    TMap<FIntPoint, UInteractiveWidget*> LegacyButtons;
    CollectNavigatableButtons(LegacyButtons);

    for (const TPair<FIntPoint, UInteractiveWidget*>& Entry : LegacyButtons)
    {
        if (!IsWidgetNavigatable(Entry.Value))
        {
            continue;
        }

        ButtonCoordMap.Add(Entry.Key, Entry.Value);
        MaxRow = FMath::Max(MaxRow, Entry.Key.Y + 1);
        MaxCol = FMath::Max(MaxCol, Entry.Key.X + 1);
        AddNavigationNode(Entry.Value);
        BindWidgetToNavigation(Entry.Value);
    }

    // Legacy grid remains an auto-generation source, not the runtime routing model.
    BuildLegacyGridNavigation();

    for (const FRPGRuntimeNavigationRegion& Region : NavigationRegions)
    {
        BuildRegionNavigation(Region);
    }

    // Deliberately late: authored rules always replace generic auto-generation.
    ApplyWidgetNavigationOverrides();
    ApplyAuthoredNavigationLinks();

    // Dynamic list rebuilds should not silently lose the player's current selection.
    if (!PreviousFocusNavigationId.IsNone() && NavigationNodes.Contains(PreviousFocusNavigationId))
    {
        CurrentFocusNavigationId = PreviousFocusNavigationId;
        if (const FRPGRuntimeNavigationNode* PreviousNode = NavigationNodes.Find(PreviousFocusNavigationId))
        {
            if (const FIntPoint* Coord = ButtonCoordMap.FindKey(PreviousNode->Widget))
            {
                CurrentFocusCoord = *Coord;
            }
        }
    }
}

FName UNavigationWidget::AddNavigationNode(UInteractiveWidget* Widget)
{
    if (!IsWidgetNavigatable(Widget))
    {
        return NAME_None;
    }

    FName Id = GetNavigationIdForWidget(Widget);
    if (const FRPGRuntimeNavigationNode* Existing = NavigationNodes.Find(Id))
    {
        if (Existing->Widget == Widget)
        {
            return Id;
        }

        // Authored UMG widgets normally have unique designer names. When two runtime
        // widgets share one object name, preserve the original name where possible and
        // assign only the colliding instance an internal alias for this graph build.
        const FName UniqueId = MakeUniqueNavigationId(Id);
        UE_LOG(LogTemp, Warning, TEXT("%s found duplicate widget name '%s'. Runtime widget '%s' uses internal graph alias '%s'."),
            *GetName(), *Id.ToString(), *GetNameSafe(Widget), *UniqueId.ToString());
        RuntimeNavigationIdAliases.Add(Widget, UniqueId);
        Id = UniqueId;
    }

    FRPGRuntimeNavigationNode& Node = NavigationNodes.FindOrAdd(Id);
    Node.Widget = Widget;
    return Id;
}

void UNavigationWidget::BuildLegacyGridNavigation()
{
    TMap<FIntPoint, UInteractiveWidget*> Buttons;
    for (const TPair<FIntPoint, TObjectPtr<UInteractiveWidget>>& Entry : ButtonCoordMap)
    {
        if (IsWidgetNavigatable(Entry.Value))
        {
            Buttons.Add(Entry.Key, Entry.Value);
        }
    }

    BuildGridLinks(Buttons, MaxRow, MaxCol, bAllowWrap, bWarpProtection);
}

void UNavigationWidget::BuildRegisteredNavigationRegions()
{
    NavigationRegions.Reset();

    // 1. Resolve authored definitions from the WidgetTree. These are appropriate for
    // fixed UMG layouts and survive Blueprint recompiles / widget reconstruction.
    for (const FRPGNavigationRegionDefinition& Definition : StaticNavigationRegions)
    {
        const FName SourceWidgetName = Definition.SourceWidget.GetFName();
        if (SourceWidgetName.IsNone())
        {
            UE_LOG(LogTemp, Warning, TEXT("%s has a StaticNavigationRegion without a SourceWidget."), *GetName());
            continue;
        }

        UPanelWidget* SourceContainer = ResolveStaticRegionContainer(Definition.SourceWidget);
        if (!SourceContainer)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("%s could not resolve StaticNavigationRegion '%s': selected widget '%s' is not a UPanelWidget in this WidgetTree."),
                *GetName(),
                *Definition.RegionId.ToString(),
                *SourceWidgetName.ToString());
            continue;
        }

        FRPGRuntimeNavigationRegion Region;
        Region.RegionId = Definition.RegionId.IsNone()
            ? SourceWidgetName
            : Definition.RegionId;
        Region.SourceContainer = SourceContainer;
        Region.Layout = Definition.Layout;
        Region.bAllowWrap = Definition.bAllowWrap;
        Region.bRequireNewPressForWrap = Definition.bRequireNewPressForWrap;

        AddOrReplaceRuntimeNavigationRegion(NavigationRegions, Region);
    }

    // 2. Discard registrations whose dynamic panels have been destroyed.
    DynamicNavigationRegions.RemoveAll([](const FRPGRuntimeNavigationRegion& Region)
    {
        return !Region.SourceContainer.IsValid();
    });

    // 3. Apply persistent runtime entries after static definitions. A matching RegionId
    // intentionally replaces a static region without modifying authored defaults.
    for (const FRPGRuntimeNavigationRegion& Region : DynamicNavigationRegions)
    {
        AddOrReplaceRuntimeNavigationRegion(NavigationRegions, Region);
    }

    // 4. ConfigureNavigationRegions() is the most current description of a page's state,
    // so its per-build entries are applied last and can replace either earlier source.
    for (const FRPGRuntimeNavigationRegion& Region : ConfiguredNavigationRegions)
    {
        if (Region.SourceContainer.IsValid())
        {
            AddOrReplaceRuntimeNavigationRegion(NavigationRegions, Region);
        }
    }
}

void UNavigationWidget::BuildRegionNavigation(const FRPGRuntimeNavigationRegion& Region)
{
    UPanelWidget* Source = Region.SourceContainer.Get();
    if (!Source)
    {
        return;
    }

    TArray<UInteractiveWidget*> Widgets;
    CollectInteractiveWidgets(Source, Widgets);

    for (UInteractiveWidget* Widget : Widgets)
    {
        AddNavigationNode(Widget);
        BindWidgetToNavigation(Widget);
    }

    switch (Region.Layout)
    {
    case ERPGNavigationRegionLayout::VerticalList:
        BuildLinearLinks(Widgets, true, Region.bAllowWrap, Region.bRequireNewPressForWrap);
        break;

    case ERPGNavigationRegionLayout::HorizontalList:
        BuildLinearLinks(Widgets, false, Region.bAllowWrap, Region.bRequireNewPressForWrap);
        break;

    case ERPGNavigationRegionLayout::Grid:
    {
        TMap<FIntPoint, UInteractiveWidget*> GridButtons;
        int32 RegionMaxRow = 0;
        int32 RegionMaxCol = 0;

        for (UInteractiveWidget* Widget : Widgets)
        {
            if (const UUniformGridSlot* GridSlot = Cast<UUniformGridSlot>(Widget ? Widget->Slot : nullptr))
            {
                const FIntPoint Coord(GridSlot->GetColumn(), GridSlot->GetRow());
                GridButtons.Add(Coord, Widget);
                RegionMaxRow = FMath::Max(RegionMaxRow, Coord.Y + 1);
                RegionMaxCol = FMath::Max(RegionMaxCol, Coord.X + 1);
            }
        }

        BuildGridLinks(GridButtons, RegionMaxRow, RegionMaxCol, Region.bAllowWrap, Region.bRequireNewPressForWrap);
        break;
    }

    case ERPGNavigationRegionLayout::Manual:
    default:
        break;
    }
}

void UNavigationWidget::BuildGridLinks(
    const TMap<FIntPoint, UInteractiveWidget*>& Buttons,
    int32 InMaxRow,
    int32 InMaxCol,
    bool bInAllowWrap,
    bool bRequireNewPressForWrap)
{
    if (Buttons.IsEmpty() || InMaxRow <= 0 || InMaxCol <= 0)
    {
        return;
    }

    const TArray<EUINavigation> Directions =
    {
        EUINavigation::Up,
        EUINavigation::Down,
        EUINavigation::Left,
        EUINavigation::Right
    };

    for (const TPair<FIntPoint, UInteractiveWidget*>& Entry : Buttons)
    {
        const FName SourceId = GetNavigationIdForWidget(Entry.Value);
        const FIntPoint Origin = Entry.Key;

        for (const EUINavigation Direction : Directions)
        {
            FIntPoint Offset = FIntPoint::ZeroValue;
            switch (Direction)
            {
            case EUINavigation::Up:    Offset.Y = -1; break;
            case EUINavigation::Down:  Offset.Y = 1; break;
            case EUINavigation::Left:  Offset.X = -1; break;
            case EUINavigation::Right: Offset.X = 1; break;
            default: continue;
            }

            FIntPoint Probe = Origin;
            bool bWrapped = false;

            const int32 MaxAttempts = FMath::Max(InMaxRow, InMaxCol);
            for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
            {
                Probe += Offset;

                const bool bOutOfBounds =
                    Probe.X < 0 || Probe.Y < 0 ||
                    Probe.X >= InMaxCol || Probe.Y >= InMaxRow;

                if (bOutOfBounds)
                {
                    if (!bInAllowWrap)
                    {
                        break;
                    }

                    bWrapped = true;
                    Probe.X = Probe.X < 0 ? InMaxCol - 1 : 0;
                    Probe.Y = Probe.Y < 0 ? InMaxRow - 1 : 0;
                }

                if (UInteractiveWidget* const* TargetWidget = Buttons.Find(Probe))
                {
                    if (*TargetWidget && *TargetWidget != Entry.Value)
                    {
                        SetNavigationEdge(
                            SourceId,
                            Direction,
                            GetNavigationIdForWidget(*TargetWidget),
                            bWrapped && bRequireNewPressForWrap
                        );
                    }
                    break;
                }
            }
        }
    }
}

void UNavigationWidget::BuildLinearLinks(
    const TArray<UInteractiveWidget*>& Widgets,
    bool bVertical,
    bool bInAllowWrap,
    bool bRequireNewPressForWrap)
{
    TArray<UInteractiveWidget*> ValidWidgets;
    for (UInteractiveWidget* Widget : Widgets)
    {
        if (IsWidgetNavigatable(Widget))
        {
            ValidWidgets.Add(Widget);
        }
    }

    if (ValidWidgets.Num() < 2)
    {
        return;
    }

    const EUINavigation Forward = bVertical ? EUINavigation::Down : EUINavigation::Right;
    const EUINavigation Backward = bVertical ? EUINavigation::Up : EUINavigation::Left;

    for (int32 Index = 0; Index < ValidWidgets.Num(); ++Index)
    {
        const FName SourceId = GetNavigationIdForWidget(ValidWidgets[Index]);

        if (Index + 1 < ValidWidgets.Num())
        {
            SetNavigationEdge(SourceId, Forward, GetNavigationIdForWidget(ValidWidgets[Index + 1]), false);
        }
        else if (bInAllowWrap)
        {
            SetNavigationEdge(SourceId, Forward, GetNavigationIdForWidget(ValidWidgets[0]), bRequireNewPressForWrap);
        }

        if (Index > 0)
        {
            SetNavigationEdge(SourceId, Backward, GetNavigationIdForWidget(ValidWidgets[Index - 1]), false);
        }
        else if (bInAllowWrap)
        {
            SetNavigationEdge(SourceId, Backward, GetNavigationIdForWidget(ValidWidgets.Last()), bRequireNewPressForWrap);
        }
    }
}

void UNavigationWidget::ApplyWidgetNavigationOverrides()
{
    for (TPair<FName, FRPGRuntimeNavigationNode>& Entry : NavigationNodes)
    {
        UInteractiveWidget* Widget = Entry.Value.Widget;
        if (!Widget)
        {
            continue;
        }

        const FRPGNavigationOverrides& Overrides = Widget->GetNavigationOverrides();
        for (const EUINavigation Direction : { EUINavigation::Up, EUINavigation::Down, EUINavigation::Left, EUINavigation::Right })
        {
            const FRPGNavigationOverride Override = GetOverrideForDirection(Overrides, Direction);
            switch (Override.Rule)
            {
            case ERPGNavigationRule::Auto:
                break;

            case ERPGNavigationRule::Block:
                ClearNavigationLink(Entry.Key, Direction);
                break;

            case ERPGNavigationRule::ExplicitTarget:
                if (!Override.TargetNavigationId.IsNone())
                {
                    SetNavigationEdge(Entry.Key, Direction, Override.TargetNavigationId, Override.bRequireNewPress);
                }
                break;

            case ERPGNavigationRule::Dynamic:
                SetNavigationEdge(Entry.Key, Direction, NAME_None, Override.bRequireNewPress, true);
                break;
            }
        }
    }
}

void UNavigationWidget::ApplyAuthoredNavigationLinks()
{
    for (const FRPGNavigationLink& Link : NavigationLinks)
    {
        /*
         * FWidgetChild stores the UMG Designer name. Authored links use that same
         * name as the Navigation Graph id, matching GetNavigationIdForWidget().
         *
         * The legacy fields remain only as a hidden compatibility fallback for
         * existing assets that were authored before the picker-based fields existed.
         */
        const FName SourceNavigationId =
            !Link.SourceWidget.GetFName().IsNone()
            ? Link.SourceWidget.GetFName()
            : Link.SourceNavigationId;

        const FName TargetNavigationId =
            !Link.TargetWidget.GetFName().IsNone()
            ? Link.TargetWidget.GetFName()
            : Link.TargetNavigationId;

        if (SourceNavigationId.IsNone() || TargetNavigationId.IsNone())
        {
            continue;
        }

        if (!NavigationNodes.Contains(SourceNavigationId))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("%s ignored Navigation Link because source widget '%s' is not a registered interactive navigation node."),
                *GetName(),
                *SourceNavigationId.ToString()
            );
            continue;
        }

        if (!NavigationNodes.Contains(TargetNavigationId))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("%s ignored Navigation Link because target widget '%s' is not a registered interactive navigation node."),
                *GetName(),
                *TargetNavigationId.ToString()
            );
            continue;
        }

        SetNavigationEdge(
            SourceNavigationId,
            Link.Direction,
            TargetNavigationId,
            Link.bRequireNewPress
        );
    }
}

bool UNavigationWidget::SetNavigationLink(FName SourceNavigationId, EUINavigation Direction, FName TargetNavigationId, bool bRequireNewPress)
{
    return SetNavigationEdge(SourceNavigationId, Direction, TargetNavigationId, bRequireNewPress);
}

bool UNavigationWidget::ClearNavigationLink(FName SourceNavigationId, EUINavigation Direction)
{
    FRPGRuntimeNavigationNode* SourceNode = NavigationNodes.Find(SourceNavigationId);
    if (!SourceNode)
    {
        return false;
    }

    if (FRPGRuntimeNavigationEdge* Edge = GetEdge(*SourceNode, Direction))
    {
        *Edge = FRPGRuntimeNavigationEdge();
        return true;
    }

    return false;
}

bool UNavigationWidget::SetNavigationEdge(FName SourceNavigationId, EUINavigation Direction, FName TargetNavigationId, bool bRequireNewPress, bool bDynamic)
{
    FRPGRuntimeNavigationNode* SourceNode = NavigationNodes.Find(SourceNavigationId);
    if (!SourceNode)
    {
        return false;
    }

    if (!bDynamic && !NavigationNodes.Contains(TargetNavigationId))
    {
        UE_LOG(LogTemp, Verbose, TEXT("%s ignored Navigation Link %s -> %s because target is not registered."), *GetName(), *SourceNavigationId.ToString(), *TargetNavigationId.ToString());
        return false;
    }

    FRPGRuntimeNavigationEdge* Edge = GetEdge(*SourceNode, Direction);
    if (!Edge)
    {
        return false;
    }

    Edge->TargetNavigationId = TargetNavigationId;
    Edge->bRequireNewPress = bRequireNewPress;
    Edge->bDynamic = bDynamic;
    return true;
}

void UNavigationWidget::SetNavigationFocus(FIntPoint Coord)
{
    if (const TObjectPtr<UInteractiveWidget>* Widget = ButtonCoordMap.Find(Coord))
    {
        SetNavigationFocusById(GetNavigationIdForWidget(*Widget));
    }
}

bool UNavigationWidget::SetNavigationFocusById(FName NavigationId)
{
    if (!NavigationNodes.Contains(NavigationId))
    {
        return false;
    }

    SetNavigationFocusInternal(NavigationId, true);
    return true;
}

void UNavigationWidget::SetNavigationFocusInternal(FName NewNavigationId, bool bPlayVisualFeedback)
{
    if (NewNavigationId.IsNone() || NewNavigationId == CurrentFocusNavigationId)
    {
        return;
    }

    const FName LastNavigationId = CurrentFocusNavigationId;

    UInteractiveWidget* OldWidget = GetFocusedNavigationWidget();

    const FRPGRuntimeNavigationNode* NewNode = NavigationNodes.Find(NewNavigationId);

    if (!NewNode || !NewNode->Widget)
    {
        return;
    }

    UInteractiveWidget* NewWidget = NewNode->Widget;

    const FIntPoint LastCoord = CurrentFocusCoord;
    const FIntPoint* NewCoord = ButtonCoordMap.FindKey(NewWidget);

#if WITH_EDITORONLY_DATA
    if (bHighlightFocusing)
    {
        if (OldWidget)
        {
            OldWidget->DebugHighlightHoveredWidget(false);
        }

        NewWidget->DebugHighlightHoveredWidget(true);
    }
#endif

    CurrentFocusNavigationId = NewNavigationId;

    if (NewCoord)
    {
        CurrentFocusCoord = *NewCoord;
    }

    if (bPlayVisualFeedback)
    {
        if (OldWidget)
        {
            OldWidget->PlayUnhover();
        }

        NewWidget->PlayHover();
    }

    if (IsTop())
    {
        OnNavigate(
            NewWidget,
            OldWidget,
            NewNavigationId,
            LastNavigationId
        );
    }
}

void UNavigationWidget::SetNavigationFocusByWidget(UInteractiveWidget* Widget)
{
    if (!Widget)
    {
        return;
    }

    const FName Id = GetNavigationIdForWidget(Widget);
    if (!NavigationNodes.Contains(Id))
    {
        return;
    }

    // Mouse hover already played the visual feedback in InteractiveWidget.
    CurrentFocusNavigationId = Id;
    if (const FIntPoint* Coord = ButtonCoordMap.FindKey(Widget))
    {
        CurrentFocusCoord = *Coord;
    }
}

void UNavigationWidget::ResetNavigationFocus()
{
    if (!DefaultFocusNavigationId.IsNone() && SetNavigationFocusById(DefaultFocusNavigationId))
    {
        return;
    }

    if (ButtonCoordMap.Contains(DefaultFocusCoord))
    {
        SetNavigationFocus(DefaultFocusCoord);
        return;
    }

    for (const TPair<FName, FRPGRuntimeNavigationNode>& Node : NavigationNodes)
    {
        if (Node.Value.Widget)
        {
            SetNavigationFocusById(Node.Key);
            return;
        }
    }
}

bool UNavigationWidget::Navigate(EUINavigation Direction)
{
    FRPGRuntimeNavigationNode* CurrentNode = NavigationNodes.Find(CurrentFocusNavigationId);
    if (!CurrentNode || !CurrentNode->Widget)
    {
        ResetNavigationFocus();
        CurrentNode = NavigationNodes.Find(CurrentFocusNavigationId);
        if (!CurrentNode)
        {
            return false;
        }
    }

    const FRPGRuntimeNavigationEdge* Edge = GetEdge(*CurrentNode, Direction);
    if (!Edge || !Edge->IsValid())
    {
        return false;
    }

    if (Edge->bRequireNewPress && UIControlComponent && UIControlComponent->bIsNavigating)
    {
        return false;
    }

    FName TargetId = Edge->TargetNavigationId;
    if (Edge->bDynamic)
    {
        if (UInteractiveWidget* DynamicTarget = ResolveDynamicNavigationTarget(CurrentNode->Widget, Direction))
        {
            TargetId = GetNavigationIdForWidget(DynamicTarget);
        }
    }

    if (TargetId.IsNone() || !NavigationNodes.Contains(TargetId))
    {
        return false;
    }

    SetNavigationFocusInternal(TargetId, true);
    PlayUISoundEffect(NavigationSound);
    return true;
}

bool UNavigationWidget::Confirm_Implementation()
{
    if (UInteractiveWidget* FocusedWidget = GetFocusedNavigationWidget())
    {
        FocusedWidget->PlayClick();

#if WITH_EDITORONLY_DATA
        if (bHighlightFocusing)
        {
            FocusedWidget->DebugHighlightClickedWidget(true);
        }
#endif

        return true;
    }

    return false;
}

UInteractiveWidget* UNavigationWidget::ResolveDynamicNavigationTarget_Implementation(UInteractiveWidget* CurrentWidget, EUINavigation Direction)
{
    return nullptr;
}

UInteractiveWidget* UNavigationWidget::GetFocusedNavigationWidget() const
{
    if (const FRPGRuntimeNavigationNode* Node = NavigationNodes.Find(CurrentFocusNavigationId))
    {
        return Node->Widget;
    }

    return nullptr;
}

void UNavigationWidget::CollectInteractiveWidgets(UPanelWidget* SourceContainer, TArray<UInteractiveWidget*>& OutWidgets) const
{
    if (!SourceContainer)
    {
        return;
    }

    for (UWidget* Child : SourceContainer->GetAllChildren())
    {
        CollectInteractiveWidgetsRecursive(Child, OutWidgets);
    }
}

void UNavigationWidget::CollectInteractiveWidgetsRecursive(UWidget* Widget, TArray<UInteractiveWidget*>& OutWidgets) const
{
    if (!Widget)
    {
        return;
    }

    if (UInteractiveWidget* InteractiveWidget = Cast<UInteractiveWidget>(Widget))
    {
        if (IsWidgetNavigatable(InteractiveWidget))
        {
            OutWidgets.AddUnique(InteractiveWidget);
        }
        return;
    }

    if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
    {
        for (UWidget* Child : Panel->GetAllChildren())
        {
            CollectInteractiveWidgetsRecursive(Child, OutWidgets);
        }
    }
}

UPanelWidget* UNavigationWidget::ResolveStaticRegionContainer(const FWidgetChild& SourceWidget) const
{
    if (!WidgetTree || SourceWidget.GetFName().IsNone())
    {
        return nullptr;
    }

    // FWidgetChild stores the UMG Designer widget name. Resolve through this
    // instance's WidgetTree so the selected object is valid after reconstruction.
    return Cast<UPanelWidget>(WidgetTree->FindWidget(SourceWidget.GetFName()));
}

void UNavigationWidget::AddOrReplaceRuntimeNavigationRegion(
    TArray<FRPGRuntimeNavigationRegion>& TargetRegions,
    const FRPGRuntimeNavigationRegion& Region)
{
    const int32 ExistingIndex = TargetRegions.IndexOfByPredicate(
        [&Region](const FRPGRuntimeNavigationRegion& Existing)
        {
            return Existing.RegionId == Region.RegionId;
        }
    );

    if (ExistingIndex != INDEX_NONE)
    {
        TargetRegions[ExistingIndex] = Region;
    }
    else
    {
        TargetRegions.Add(Region);
    }
}

FName UNavigationWidget::GetNavigationIdForWidget(const UInteractiveWidget* Widget) const
{
    if (!Widget)
    {
        return NAME_None;
    }

    if (const FName* Alias = RuntimeNavigationIdAliases.Find(Widget))
    {
        return *Alias;
    }

    // Navigation ids intentionally mirror the widget's own UMG/Object name.
    // This keeps authored links readable and removes a separate id field from
    // every interactive widget.
    return Widget->GetFName();
}

FName UNavigationWidget::MakeUniqueNavigationId(FName DesiredId) const
{
    if (!NavigationNodes.Contains(DesiredId))
    {
        return DesiredId;
    }

    int32 Suffix = 1;
    FName Candidate;
    do
    {
        Candidate = FName(*FString::Printf(TEXT("%s_%d"), *DesiredId.ToString(), Suffix++));
    }
    while (NavigationNodes.Contains(Candidate));

    return Candidate;
}

FRPGRuntimeNavigationEdge* UNavigationWidget::GetEdge(FRPGRuntimeNavigationNode& Node, EUINavigation Direction)
{
    switch (Direction)
    {
    case EUINavigation::Up: return &Node.Up;
    case EUINavigation::Down: return &Node.Down;
    case EUINavigation::Left: return &Node.Left;
    case EUINavigation::Right: return &Node.Right;
    default: return nullptr;
    }
}

const FRPGRuntimeNavigationEdge* UNavigationWidget::GetEdge(const FRPGRuntimeNavigationNode& Node, EUINavigation Direction) const
{
    switch (Direction)
    {
    case EUINavigation::Up: return &Node.Up;
    case EUINavigation::Down: return &Node.Down;
    case EUINavigation::Left: return &Node.Left;
    case EUINavigation::Right: return &Node.Right;
    default: return nullptr;
    }
}

FRPGNavigationOverride UNavigationWidget::GetOverrideForDirection(const FRPGNavigationOverrides& Overrides, EUINavigation Direction) const
{
    switch (Direction)
    {
    case EUINavigation::Up: return Overrides.Up;
    case EUINavigation::Down: return Overrides.Down;
    case EUINavigation::Left: return Overrides.Left;
    case EUINavigation::Right: return Overrides.Right;
    default: return FRPGNavigationOverride();
    }
}

bool UNavigationWidget::IsWidgetNavigatable(const UInteractiveWidget* Widget) const
{
    return Widget && Widget->GetVisibility() != ESlateVisibility::Collapsed && Widget->GetVisibility() != ESlateVisibility::Hidden;
}

void UNavigationWidget::BindWidgetToNavigation(UInteractiveWidget* Widget)
{
    if (Widget)
    {
        Widget->OnWidgetHovered.AddUniqueDynamic(this, &UNavigationWidget::SetNavigationFocusByWidget);
    }
}
