// Copyright Ironic Studio. All Rights Reserved.

#include "RPGIdReferenceMigrationConfigUndo.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogRPGIdReferenceMigrationConfigUndo, Log, All);

void URPGIdReferenceMigrationConfigUndo::Initialize(const FString& InFilename, const FString& InContents)
{
	Filename = InFilename;
	Contents = InContents;
}

void URPGIdReferenceMigrationConfigUndo::SetCommittedContents(const FString& InContents)
{
	Modify();
	Contents = InContents;
}

void URPGIdReferenceMigrationConfigUndo::PostEditUndo()
{
	Super::PostEditUndo();
	if (!FFileHelper::SaveStringToFile(Contents, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogRPGIdReferenceMigrationConfigUndo, Error, TEXT("Could not restore migration config transaction file %s."), *Filename);
		return;
	}
	if (GConfig)
	{
		GConfig->UnloadFile(Filename);
		GConfig->LoadFile(Filename);
	}
}
