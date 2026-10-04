// Copyright Ironic Studio. All Rights Reserved.

#include "SaveGame/RPGSaveGameMigration.h"

#include "Assets/RPGReleaseManifest.h"
#include "SaveGame/RPGSaveGame.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/UnrealType.h"

namespace
{
	struct FRPGSaveMigrationContext
	{
		const FRPGIdMigrationChain& Chain;
		int32 MigratedIdCount = 0;
		FString Diagnostic;
	};

	bool MigrateStruct(void* Container, const UStruct& Struct, const FString& Path, FRPGSaveMigrationContext& Context, bool& bOutChanged);

	FString JoinPath(const FString& Prefix, const FString& Segment)
	{
		return Prefix.IsEmpty() ? Segment : Prefix + TEXT(".") + Segment;
	}

	bool HasDuplicateSetElements(FScriptSetHelper& Helper, const FProperty& ElementProperty)
	{
		for (int32 Left = 0; Left < Helper.GetMaxIndex(); ++Left)
		{
			if (!Helper.IsValidIndex(Left))
			{
				continue;
			}
			for (int32 Right = Left + 1; Right < Helper.GetMaxIndex(); ++Right)
			{
				if (Helper.IsValidIndex(Right) && ElementProperty.Identical(Helper.GetElementPtr(Left), Helper.GetElementPtr(Right)))
				{
					return true;
				}
			}
		}
		return false;
	}

	bool HasDuplicateMapKeys(FScriptMapHelper& Helper, const FProperty& KeyProperty)
	{
		for (int32 Left = 0; Left < Helper.GetMaxIndex(); ++Left)
		{
			if (!Helper.IsValidIndex(Left))
			{
				continue;
			}
			for (int32 Right = Left + 1; Right < Helper.GetMaxIndex(); ++Right)
			{
				if (Helper.IsValidIndex(Right) && KeyProperty.Identical(Helper.GetKeyPtr(Left), Helper.GetKeyPtr(Right)))
				{
					return true;
				}
			}
		}
		return false;
	}

	bool MigrateValue(FProperty& Property, void* Value, const FString& Path, FRPGSaveMigrationContext& Context, bool& bOutChanged)
	{
		bOutChanged = false;
		if (FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
		{
			if (StructProperty->Struct == FRPGId::StaticStruct())
			{
				FRPGId& Id = *static_cast<FRPGId*>(Value);
				FRPGId MigratedId;
				if (!Context.Chain.TryResolve(Id, MigratedId))
				{
					Context.Diagnostic = FString::Printf(TEXT("The migration chain could not resolve %s."), *Path);
					return false;
				}
				if (MigratedId != Id)
				{
					Id = MigratedId;
					++Context.MigratedIdCount;
					bOutChanged = true;
				}
				return true;
			}

			if (StructProperty->Struct == FInstancedStruct::StaticStruct())
			{
				FInstancedStruct& Instance = *static_cast<FInstancedStruct*>(Value);
				if (!Instance.IsValid())
				{
					return true;
				}
				return MigrateStruct(Instance.GetMutableMemory(), *Instance.GetScriptStruct(), Path, Context, bOutChanged);
			}
			return MigrateStruct(Value, *StructProperty->Struct, Path, Context, bOutChanged);
		}

		if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(&Property))
		{
			FScriptArrayHelper Helper(ArrayProperty, Value);
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				bool bElementChanged = false;
				if (!MigrateValue(*ArrayProperty->Inner, Helper.GetRawPtr(Index), FString::Printf(TEXT("%s[%d]"), *Path, Index), Context,
					bElementChanged))
				{
					return false;
				}
				bOutChanged |= bElementChanged;
			}
			return true;
		}

		if (FSetProperty* SetProperty = CastField<FSetProperty>(&Property))
		{
			FScriptSetHelper Helper(SetProperty, Value);
			for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
			{
				if (!Helper.IsValidIndex(Index))
				{
					continue;
				}
				bool bElementChanged = false;
				if (!MigrateValue(*SetProperty->ElementProp, Helper.GetElementPtr(Index), FString::Printf(TEXT("%s{%d}"), *Path, Index),
					Context, bElementChanged))
				{
					return false;
				}
				bOutChanged |= bElementChanged;
			}
			if (bOutChanged)
			{
				if (HasDuplicateSetElements(Helper, *SetProperty->ElementProp))
				{
					Context.Diagnostic = FString::Printf(TEXT("RPG Id migration creates duplicate set elements at %s."), *Path);
					return false;
				}
				Helper.Rehash();
			}
			return true;
		}

		if (FMapProperty* MapProperty = CastField<FMapProperty>(&Property))
		{
			FScriptMapHelper Helper(MapProperty, Value);
			bool bAnyKeyChanged = false;
			for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
			{
				if (!Helper.IsValidIndex(Index))
				{
					continue;
				}
				const FString EntryPath = FString::Printf(TEXT("%s{%d}"), *Path, Index);
				bool bKeyChanged = false;
				bool bValueChanged = false;
				if (!MigrateValue(*MapProperty->KeyProp, Helper.GetKeyPtr(Index), EntryPath + TEXT(".Key"), Context, bKeyChanged)
					|| !MigrateValue(*MapProperty->ValueProp, Helper.GetValuePtr(Index), EntryPath + TEXT(".Value"), Context, bValueChanged))
				{
					return false;
				}
				bAnyKeyChanged |= bKeyChanged;
				bOutChanged |= bKeyChanged || bValueChanged;
			}
			if (bAnyKeyChanged)
			{
				if (HasDuplicateMapKeys(Helper, *MapProperty->KeyProp))
				{
					Context.Diagnostic = FString::Printf(TEXT("RPG Id migration creates duplicate map keys at %s."), *Path);
					return false;
				}
				Helper.Rehash();
			}
		}
		return true;
	}

	bool MigrateStruct(void* Container, const UStruct& Struct, const FString& Path, FRPGSaveMigrationContext& Context, bool& bOutChanged)
	{
		bOutChanged = false;
		for (TFieldIterator<FProperty> It(&Struct); It; ++It)
		{
			FProperty& Property = **It;
			if (!Property.HasAnyPropertyFlags(CPF_SaveGame))
			{
				continue;
			}
			for (int32 ArrayIndex = 0; ArrayIndex < Property.ArrayDim; ++ArrayIndex)
			{
				bool bPropertyChanged = false;
				const FString PropertyPath = JoinPath(Path, Property.GetName());
				if (!MigrateValue(Property, Property.ContainerPtrToValuePtr<void>(Container, ArrayIndex), PropertyPath, Context, bPropertyChanged))
				{
					return false;
				}
				bOutChanged |= bPropertyChanged;
			}
		}
		return true;
	}
}

FRPGSaveGameMigrationResult FRPGSaveGameMigrator::MigrateToCurrent(URPGSaveGame& SaveGame)
{
	FRPGReleaseMigrationCatalog Catalog;
	const FRPGReleaseMigrationCatalogResult CatalogResult = FRPGReleaseMigrationCatalogReader::ReadCurrent(Catalog);
	if (!CatalogResult.IsSuccess())
	{
		return { ERPGSaveGameMigrationResult::InvalidReleaseCatalog, 0, CatalogResult.Diagnostic };
	}
	return Migrate(SaveGame, Catalog);
}

FRPGSaveGameMigrationResult FRPGSaveGameMigrator::Migrate(URPGSaveGame& SaveGame,
	const FRPGReleaseMigrationCatalog& Catalog)
{
	return Migrate(SaveGame, Catalog.CurrentVersion, Catalog.Steps);
}

FRPGSaveGameMigrationResult FRPGSaveGameMigrator::Migrate(URPGSaveGame& SaveGame, uint16 TargetVersion,
	TConstArrayView<FRPGIdMigrationStep> Steps)
{
	FRPGIdMigrationChain Chain;
	const FRPGIdMigrationResult ChainResult = FRPGIdMigrationChain::Build(SaveGame.SaveDataVersion, TargetVersion, Steps, Chain);
	if (!ChainResult.IsSuccess())
	{
		return { ERPGSaveGameMigrationResult::InvalidMigrationChain, 0, ChainResult.Diagnostic };
	}

	TMap<FName, FInstancedStruct> MigratedModules = SaveGame.SaveModules;
	FRPGSaveMigrationContext Context{ Chain };
	for (TPair<FName, FInstancedStruct>& Pair : MigratedModules)
	{
		if (!Pair.Value.IsValid())
		{
			continue;
		}

		bool bModuleChanged = false;
		if (!MigrateStruct(Pair.Value.GetMutableMemory(), *Pair.Value.GetScriptStruct(), Pair.Key.ToString(), Context, bModuleChanged))
		{
			return { ERPGSaveGameMigrationResult::ContainerKeyCollision, 0, Context.Diagnostic };
		}
	}

	SaveGame.SaveModules = MoveTemp(MigratedModules);
	SaveGame.SaveDataVersion = TargetVersion;
	return { ERPGSaveGameMigrationResult::Success, Context.MigratedIdCount, FString() };
}
