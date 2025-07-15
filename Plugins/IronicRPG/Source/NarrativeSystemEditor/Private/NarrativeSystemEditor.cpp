// Copyright Epic Games, Inc. All Rights Reserved.

#include "NarrativeSystemEditor.h"
#include "AssetToolsModule.h"
#include "AssetToolsModule.h"
#include "Assets/NarrativeAssetAction.h"
#include "Interfaces/IPluginManager.h"
#include "EdGraphUtilities.h"
#include "KismetPins/SGraphPinColor.h"
#include "EdGraph/EdGraphPin.h"
#include "Framework/Commands/GenericCommands.h"
#include "KismetCompiler.h"
#include "NarrativeAsset.h"
#include "Blueprint/DialogueBlueprintCompiler.h"
#include "KismetCompilerModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Narrative/NarrativeEventBlueprintBase.h"

#define LOCTEXT_NAMESPACE "FNarrativeSystemEditorModule"

/*
* This class is used to create a custom pin type for the Narrative System.
*/
class SNarrativeGraphPin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SNarrativeGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin)
	{
		SGraphPin::Construct(SGraphPin::FArguments(), InPin);
	}

protected:
	virtual FSlateColor GetPinColor() const override
	{
		return FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f));
	}
};

class SNarrativeStartGraphPin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SNarrativeStartGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin)
	{
		SGraphPin::Construct(SGraphPin::FArguments(), InPin);
	}

protected:
	virtual FSlateColor GetPinColor() const override
	{
		return FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f));
	}
};

struct FNarrativePinFactory : public FGraphPanelPinFactory
{
public:
	virtual ~FNarrativePinFactory() {}
	virtual TSharedPtr<SGraphPin> CreatePin(UEdGraphPin* Pin) const override
	{
		if (FName(TEXT("NarrativePin")) == Pin->PinType.PinSubCategory)
		{
			return SNew(SNarrativeGraphPin, Pin);
		}
		else if (FName(TEXT("StartPin")) == Pin->PinType.PinSubCategory)
		{
			return SNew(SNarrativeStartGraphPin, Pin);
		}

		return nullptr;
	}
};

void FNarrativeSystemEditorModule::StartupModule()
{
	// Register asset category and asset actions
	IAssetTools& AssetTools = IAssetTools::Get();
	EAssetTypeCategories::Type AssetType = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("NarrativeAssets")), FText::FromString("Narrative Assets"));
	TSharedPtr<NarrativeAssetAction> AssetAction = MakeShareable(new NarrativeAssetAction(AssetType));
	AssetTools.RegisterAssetTypeActions(AssetAction.ToSharedRef());

	// Register generic editor commands
	FGenericCommands::Register();

	// Initialize and register the custom StyleSet
	FNarrativeSystemEditorStyleSet& StyleSet = FNarrativeSystemEditorStyleSet::Get();

	// Register custom pin factory
	_PinFactory = MakeShareable(new FNarrativePinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(_PinFactory);

	// Register custom Blueprint compiler for DialogueBlueprint
	IKismetCompilerInterface& KismetCompiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
	KismetCompiler.GetCompilers().Add(new FDialogueBlueprintCompilerModule());

	FKismetCompilerContext::RegisterCompilerForBP(
		UDialogueBlueprint::StaticClass(),
		[](UBlueprint* Blueprint, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions) -> TSharedPtr<FKismetCompilerContext>
		{
			return MakeShareable(new DialogueBlueprintCompiler(CastChecked<UDialogueBlueprint>(Blueprint), InMessageLog, InCompileOptions));
		}
	);

	FCoreDelegates::OnFEngineLoopInitComplete.AddLambda([]()
		{
			// Refresh Blueprint node actions for DialogueBlueprint
			FBlueprintActionDatabase::Get().RefreshAssetActions(UDialogueBlueprint::StaticClass());
		});
}

void FNarrativeSystemEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	FGenericCommands::Unregister();

	// Unregister style
	if (_StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*_StyleSet);

		ensure(_StyleSet.IsUnique());
		_StyleSet.Reset();
	}

	if (_PinFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinFactory(_PinFactory);
		_PinFactory.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FNarrativeSystemEditorModule, NarrativeSystemEditor)