// Copyright Ironic Studio. All Rights Reserved.


#include "Narrative/SpeakerData.h"
#include "Assets/RPGAssetLibrary.h"
#include "Characters/CharacterAsset.h"

FText FSpeakerData::GetSpeakerDisplayName() const
{
	if (SpeakerSource == ESpeakerSource::CharacterId)
	{
		if (UCharacterAsset* CharacterAsset = Cast<UCharacterAsset>(URPGAssetLibrary::GetAssetByRPGId(SpeakerId)))
		{
			return CharacterAsset->GetDisplayName().DefaultName;
		}
	}
	else
	{
		return SpeakerName;
	}

	return FText::FromString(TEXT("Unknown Speaker"));
}
