// Copyright Ironic Studio. All Rights Reserved.


#include "Misc/SynchronousTableMaster.h"

USynchronousTableMaster::USynchronousTableMaster(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

TArray<FName> USynchronousTableMaster::GetMasterKeys() const
{
	TArray<FName> Results;
	for (const FSyncTableEntry Entry : MasterKeys)
	{
		Results.Add(Entry.Key);
	}

	return Results;
}

TArray<FName> USynchronousTableMaster::GetMasterCategories() const
{
	TArray<FName> Results = Categories;
	Results.Add(NAME_None); // Add none category

	return Results;
}

#if WITH_EDITOR
void USynchronousTableMaster::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent)
{
	Super::PostEditChangeProperty(PropertyChangedChainEvent);
	
	const FName PropertyName = PropertyChangedChainEvent.MemberProperty->GetFName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(USynchronousTableMaster, MasterKeys))
	{
		
	}
}
#endif // WITH_EDITOR
