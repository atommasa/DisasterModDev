// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ComponentInstanceDataCache.h"
#include "Levels/GameZonePointData.h"
#include "GameZonePointComponent.generated.h"

class UGameZonePointComponent;
class UGameInstanceSubsystem;

USTRUCT()
struct RPGCORE_API FGameZonePointComponentInstanceData : public FActorComponentInstanceData
{
	GENERATED_BODY()

public:
	FGameZonePointComponentInstanceData() = default;
	explicit FGameZonePointComponentInstanceData(const UGameZonePointComponent* SourceComponent);

	virtual bool ContainsData() const override { return true; }
	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override;

	UPROPERTY()
	FGuid PointId;
};

UCLASS( ClassGroup=(Map), meta=(BlueprintSpawnableComponent) )
class RPGCORE_API UGameZonePointComponent : public UActorComponent
{
	GENERATED_BODY()
	friend struct FGameZonePointComponentInstanceData;

public:
	UGameZonePointComponent(const FObjectInitializer& ObjectInitializer);
	virtual void OnComponentCreated() override;
	virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;

protected:
	virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UGameInstanceSubsystem* FindRegistryProvider() const;

#if WITH_EDITOR
    virtual void PostEditImport() override;
#endif // WITH_EDITOR

protected:
    UPROPERTY(VisibleAnywhere, Category = "Marker")
    FGuid PointId = FGuid::NewGuid();

    UPROPERTY(EditAnywhere, Category = "Marker")
    FGameZonePointData PointData;

    UPROPERTY(EditAnywhere, Category = "Marker")
    EGameZonePointUpdateMode UpdateMode = EGameZonePointUpdateMode::EventDriven;

	TWeakObjectPtr<UGameInstanceSubsystem> RegistryProvider;
	bool bRegisteredWithProvider = false;

public:
    UFUNCTION(BlueprintCallable, Category = "RPG|GameZone")
    FGuid GetPointId() const { return PointId; }

    UFUNCTION(BlueprintCallable, Category = "RPG|GameZone")
    FGameZonePointData MakePointSnapshot() const;

    // Applies persisted marker data without moving the owning Actor or
    // publishing a change. The registry publishes after source resolution.
    bool ApplyPointDataInitialization(const FGameZonePointData& InitialData);

    EGameZonePointUpdateMode GetUpdateMode() const { return UpdateMode; }

    UFUNCTION(BlueprintCallable, Category = "RPG|GameZone|Marker")
    void SetMarkerState(EGameZonePointState NewState);

    UFUNCTION(BlueprintCallable, Category = "RPG|GameZone|Marker")
    void MarkMarkerDirty();

#if WITH_EDITOR
    bool ShouldBakeMarker() const { return bBakeMarker; }
#endif

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Marker")
    bool bBakeMarker = false;
#endif // WITH_EDITORONLY_DATA

};
