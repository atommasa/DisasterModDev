// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "GenericTeamAgentInterface.h"
#include "RPGTeamAgentInterface.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(Team_ROOT)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Team_Ally)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Team_Enemy)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Team_Neutral)

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class URPGTeamAgentInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for actors that can be assigned to teams.
 * This is used for things like AI perception and damage types to determine if an actor should be considered an ally or an enemy.
 */
class RPGCORE_API IRPGTeamAgentInterface
{
	GENERATED_BODY()

public:
	// Returns the default team tag for this actor. It can be overridden by child classes to provide different default team tags.
	virtual const FGameplayTag& GetDefaultTeamTag() const = 0;

	// Returns the attitude of this actor towards another actor based on their team tags. It can be overridden by child classes to provide custom attitude logic.
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor* Other) const;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const IRPGTeamAgentInterface* Other) const;

	// Gets the current team tag of this actor. This can be used to determine the team of this actor at runtime.
	const FGameplayTag& GetTeamTag() const { return TeamTag; }

	// Sets the team tag of this actor. This can be used to change the team of this actor at runtime.
	void SetTeamTag(const FGameplayTag& NewTeamTag) { TeamTag = NewTeamTag;}

	// Resets the team tag to the default team tag. It can be overridden by child classes to provide custom reset behavior.
	void ResetDefaultTeamTag() { TeamTag = GetDefaultTeamTag(); }

private:
	// The current team tag of this actor. This can be changed at runtime to change the team of this actor.
	FGameplayTag TeamTag;

};

/**
 * Struct that defines the relationship between teams. This is used to determine if an actor should be considered an ally or an enemy based on their team tag.
 * If an actor's team tag does not match any of the tags in HostileAgainstTags or FriendlyWithTags, it will be considered neutral.
 */
USTRUCT(BlueprintType)
struct FTeamRelationInfo
{
	GENERATED_BODY()

	// Tags that this team is hostile towards. If an actor's team tag matches any of these tags, it will be considered an enemy.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(Categories ="Team"))
	FGameplayTagContainer HostileAgainstTags;

	// Tags that this team is friendly towards. If an actor's team tag matches any of these tags, it will be considered an ally.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(Categories = "Team"))
	FGameplayTagContainer FriendlyWithTags;

	// Recursively searches the provided map for a FTeamRelationInfo associated with the given team tag. If no direct match is found, it will check the parent tags of the team tag until a match is found or there are no more parent tags.
	static const FTeamRelationInfo* FindTeamRelationInfoInMap(const TMap<FGameplayTag, FTeamRelationInfo>& Map, const FGameplayTag& TeamTag);

	static FTeamRelationInfo AllyTeamRelationInfo;
	static FTeamRelationInfo EnemyTeamRelationInfo;

};