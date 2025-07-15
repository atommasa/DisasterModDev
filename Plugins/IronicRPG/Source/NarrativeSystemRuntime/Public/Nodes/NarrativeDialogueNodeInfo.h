

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Kismet/GameplayStatics.h"
#include "NarrativeNodeInfo.h"
#include "NarrativeDialogueNodeInfo.generated.h"

UCLASS()
class NARRATIVESYSTEMRUNTIME_API ANarrativeAudioHelper : public AActor
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Narrative|Audio")
	void Play2DSound(USoundBase* Sound)
	{
		if (Sound)
		{
			UGameplayStatics::PlaySound2D(this, Sound);
		}
	}
};

/**
 * 
 */
UCLASS(BlueprintType)
class NARRATIVESYSTEMRUNTIME_API UNarrativeDialogueNodeInfo : public UNarrativeNodeInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Details")
	FRPGId SpeakerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Line")
	FText Dialogue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Line")
	TSoftObjectPtr<USoundBase> DialogueSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Line")
	TSoftObjectPtr<UAnimMontage> DialogueMontage;

#if WITH_EDITOR
public:
	UFUNCTION(CallInEditor, Category = "Editor Debug")
	void TestDialogueSound()
	{
		for (TObjectIterator<UWorld> It; It; ++It)
		{
			UWorld* World = *It;
			if (World->WorldType == EWorldType::Editor)
			{
				TWeakObjectPtr<ANarrativeAudioHelper> Helper = World->SpawnActor<ANarrativeAudioHelper>();
				if (Helper.IsValid())
				{
					Helper->Play2DSound(DialogueSound.Get());
					Helper->SetLifeSpan(3.0f);
				}

				break;
			}
		}
	}

#endif

};