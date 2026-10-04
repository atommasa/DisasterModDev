// Copyright Ironic Studio. All Rights Reserved.
#include "RPGReleaseSealCommandlet.h"
#include "RPGReleaseSealWorkflow.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/Parse.h"

URPGReleaseSealCommandlet::URPGReleaseSealCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 URPGReleaseSealCommandlet::Main(const FString& Params)
{
	FString Action, ReleaseId, SaveVersion;
	FParse::Value(*Params, TEXT("Action="), Action);
	FParse::Value(*Params, TEXT("ReleaseId="), ReleaseId);
	FParse::Value(*Params, TEXT("SaveDataVersion="), SaveVersion);
	ERPGReleaseSealAction Mode;
	if (Action == TEXT("Preview")) { Mode = ERPGReleaseSealAction::Preview; }
	else if (Action == TEXT("Seal")) { Mode = ERPGReleaseSealAction::Seal; }
	else if (Action == TEXT("Resume")) { Mode = ERPGReleaseSealAction::Resume; }
	else if (Action == TEXT("Abort")) { Mode = ERPGReleaseSealAction::Abort; }
	else { UE_LOG(LogTemp, Error, TEXT("Use -Action=Preview|Seal|Resume|Abort -ReleaseId=<id> [-SaveDataVersion=<version>]")); return 1; }
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().SearchAllAssets(true);
	const FRPGReleaseSealWorkflowResult Result = FRPGReleaseSealWorkflow().Execute(Mode, ReleaseId, SaveVersion);
	if (Result.bSucceeded) { UE_LOG(LogTemp, Display, TEXT("%s"), *Result.Message.ToString()); }
	else { UE_LOG(LogTemp, Error, TEXT("%s"), *Result.Message.ToString()); }
	return Result.bSucceeded ? 0 : 1;
}
