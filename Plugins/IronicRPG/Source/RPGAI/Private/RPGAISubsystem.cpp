// Copyright Ironic Studio. All Rights Reserved.


#include "RPGAISubsystem.h"
#include "FollowLocationTracker.h"
#include "Kismet/GameplayStatics.h"

#include "PartyFollow/PartyFollowComponent.h"

#include "Characters/BaseCharacter.h"
#include "CharacterSubsystem.h"
#include "Controllers/RPGPlayerController.h"

void URPGAISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

}

void URPGAISubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (UCharacterSubsystem* CharacterSubsystem = UGameplayStatics::GetGameInstance(this)->GetSubsystem<UCharacterSubsystem>())
	{
		CharacterSubsystem->OnPartyConstructed.AddWeakLambda(this, [this, CharacterSubsystem]()
			{
				ConfigurePartyFollow(CharacterSubsystem->GetPlayerCharacter(), true);
			});
	}

	if (ARPGPlayerController* PC = Cast<ARPGPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		PC->OnControlledCharacterChanged.AddUniqueDynamic(this, &URPGAISubsystem::OnControlledCharacterChanged);
	}
}

void URPGAISubsystem::Deinitialize()
{
	Super::Deinitialize();

}

void URPGAISubsystem::OnControlledCharacterChanged(const ABaseCharacter* NewCharacter, const ABaseCharacter* OldCharacter)
{
	const bool bIsControlledCharacterSwitch = OldCharacter && OldCharacter != NewCharacter;
	ConfigurePartyFollow(NewCharacter, bIsControlledCharacterSwitch);
}

void URPGAISubsystem::ConfigurePartyFollow(
	const ABaseCharacter* LeaderCharacter,
	bool bWaitForFormationActivation)
{
	if (!LeaderCharacter)
	{
		return;
	}

	UPartyFollowComponent* LeaderFollow = LeaderCharacter->FindComponentByClass<UPartyFollowComponent>();
	if (!LeaderFollow)
	{
		return;
	}

	LeaderFollow->SetAsLeader();

	UCharacterSubsystem* CharacterSubsystem = UGameplayStatics::GetGameInstance(this)->GetSubsystem<UCharacterSubsystem>();
	if (!CharacterSubsystem)
	{
		return;
	}

	TArray<ABaseCharacter*> Followers;
	for (ABaseCharacter* Member : CharacterSubsystem->GetPartyMemberInstances())
	{
		if (!Member || Member == LeaderCharacter)
		{
			continue;
		}

		Followers.Add(Member);
	}

	// The closest member receives the first longitudinal position. This order is
	// assigned once per formation configuration and remains stable afterwards.
	Followers.StableSort([LeaderCharacter](const ABaseCharacter& A, const ABaseCharacter& B)
	{
		return FVector::DistSquared(A.GetActorLocation(), LeaderCharacter->GetActorLocation())
			< FVector::DistSquared(B.GetActorLocation(), LeaderCharacter->GetActorLocation());
	});

	EPartyFollowSide FirstSide = EPartyFollowSide::Left;
	if (!Followers.IsEmpty())
	{
		const FVector ToClosestFollower =
			Followers[0]->GetActorLocation() - LeaderCharacter->GetActorLocation();

		FirstSide = FVector::DotProduct(ToClosestFollower, LeaderCharacter->GetActorRightVector()) >= 0.0f
			? EPartyFollowSide::Right
			: EPartyFollowSide::Left;
	}

	int32 Order = 0;
	for (ABaseCharacter* FollowerCharacter : Followers)
	{
		if (UPartyFollowComponent* Follow = FollowerCharacter->FindComponentByClass<UPartyFollowComponent>())
		{
			Follow->SetAsFollower(
				LeaderCharacter,
				Order,
				bWaitForFormationActivation);

			const bool bUseFirstSide = Order % 2 == 0;
			const EPartyFollowSide AssignedSide = bUseFirstSide
				? FirstSide
				: (FirstSide == EPartyFollowSide::Left
					? EPartyFollowSide::Right
					: EPartyFollowSide::Left);

			Follow->SetPreferredSide(AssignedSide);
			++Order;
		}
	}
}
