// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"

class FRPGEditorStyleSet final : public FSlateStyleSet
{
public:

	FRPGEditorStyleSet()
		: FSlateStyleSet("RPGEditor")
	{
		SetParentStyleName(FAppStyle::GetAppStyleSetName());

		SetContentRoot(FPaths::ProjectPluginsDir() / TEXT("IronicRPG/Resources"));
		SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

		static const FVector2D Icon16(16.0f, 16.0f);
		static const FVector2D Icon64(64.0f, 64.0f);

		Set("Icons.Maximize", new IMAGE_BRUSH("Icons/Maximize", Icon16));

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	~FRPGEditorStyleSet()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}

	static FRPGEditorStyleSet& Get()
	{
		static FRPGEditorStyleSet Inst;
		return Inst;
	}
};

class FRPGEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

};
