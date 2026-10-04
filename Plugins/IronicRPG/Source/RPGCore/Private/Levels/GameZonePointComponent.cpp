// Copyright Ironic Studio. All Rights Reserved.


#include "Levels/GameZonePointComponent.h"
#include "Levels/GameZonePointRegistryProvider.h"

#include "Engine/GameInstance.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/UObjectIterator.h"

FGameZonePointComponentInstanceData::FGameZonePointComponentInstanceData(
	const UGameZonePointComponent* SourceComponent)
	: FActorComponentInstanceData(SourceComponent)
	, PointId(SourceComponent->GetPointId())
{
}

void FGameZonePointComponentInstanceData::ApplyToComponent(
	UActorComponent* Component,
	const ECacheApplyPhase CacheApplyPhase)
{
	Super::ApplyToComponent(Component, CacheApplyPhase);

	if (CacheApplyPhase == ECacheApplyPhase::PostUserConstructionScript)
	{
		CastChecked<UGameZonePointComponent>(Component)->PointId = PointId;
	}
}

UGameZonePointComponent::UGameZonePointComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	
}

void UGameZonePointComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

	if (IsTemplate())
	{
		return;
	}

	const UGameZonePointComponent* Archetype = Cast<UGameZonePointComponent>(GetArchetype());
	const bool bInheritedArchetypeId = Archetype && Archetype != this && PointId == Archetype->PointId;

	if (!PointId.IsValid() || bInheritedArchetypeId)
	{
		PointId = FGuid::NewGuid();
	}
}

TStructOnScope<FActorComponentInstanceData>
UGameZonePointComponent::GetComponentInstanceData() const
{
	return MakeStructOnScope<
		FActorComponentInstanceData,
		FGameZonePointComponentInstanceData>(this);
}

void UGameZonePointComponent::BeginPlay()
{
	Super::BeginPlay();

	UGameInstanceSubsystem* ProviderObject = FindRegistryProvider();
	IGameZonePointRegistryProvider* Provider =
		Cast<IGameZonePointRegistryProvider>(ProviderObject);
	if (!Provider)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[GameZone] No marker registry provider for %s."),
			*GetPathName());
#endif
		return;
	}

	const EGameZonePointRegistrationResult Result = Provider->RegisterPoint(*this);
	if (Result == EGameZonePointRegistrationResult::Registered
		|| Result == EGameZonePointRegistrationResult::AlreadyRegistered)
	{
		RegistryProvider = ProviderObject;
		bRegisteredWithProvider = true;
	}
}

void UGameZonePointComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bRegisteredWithProvider)
	{
		if (IGameZonePointRegistryProvider* Provider =
			Cast<IGameZonePointRegistryProvider>(RegistryProvider.Get()))
		{
			Provider->UnregisterPoint(*this);
		}
	}

	RegistryProvider.Reset();
	bRegisteredWithProvider = false;

	Super::EndPlay(EndPlayReason);
}

UGameInstanceSubsystem* UGameZonePointComponent::FindRegistryProvider() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		return nullptr;
	}

	UGameInstanceSubsystem* FoundProvider = nullptr;
	for (TObjectIterator<UGameInstanceSubsystem> It; It; ++It)
	{
		if (It->GetGameInstance() != GameInstance
			|| !It->GetClass()->ImplementsInterface(
				UGameZonePointRegistryProvider::StaticClass()))
		{
			continue;
		}

		if (FoundProvider)
		{
#if !UE_BUILD_SHIPPING
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[GameZone] Multiple marker registry providers for %s."),
				*GameInstance->GetPathName());
#endif
			return nullptr;
		}

		FoundProvider = *It;
	}

	return FoundProvider;
}

#if WITH_EDITOR
void UGameZonePointComponent::PostEditImport()
{
	Super::PostEditImport();

	PointId = FGuid::NewGuid();
}
#endif // WITH_EDITOR

FGameZonePointData UGameZonePointComponent::MakePointSnapshot() const
{
	FGameZonePointData Result = PointData;
	Result.PointId = PointId;

	if (const AActor* Owner = GetOwner())
	{
		Result.WorldTransform = Owner->GetActorTransform();
	}

	return Result;
}

bool UGameZonePointComponent::ApplyPointDataInitialization(const FGameZonePointData& InitialData)
{
	if (!InitialData.PointId.IsValid() || InitialData.PointId != PointId)
	{
		return false;
	}

	PointData = InitialData;
	return true;
}

void UGameZonePointComponent::SetMarkerState(EGameZonePointState NewState)
{
	if (PointData.MarkerState == NewState)
	{
		return;
	}

	PointData.MarkerState = NewState;
	MarkMarkerDirty();
}

void UGameZonePointComponent::MarkMarkerDirty()
{
	if (!bRegisteredWithProvider)
	{
		return;
	}

	if (IGameZonePointRegistryProvider* Provider =
		Cast<IGameZonePointRegistryProvider>(RegistryProvider.Get()))
	{
		Provider->NotifyPointChanged(*this);
	}
}
