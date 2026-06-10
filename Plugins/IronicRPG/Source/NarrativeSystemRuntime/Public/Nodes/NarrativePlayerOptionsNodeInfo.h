

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NarrativeNodeInfo.h"
#include "NarrativePlayerOptionsNodeInfo.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class NARRATIVESYSTEMRUNTIME_API UNarrativePlayerOptionsNodeInfo : public UNarrativeNodeInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node Info")
	TArray<FText> Options;

#if WITH_EDITOR
private:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
	{
		const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UNarrativePlayerOptionsNodeInfo, Options))
		{
			if (Options.IsEmpty())
			{
				Options.Add({ });
			}
		}
	}
#endif // WITH_EDITOR

};
