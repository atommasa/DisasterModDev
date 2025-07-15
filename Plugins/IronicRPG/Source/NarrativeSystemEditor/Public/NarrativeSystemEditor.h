// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"


class FNarrativeSystemEditorStyleSet final : public FSlateStyleSet
{
public:

	FNarrativeSystemEditorStyleSet()
		: FSlateStyleSet("NarrativeSystemEditor")
	{
		SetParentStyleName(FAppStyle::GetAppStyleSetName());

		SetContentRoot(FPaths::ProjectPluginsDir() / TEXT("IronicRPG/Resources"));
		SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

		static const FVector2D Icon16(16.0f, 16.0f);
		static const FVector2D Icon64(64.0f, 64.0f);

		Set("ClassIcon.DialogueBlueprint", new IMAGE_BRUSH("Icons/NarrativeSystemEditorIcon", Icon16));
		Set("ClassThumbnail.DialogueBlueprint", new IMAGE_BRUSH("Icons/NarrativeSystemEditorThumbnail", Icon64));
		Set("ClassIcon.Dialogue", new IMAGE_BRUSH("Icons/NarrativeSystemEditorIcon", Icon16));
		Set("ClassThumbnail.Dialogue", new IMAGE_BRUSH("Icons/NarrativeSystemEditorThumbnail", Icon64));
		Set("NarrativeEditor.NodeAddPinIcon", new IMAGE_BRUSH("Icons/NodeAddPinIcon", Icon16));
		Set("NarrativeEditor.NodeDeletePinIcon", new IMAGE_BRUSH("Icons/NodeDeletePinIcon", Icon16));

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	~FNarrativeSystemEditorStyleSet()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}

	static FNarrativeSystemEditorStyleSet& Get()
	{
		static FNarrativeSystemEditorStyleSet Inst;
		return Inst;
	}
};

class FNarrativeSystemEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<FSlateStyleSet> _StyleSet = nullptr;
	TSharedPtr<struct FNarrativePinFactory> _PinFactory = nullptr;
};
