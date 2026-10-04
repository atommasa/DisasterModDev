// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DialogueDataTypes.generated.h"

UENUM(BlueprintType)
enum class EOptionDisabledDisplayPolicy : uint8
{
	None,
	ShowDisabled,
	Hide
};

UENUM(BlueprintType)
enum class EOptionSelectedDisplayPolicy : uint8
{
	None,
	ShowNormally,
	ShowAsSelected,
	Hide
};

/**
 * 
 */
USTRUCT(BlueprintType)
struct NARRATIVESYSTEMRUNTIME_API FDialogueLine
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(MultiLine = "true"))
	FText DialogueText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<USoundBase> DialogueSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UAnimMontage> DialogueMontage;

	static void ExtractDialogueVariableNames(const FText& Text, TSet<FName>& OutVariableNames)
	{
		const FString SourceString = Text.ToString();

		// Support:
		// {gold}
		// { player.name }
		// {quest:reward-gold}
		FRegexPattern Pattern(
			TEXT("\\{\\s*([A-Za-z_][A-Za-z0-9_\\.:-]*)\\s*\\}")
		);

		FRegexMatcher Matcher(Pattern, SourceString);

		while (Matcher.FindNext())
		{
			const FString VariableString = Matcher.GetCaptureGroup(1);

			if (!VariableString.IsEmpty())
			{
				OutVariableNames.Add(FName(*VariableString));
			}
		}
	}

};

/**
 *
 */
USTRUCT(BlueprintType)
struct NARRATIVESYSTEMRUNTIME_API FOptionLine
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, meta=(IgnoreForMemberInitializationTest))
	FGuid OptionGuid = FGuid::NewGuid();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FDialogueLine DialogueLine;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bTrackSelected = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName = "Selected Display Policy Overridden"))
	EOptionSelectedDisplayPolicy SelectedDisplayPolicy = EOptionSelectedDisplayPolicy::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName = "Disabled Display Policy Overridden"))
	EOptionDisabledDisplayPolicy DisabledDisplayPolicy = EOptionDisabledDisplayPolicy::None;

};
