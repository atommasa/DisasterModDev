// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/RPGGameInstanceSubsystem.h"
#include "Templates/PimplPtr.h"

#include "RPGFlow.h"

#include "Levels/GameZoneContext.h"
#include "Levels/GameZoneMarkerAction.h"
#include "Levels/GameZonePointData.h"
#include "Levels/GameZonePointRegistryProvider.h"
#include "MapMarkers/GameZoneMarkerTypes.h"
#include "Maps/GameZoneMapTypes.h"

#include "SaveGame/Saveable.h"

#include "GameZoneSubsystem.generated.h"

class UGameZonePointComponent;
class APlayerController;
class FGameZoneMarkerRegistry;
struct FGameZoneMapPresentationState;
struct FGameZoneMarkerMutationResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameZoneTravelCompleted, const FGameZoneContext&, Context);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMapMarkerChange, const FMapMarkerChange&, Change);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnGameZonePresentationReady, const FGameZonePresentationSnapshot&, Snapshot);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnGameZoneMapTextureReady, const FGameZoneMapTextureResult&, Result);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnGameZoneMarkerActionCompleted, const FGameZoneMarkerActionResult&, Result);

#if WITH_EDITOR
DECLARE_MULTICAST_DELEGATE(FOnStartGameInstantly);
#endif // WITH_EDITOR

/**
 * 
 */
UCLASS(Blueprintable)
class GAMEZONESYSTEM_API UGameZoneSubsystem : public URPGGameInstanceSubsystem,
	public ISaveable,
	public IGameZonePointRegistryProvider
{
	GENERATED_BODY()

public:
	UGameZoneSubsystem();
	virtual ~UGameZoneSubsystem() override;

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	// Enters a game zone with the specified context.
	UFUNCTION(BlueprintCallable, Category = "RPG|GameZone")
	void EnterGameZone(const FGameZoneContext& NewContext);
	void EnterGameZone(const FGameZoneContext& NewContext, TDelegate<void()> Completion);

	void HandlePostSeamlessTravel();

	UPROPERTY(BlueprintAssignable, Category = "RPG|GameZone")
	FOnGameZoneTravelCompleted OnGameZoneTravelCompleted;

	UFUNCTION(BlueprintCallable, Category = "RPG|GameZone")
	FGameZoneContext GetCurrentContext() const { return CurrentContext; }

	UFUNCTION(BlueprintCallable, Category = "RPG|GameZone")
	void UpdateCurrentContext();

	// Only used for loading the player start point when loading a game, it will find the player start point based on the current context and return its transform
	UFUNCTION(BlueprintCallable, Category = "RPG|GameZone")
	FTransform ResolveEntryTransform() const;

#if WITH_EDITOR
	FOnStartGameInstantly OnStartGameInstantly;
#endif // WITH_EDITOR

private:
	TRPGCoroutine<> BeginZoneTravel(const FGameZoneContext& NewContext, TDelegate<void()> Completion);
	TRPGCoroutine<> CompleteZoneTravel();

public:
	/** True until travel completion callbacks have finished. Presentation adapters must wait before starting a new session. */
	bool IsTravelling() const { return bIsTravelling; }

private:
	void FailZoneTravel(const FString& Reason);

	bool ResolveZoneMap(const FRPGId& ZoneId, FString& OutMapPackageName) const;

public:
	virtual EGameZonePointRegistrationResult RegisterPoint(
		UGameZonePointComponent& Component) override;
	virtual void UnregisterPoint(UGameZonePointComponent& Component) override;
	virtual void NotifyPointChanged(UGameZonePointComponent& Component) override;

	UFUNCTION(BlueprintCallable, Category = "RPG|GameZone|Marker")
	bool TryResolveMapMarker(
		const FGuid& PointId,
		FResolvedGameZonePoint& OutPoint) const;

	UFUNCTION(BlueprintCallable, Category = "RPG|GameZone|Marker|Save")
	bool SetMapMarkerSaveOverride(const FGameZonePointData& Override);

	UFUNCTION(BlueprintCallable, Category = "RPG|GameZone|Marker|Save")
	bool ClearMapMarkerSaveOverride(const FGuid& PointId);

	UFUNCTION(BlueprintCallable, Category = "RPG|GameZone|Marker")
	bool GetMapMarkerActionOptions(
		const FGuid& PointId,
		EMapMarkerDisplayMode PresentationMode,
		APlayerController* PlayerController,
		TArray<FGameZoneMarkerActionOption>& OutOptions) const;

	UFUNCTION(BlueprintCallable, Category = "RPG|GameZone|Marker")
	FGuid ExecuteMapMarkerAction(
		const FGuid& PointId,
		const FGameplayTag& ActionTag,
		EMapMarkerDisplayMode PresentationMode,
		APlayerController* PlayerController,
		FOnGameZoneMarkerActionCompleted Completion);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "RPG|GameZone")
	TObjectPtr<class UGameZoneAsset> CurrentAsset = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "RPG|GameZone")
	FGameZoneContext CurrentContext;

	UPROPERTY(BlueprintReadOnly, Category = "RPG|GameZone")
	FGameZoneContext PendingContext;

	bool bIsTravelling = false;

	TDelegate<void()> PendingTravelCompletion;

#if WITH_EDITOR
	bool bStartGameInstantly = false;
#endif // WITH_EDITOR

public:
	UFUNCTION(BlueprintCallable, Category = "RPG|GameZone|Map")
	FGameZonePresentationHandle BeginMapPresentation(
		const TArray<FRPGId>& ZoneIds,
		EMapMarkerDisplayMode Mode,
		FOnGameZonePresentationReady Completion);

	UFUNCTION(BlueprintCallable, Category = "RPG|GameZone|Map")
	void EndMapPresentation(FGameZonePresentationHandle Handle);

	UFUNCTION(BlueprintCallable, Category = "RPG|GameZone|Map")
	bool ResolveMapSheetAtLocation(
		const FRPGId& ZoneId,
		const FVector& WorldLocation,
		FResolvedGameZoneMapSheet& OutSheet) const;

	UFUNCTION(BlueprintCallable, Category = "RPG|GameZone|Map")
	bool ResolveMapSheetInLayerAtLocation(
		const FRPGId& ZoneId,
		const FGameZoneMapLayerId& LayerId,
		const FVector& WorldLocation,
		FResolvedGameZoneMapSheet& OutSheet) const;

	UFUNCTION(BlueprintCallable, Category = "RPG|GameZone|Map")
	bool ResolveMapSheetById(
		const FRPGId& ZoneId,
		const FGameZoneMapSheetId& SheetId,
		FResolvedGameZoneMapSheet& OutSheet) const;

	UFUNCTION(BlueprintCallable, Category = "RPG|GameZone|Map")
	bool ProjectWorldLocationToMapSheet(
		const FRPGId& ZoneId,
		const FGameZoneMapSheetId& SheetId,
		const FVector& WorldLocation,
		FGameZoneMapProjection& OutProjection) const;

	UFUNCTION(BlueprintCallable, Category = "RPG|GameZone|Map")
	FGuid RequestMapSheetTexture(
		FGameZonePresentationHandle Handle,
		const FRPGId& ZoneId,
		const FGameZoneMapSheetId& SheetId,
		FOnGameZoneMapTextureReady Completion);

public:
	UPROPERTY(BlueprintAssignable, Category = "RPG|GameZone")
	FOnMapMarkerChange OnMapMarkerChange;

private:
	void HandlePostGarbageCollect();
	void HandleMarkerActionCompleted(const FGuid& ExecutionId, const FGameZoneMarkerActionResult& Result);
	void PublishMarkerMutation(const FGameZoneMarkerMutationResult& Mutation);
	static TRPGCoroutine<> LoadMapPresentationZone(
		TWeakObjectPtr<UGameZoneSubsystem> WeakSubsystem,
		FRPGId ZoneId,
		uint32 ZoneGeneration);
	void FinishMapPresentationZoneLoad(
		const FRPGId& ZoneId,
		uint32 ZoneGeneration,
		UGameZoneAsset* ZoneAsset,
		bool bMarkerTypesLoaded);
	void TryCompleteMapPresentationsWaitingFor(const FRPGId& ZoneId);
	void TryCompleteMapPresentation(const FGuid& SessionId);
	void ReleaseMapPresentationZone(const FRPGId& ZoneId);
	void QueueMapTexturePath(const FGuid& RequestId, const FSoftObjectPath& TexturePath);
	void FinishMapTextureLoad(const FSoftObjectPath& TexturePath);
	void CompleteMapTextureRequest(const FGuid& RequestId, UTexture2D* Texture);
	void CancelMapTextureRequestsForSession(const FGuid& SessionId);
	void TryReleaseMapTextureLoad(const FSoftObjectPath& TexturePath);

	TPimplPtr<FGameZoneMarkerRegistry> MarkerRegistry;
	TPimplPtr<FGameZoneMapPresentationState> MapPresentations;
	FDelegateHandle PostGarbageCollectHandle;

	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UGameZoneMarkerAction>> ActiveMarkerActions;

	TMap<FGuid, FOnGameZoneMarkerActionCompleted> MarkerActionCompletions;

public: // ISaveable
	virtual FName GetSaveModuleType() const override;
	virtual void SaveDataTo(FInstancedStruct& SaveData) override;
	virtual void LoadDataFrom(const FInstancedStruct& SaveData) override;

	virtual FSimpleMulticastDelegate& OnLoadComplete() override;
	FSimpleMulticastDelegate LoadCompleteDelegate;
};
