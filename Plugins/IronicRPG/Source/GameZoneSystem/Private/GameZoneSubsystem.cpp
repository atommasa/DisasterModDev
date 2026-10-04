// Copyright Ironic Studio. All Rights Reserved.


#include "GameZoneSubsystem.h"
#include "EngineUtils.h"
#include "GameMapsSettings.h"
#include "GameFramework/GameModeBase.h"

#include "Settings/GameZoneSystemSettings.h"
#include "SaveGameSubsystem.h"
#include "LoadingScreenSubsystem.h"

# include "Kismet/GameplayStatics.h"
#include "Engine/LevelStreamingDynamic.h"
#include "GameFramework/PlayerStart.h"

#include "GameZoneSaveModule.h"
#include "Assets/RPGAssetManager.h"
#include "Assets/RPGAssetLibrary.h"
#include "Levels/GameZoneAsset.h"
#include "Levels/MapMarkerTypeAsset.h"
#include "Levels/RPGWorldSettings.h"
#include "Levels/GameZonePointComponent.h"
#include "MapMarkers/GameZoneMarkerRegistry.h"
#include "Maps/GameZoneMapResolver.h"
#include "Async/Async.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"

#include "CharacterSubsystem.h"

#if WITH_EDITOR
#include "Engine/PlayerStartPIE.h"
#endif // WITH_EDITOR

namespace
{
	enum class EGameZonePresentationLoadState : uint8
	{
		Loading,
		Loaded,
		Failed,
	};

	struct FGameZonePresentationSession
	{
		TSet<FRPGId> ZoneIds;
		EMapMarkerDisplayMode Mode = EMapMarkerDisplayMode::None;
		FOnGameZonePresentationReady Completion;
		bool bCompletionSent = false;
	};

	struct FGameZonePresentationZone
	{
		int32 RefCount = 0;
		uint32 Generation = 0;
		EGameZonePresentationLoadState LoadState =
			EGameZonePresentationLoadState::Loading;
		TRPGCoroutine<> LoadTask;
		TStrongObjectPtr<UGameZoneAsset> Asset;
		TUniquePtr<FGameZoneMapResolver> MapResolver;
	};

	struct FGameZoneMapTextureRequest
	{
		FGuid SessionId;
		FRPGId ZoneId;
		FGameZoneMapSheetId SheetId;
		FSoftObjectPath CurrentPath;
		FSoftObjectPath DefaultPath;
		FOnGameZoneMapTextureReady Completion;
		bool bUsingDefaultTexture = false;
	};

	struct FGameZoneMapTextureLoad
	{
		TSharedPtr<FStreamableHandle> Handle;
		TSet<FGuid> PendingRequestIds;
		TSet<FGuid> RetainingSessionIds;
	};

	void AppendMapCatalog(
		const FRPGId& ZoneId,
		const UGameZoneAsset& Asset,
		const FGameZoneMapResolver& MapResolver,
		FGameZonePresentationSnapshot& Snapshot)
	{
		Snapshot.MapWorldBounds.Add(MapResolver.GetWorldBounds());

		for (const FGameZoneMapLayer& Layer : Asset.GetMapLayers())
		{
			FGameZoneMapLayerCatalogEntry& Entry = Snapshot.MapLayers.AddDefaulted_GetRef();
			Entry.ZoneId = ZoneId;
			Entry.LayerId = Layer.LayerId;
			Entry.DisplayName = Layer.DisplayName;
			Entry.SortOrder = Layer.SortOrder;
			Entry.ElevationOrder = Layer.ElevationOrder;
		}

		for (const FGameZoneMapSheet& Sheet : Asset.GetMapSheets())
		{
			FGameZoneMapSheetCatalogEntry& Entry = Snapshot.MapSheets.AddDefaulted_GetRef();
			Entry.ZoneId = ZoneId;
			Entry.LayerId = Sheet.LayerId;
			Entry.SheetId = Sheet.SheetId;
			Entry.DisplayName = Sheet.DisplayName;
			Entry.SortOrder = Sheet.SortOrder;
			Entry.MapBakeRevision = Asset.GetMapBakeRevision();
		}
	}
}

struct FGameZoneMapPresentationState
{
	TMap<FGuid, FGameZonePresentationSession> Sessions;
	TMap<FRPGId, FGameZonePresentationZone> Zones;
	TMap<FGuid, FGameZoneMapTextureRequest> TextureRequests;
	TMap<FSoftObjectPath, FGameZoneMapTextureLoad> TextureLoads;
};

UGameZoneSubsystem::UGameZoneSubsystem()
	: MarkerRegistry(MakePimpl<FGameZoneMarkerRegistry>())
	, MapPresentations(MakePimpl<FGameZoneMapPresentationState>())
{
}

UGameZoneSubsystem::~UGameZoneSubsystem() = default;

void UGameZoneSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

	PostGarbageCollectHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddUObject(
		this,
		&UGameZoneSubsystem::HandlePostGarbageCollect);

	Collection.ActivateExternalSubsystem(USaveGameSubsystem::StaticClass());
	Collection.ActivateExternalSubsystem(ULoadingScreenSubsystem::StaticClass());
	Collection.ActivateExternalSubsystem(UCharacterSubsystem::StaticClass());

#if WITH_EDITOR
	FWorldDelegates::OnStartGameInstance.AddWeakLambda(this, [this](UGameInstance*)
		{
			UWorld* World = GetWorld();
			if (!World)
			{
				return;
			}

			const UGameMapsSettings* MapsSettings = GetDefault<UGameMapsSettings>();
			if (!MapsSettings)
			{
				return;
			}

			FString PackageNameStr;

			if (UPackage* WorldPackage = World->GetOutermost())
			{
				FName PackageFName = WorldPackage->GetFName();
				PackageNameStr = UWorld::RemovePIEPrefix(PackageFName.ToString());
			}

			bStartGameInstantly = MapsSettings->GetGameDefaultMap() != PackageNameStr;

			UpdateCurrentContext();

			UE_LOG(LogTemp, Display, TEXT("Current context update to %s when game started."), *CurrentContext.ZoneId.ToString());

			if (bStartGameInstantly)
			{
				World->GetTimerManager().SetTimerForNextTick([this]()
					{
						OnStartGameInstantly.Broadcast();
					});
			}
		});
#endif // WITH_EDITOR

}

void UGameZoneSubsystem::Deinitialize()
{
	if (PostGarbageCollectHandle.IsValid())
	{
		FCoreUObjectDelegates::GetPostGarbageCollect().Remove(PostGarbageCollectHandle);
		PostGarbageCollectHandle.Reset();
	}

	for (TPair<FRPGId, FGameZonePresentationZone>& Pair : MapPresentations->Zones)
	{
		if (Pair.Value.LoadTask.IsValid() && !Pair.Value.LoadTask.IsDone())
		{
			Pair.Value.LoadTask.Cancel();
		}
	}
	for (TPair<FSoftObjectPath, FGameZoneMapTextureLoad>& Pair : MapPresentations->TextureLoads)
	{
		if (Pair.Value.Handle.IsValid() && !Pair.Value.Handle->HasLoadCompleted())
		{
			Pair.Value.Handle->CancelHandle();
		}
	}

	MapPresentations->Sessions.Reset();
	MapPresentations->Zones.Reset();
	MapPresentations->TextureRequests.Reset();
	MapPresentations->TextureLoads.Reset();

	TArray<TObjectPtr<UGameZoneMarkerAction>> ActionsToCancel;
	ActiveMarkerActions.GenerateValueArray(ActionsToCancel);
	for (UGameZoneMarkerAction* Action : ActionsToCancel)
	{
		if (Action)
		{
			Action->CancelAction();
		}
	}
	ActiveMarkerActions.Reset();
	MarkerActionCompletions.Reset();

    Super::Deinitialize();
}

void UGameZoneSubsystem::EnterGameZone(const FGameZoneContext& NewContext)
{
	EnterGameZone(NewContext, nullptr);
}

void UGameZoneSubsystem::EnterGameZone(const FGameZoneContext& NewContext, TDelegate<void()> Completion)
{
	if (!NewContext.ZoneId.IsValid())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[GameZone] Invalid destination ZoneId."));
		Completion.ExecuteIfBound();
		return;
	}

	PendingContext = NewContext;

	BeginZoneTravel(NewContext, MoveTemp(Completion));
}

void UGameZoneSubsystem::HandlePostSeamlessTravel()
{
	if (!bIsTravelling)
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				CompleteZoneTravel();
			})
	);
}

void UGameZoneSubsystem::UpdateCurrentContext()
{
	UWorld* Current = GetWorld();
	if (!Current)
	{
		return;
	}

	ARPGWorldSettings* WorldSettings = Cast<ARPGWorldSettings>(Current->GetWorldSettings());
	if (!WorldSettings)
	{
		return;
	}

	FGameZoneContext NewContext;
	NewContext.ZoneId = WorldSettings->GetGameZoneId();
	NewContext.EntryId = WorldSettings->GetDefaultEntryId();

	CurrentContext = NewContext;
}

FTransform UGameZoneSubsystem::ResolveEntryTransform() const
{
	// If we have a saved transform, use that for spawning
	if (CurrentContext.bUseSavedTransform)
	{
		return CurrentContext.SavedTransform;
	}

	// Otherwise, find a PlayerStart with a matching tag
	if (UGameZoneAsset* CurrentGameZone = URPGAssetLibrary::GetRPGAsset<UGameZoneAsset>(CurrentContext.ZoneId))
	{
		if (const FGameZonePointData* DataPtr = CurrentGameZone->GetBakedPoints().Find(CurrentContext.EntryId.EntryGuid))
		{
			return DataPtr->WorldTransform;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("No PlayerStart found with id [%s], spawning at origin!"), *CurrentContext.EntryId.EntryGuid.ToString());
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load asset [%s], spawning at origin!"), *CurrentContext.ZoneId.ToString());
	}

	return FTransform::Identity;
}

TRPGCoroutine<> UGameZoneSubsystem::BeginZoneTravel(const FGameZoneContext& NewContext, TDelegate<void()> Completion)
{
	if (IsTravelling())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameZone] Travel request rejected because another travel is active."));
		co_return;
	}

	bIsTravelling = true;

	PendingContext = NewContext;
	PendingTravelCompletion = MoveTemp(Completion);

	if (ULoadingScreenSubsystem* LoadingSubsystem = GetGameInstance()->GetSubsystem<ULoadingScreenSubsystem>())
	{
		// Preset control mode to ERPGControlMode::Gameplay
		if (ARPGPlayerController* PC = Cast<ARPGPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
		{
			PC->SetControlMode(ERPGControlMode::Gameplay);
		}

		LoadingSubsystem->StartLoadingScreen();
	}

	const auto Result = co_await URPGAssetLibrary::LoadAssetByRPGIdAsync<UGameZoneAsset>(NewContext.ZoneId, { "World" });
	CurrentAsset = Result.Value;

	if (!Result.IsSuccess() || Result.Value == nullptr)
	{
		FailZoneTravel(FString::Printf(TEXT("Could not load asset: %s"), *NewContext.ZoneId.ToString()));
		co_return;
	}

	if (Result.Value->GetLevelToLoad().IsNull())
	{
		FailZoneTravel(FString::Printf(TEXT("Zone has no map: %s"), *NewContext.ZoneId.ToString()));
		co_return;
	}

	const FString& DestinationMap = Result.Value->GetLevelToLoad().ToSoftObjectPath().GetLongPackageName();

	if (UCharacterSubsystem* CharacterSubsystem = GetGameInstance()->GetSubsystem<UCharacterSubsystem>())
	{
		CharacterSubsystem->DespawnPartyMembers();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		FailZoneTravel(TEXT("World is invalid."));
		co_return;
	}

	const bool bStarted = World->ServerTravel(DestinationMap, true, false);

	if (!bStarted)
	{
		FailZoneTravel(FString::Printf(TEXT("ServerTravel failed: %s"), *DestinationMap));
	}

	co_return;
}

TRPGCoroutine<> UGameZoneSubsystem::CompleteZoneTravel()
{
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[RPGFlow] Travel: Complete begin"));

	UWorld* World = GetWorld();
	if (!World)
	{
		FailZoneTravel(TEXT("Destination world is invalid."));
		co_return;
	}

	CurrentContext = PendingContext;

	const FTransform DestinationTransform = ResolveEntryTransform();

	UCharacterSubsystem* CharacterSubsystem = GetGameInstance()->GetSubsystem<UCharacterSubsystem>();

	if (!CharacterSubsystem)
	{
		FailZoneTravel(TEXT("CharacterSubsystem is invalid."));
		co_return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[RPGFlow] Travel: Waiting for party"));

	co_await CharacterSubsystem->SpawnPartyMembersCoreAsync(
		DestinationTransform.GetLocation(),
		DestinationTransform.Rotator(),
		ESpawnPartyMode::KeepControlSameCharacter
	);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[RPGFlow] Travel: Party ready"));

	TDelegate<void()> Completion = MoveTemp(PendingTravelCompletion);

	OnGameZoneTravelCompleted.Broadcast(CurrentContext);
	Completion.ExecuteIfBound();

	if (ULoadingScreenSubsystem* LoadingSubsystem = GetGameInstance()->GetSubsystem<ULoadingScreenSubsystem>())
	{
		LoadingSubsystem->StopLoadingScreen();
	}

	bIsTravelling = false;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[RPGFlow] Travel: Complete"));

	co_return;
}

void UGameZoneSubsystem::FailZoneTravel(const FString& Reason)
{
	CurrentAsset = nullptr;

	bIsTravelling = false;
	PendingTravelCompletion = nullptr;

	UE_LOG(LogTemp, Error, TEXT("[GameZone] %s"), *Reason);
}

bool UGameZoneSubsystem::ResolveZoneMap(const FRPGId& ZoneId, FString& OutMapPackageName) const
{
	UGameZoneAsset* const ZoneAsset = Cast<UGameZoneAsset>(URPGAssetLibrary::LoadAssetByRPGId(ZoneId));

	if (!ZoneAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("[GameZone] Zone asset is not loaded: %s"), *ZoneId.ToString());

		return false;
	}

	if (ZoneAsset->GetLevelToLoad().IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("[GameZone] Zone has no map: %s"), *ZoneId.ToString());

		return false;
	}

	OutMapPackageName = ZoneAsset->GetLevelToLoad().ToSoftObjectPath().GetLongPackageName();

	return !OutMapPackageName.IsEmpty();
}

EGameZonePointRegistrationResult UGameZoneSubsystem::RegisterPoint(
	UGameZonePointComponent& Component)
{
	if (Component.GetWorld() != GetWorld())
	{
		return EGameZonePointRegistrationResult::WrongWorld;
	}

	const FRPGId RuntimeZoneId = CurrentAsset
		? CurrentAsset->GetId()
		: CurrentContext.ZoneId;
	const FGameZoneMarkerMutationResult Mutation =
		MarkerRegistry->RegisterLive(Component, RuntimeZoneId);
	PublishMarkerMutation(Mutation);

	switch (Mutation.Error)
	{
	case EGameZoneMarkerMutationError::None:
		return Mutation.WasApplied()
			? EGameZonePointRegistrationResult::Registered
			: EGameZonePointRegistrationResult::AlreadyRegistered;
	case EGameZoneMarkerMutationError::InvalidPointId:
		return EGameZonePointRegistrationResult::InvalidPointId;
	case EGameZoneMarkerMutationError::DuplicateLiveSource:
		return EGameZonePointRegistrationResult::DuplicatePointId;
	default:
		return EGameZonePointRegistrationResult::DuplicatePointId;
	}
}

void UGameZoneSubsystem::UnregisterPoint(UGameZonePointComponent& Component)
{
	if (Component.GetWorld() == GetWorld())
	{
		PublishMarkerMutation(MarkerRegistry->UnregisterLive(Component));
	}
}

void UGameZoneSubsystem::NotifyPointChanged(UGameZonePointComponent& Component)
{
	if (Component.GetWorld() == GetWorld())
	{
		PublishMarkerMutation(MarkerRegistry->NotifyLiveChanged(Component));
	}
}

bool UGameZoneSubsystem::TryResolveMapMarker(
	const FGuid& PointId,
	FResolvedGameZonePoint& OutPoint) const
{
	return MarkerRegistry->TryResolve(PointId, OutPoint);
}

bool UGameZoneSubsystem::SetMapMarkerSaveOverride(const FGameZonePointData& Override)
{
	const FGameZoneMarkerMutationResult Mutation = MarkerRegistry->SetSaveOverride(Override);
	PublishMarkerMutation(Mutation);
	return Mutation.Error == EGameZoneMarkerMutationError::None;
}

bool UGameZoneSubsystem::ClearMapMarkerSaveOverride(const FGuid& PointId)
{
	const FGameZoneMarkerMutationResult Mutation = MarkerRegistry->ClearSaveOverride(PointId);
	PublishMarkerMutation(Mutation);
	return Mutation.Error == EGameZoneMarkerMutationError::None;
}

bool UGameZoneSubsystem::GetMapMarkerActionOptions(
	const FGuid& PointId,
	EMapMarkerDisplayMode PresentationMode,
	APlayerController* PlayerController,
	TArray<FGameZoneMarkerActionOption>& OutOptions) const
{
	OutOptions.Reset();

	FResolvedGameZonePoint Point;
	if (!PlayerController
		|| PresentationMode == EMapMarkerDisplayMode::None
		|| !TryResolveMapMarker(PointId, Point)
		|| Point.Data.MarkerState == EGameZonePointState::Hide)
	{
		return false;
	}

	const EMapMarkerDisplayMode PointDisplayMode = static_cast<EMapMarkerDisplayMode>(Point.Data.DisplayMode);
	if (!EnumHasAnyFlags(PointDisplayMode, PresentationMode))
	{
		return false;
	}

	const UMapMarkerTypeAsset* MarkerType = URPGAssetLibrary::GetRPGAsset<UMapMarkerTypeAsset>(Point.Data.MarkerTypeId);
	if (!MarkerType)
	{
		return false;
	}

	FGameZoneMarkerActionContext Context;
	Context.Point = Point.Data;
	Context.PresentationMode = PresentationMode;
	Context.PlayerController = PlayerController;
	Context.WorldContextObject = const_cast<UGameZoneSubsystem*>(this);

	for (const FGameZoneMarkerActionDefinition& Definition : MarkerType->GetActions())
	{
		if (!Definition.ActionTag.IsValid() || !Definition.ActionClass)
		{
			continue;
		}

		const UGameZoneMarkerAction* Action = Definition.ActionClass->GetDefaultObject<UGameZoneMarkerAction>();
		if (!Action)
		{
			continue;
		}

		FGameZoneMarkerActionOption& Option = OutOptions.AddDefaulted_GetRef();
		Option.Definition = Definition;
		Option.Availability = Action->GetAvailability(Context);
		if (Option.Availability.Availability != EGameZoneMarkerActionAvailability::Hidden)
		{
			const FText DisplayName = Action->GetDisplayName(Context, Definition.DisplayName);
			if (!DisplayName.IsEmptyOrWhitespace())
			{
				Option.Definition.DisplayName = DisplayName;
			}
		}
	}

	OutOptions.RemoveAll([](const FGameZoneMarkerActionOption& Option)
		{
			return Option.Availability.Availability == EGameZoneMarkerActionAvailability::Hidden;
		});
	OutOptions.Sort([](const FGameZoneMarkerActionOption& Left, const FGameZoneMarkerActionOption& Right)
		{
			if (Left.Definition.SortOrder != Right.Definition.SortOrder)
			{
				return Left.Definition.SortOrder < Right.Definition.SortOrder;
			}

			return Left.Definition.ActionTag.ToString() < Right.Definition.ActionTag.ToString();
		});
	return true;
}

FGuid UGameZoneSubsystem::ExecuteMapMarkerAction(
	const FGuid& PointId,
	const FGameplayTag& ActionTag,
	EMapMarkerDisplayMode PresentationMode,
	APlayerController* PlayerController,
	FOnGameZoneMarkerActionCompleted Completion)
{
	if (!ActionTag.IsValid() || !Completion.IsBound())
	{
		return {};
	}

	FResolvedGameZonePoint Point;
	if (!PlayerController
		|| PresentationMode == EMapMarkerDisplayMode::None
		|| !TryResolveMapMarker(PointId, Point)
		|| Point.Data.MarkerState == EGameZonePointState::Hide)
	{
		return {};
	}

	const EMapMarkerDisplayMode PointDisplayMode = static_cast<EMapMarkerDisplayMode>(Point.Data.DisplayMode);
	if (!EnumHasAnyFlags(PointDisplayMode, PresentationMode))
	{
		return {};
	}

	const UMapMarkerTypeAsset* MarkerType = URPGAssetLibrary::GetRPGAsset<UMapMarkerTypeAsset>(Point.Data.MarkerTypeId);
	const FGameZoneMarkerActionDefinition* Definition = MarkerType
		? MarkerType->GetActions().FindByPredicate([&ActionTag](const FGameZoneMarkerActionDefinition& Candidate)
			{
				return Candidate.ActionTag == ActionTag;
			})
		: nullptr;
	if (!Definition
		|| !Definition->ActionClass
		|| Definition->ActionClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return {};
	}

	FGameZoneMarkerActionContext Context;
	Context.Point = Point.Data;
	Context.PresentationMode = PresentationMode;
	Context.PlayerController = PlayerController;
	Context.WorldContextObject = this;

	const UGameZoneMarkerAction* ActionDefault = Definition->ActionClass->GetDefaultObject<UGameZoneMarkerAction>();
	if (!ActionDefault
		|| ActionDefault->GetAvailability(Context).Availability != EGameZoneMarkerActionAvailability::Available)
	{
		return {};
	}

	UGameZoneMarkerAction* Action = NewObject<UGameZoneMarkerAction>(this, Definition->ActionClass);
	if (!Action)
	{
		return {};
	}

	const FGuid ExecutionId = FGuid::NewGuid();
	ActiveMarkerActions.Add(ExecutionId, Action);
	MarkerActionCompletions.Add(ExecutionId, MoveTemp(Completion));

	TWeakObjectPtr<UGameZoneSubsystem> WeakThis(this);
	Action->StartAction(Context, [WeakThis, ExecutionId](const FGameZoneMarkerActionResult& Result)
		{
			if (UGameZoneSubsystem* Subsystem = WeakThis.Get())
			{
				Subsystem->HandleMarkerActionCompleted(ExecutionId, Result);
			}
		});
	return ExecutionId;
}

void UGameZoneSubsystem::HandleMarkerActionCompleted(const FGuid& ExecutionId, const FGameZoneMarkerActionResult& Result)
{
	FOnGameZoneMarkerActionCompleted Completion;
	if (FOnGameZoneMarkerActionCompleted* ExistingCompletion = MarkerActionCompletions.Find(ExecutionId))
	{
		Completion = MoveTemp(*ExistingCompletion);
	}

	MarkerActionCompletions.Remove(ExecutionId);
	ActiveMarkerActions.Remove(ExecutionId);
	Completion.ExecuteIfBound(Result);
}

void UGameZoneSubsystem::HandlePostGarbageCollect()
{
	PublishMarkerMutation(MarkerRegistry->SanitizeInvalidLive());
}

void UGameZoneSubsystem::PublishMarkerMutation(
	const FGameZoneMarkerMutationResult& Mutation)
{
	for (const FMapMarkerChange& Change : Mutation.Changes)
	{
		OnMapMarkerChange.Broadcast(Change);
	}
}

FGameZonePresentationHandle UGameZoneSubsystem::BeginMapPresentation(
	const TArray<FRPGId>& ZoneIds,
	EMapMarkerDisplayMode Mode,
	FOnGameZonePresentationReady Completion)
{
	TSet<FRPGId> UniqueZoneIds;
	for (const FRPGId& ZoneId : ZoneIds)
	{
		if (ZoneId.IsValid())
		{
			UniqueZoneIds.Add(ZoneId);
		}
	}

	if (UniqueZoneIds.IsEmpty() || Mode == EMapMarkerDisplayMode::None)
	{
		return {};
	}

	FGameZonePresentationHandle Handle;
	Handle.SessionId = FGuid::NewGuid();

	FGameZonePresentationSession& Session = MapPresentations->Sessions.Add(Handle.SessionId);
	Session.ZoneIds = UniqueZoneIds;
	Session.Mode = Mode;
	Session.Completion = MoveTemp(Completion);

	TArray<TPair<FRPGId, uint32>> LoadsToStart;
	for (const FRPGId& ZoneId : UniqueZoneIds)
	{
		FGameZonePresentationZone* LoadedZone =
			MapPresentations->Zones.Find(ZoneId);
		if (LoadedZone)
		{
			++LoadedZone->RefCount;
			continue;
		}

		FGameZonePresentationZone& NewZone =
			MapPresentations->Zones.Add(ZoneId);
		NewZone.RefCount = 1;
		NewZone.Generation = 1;
		NewZone.LoadState = EGameZonePresentationLoadState::Loading;
		LoadsToStart.Emplace(ZoneId, NewZone.Generation);
	}

	for (const TPair<FRPGId, uint32>& Load : LoadsToStart)
	{
		TRPGCoroutine<> LoadTask = LoadMapPresentationZone(
			this,
			Load.Key,
			Load.Value);

		if (FGameZonePresentationZone* LoadedZone =
			MapPresentations->Zones.Find(Load.Key);
			LoadedZone && LoadedZone->Generation == Load.Value)
		{
			LoadedZone->LoadTask = MoveTemp(LoadTask);
		}
	}

	TryCompleteMapPresentation(Handle.SessionId);
	return Handle;
}

void UGameZoneSubsystem::EndMapPresentation(FGameZonePresentationHandle Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}

	FGameZonePresentationSession Session;
	if (!MapPresentations->Sessions.RemoveAndCopyValue(
		Handle.SessionId,
		Session))
	{
		return;
	}

	CancelMapTextureRequestsForSession(Handle.SessionId);

	for (const FRPGId& ZoneId : Session.ZoneIds)
	{
		ReleaseMapPresentationZone(ZoneId);
	}
}

bool UGameZoneSubsystem::ResolveMapSheetAtLocation(
	const FRPGId& ZoneId,
	const FVector& WorldLocation,
	FResolvedGameZoneMapSheet& OutSheet) const
{
	OutSheet = {};
	const FGameZonePresentationZone* Zone = MapPresentations->Zones.Find(ZoneId);
	return Zone
		&& Zone->LoadState == EGameZonePresentationLoadState::Loaded
		&& Zone->MapResolver
		&& Zone->MapResolver->TryResolveAtLocation(WorldLocation, {}, OutSheet);
}

bool UGameZoneSubsystem::ResolveMapSheetInLayerAtLocation(
	const FRPGId& ZoneId,
	const FGameZoneMapLayerId& LayerId,
	const FVector& WorldLocation,
	FResolvedGameZoneMapSheet& OutSheet) const
{
	OutSheet = {};
	const FGameZonePresentationZone* Zone = MapPresentations->Zones.Find(ZoneId);
	return Zone
		&& Zone->LoadState == EGameZonePresentationLoadState::Loaded
		&& Zone->MapResolver
		&& Zone->MapResolver->TryResolveInLayerAtLocation(
			LayerId,
			WorldLocation,
			OutSheet);
}

bool UGameZoneSubsystem::ResolveMapSheetById(
	const FRPGId& ZoneId,
	const FGameZoneMapSheetId& SheetId,
	FResolvedGameZoneMapSheet& OutSheet) const
{
	OutSheet = {};
	const FGameZonePresentationZone* Zone = MapPresentations->Zones.Find(ZoneId);
	return Zone
		&& Zone->LoadState == EGameZonePresentationLoadState::Loaded
		&& Zone->MapResolver
		&& Zone->MapResolver->TryResolveById(SheetId, OutSheet);
}

bool UGameZoneSubsystem::ProjectWorldLocationToMapSheet(
	const FRPGId& ZoneId,
	const FGameZoneMapSheetId& SheetId,
	const FVector& WorldLocation,
	FGameZoneMapProjection& OutProjection) const
{
	OutProjection = {};
	const FGameZonePresentationZone* Zone = MapPresentations->Zones.Find(ZoneId);
	return Zone
		&& Zone->LoadState == EGameZonePresentationLoadState::Loaded
		&& Zone->MapResolver
		&& Zone->MapResolver->TryProjectWorldLocation(
			SheetId,
			WorldLocation,
			OutProjection);
}

FGuid UGameZoneSubsystem::RequestMapSheetTexture(
	FGameZonePresentationHandle Handle,
	const FRPGId& ZoneId,
	const FGameZoneMapSheetId& SheetId,
	FOnGameZoneMapTextureReady Completion)
{
	const FGameZonePresentationSession* Session = MapPresentations->Sessions.Find(Handle.SessionId);
	const FGameZonePresentationZone* Zone = MapPresentations->Zones.Find(ZoneId);
	if (!Session
		|| !Session->ZoneIds.Contains(ZoneId)
		|| !Zone
		|| Zone->LoadState != EGameZonePresentationLoadState::Loaded
		|| !Zone->Asset.IsValid()
		|| !Zone->MapResolver)
	{
		return {};
	}

	FResolvedGameZoneMapSheet ResolvedSheet;
	if (!Zone->MapResolver->TryResolveById(SheetId, ResolvedSheet))
	{
		return {};
	}

	const FGameZoneMapSheet* Sheet = Zone->Asset->GetMapSheets().FindByPredicate(
		[&SheetId](const FGameZoneMapSheet& Candidate)
		{
			return Candidate.SheetId == SheetId;
		});

	if (!Sheet)
	{
		return {};
	}

	// First-stage presentations display one Sheet at a time. A new accepted
	// request replaces pending and retained Texture state for this session.
	CancelMapTextureRequestsForSession(Handle.SessionId);

	FGameZoneMapTextureRequest Request;
	Request.SessionId = Handle.SessionId;
	Request.ZoneId = ZoneId;
	Request.SheetId = SheetId;
	Request.DefaultPath = GetDefault<UGameZoneSystemSettings>()->DefaultMapTexture.ToSoftObjectPath();
	Request.Completion = MoveTemp(Completion);

	FSoftObjectPath InitialPath = Sheet->MapTexture.ToSoftObjectPath();
	if (InitialPath.IsNull())
	{
		InitialPath = Request.DefaultPath;
		Request.bUsingDefaultTexture = true;
	}

	const FGuid RequestId = FGuid::NewGuid();
	MapPresentations->TextureRequests.Add(RequestId, MoveTemp(Request));

	if (InitialPath.IsNull())
	{
		TWeakObjectPtr<UGameZoneSubsystem> WeakThis(this);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestId]()
		{
			if (UGameZoneSubsystem* Subsystem = WeakThis.Get())
			{
				Subsystem->CompleteMapTextureRequest(RequestId, nullptr);
			}
		});
	}
	else
	{
		QueueMapTexturePath(RequestId, InitialPath);
	}

	return RequestId;
}

void UGameZoneSubsystem::QueueMapTexturePath(
	const FGuid& RequestId,
	const FSoftObjectPath& TexturePath)
{
	FGameZoneMapTextureRequest* Request =
		MapPresentations->TextureRequests.Find(RequestId);
	if (!Request || TexturePath.IsNull())
	{
		return;
	}

	Request->CurrentPath = TexturePath;
	FGameZoneMapTextureLoad* ExistingLoad =
		MapPresentations->TextureLoads.Find(TexturePath);
	if (ExistingLoad)
	{
		ExistingLoad->PendingRequestIds.Add(RequestId);
		if (ExistingLoad->Handle.IsValid()
			&& ExistingLoad->Handle->HasLoadCompleted())
		{
			// World timers do not advance while gameplay is paused. Dispatch through
			// the task graph so cached loads keep the asynchronous request contract.
			TWeakObjectPtr<UGameZoneSubsystem> WeakThis(this);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, TexturePath]()
			{
				if (UGameZoneSubsystem* Subsystem = WeakThis.Get())
				{
					Subsystem->FinishMapTextureLoad(TexturePath);
				}
			});
		}
		return;
	}

	FGameZoneMapTextureLoad& NewLoad =
		MapPresentations->TextureLoads.Add(TexturePath);
	NewLoad.PendingRequestIds.Add(RequestId);
	NewLoad.Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		TexturePath,
		FStreamableDelegate::CreateWeakLambda(this, [this, TexturePath]()
		{
			FinishMapTextureLoad(TexturePath);
		}));

	if (!NewLoad.Handle.IsValid())
	{
		TWeakObjectPtr<UGameZoneSubsystem> WeakThis(this);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, TexturePath]()
		{
			if (UGameZoneSubsystem* Subsystem = WeakThis.Get())
			{
				Subsystem->FinishMapTextureLoad(TexturePath);
			}
		});
	}
}

void UGameZoneSubsystem::FinishMapTextureLoad(const FSoftObjectPath& TexturePath)
{
	FGameZoneMapTextureLoad* Load = MapPresentations->TextureLoads.Find(TexturePath);
	if (!Load)
	{
		return;
	}

	UTexture2D* Texture = Load->Handle.IsValid()
		? Load->Handle->GetLoadedAsset<UTexture2D>()
		: Cast<UTexture2D>(TexturePath.ResolveObject());
	const TArray<FGuid> PendingRequestIds = Load->PendingRequestIds.Array();

	for (const FGuid& RequestId : PendingRequestIds)
	{
		if (FGameZoneMapTextureLoad* CurrentLoad =
			MapPresentations->TextureLoads.Find(TexturePath))
		{
			CurrentLoad->PendingRequestIds.Remove(RequestId);
		}

		FGameZoneMapTextureRequest* Request =
			MapPresentations->TextureRequests.Find(RequestId);
		if (!Request || Request->CurrentPath != TexturePath)
		{
			continue;
		}

		if (Texture)
		{
			if (FGameZoneMapTextureLoad* CurrentLoad =
				MapPresentations->TextureLoads.Find(TexturePath))
			{
				CurrentLoad->RetainingSessionIds.Add(Request->SessionId);
			}
			CompleteMapTextureRequest(RequestId, Texture);
			continue;
		}

		if (!Request->bUsingDefaultTexture
			&& !Request->DefaultPath.IsNull()
			&& Request->DefaultPath != TexturePath)
		{
			Request->bUsingDefaultTexture = true;
			QueueMapTexturePath(RequestId, Request->DefaultPath);
		}
		else
		{
			CompleteMapTextureRequest(RequestId, nullptr);
		}
	}

	TryReleaseMapTextureLoad(TexturePath);
}

void UGameZoneSubsystem::CompleteMapTextureRequest(
	const FGuid& RequestId,
	UTexture2D* Texture)
{
	FGameZoneMapTextureRequest Request;
	if (!MapPresentations->TextureRequests.RemoveAndCopyValue(RequestId, Request)
		|| !MapPresentations->Sessions.Contains(Request.SessionId))
	{
		return;
	}

	FGameZoneMapTextureResult Result;
	Result.RequestId = RequestId;
	Result.ZoneId = Request.ZoneId;
	Result.SheetId = Request.SheetId;
	Result.Texture = Texture;
	Result.bSucceeded = Texture != nullptr;
	Result.bUsedDefaultTexture = Texture != nullptr && Request.bUsingDefaultTexture;
	Request.Completion.ExecuteIfBound(Result);
}

void UGameZoneSubsystem::CancelMapTextureRequestsForSession(const FGuid& SessionId)
{
	TArray<FGuid> RequestIdsToCancel;
	for (const TPair<FGuid, FGameZoneMapTextureRequest>& Pair :
		MapPresentations->TextureRequests)
	{
		if (Pair.Value.SessionId == SessionId)
		{
			RequestIdsToCancel.Add(Pair.Key);
		}
	}

	TSet<FSoftObjectPath> LoadsToCheck;
	for (const FGuid& RequestId : RequestIdsToCancel)
	{
		FGameZoneMapTextureRequest Request;
		if (MapPresentations->TextureRequests.RemoveAndCopyValue(RequestId, Request))
		{
			if (FGameZoneMapTextureLoad* Load =
				MapPresentations->TextureLoads.Find(Request.CurrentPath))
			{
				Load->PendingRequestIds.Remove(RequestId);
				LoadsToCheck.Add(Request.CurrentPath);
			}
		}
	}

	for (TPair<FSoftObjectPath, FGameZoneMapTextureLoad>& Pair :
		MapPresentations->TextureLoads)
	{
		if (Pair.Value.RetainingSessionIds.Remove(SessionId) > 0)
		{
			LoadsToCheck.Add(Pair.Key);
		}
	}

	for (const FSoftObjectPath& TexturePath : LoadsToCheck)
	{
		TryReleaseMapTextureLoad(TexturePath);
	}
}

void UGameZoneSubsystem::TryReleaseMapTextureLoad(
	const FSoftObjectPath& TexturePath)
{
	FGameZoneMapTextureLoad* Load = MapPresentations->TextureLoads.Find(TexturePath);
	if (!Load
		|| !Load->PendingRequestIds.IsEmpty()
		|| !Load->RetainingSessionIds.IsEmpty())
	{
		return;
	}

	if (Load->Handle.IsValid() && !Load->Handle->HasLoadCompleted())
	{
		Load->Handle->CancelHandle();
	}
	MapPresentations->TextureLoads.Remove(TexturePath);
}

TRPGCoroutine<> UGameZoneSubsystem::LoadMapPresentationZone(
	TWeakObjectPtr<UGameZoneSubsystem> WeakSubsystem,
	FRPGId ZoneId,
	uint32 ZoneGeneration)
{
	UGameZoneSubsystem* InitialSubsystem = WeakSubsystem.Get();
	if (!InitialSubsystem)
	{
		co_return;
	}

	// Keep BeginMapPresentation's completion consistently asynchronous, even
	// when every requested primary asset is already resident.
	co_await RPGFlow::NextTick(InitialSubsystem->GetWorld());
	if (!WeakSubsystem.IsValid())
	{
		co_return;
	}

	const TRPGAsyncResult<UGameZoneAsset*> ZoneResult = co_await
		URPGAssetLibrary::LoadAssetByRPGIdAsync<UGameZoneAsset>(ZoneId, {});

	UGameZoneAsset* ZoneAsset = ZoneResult.IsSuccess()
		? ZoneResult.Value
		: nullptr;
	bool bMarkerTypesLoaded = ZoneAsset != nullptr;

	if (ZoneAsset)
	{
		TArray<FRPGId> MarkerTypeIds;
		for (const FRPGId& MarkerTypeId : ZoneAsset->GetStaticPointMarkerTypes())
		{
			if (MarkerTypeId.IsValid())
			{
				MarkerTypeIds.Add(MarkerTypeId);
			}
		}

		if (!MarkerTypeIds.IsEmpty())
		{
			const TRPGAsyncResult<TArray<UMapMarkerTypeAsset*>> MarkerResult =
				co_await URPGAssetLibrary::LoadAssetArrayByRPGIdsAsync<
					UMapMarkerTypeAsset>(MarkerTypeIds, {});
			bMarkerTypesLoaded = MarkerResult.IsSuccess()
				&& MarkerResult.Value.Num() == MarkerTypeIds.Num();
		}
	}

	if (UGameZoneSubsystem* Subsystem = WeakSubsystem.Get())
	{
		Subsystem->FinishMapPresentationZoneLoad(
			ZoneId,
			ZoneGeneration,
			ZoneAsset,
			bMarkerTypesLoaded);
	}
}

void UGameZoneSubsystem::FinishMapPresentationZoneLoad(
	const FRPGId& ZoneId,
	uint32 ZoneGeneration,
	UGameZoneAsset* ZoneAsset,
	bool bMarkerTypesLoaded)
{
	FGameZonePresentationZone* LoadedZone =
		MapPresentations->Zones.Find(ZoneId);
	if (!LoadedZone
		|| LoadedZone->Generation != ZoneGeneration
		|| LoadedZone->RefCount <= 0)
	{
		return;
	}

	if (!ZoneAsset || !bMarkerTypesLoaded)
	{
		LoadedZone->LoadState = EGameZonePresentationLoadState::Failed;
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[GameZone] Failed to load map presentation assets for Zone %s."),
			*ZoneId.ToString());
	}
	else
	{
		LoadedZone->Asset.Reset(ZoneAsset);
		LoadedZone->MapResolver = MakeUnique<FGameZoneMapResolver>(
			ZoneId,
			ZoneAsset->GetMapLayers(),
			ZoneAsset->GetMapSheets(),
			ZoneAsset->GetBakedSheetMappings(),
			ZoneAsset->GetBakedMapRegions(),
			ZoneAsset->GetDefaultSheetId(),
			ZoneAsset->GetMapBakeRevision());

		const FGameZoneMarkerMutationResult Mutation =
			MarkerRegistry->CommitBakedZone(
				ZoneId,
				ZoneAsset->GetBakedPoints());

		if (Mutation.Error == EGameZoneMarkerMutationError::None)
		{
			LoadedZone->LoadState = EGameZonePresentationLoadState::Loaded;
		}
		else
		{
			LoadedZone->LoadState = EGameZonePresentationLoadState::Failed;
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[GameZone] Rejected baked markers for Zone %s (error %d)."),
				*ZoneId.ToString(),
				static_cast<int32>(Mutation.Error));
		}

		// Publish only after the state transition. A listener may close the
		// presentation re-entrantly and invalidate LoadedZone.
		PublishMarkerMutation(Mutation);
	}

	TryCompleteMapPresentationsWaitingFor(ZoneId);
}

void UGameZoneSubsystem::TryCompleteMapPresentationsWaitingFor(
	const FRPGId& ZoneId)
{
	TArray<FGuid> WaitingSessionIds;
	for (const TPair<FGuid, FGameZonePresentationSession>& Pair :
		MapPresentations->Sessions)
	{
		if (!Pair.Value.bCompletionSent && Pair.Value.ZoneIds.Contains(ZoneId))
		{
			WaitingSessionIds.Add(Pair.Key);
		}
	}

	for (const FGuid& SessionId : WaitingSessionIds)
	{
		TryCompleteMapPresentation(SessionId);
	}
}

void UGameZoneSubsystem::TryCompleteMapPresentation(const FGuid& SessionId)
{
	FGameZonePresentationSession* Session =
		MapPresentations->Sessions.Find(SessionId);
	if (!Session || Session->bCompletionSent)
	{
		return;
	}

	for (const FRPGId& ZoneId : Session->ZoneIds)
	{
		const FGameZonePresentationZone* LoadedZone =
			MapPresentations->Zones.Find(ZoneId);
		if (!LoadedZone
			|| LoadedZone->LoadState == EGameZonePresentationLoadState::Loading)
		{
			return;
		}
	}

	FGameZonePresentationSnapshot Snapshot;
	Snapshot.Handle.SessionId = SessionId;
	Snapshot.Revision = MarkerRegistry->GetRevision();
	Snapshot.Markers = MarkerRegistry->BuildSnapshot(
		Session->ZoneIds,
		Session->Mode);
	for (const FRPGId& ZoneId : Session->ZoneIds)
	{
		const FGameZonePresentationZone* Zone = MapPresentations->Zones.Find(ZoneId);
		if (Zone
			&& Zone->LoadState == EGameZonePresentationLoadState::Loaded
			&& Zone->Asset.IsValid()
			&& Zone->MapResolver)
		{
			AppendMapCatalog(ZoneId, *Zone->Asset, *Zone->MapResolver, Snapshot);
		}
	}

	Snapshot.MapLayers.Sort([](
		const FGameZoneMapLayerCatalogEntry& Left,
		const FGameZoneMapLayerCatalogEntry& Right)
		{
			if (Left.ZoneId != Right.ZoneId)
			{
				return Left.ZoneId.ToString() < Right.ZoneId.ToString();
			}
			if (Left.SortOrder != Right.SortOrder)
			{
				return Left.SortOrder < Right.SortOrder;
			}
			return Left.LayerId.ToString() < Right.LayerId.ToString();
		});
	Snapshot.MapSheets.Sort([](
		const FGameZoneMapSheetCatalogEntry& Left,
		const FGameZoneMapSheetCatalogEntry& Right)
		{
			if (Left.ZoneId != Right.ZoneId)
			{
				return Left.ZoneId.ToString() < Right.ZoneId.ToString();
			}
			if (Left.LayerId != Right.LayerId)
			{
				return Left.LayerId.ToString() < Right.LayerId.ToString();
			}
			if (Left.SortOrder != Right.SortOrder)
			{
				return Left.SortOrder < Right.SortOrder;
			}
			return Left.SheetId.ToString() < Right.SheetId.ToString();
		});
	Snapshot.MapWorldBounds.Sort([](const FGameZoneMapWorldBounds& Left, const FGameZoneMapWorldBounds& Right)
		{
			return Left.ZoneId.ToString() < Right.ZoneId.ToString();
		});

	Session->bCompletionSent = true;
	FOnGameZonePresentationReady Completion = MoveTemp(Session->Completion);
	Completion.ExecuteIfBound(Snapshot);
}

void UGameZoneSubsystem::ReleaseMapPresentationZone(const FRPGId& ZoneId)
{
	FGameZonePresentationZone* LoadedZone =
		MapPresentations->Zones.Find(ZoneId);
	if (!LoadedZone)
	{
		return;
	}

	check(LoadedZone->RefCount > 0);
	if (--LoadedZone->RefCount > 0)
	{
		return;
	}

	++LoadedZone->Generation;
	if (LoadedZone->LoadTask.IsValid() && !LoadedZone->LoadTask.IsDone())
	{
		LoadedZone->LoadTask.Cancel();
	}

	const FGameZoneMarkerMutationResult Mutation =
		MarkerRegistry->RemoveBakedZone(ZoneId);
	MapPresentations->Zones.Remove(ZoneId);
	PublishMarkerMutation(Mutation);
}
FName UGameZoneSubsystem::GetSaveModuleType() const
{
	return FGameZoneSaveModule::StaticStruct()->GetFName();
}

void UGameZoneSubsystem::SaveDataTo(FInstancedStruct& SaveData)
{
	FGameZoneSaveModule ZoneSave;

	ZoneSave.CurrentContext = CurrentContext;
	ZoneSave.PointOverrides = MarkerRegistry->CaptureSaveOverrides();

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	// A save resumes at the exact Pawn transform. EntryId remains the authored
	// destination used by explicit travel and new-game contexts.
	if (Pawn)
	{
		ZoneSave.CurrentContext.bUseSavedTransform = true;
		ZoneSave.CurrentContext.SavedTransform = Pawn->GetActorTransform();
	}

	SaveData.InitializeAs<FGameZoneSaveModule>(ZoneSave);
}

void UGameZoneSubsystem::LoadDataFrom(const FInstancedStruct& SaveData)
{
	FGameZoneContext ContextToLoad;
	const FGameZoneSaveModule* ZoneSave = SaveData.GetPtr<FGameZoneSaveModule>();
	if (ZoneSave)
	{
		const FGameZoneMarkerMutationResult Mutation = MarkerRegistry->ReplaceSaveOverrides(ZoneSave->PointOverrides);
		if (Mutation.Error == EGameZoneMarkerMutationError::None)
		{
			PublishMarkerMutation(Mutation);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[GameZone] Rejected invalid marker save overrides."));
			PublishMarkerMutation(MarkerRegistry->ReplaceSaveOverrides({}));
		}
	}
	else
	{
		PublishMarkerMutation(MarkerRegistry->ReplaceSaveOverrides({}));
	}

	if (ZoneSave)
	{
		ContextToLoad = ZoneSave->CurrentContext;
	}
#if WITH_EDITOR
	else if (bStartGameInstantly)
	{
		ContextToLoad = CurrentContext;

		AGameModeBase* GameMode = UGameplayStatics::GetGameMode(this);

		if (APlayerStartPIE* PIEStart = Cast<APlayerStartPIE>(GameMode->ChoosePlayerStart(UGameplayStatics::GetPlayerController(this, 0))))
		{
			ContextToLoad.bUseSavedTransform = true;
			ContextToLoad.SavedTransform = PIEStart->GetActorTransform();
		}
		else
		{
			ContextToLoad.bUseSavedTransform = false;
		}

		bStartGameInstantly = false;
	}
#endif // WITH_EDITOR
	else
	{
		const UGameZoneSystemSettings* Settings = GetDefault<UGameZoneSystemSettings>();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get RPGSettings"));
			LoadCompleteDelegate.Broadcast();
			return;
		}

		ContextToLoad = Settings->DefaultGameZoneContext;
	}

	EnterGameZone(ContextToLoad, TDelegate<void()>::CreateWeakLambda(this, [this]()
		{
			LoadCompleteDelegate.Broadcast();
		}));
}

FSimpleMulticastDelegate& UGameZoneSubsystem::OnLoadComplete()
{
	return LoadCompleteDelegate;
}
