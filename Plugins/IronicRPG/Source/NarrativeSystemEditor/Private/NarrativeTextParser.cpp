// Fill out your copyright notice in the Description page of Project Settings.


#include "NarrativeTextParser.h"

TArray<FParsedDialogueLine> NarrativeTextParser::ParseFromText(const FString& InRawText)
{
	TArray<FParsedDialogueLine> Result;
	TArray<FString> Lines;
	InRawText.ParseIntoArrayLines(Lines, true);

	for (const FString& Line : Lines)
	{
		FString Trimmed = Line.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			continue;
		}

		FString Speaker;
		FString Dialogue;

		if (Trimmed.Split(TEXT(":"), &Speaker, &Dialogue))
		{
			Result.Add({
					Speaker.TrimStartAndEnd(),
					Dialogue.TrimStartAndEnd(),
				});
		}
	}

	return Result;
}

bool NarrativeTextParser::CanParseFromText(const FString& InRawText)
{
	return !NarrativeTextParser::ParseFromText(InRawText).IsEmpty();
}
