// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

enum class ERPGCollectionItemType : uint8
{
	Type,
	Section,
};

struct FRPGCollectionItem : public TSharedFromThis<FRPGCollectionItem>
{
	FName CollectionName;
	ERPGCollectionItemType CollectionType;

	FName Path;

	TArray<TSharedPtr<FRPGCollectionItem>> Children;

	FString GetIconStyleName() const
	{
		return FString::Printf(TEXT("AssetType.%s"), *CollectionName.ToString());
	}
};
