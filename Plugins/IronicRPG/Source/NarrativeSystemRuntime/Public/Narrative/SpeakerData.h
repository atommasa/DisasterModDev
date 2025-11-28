// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGId.h"
#include "SpeakerData.generated.h"

class UCharacterAsset;

/**
 * 
 */
USTRUCT(BlueprintType)
struct NARRATIVESYSTEMRUNTIME_API FSpeakerData
{
	GENERATED_BODY()

public:
	FSpeakerData() = default;

	FSpeakerData(FRPGId InSpeakerId)
		: SpeakerId(InSpeakerId) {};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker")
	FRPGId SpeakerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker")
	TSoftObjectPtr<UCharacterAsset> SpeakerAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker")
	FColor SpeakerNodeColor = FColor::Silver;
};
