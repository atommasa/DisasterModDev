// Copyright Ironic Studio. All Rights Reserved.


#include "Assets/NarrativeAssetAction.h"
#include "NarrativeAsset.h"
#include "Assets/NarrativeAssetEditorApp.h"
#include "Narrative/Dialogue.h"

NarrativeAssetAction::NarrativeAssetAction(EAssetTypeCategories::Type Category)
{
	AssetCategory = Category;
}

UClass* NarrativeAssetAction::GetSupportedClass() const
{
	return UDialogueBlueprint::StaticClass();
}

void NarrativeAssetAction::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	// Check if the EditWithinLevelEditor is valid
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	// [Step 2] 初始化自訂編輯器
	TSharedRef<NarrativeAssetEditorApp> Editor = MakeShared<NarrativeAssetEditorApp>();
	TArray<UBlueprint*> NarrativeBlueprints; // Array to hold the selected blueprints

	for (UObject* Object : InObjects) // Iterate through the selected objects
	{
		UE_LOG(LogTemp, Warning, TEXT(">>> Trying to open asset of type: %s"), *Object->GetClass()->GetName());

		// 如果傳入的是 Blueprint 資產，直接打開編輯器
		if (UDialogueBlueprint* NarrativeBlueprint = Cast<UDialogueBlueprint>(Object))
		{
			// [Step 1] 檢查是否有 parent class，否則設置預設 ParentClass
			if (!NarrativeBlueprint->ParentClass || NarrativeBlueprint->ParentClass == UObject::StaticClass())
			{
				NarrativeBlueprint->ParentClass = UDialogue::StaticClass(); // 你想指定的類別
			}

			// [Step 3] 建立反向引用（可選）
			NarrativeBlueprints.Add(NarrativeBlueprint); // Add the blueprint to the array
			NarrativeBlueprint->EditorApp = Editor;
		}
	}

	Editor->InitBlueprintEditor(Mode, EditWithinLevelEditor, NarrativeBlueprints, false);
}
