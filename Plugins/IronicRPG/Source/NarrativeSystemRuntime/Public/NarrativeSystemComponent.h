// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NarrativeSystemComponent.generated.h"

class UDialogue;

UCLASS(ClassGroup = NarrativeSystem, hidecategories=(Object, LOD, Lighting, Transform, Sockets, TextureStreaming), editinlinenew, meta=(BlueprintSpawnableComponent))
class NARRATIVESYSTEMRUNTIME_API UNarrativeSystemComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable)
	bool BeginDialogue(TSubclassOf<UDialogue> DialogueClass);

};
