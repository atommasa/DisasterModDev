

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Nodes/DialogueVariableParsable.h"
#include "Kismet/GameplayStatics.h"
#include "NarrativeNodeInfo.h"
#include "Narrative/DialogueDataTypes.h"
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
class NARRATIVESYSTEMRUNTIME_API UNarrativeDialogueNodeInfo : public UNarrativeNodeInfo, public IDialogueVariableParsable
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FText SpeakerName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText OverriddenName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FDialogueLine DialogueLine;

public: // IDialogueVariableParsable interface
	virtual void GetDialogueLinesForVariableParsing(TArray<FDialogueLine>& OutDialogueLines) const override { OutDialogueLines.Add(DialogueLine); }

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
					Helper->Play2DSound(DialogueLine.DialogueSound.Get());
					Helper->SetLifeSpan(3.0f);
				}

				break;
			}
		}
	}

#endif

};