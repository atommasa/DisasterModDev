// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NarrativeRuntimeGraph.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "Narrative/NarrativeEventBlueprintBase.h"
#include "NarrativeAsset.generated.h"

UCLASS(BlueprintType)
class NARRATIVESYSTEMRUNTIME_API UNarrativeBlueprintBase : public UBlueprint
{
	GENERATED_BODY()

public:
	void SetPreSaveListener(TFunction<void()> OnPreSaveListener) { _OnPreSaveListener = OnPreSaveListener; }

	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual void GetTypeActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;

	mutable TWeakPtr<class NarrativeAssetEditorApp> EditorApp;

public:	// Helper functions
	template<typename T>
	static T* TryGetGeneratedCDO(const UNarrativeBlueprintBase* Blueprint)
	{
		if (!Blueprint || !Blueprint->GeneratedClass)
		{
			return nullptr;
		}

		return Cast<T>(Blueprint->GeneratedClass->GetDefaultObject());
	}

	UPROPERTY()
	UNarrativeRuntimeGraph* Graph = nullptr;

private:
	TFunction<void()> _OnPreSaveListener = nullptr;
};

/**
 * This class is used to store narrative data, such as the speakers and their dialogues.
 */
UCLASS(BlueprintType)
class NARRATIVESYSTEMRUNTIME_API UDialogueBlueprint : public UNarrativeBlueprintBase
{
	GENERATED_BODY()
	
public:

};
