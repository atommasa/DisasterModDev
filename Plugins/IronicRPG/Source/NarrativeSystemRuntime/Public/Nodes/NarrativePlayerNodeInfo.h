

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NarrativeNodeInfo.h"
#include "NarrativePlayerNodeInfo.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class NARRATIVESYSTEMRUNTIME_API UNarrativePlayerNodeInfo : public UNarrativeNodeInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node Info")
	TArray<FText> Options;

};
