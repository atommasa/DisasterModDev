// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/NarrativeNodeType.h"
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
