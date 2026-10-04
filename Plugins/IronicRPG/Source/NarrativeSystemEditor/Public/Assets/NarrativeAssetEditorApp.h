// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphPanel.h"
#include "BlueprintEditor.h"
#include "Blueprints/NarrativeEventBlueprint.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"

/**
 * 
 */
class NarrativeAssetEditorApp : public FBlueprintEditor
{
public:
	NarrativeAssetEditorApp();
	~NarrativeAssetEditorApp();

public:
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	void InitBlueprintEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode);

	class UNarrativeBlueprintBase* GetNarrativeAsset() { return _WorkingAsset; }
	class UEdGraph* GetWorkingGraph() { return _WorkingGraph; }
	FGraphPanelSelectionSet GetSelectedNodes() const;

	virtual void LoadEditorSettings() override;

	void SetWorkingGraphUI(TSharedPtr<SGraphEditor> InGraph) { _WorkingGraphUI = InGraph; }
	void SetSelectedNodeDetailView(TSharedPtr<class IDetailsView> InDetailView);

	virtual void OnGraphEditorFocused(const TSharedRef<class SGraphEditor>& InGraphEditor) override;
	void OnGraphSelectionChanged(const FGraphPanelSelectionSet& Slelction);
	virtual void OnSelectedNodesChangedImpl(const TSet<class UObject*>& NewSelection) override;
	virtual void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent) override;

public: // FAssetEditorToolkit interface
	virtual FName GetToolkitFName() const override { return FName(TEXT("NarrativeAssetEditorApp")); }
	virtual FText GetBaseToolkitName() const override { return FText::FromString(TEXT("NarrativeAssetEditorApp")); }
	virtual FString GetWorldCentricTabPrefix() const override { return TEXT("NarrativeAssetEditorApp"); }
	virtual FLinearColor GetWorldCentricTabColorScale() const override{ return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f); }
	virtual FString GetDocumentationLink() const override { return TEXT("https://github.com/atommasa"); }
	virtual void OnToolkitHostingStarted(const TSharedRef<class IToolkit>& Toolkit) override {}
	virtual void OnToolkitHostingFinished(const TSharedRef<class IToolkit>& Toolkit) override {}

	virtual void OnClose() override;
	// void OnNodeDetailViewPropertiesUpdated(const FPropertyChangedEvent& Event);
	void OnWorkingAssetPreSave(const struct FEdGraphEditAction& InAction);

public: // FUICommandList
	TSharedPtr<FUICommandList> GetCommandList() const { return GraphEditorCommands; }
	virtual void CreateGraphEditorCommands();

	// Delete Commands
	void DeleteSelectedNodes();
	bool CanDeleteNodes() const;

	// Cut Commands
	void CutSelectedNodes();
	bool CanCutNodes() const;
	void DeleteSelectedDuplicatableNodes();

	// Copy Commands
	void CopySelectedNodes();
	bool CanCopyNodes() const;

	// Duplicate Commands
	void DuplicateNodes();
	bool CanDuplicateNodes() const;

	// Past Commands
	void PasteNodes();
	void PasteNodesHere(const FVector2D& Location);
	bool CanPasteNodes() const;
	void PasteDialogueTextAsNodes(UEdGraph* Graph, const FString& DialogueText, const FVector2D& PasteLocation) const;

	// Undo Commands
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	void Undo();
	void Redo();
	bool CanUndo();
	bool CanRedo();

public: // Event Blueprint
	// class UBlueprint* GetEventBlueprint() const { return _EventBlueprint; }

	void OpenEventGraph();

public:
	UObject* GetClassDefaultsObject() const;
	class UBlueprint* GetSharedEventBlueprint();
	UEdGraph* GetOrCreateSharedEventGraph();

protected:
	void UpdateWorkingAssetFromGraph();
	// void RebuildEditorGraphFromRuntimeGraph();
	class UNarrativeGraphNodeBase* GetSelectedNode(const FGraphPanelSelectionSet& Selection);

	/** The command list for this editor */
	TSharedPtr<FUICommandList> GraphEditorCommands;

private:
	UPROPERTY()
	class UNarrativeBlueprintBase* _WorkingAsset = nullptr;

	UPROPERTY()
	class UEdGraph* _WorkingGraph = nullptr;

	TSharedPtr<SGraphEditor> _WorkingGraphUI = nullptr;

	TSharedPtr<class IDetailsView> _SelectedNodeDetailView = nullptr;

};
