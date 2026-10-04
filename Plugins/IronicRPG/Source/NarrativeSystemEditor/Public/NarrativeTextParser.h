// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NarrativeTextParser.generated.h"

/**
 *
 */
USTRUCT()
struct FParsedDialogueLine
{
	GENERATED_BODY() 

	UPROPERTY()
	FString Speaker;

	UPROPERTY()
	FString Dialogue;

};

/**
 * 
 */
class NarrativeTextParser
{
public:
	static TArray<FParsedDialogueLine> ParseFromText(const FString& InRawText);
	static bool CanParseFromText(const FString& InRawText);
};
