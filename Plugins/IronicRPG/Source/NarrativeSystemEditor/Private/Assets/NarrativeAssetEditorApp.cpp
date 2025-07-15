// Fill out your copyright notice in the Description page of Project Settings.


#include "Assets/NarrativeAssetEditorApp.h"
#include "Assets/NarrativeAssetAppMode.h"
#include "NarrativeAsset.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "NarrativeGraphSchema.h"
#include "NarrativeGraphNode.h"
#include "NarrativeRuntimeGraph.h"
#include "Nodes/NarrativeStartGraphNode.h"
#include "Nodes/NarrativePlayerGraphNode.h"
#include "Framework/Commands/GenericCommands.h"
#include "Editor/Transactor.h"
#include "HAL/PlatformApplicationMisc.h"
#include "EdGraphUtilities.h"
#include "NarrativeTextParser.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Toolkits/ToolkitManager.h"
#include "Narrative/NarrativeEventBlueprintBase.h"
#include "SBlueprintEditorToolbar.h"
#include "Kismet2/DebuggerCommands.h"
#include "EngineAnalytics.h"
#include "Nodes/NarrativeCutsceneNode.h"

DEFINE_LOG_CATEGORY_STATIC(LogNarrativeAssetEditor, Log, All);

NarrativeAssetEditorApp::NarrativeAssetEditorApp()
    : FBlueprintEditor()
{

}

NarrativeAssetEditorApp::~NarrativeAssetEditorApp()
{
    // Stop watching the settings
    /*GetMutableDefault<UBlueprintEditorSettings>()->OnSettingChanged().Remove(BlueprintEditorSettingsChangedHandle);
    GetMutableDefault<UBlueprintEditorProjectSettings>()->OnSettingChanged().Remove(BlueprintProjectSettingsChangedHandle);*/

    // Clean up the preview
    DestroyPreview();

    // NOTE: Any tabs that we still have hanging out when destroyed will be cleaned up by FBaseToolkit's destructor
    UEditorEngine* Editor = (UEditorEngine*)GEngine;
    if (Editor)
    {
        Editor->UnregisterForUndo(this);
    }

    CloseMergeTool();

}

void NarrativeAssetEditorApp::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
    DocumentManager->SetTabManager(InTabManager);

    FWorkflowCentricApplication::RegisterTabSpawners(InTabManager);
}

void NarrativeAssetEditorApp::InitBlueprintEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode)
{
    check(InBlueprints.Num() == 1 || bShouldOpenInDefaultsMode);

    // TRUE if a single Blueprint is being opened and is marked as newly created
    bool bNewlyCreated = InBlueprints.Num() == 1 && InBlueprints[0]->bIsNewlyCreated;

    // Load editor settings from disk.
    LoadEditorSettings();

    TArray<UObject*> Objects;
    for (UBlueprint* Blueprint : InBlueprints)
    {
        // Flag the blueprint as having been opened
        Blueprint->bIsNewlyCreated = false;

        Objects.Add(Blueprint);
    }

    if (!Toolbar.IsValid())
    {
        Toolbar = MakeShareable(new FBlueprintEditorToolbar(SharedThis(this)));
    }

    GetToolkitCommands()->Append(FPlayWorldCommands::GlobalPlayWorldActions.ToSharedRef());

    CreateDefaultCommands();

    RegisterMenus();

    // Initialize the asset editor and spawn nothing (dummy layout)
    const bool bCreateDefaultStandaloneMenu = true;
    const bool bCreateDefaultToolbar = true;
    const FName BlueprintEditorAppName = FName(TEXT("BlueprintEditorApp"));
    InitAssetEditor(Mode, InitToolkitHost, BlueprintEditorAppName, FTabManager::FLayout::NullLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Objects);

    CommonInitialization(InBlueprints, bShouldOpenInDefaultsMode);

    InitalizeExtenders();

    RegenerateMenusAndToolbars();

    RegisterApplicationModes(InBlueprints, bShouldOpenInDefaultsMode, bNewlyCreated);

    // Post-layout initialization
    PostLayoutBlueprintEditorInitialization();

    // Find and set any instances of this blueprint type if any exists and we are not already editing one
    FBlueprintEditorUtils::FindAndSetDebuggableBlueprintInstances();

    _WorkingAsset = Cast<UNarrativeBlueprintBase>(GetBlueprintObj());
    if (!_WorkingAsset)
    {
        return;
    }

    FKismetEditorUtilities::CompileBlueprint(_WorkingAsset);

    if (bNewlyCreated)
    {
        if (_WorkingAsset->BlueprintType == BPTYPE_MacroLibrary)
        {
            NewDocument_OnClick(CGT_NewMacroGraph);
        }
        else if (_WorkingAsset->BlueprintType == BPTYPE_Interface)
        {
            NewDocument_OnClick(CGT_NewFunctionGraph);
        }
        else if (_WorkingAsset->BlueprintType == BPTYPE_FunctionLibrary)
        {
            NewDocument_OnClick(CGT_NewFunctionGraph);
        }
    }

    if (_WorkingAsset->GetClass() == UNarrativeBlueprintBase::StaticClass() && _WorkingAsset->BlueprintType == BPTYPE_Normal)
    {
        if (!bShouldOpenInDefaultsMode)
        {
            GetToolkitCommands()->ExecuteAction(FFullBlueprintEditorCommands::Get().EditClassDefaults.ToSharedRef());
        }
    }

    // There are upgrade notes, open the log and dump the messages to it
    if (_WorkingAsset->UpgradeNotesLog.IsValid())
    {
        DumpMessagesToCompilerLog(_WorkingAsset->UpgradeNotesLog->Messages, true);
    }

    // Register for notifications when settings change
    /*BlueprintEditorSettingsChangedHandle = GetMutableDefault<UBlueprintEditorSettings>()->OnSettingChanged()
        .AddRaw(this, &FBlueprintEditor::OnBlueprintEditorPreferencesChanged);
    BlueprintProjectSettingsChangedHandle = GetMutableDefault<UBlueprintEditorProjectSettings>()->OnSettingChanged()
        .AddRaw(this, &FBlueprintEditor::OnBlueprintProjectSettingsChanged);*/

    CreateGraphEditorCommands();

    // Only create if no graph exists
    const FName& GraphName = FName(TEXT("Dialogue Graph"));
    UObject* ExistingObject = FindObject<UObject>(_WorkingAsset, *(GraphName.ToString()));
    if (!ExistingObject)
    {
        _WorkingGraph = FBlueprintEditorUtils::CreateNewGraph(
            _WorkingAsset,
            GraphName,
            UEdGraph::StaticClass(),
            UNarrativeGraphSchema::StaticClass()
        );

        _WorkingGraph->bAllowDeletion = false;
        FBlueprintEditorUtils::AddUbergraphPage(_WorkingAsset, _WorkingGraph);
        _WorkingAsset->LastEditedDocuments.AddUnique(_WorkingGraph);
        
        _WorkingGraph->GetSchema()->CreateDefaultNodesForGraph(*_WorkingGraph);
    }
    else if (ExistingObject->IsA<UEdGraph>())
    {
        _WorkingGraph = Cast<UEdGraph>(ExistingObject);
    }
    
    // Load the UI from the asset
    UpdateEditorGraphFromWorkingAsset();
}

FGraphPanelSelectionSet NarrativeAssetEditorApp::GetSelectedNodes() const
{
    FGraphPanelSelectionSet CurrentSelection;
    if (_WorkingGraphUI.IsValid())
    {
        CurrentSelection = _WorkingGraphUI->GetSelectedNodes();
    }

    return CurrentSelection;
}

void NarrativeAssetEditorApp::LoadEditorSettings()
{
	
}

void NarrativeAssetEditorApp::SetSelectedNodeDetailView(TSharedPtr<class IDetailsView> InDetailView)
{
    /*_SelectedNodeDetailView = InDetailView;
    _SelectedNodeDetailView->OnFinishedChangingProperties().AddRaw(this, &NarrativeAssetEditorApp::OnNodeDetailViewPropertiesUpdated);*/
}

void NarrativeAssetEditorApp::OnGraphEditorFocused(const TSharedRef<class SGraphEditor>& InGraphEditor)
{
	FBlueprintEditor::OnGraphEditorFocused(InGraphEditor);

	_WorkingGraphUI = InGraphEditor;
}

void NarrativeAssetEditorApp::OnGraphSelectionChanged(const FGraphPanelSelectionSet& Selection)
{
    UNarrativeGraphNodeBase* SelectedNode = GetSelectedNode(Selection);
    if (SelectedNode)
    {
        UNarrativeNodeInfo* NodeInfo = SelectedNode->GetNodeInfo();
        if (!NodeInfo)
        {
            return;
        }

        FString Title;
        SelectedNode->GetName(Title);

        SKismetInspector::FShowDetailsOptions Options(FText::FromString(Title), true);
        Options.bShowComponents = false;

        Inspector->ShowDetailsForSingleObject(NodeInfo, Options);
    }
}

void NarrativeAssetEditorApp::OnSelectedNodesChangedImpl(const TSet<class UObject*>& NewSelection)
{
	FBlueprintEditor::OnSelectedNodesChangedImpl(NewSelection);
    
	// Update the details view with the selected node
	if (_WorkingGraphUI.IsValid())
	{
		const FGraphPanelSelectionSet Selection = _WorkingGraphUI->GetSelectedNodes();
		OnGraphSelectionChanged(Selection);
	}
}

void NarrativeAssetEditorApp::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	FBlueprintEditor::OnFinishedChangingProperties(PropertyChangedEvent);

    if (UBlueprint* Blueprint = GetBlueprintObj())
    {
        // 標記 Blueprint Dirty（PackageDirty + 重新編譯提示）
        Blueprint->Modify();
        Blueprint->MarkPackageDirty();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        // 建議：必要時也可以選擇強制重新編譯
        // FKismetEditorUtilities::CompileBlueprint(Blueprint);
    }
}

void NarrativeAssetEditorApp::OnClose()
{
    //UpdateWorkingAssetFromGraph();
	_WorkingAsset->SetPreSaveListener(nullptr);
    FAssetEditorToolkit::OnClose();
}

void NarrativeAssetEditorApp::OnNodeDetailViewPropertiesUpdated(const FPropertyChangedEvent& Event)
{
    if (_WorkingGraphUI)
    {
        // Get the node being modified
        UNarrativeGraphNodeBase* DialogNode = GetSelectedNode(_WorkingGraphUI->GetSelectedNodes());
        if (UNarrativePlayerGraphNode* PlayerGraphNode = Cast<UNarrativePlayerGraphNode>(DialogNode))
        {
            PlayerGraphNode->Modify();
            
            if (PlayerGraphNode->GetNodeInfo())
            {
                PlayerGraphNode->GetNodeInfo()->Modify();
            }

            PlayerGraphNode->SyncPinWithResponse();

        }

        _WorkingGraphUI->NotifyGraphChanged();
    }
}

void NarrativeAssetEditorApp::OnWorkingAssetPreSave(const FEdGraphEditAction& InAction)
{
    UpdateWorkingAssetFromGraph();
}

void NarrativeAssetEditorApp::CreateGraphEditorCommands()
{
    GraphEditorCommands = MakeShareable(new FUICommandList);

    GraphEditorCommands->MapAction(
        FGenericCommands::Get().Delete,
        FExecuteAction::CreateRaw(this, &NarrativeAssetEditorApp::DeleteSelectedNodes),
        FCanExecuteAction::CreateRaw(this, &NarrativeAssetEditorApp::CanDeleteNodes)
    );

    GraphEditorCommands->MapAction(
        FGenericCommands::Get().Cut,
        FExecuteAction::CreateRaw(this, &NarrativeAssetEditorApp::CutSelectedNodes),
        FCanExecuteAction::CreateRaw(this, &NarrativeAssetEditorApp::CanCutNodes)
    );

    GraphEditorCommands->MapAction(
        FGenericCommands::Get().Copy,
        FExecuteAction::CreateRaw(this, &NarrativeAssetEditorApp::CopySelectedNodes),
        FCanExecuteAction::CreateRaw(this, &NarrativeAssetEditorApp::CanCopyNodes)
    );

    GraphEditorCommands->MapAction(
        FGenericCommands::Get().Duplicate,
        FExecuteAction::CreateRaw(this, &NarrativeAssetEditorApp::DuplicateNodes),
        FCanExecuteAction::CreateRaw(this, &NarrativeAssetEditorApp::CanDuplicateNodes)
    );

    GraphEditorCommands->MapAction(
        FGenericCommands::Get().Paste,
        FExecuteAction::CreateRaw(this, &NarrativeAssetEditorApp::PasteNodes),
        FCanExecuteAction::CreateRaw(this, &NarrativeAssetEditorApp::CanPasteNodes)
    );

    GraphEditorCommands->MapAction(
        FGenericCommands::Get().Undo,
        FExecuteAction::CreateRaw(this, &NarrativeAssetEditorApp::Undo),
        FCanExecuteAction::CreateRaw(this, &NarrativeAssetEditorApp::CanUndo)
    );

    GraphEditorCommands->MapAction(
        FGenericCommands::Get().Redo,
        FExecuteAction::CreateRaw(this, &NarrativeAssetEditorApp::Redo),
        FCanExecuteAction::CreateRaw(this, &NarrativeAssetEditorApp::CanRedo)
    );
}

void NarrativeAssetEditorApp::DeleteSelectedNodes()
{
    if (!_WorkingGraphUI.IsValid())
    {
        return;
    }

    const FScopedTransaction Transaction(FGenericCommands::Get().Delete->GetDescription());
    _WorkingGraphUI->GetCurrentGraph()->Modify();

    const FGraphPanelSelectionSet SelectedNodes = _WorkingGraphUI->GetSelectedNodes();
    _WorkingGraphUI->ClearSelectionSet();

    for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
    {
        if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
        {
            if (Node->CanUserDeleteNode())
            {
                Node->Modify();
                Node->DestroyNode();
            }
        }
    }
}

bool NarrativeAssetEditorApp::CanDeleteNodes() const
{
    // If any of the nodes can be deleted then we should allow deleting
    const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
    for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
    {
        UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
        if (Node && Node->CanUserDeleteNode())
        {
            return true;
        }
    }

    return false;
}

void NarrativeAssetEditorApp::CutSelectedNodes()
{
    CopySelectedNodes();
    DeleteSelectedDuplicatableNodes();
}

bool NarrativeAssetEditorApp::CanCutNodes() const
{
    return CanCopyNodes() && CanDeleteNodes();
}

void NarrativeAssetEditorApp::DeleteSelectedDuplicatableNodes()
{
    if (!_WorkingGraphUI.IsValid())
    {
        return;
    }

    const FGraphPanelSelectionSet OldSelectedNodes = _WorkingGraphUI->GetSelectedNodes();
    _WorkingGraphUI->ClearSelectionSet();

    for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
    {
        UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
        if (Node && Node->CanDuplicateNode())
        {
            _WorkingGraphUI->SetNodeSelection(Node, true);
        }
    }

    // Delete the duplicatable nodes
    DeleteSelectedNodes();

    _WorkingGraphUI->ClearSelectionSet();

    for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
    {
        if (UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
        {
            _WorkingGraphUI->SetNodeSelection(Node, true);
        }
    }
}

void NarrativeAssetEditorApp::CopySelectedNodes()
{
    // Export the selected nodes and place the text on the clipboard
    FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

    FString ExportedText;

    int32 CopySubNodeIndex = 0;
    for (FGraphPanelSelectionSet::TIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
    {
        UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
        UNarrativeGraphNodeBase* NarrativeNode = Cast<UNarrativeGraphNodeBase>(Node);
        if (!Node)
        {
            SelectedIter.RemoveCurrent();
            continue;
        }

        Node->PrepareForCopying();
    }

    FEdGraphUtilities::ExportNodesToText(SelectedNodes, ExportedText);
    FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
}

bool NarrativeAssetEditorApp::CanCopyNodes() const
{
    // If any of the nodes can be duplicated then we should allow copying
    const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
    for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
    {
        UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
        if (Node && Node->CanDuplicateNode())
        {
            return true;
        }
    }

    return false;
}

void NarrativeAssetEditorApp::DuplicateNodes()
{
    CopySelectedNodes();
    PasteNodes();
}

bool NarrativeAssetEditorApp::CanDuplicateNodes() const
{
    return CanCopyNodes();
}

void NarrativeAssetEditorApp::PasteNodes()
{
    if (_WorkingGraphUI)
    {
        PasteNodesHere(_WorkingGraphUI->GetPasteLocation());
    }
}

void NarrativeAssetEditorApp::PasteNodesHere(const FVector2D& Location)
{
    const FScopedTransaction Transaction(NSLOCTEXT("Narrative", "PasteNodes", "Paste Dialogue Nodes"));
    _WorkingGraph->Modify();

	// Get the clipboard content
    FString ClipboardContent;
    FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

    if (FEdGraphUtilities::CanImportNodesFromText(_WorkingGraphUI->GetCurrentGraph(), ClipboardContent))
    {
        // Import the nodes from the clipboard content
        TSet<UEdGraphNode*> PastedNodes;
        FEdGraphUtilities::ImportNodesFromText(_WorkingGraph, ClipboardContent, PastedNodes);

        // Set the pasted nodes' positions
        FVector2D AveragePos(0.0f, 0.0f);
        for (UEdGraphNode* Node : PastedNodes)
        {
            AveragePos.X += Node->NodePosX;
            AveragePos.Y += Node->NodePosY;
        }

        // Calculate the average position
        const int32 NumNodes = PastedNodes.Num();
        if (NumNodes > 0)
        {
            AveragePos /= static_cast<float>(NumNodes);
        }

        // Move the nodes to the new location
        _WorkingGraphUI->ClearSelectionSet();
        for (UEdGraphNode* Node : PastedNodes)
        {
            Node->Modify();

            Node->NodePosX = Node->NodePosX - AveragePos.X + Location.X;
            Node->NodePosY = Node->NodePosY - AveragePos.Y + Location.Y;
            Node->SnapToGrid(16);

            _WorkingGraphUI->SetNodeSelection(Node, true);
            Node->CreateNewGuid();
        }
    }
	else if (NarrativeTextParser::CanParseFromText(ClipboardContent))
	{
		// Parse the text and create nodes
        PasteDialogueTextAsNodes(_WorkingGraph, ClipboardContent, Location);
	}

    _WorkingGraphUI->NotifyGraphChanged();
}

bool NarrativeAssetEditorApp::CanPasteNodes() const
{
    if (!_WorkingGraphUI.IsValid())
    {
        return false;
    }
    
    FString ClipboardContent;
    FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

    bool bCanImport = FEdGraphUtilities::CanImportNodesFromText(_WorkingGraphUI->GetCurrentGraph(), ClipboardContent);
    bool bCanParse = NarrativeTextParser::CanParseFromText(ClipboardContent);
    
    return bCanImport || bCanParse;
}

void NarrativeAssetEditorApp::PasteDialogueTextAsNodes(UEdGraph* Graph, const FString& DialogueText, const FVector2D& PasteLocation) const
{
    TArray<FParsedDialogueLine> DialogueLines = NarrativeTextParser::ParseFromText(DialogueText);

    if (DialogueLines.IsEmpty())
    {
        return;
    }

	UDialogueBlueprint* DialogueBlueprint = Cast<UDialogueBlueprint>(Graph->GetOuter());
	if (!DialogueBlueprint)
	{
		return;
	}

    UDialogue* Dialogue = Cast<UDialogue>(DialogueBlueprint->GeneratedClass->GetDefaultObject());
    if (!Dialogue)
    {
        return;
    }

    const float NodeSpacingX = 300.0f;

    TArray<UNarrativeGraphNode*> Nodes;
    FVector2D CurrentLocation = PasteLocation;

    // Create Nodes
    for (const FParsedDialogueLine& DialogueLine : DialogueLines)
    {
		// Create a new node
        UNarrativeGraphNode* DialogueGraphNode = NewObject<UNarrativeGraphNode>(Graph);
        DialogueGraphNode->CreateNewGuid();
        DialogueGraphNode->NodePosX = CurrentLocation.X;
        DialogueGraphNode->NodePosY = CurrentLocation.Y;

		// Create Node Info
        auto* NodeInfo = NewObject<UNarrativeDialogueNodeInfo>(DialogueGraphNode);
        NodeInfo->Dialogue = FText::FromString(DialogueLine.Dialogue);
        NodeInfo->SpeakerId = FName(DialogueLine.Speaker);

        DialogueGraphNode->SetNodeInfo(NodeInfo);

        DialogueGraphNode->CreateNarrativePin(EGPD_Input, TEXT("In"));
        DialogueGraphNode->CreateNarrativePin(EGPD_Output, TEXT("Out"));

        Graph->Modify();
        Graph->AddNode(DialogueGraphNode, true, true);

        Nodes.Add(DialogueGraphNode);

        CurrentLocation.X += NodeSpacingX;
    }
    
	// Connect Nodes
    for (int32 i = 0; i < Nodes.Num(); i++)
    {
        UNarrativeGraphNode* FromNodePtr = nullptr;
        UNarrativeGraphNode* ToNodePtr = nullptr;
        
        if (Nodes.IsValidIndex(i + 1))
        {
            FromNodePtr = Nodes[i + 1];
            ToNodePtr = Nodes[i];
        }

        if (FromNodePtr && ToNodePtr)
        {
            UEdGraphPin* OutPin = (FromNodePtr)->FindPin(TEXT("In"));
            UEdGraphPin* InPin = (ToNodePtr)->FindPin(TEXT("Out"));

            if (OutPin && InPin)
            {
                OutPin->PinName = TEXT("");
                OutPin->Modify();
                InPin->PinName = TEXT("");
                InPin->Modify();

                OutPin->MakeLinkTo(InPin);
            }
        }
    }

    if (_WorkingGraphUI.IsValid())
    {
        _WorkingGraphUI->NotifyGraphChanged();
    }
}

void NarrativeAssetEditorApp::PostUndo(bool bSuccess)
{
    if (_WorkingGraphUI.IsValid())
    {
        _WorkingGraphUI->ClearSelectionSet();

        for (UEdGraphNode* Node : _WorkingGraph->Nodes)
        {
            if (UNarrativeGraphNodeBase* NarrativeNode = Cast<UNarrativeGraphNodeBase>(Node))
            {
                NarrativeNode->SyncPinWithResponse();
            }
        }

        _WorkingGraphUI->NotifyGraphChanged();
    }

    UE_LOG(LogTemp, Log, TEXT("Undo executed"));
}

void NarrativeAssetEditorApp::PostRedo(bool bSuccess)
{
    if (_WorkingGraphUI.IsValid())
    {
        _WorkingGraphUI->ClearSelectionSet();

        for (UEdGraphNode* Node : _WorkingGraph->Nodes)
        {
            if (UNarrativePlayerGraphNode* PlayerNode = Cast<UNarrativePlayerGraphNode>(Node))
            {
                PlayerNode->SyncPinWithResponse();
            }
            else if (UNarrativeGraphNode* DialogueNode = Cast<UNarrativeGraphNode>(Node))
            {
                // 如果 DialogueNode 也有 sync 函式
                // DialogueNode->SyncPins(); // 可選
            }
        }

        _WorkingGraphUI->NotifyGraphChanged();
    }

    UE_LOG(LogTemp, Log, TEXT("Redo executed"));
}

void NarrativeAssetEditorApp::Undo()
{
    UE_LOG(LogTemp, Warning, TEXT(">>> Undo pressed"));

    if (GEditor && GEditor->Trans->CanUndo())
    {
        GEditor->UndoTransaction();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT(">>> Cannot Undo! Transaction list empty."));
    }
}

void NarrativeAssetEditorApp::Redo()
{
    if (GEditor)
    {
        GEditor->RedoTransaction();
    }
}

bool NarrativeAssetEditorApp::CanUndo()
{
    return GEditor && GEditor->Trans->CanUndo();
}

bool NarrativeAssetEditorApp::CanRedo()
{
    return GEditor && GEditor->Trans->CanRedo();
}

void NarrativeAssetEditorApp::OpenEventGraph()
{
    // 確保已經建立 Factory
    const FName EventTabName = FName("EventGraphTab");

    // 呼叫 AssetEditorToolkit 的 OpenTab
    TSharedPtr<FTabManager> Manager = GetTabManager();
    if (Manager.IsValid())
    {
        TSharedPtr<SDockTab> Tab = Manager->TryInvokeTab(EventTabName);
        if (Tab.IsValid())
        {
            Tab->DrawAttention();
            SetCurrentMode(TEXT("NarrativeEventGraphAppMode"));
        }
    }
}

UBlueprint* NarrativeAssetEditorApp::CreateSharedEventBlueprint()
{
    if (!_WorkingAsset)
    {
        UE_LOG(LogTemp, Error, TEXT("Working asset is null."));
        return nullptr;
    }

    // Use the transient package safely
    UPackage* TransientPackage = GetTransientPackage();

    // Create Blueprint
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        UDialogue::StaticClass(),
        TransientPackage,
        FName("NarrativeAsset_BP"),
        EBlueprintType::BPTYPE_Normal,
        UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass(),
        FName("NarrativeEventBlueprint")
    );

    if (!Blueprint)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create Blueprint."));
        return nullptr;
    }

    // Create EventGraph (Ubergraph)
    UEdGraph* EventGraph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint,
        FBlueprintEditorUtils::GenerateUniqueGraphName(Blueprint, "Event Graph"),
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass()
    );

    if (!EventGraph)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create EventGraph."));
        return nullptr;
    }

    EventGraph->bAllowDeletion = false;
    EventGraph->SetFlags(RF_Transactional);

    // Add to blueprint
    FBlueprintEditorUtils::AddUbergraphPage(Blueprint, EventGraph);
    Blueprint->LastEditedDocuments.Add(EventGraph);

    return Blueprint;
}

void NarrativeAssetEditorApp::UpdateWorkingAssetFromGraph()
{
    if (!_WorkingAsset || !_WorkingGraph)
    {
		return;
    }

    // Update the state we need into our saveable format
    UNarrativeRuntimeGraph* RuntimeGraph = NewObject<UNarrativeRuntimeGraph>(_WorkingAsset);
    _WorkingAsset->Graph = RuntimeGraph;

    TArray<std::pair<FGuid, FGuid>> Connections;
    TMap<FGuid, UNarrativeRuntimePin*> IDToPinMap;

    // First create all the nodes/pins and record the connections
    for (UEdGraphNode* UINode : _WorkingGraph->Nodes)
    {
        UNarrativeRuntimeNode* RuntimeNode = NewObject<UNarrativeRuntimeNode>(RuntimeGraph);
        RuntimeNode->Position = FVector2D(UINode->NodePosX, UINode->NodePosY);

        for (UEdGraphPin* UIPin : UINode->Pins)
        {
            UNarrativeRuntimePin* RuntimePin = NewObject<UNarrativeRuntimePin>(RuntimeNode);
            RuntimePin->PinName = UIPin->PinName;
            RuntimePin->PinId = UIPin->PinId;
            RuntimePin->Parent = RuntimeNode;

            // Only record the the output side of the connection since this is a directed graph
            if (UIPin->HasAnyConnections() && UIPin->Direction == EEdGraphPinDirection::EGPD_Output)
            {
                // Only 1 connection is allowed so just take the first one
                std::pair<FGuid, FGuid> Connection = std::make_pair(UIPin->PinId, UIPin->LinkedTo[0]->PinId);
                Connections.Add(Connection);
            }

            IDToPinMap.Add(UIPin->PinId, RuntimePin);
            if (UIPin->Direction == EEdGraphPinDirection::EGPD_Input)
            {
                RuntimeNode->InputPin = RuntimePin;
            }
            else
            {
                RuntimeNode->OutputPins.Add(RuntimePin);
            }
        }

        if (UINode->IsA(UNarrativeGraphNode::StaticClass()))
        {
			UNarrativeGraphNode* NarrativeNode = Cast<UNarrativeGraphNode>(UINode);
			RuntimeNode->NodeType = ENarrativeNodeType::DialogueNode;
            RuntimeNode->NodeInfo = NarrativeNode->GetNodeInfo();
		}
        else if (UINode->IsA(UNarrativePlayerGraphNode::StaticClass()))
        {
            UNarrativePlayerGraphNode* NarrativeNode = Cast<UNarrativePlayerGraphNode>(UINode);
            RuntimeNode->NodeType = ENarrativeNodeType::PlayerNode;
            RuntimeNode->NodeInfo = NarrativeNode->GetNodeInfo();
        }
        else if (UINode->IsA(UNarrativeStartGraphNode::StaticClass()))
		{
			RuntimeNode->NodeType = ENarrativeNodeType::StartNode;
		}
		else if (UINode->IsA(UNarrativeCutsceneNode::StaticClass()))
		{
			RuntimeNode->NodeType = ENarrativeNodeType::CutsceneNode;
		}
		else
		{
			RuntimeNode->NodeType = ENarrativeNodeType::UnknownNode;
        }

        RuntimeGraph->Nodes.Add(RuntimeNode);
    }

    // Now make all the connections
    for (std::pair<FGuid, FGuid> Connection : Connections)
    {
        UNarrativeRuntimePin* Pin1 = IDToPinMap.FindRef(Connection.first);
        UNarrativeRuntimePin* Pin2 = IDToPinMap.FindRef(Connection.second);
        if (Pin1 && Pin2)
        {
            Pin1->Connection = Pin2;
        }
    }
}

void NarrativeAssetEditorApp::UpdateEditorGraphFromWorkingAsset()
{
    if (!_WorkingAsset->Graph)
    {
		
        return;
    }

    // Create all the nodes/pins first
    TArray<std::pair<FGuid, FGuid>> Connections;
    TMap<FGuid, UEdGraphPin*> IDToPinMap;
    for (UNarrativeRuntimeNode* RuntimeNode : _WorkingAsset->Graph->Nodes)
    {
        UNarrativeGraphNodeBase* NewNode = nullptr;
		if (RuntimeNode->NodeType == ENarrativeNodeType::StartNode)
		{
			NewNode = NewObject<UNarrativeStartGraphNode>(_WorkingGraph);
		}
        else if (RuntimeNode->NodeType == ENarrativeNodeType::DialogueNode)
        {
            NewNode = NewObject<UNarrativeGraphNode>(_WorkingGraph);
        }
        else if (RuntimeNode->NodeType == ENarrativeNodeType::PlayerNode)
        {
            NewNode = NewObject<UNarrativePlayerGraphNode>(_WorkingGraph);
        }
        else if (RuntimeNode->NodeType == ENarrativeNodeType::CutsceneNode)
        {
            NewNode = NewObject<UNarrativeCutsceneNode>(_WorkingGraph);
        }
		else
		{
			UE_LOG(LogNarrativeAssetEditor, Warning, TEXT(__FUNCTION__": Unknown node type %d"), (int32)RuntimeNode->NodeType);
			continue;
		}

        NewNode->SetFlags(RF_Transactional);
        NewNode->Rename(nullptr, _WorkingGraph, REN_NonTransactional);

        NewNode->CreateNewGuid();
        NewNode->NodePosX = RuntimeNode->Position.X;
        NewNode->NodePosY = RuntimeNode->Position.Y;

		if (RuntimeNode->NodeInfo)
		{
            NewNode->SetNodeInfo(DuplicateObject(RuntimeNode->NodeInfo, RuntimeNode));
		}
		else if (RuntimeNode->NodeType != ENarrativeNodeType::StartNode)
		{
			NewNode->SetNodeInfo(NewObject<UNarrativeNodeInfo>(RuntimeNode));
		}

        if (RuntimeNode->InputPin)
        {
            UNarrativeRuntimePin* Pin = RuntimeNode->InputPin;
            UEdGraphPin* UiPin = NewNode->CreateNarrativePin(EEdGraphPinDirection::EGPD_Input, Pin->PinName);
            UiPin->PinId = Pin->PinId;

            if (Pin->Connection)
            {
                Connections.Add(std::make_pair(Pin->PinId, Pin->Connection->PinId));
            }
            IDToPinMap.Add(Pin->PinId, UiPin);
        }

        for (UNarrativeRuntimePin* Pin : RuntimeNode->OutputPins)
        {
            UEdGraphPin* UIPin = NewNode->CreateNarrativePin(EEdGraphPinDirection::EGPD_Output, Pin->PinName);
            UIPin->PinId = Pin->PinId;

            if (Pin->Connection)
            {
                Connections.Add(std::make_pair(Pin->PinId, Pin->Connection->PinId));
            }

            IDToPinMap.Add(Pin->PinId, UIPin);
        }

        _WorkingGraph->AddNode(NewNode, true, true);
    }

    for (std::pair<FGuid, FGuid> Connection : Connections)
    {
        UEdGraphPin* FromPin = IDToPinMap.FindRef(Connection.first);
        UEdGraphPin* ToPin = IDToPinMap.FindRef(Connection.second);
        if (FromPin && ToPin)
        {
            FromPin->LinkedTo.Add(ToPin);
            ToPin->LinkedTo.Add(FromPin);
        }
    }
}

UNarrativeGraphNodeBase* NarrativeAssetEditorApp::GetSelectedNode(const FGraphPanelSelectionSet& Selection)
{
    for (UObject* Obj : Selection)
    {
        UNarrativeGraphNodeBase* Node = Cast<UNarrativeGraphNodeBase>(Obj);
        if (Node)
        {
            return Node;
        }
    }

    return nullptr;
}
