// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGId.h"
#include "GameplayTagContainer.h"
#include "SpeakerData.generated.h"

UENUM(BlueprintType)
enum class ESpeakerSource : uint8
{
	// The speaker is defined by a specific CharacterId.
	CharacterId UMETA(DisplayName = "Character ID"),

	// The speaker is defined by a specific literal name.
	LiteralName UMETA(DisplayName = "Literal Name"),

	// The speaker is defined by a specific tag.
	SpeakerTag UMETA(DisplayName = "Speaker Tag"),

	// The speaker is defined by a reference to an actor in the world.
	ActorReference UMETA(DisplayName = "Actor Reference"),
};

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
	ESpeakerSource SpeakerSource = ESpeakerSource::CharacterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker", meta=(Type = "Character", EditCondition = "SpeakerSource == ESpeakerSource::CharacterId", EditConditionHides))
	FRPGId SpeakerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker", meta=(EditCondition = "SpeakerSource != ESpeakerSource::CharacterId", EditConditionHides))
	FText SpeakerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker", meta=(EditCondition = "SpeakerSource == ESpeakerSource::SpeakerTag", EditConditionHides))
	FGameplayTag SpeakerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker", meta=(EditCondition = "SpeakerSource == ESpeakerSource::ActorReference", EditConditionHides))
	TSoftObjectPtr<AActor> SpeakerActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker")
	FColor SpeakerNodeColor = FColor::Silver;

public:
	FText GetSpeakerDisplayName() const;
};
