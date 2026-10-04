// Copyright Ironic Studio. All Rights Reserved.


#include "Settings/CharacterSystemSettings.h"

#if WITH_EDITOR
void UCharacterSystemSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName& PropertyName = PropertyChangedEvent.MemberProperty->GetFName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UCharacterSystemSettings, DefaultPartyMembers) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UCharacterSystemSettings, MaxPartyMembers))
	{
		if (DefaultPartyMembers.Num() > MaxPartyMembers)
		{
			DefaultPartyMembers.SetNum(MaxPartyMembers);
		}
	}
}
#endif // WITH_EDITOR