// Copyright Epic Games, Inc. All Rights Reserved.


#include "RPGEditor.h"
#include "IPropertyChangeListener.h"
#include "PropertyEditorModule.h"
#include "Customizations/RPGIdPropCustomization.h"

#define LOCTEXT_NAMESPACE "FRPGEditorModule"

void FRPGEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout("RPGId", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FRPGIdCustomization::MakeInstance));
}

void FRPGEditorModule::ShutdownModule()
{
	
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRPGEditorModule, RPGEditor)