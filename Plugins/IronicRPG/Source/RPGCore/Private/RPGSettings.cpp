// Copyright Ironic Studio. All Rights Reserved.


#include "RPGSettings.h"

URPGSettings::URPGSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

#if WITH_EDITOR
void URPGSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(URPGSettings, DefaultPartyMembers))
	{
		CLAMPED_ARRAY(DefaultPartyMembers, MaxPartyMembers)
	}
}
#endif // WITH_EDITOR