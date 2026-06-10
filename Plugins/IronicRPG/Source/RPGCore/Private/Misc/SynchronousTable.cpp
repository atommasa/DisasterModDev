// Copyright Ironic Studio. All Rights Reserved.


#include "Misc/SynchronousTable.h"
#include "Misc/SynchronousTableMaster.h"

#include "Misc/TransactionObjectEvent.h"

USynchronousTable::USynchronousTable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectTransacted.AddUObject(this, &USynchronousTable::OnObjectTransacted);
#endif // WITH_EDITOR
}

TArray<FName> USynchronousTable::GetMasterCategories() const
{
	if (!TableMasterClass)
	{
		return TArray<FName>();
	}

	USynchronousTableMaster* Master = TableMasterClass->GetDefaultObject<USynchronousTableMaster>();
	if (!Master)
	{
		return TArray<FName>();
	}

	return Master->GetMasterCategories();
}

FInstancedStruct USynchronousTable::GetData(const FName& Key) const
{
	return Table.FindRef(Key);
}

#if WITH_EDITOR
void USynchronousTable::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent)
{
	Super::PostEditChangeProperty(PropertyChangedChainEvent);
	
	const FName PropertyName = PropertyChangedChainEvent.MemberProperty->GetFName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(USynchronousTable, TableMasterClass)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(USynchronousTable, SupportedCategories))
	{
		Table.Empty();
		UpdateTableKeys();
	}
}

void USynchronousTable::UpdateTableSchema()
{
	if (!TableMasterClass)
	{
		return;
	}

	USynchronousTableMaster* Master = TableMasterClass->GetDefaultObject<USynchronousTableMaster>();
	if (!Master)
	{
		return;
	}

	for (auto& Pair : Table)
	{
		if (Pair.Value.GetScriptStruct() != Master->SchemaStruct)
		{
			Pair.Value.InitializeAs(Master->SchemaStruct);
		}
	}
}

void USynchronousTable::UpdateTableKeys()
{
	if (!TableMasterClass)
	{
		return;
	}

	USynchronousTableMaster* Master = TableMasterClass->GetDefaultObject<USynchronousTableMaster>();
	if (!Master)
	{
		return;
	}

	for (const FSyncTableEntry& TableKey : Master->MasterKeys)
	{
		if (!SupportedCategories.Contains(TableKey.Category))
		{
			continue;
		}

		if (!Table.Contains(TableKey.Key))
		{
			FInstancedStruct NewRow;
			NewRow.InitializeAs(Master->SchemaStruct);
			Table.Add(TableKey.Key, NewRow);
		}
	}

	for (auto It = Table.CreateIterator(); It; ++It)
	{
		for (const FName& Category : SupportedCategories)
		{
			if (!Master->MasterKeys.Contains(FSyncTableEntry(It.Key(), Category)))
			{
				It.RemoveCurrent();
			}
		}
	}
}

void USynchronousTable::OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event)
{
	if (!TableMasterClass)
	{
		return;
	}

	USynchronousTableMaster* Master = TableMasterClass->GetDefaultObject<USynchronousTableMaster>();
	if (Master && Master == Object)
	{
		const TArray<FName>& Properties = Event.GetChangedProperties();

		if (Properties.Contains(GET_MEMBER_NAME_CHECKED(USynchronousTableMaster, SchemaStruct)))
		{
			UpdateTableSchema();
		}

		if (Properties.Contains(GET_MEMBER_NAME_CHECKED(USynchronousTableMaster, MasterKeys)))
		{
			UpdateTableKeys();
		}
	}
}
#endif // WITH_EDITOR