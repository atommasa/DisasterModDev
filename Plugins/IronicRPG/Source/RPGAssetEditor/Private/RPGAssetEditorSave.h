// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DataTypes/RPGId.h"
#include "ContentBrowser/RPGAssetSection.h"
#include "RPGAssetEditorSave.generated.h"

UCLASS(config = RPGEditorSetting, defaultconfig)
class RPGASSETEDITOR_API URPGAssetEditorSave : public UObject
{
	GENERATED_BODY()

public:
    static URPGAssetEditorSave* Get()
    {
        return GetMutableDefault<URPGAssetEditorSave>();
    }

    void Save()
    {
        URPGAssetEditorSave* Default = GetMutableDefault<URPGAssetEditorSave>();
        if (Default)
        {
            Default->SaveConfig(CPF_Config);
            GConfig->Flush(false);
        }
	}

    void Load()
    {
        URPGAssetEditorSave* Default = GetMutableDefault<URPGAssetEditorSave>();
        if (Default)
        {
			GConfig->Flush(true);
            Default->LoadConfig();
        }
	}

public:
	// Sections in the asset editor
    UPROPERTY(config)
    TMap<FName, FRPGAssetSection> Sections;

	// Map of section IDs to section data
    UPROPERTY(config)
    TMap<FRPGId, FRPGAssetSection> SectionMap;
};