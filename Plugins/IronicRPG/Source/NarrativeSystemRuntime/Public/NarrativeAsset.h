// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NarrativeRuntimeGraph.h"
#include "Narrative/Dialogue.h"
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
 * 
 */
UCLASS(BlueprintType)
class NARRATIVESYSTEMRUNTIME_API UDialogueBlueprint : public UNarrativeBlueprintBase
{
	GENERATED_BODY()
	
public:
	static FName DialogueGraphName;
	static FName DialogueEventGraphName;
};
