// Copyright Ironic Studio. All Rights Reserved.


#include "Misc/RPGTeamAgentInterface.h"
#include "RPGSettings.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(Team_ROOT, "Team", "Root tag for all team tags. You can add subtags under Ally, Enemy, and Neutral to create more specific team tags. It is not recommended to add tags directly under the root tag, as it may cause unintended relationships between teams.")
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Team_Ally, "Team.Ally", "This tag is used to identify allies.")
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Team_Enemy, "Team.Enemy", "This tag is used to identify enemies.")
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Team_Neutral, "Team.Neutral", "This tag is used to identify neutral actors.")

FTeamRelationInfo FTeamRelationInfo::AllyTeamRelationInfo = {
	.HostileAgainstTags = FGameplayTagContainer(Team_Enemy),
};

FTeamRelationInfo FTeamRelationInfo::EnemyTeamRelationInfo = {
	.HostileAgainstTags = FGameplayTagContainer(Team_Ally),
};

ETeamAttitude::Type IRPGTeamAgentInterface::GetTeamAttitudeTowards(const AActor* Other) const
{
	const IRPGTeamAgentInterface* OtherTeamAgent = Cast<const IRPGTeamAgentInterface>(Other);
	if (!OtherTeamAgent)
	{
		return ETeamAttitude::Neutral;
	}
	
	return GetTeamAttitudeTowards(OtherTeamAgent);
}

ETeamAttitude::Type IRPGTeamAgentInterface::GetTeamAttitudeTowards(const IRPGTeamAgentInterface* Other) const
{
	if (!Other)
	{
		return ETeamAttitude::Neutral;
	}

	const URPGSettings* RPGSettings = URPGSettings::GetRPGSettings();
	if (!RPGSettings)
	{
		return ETeamAttitude::Neutral;
	}

	const FTeamRelationInfo* RelationInfo = FTeamRelationInfo::FindTeamRelationInfoInMap(RPGSettings->TeamRelations, GetTeamTag());
	if (!RelationInfo)
	{
		return ETeamAttitude::Neutral;
	}

	if (RelationInfo->HostileAgainstTags.HasTag(Other->GetTeamTag()))
	{
		return ETeamAttitude::Hostile;
	}
	else if (RelationInfo->FriendlyWithTags.HasTag(Other->GetTeamTag()))
	{
		return ETeamAttitude::Friendly;
	}

	return ETeamAttitude::Neutral;
}

const FTeamRelationInfo* FTeamRelationInfo::FindTeamRelationInfoInMap(const TMap<FGameplayTag, FTeamRelationInfo>& Map, const FGameplayTag& TeamTag)
{
	if (Map.IsEmpty() || !TeamTag.IsValid())
	{
		return nullptr;
	}
	
	if (const FTeamRelationInfo* RelationInfo = Map.Find(TeamTag))
	{
		return RelationInfo;
	}

	FGameplayTag ParentTag = TeamTag.RequestDirectParent();
	if (ParentTag.IsValid())
	{
		return FindTeamRelationInfoInMap(Map, ParentTag);
	}

	return nullptr;
}
