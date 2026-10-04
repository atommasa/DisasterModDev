// Copyright Ironic Studio. All Rights Reserved.
#pragma once

#include "ContentBrowserFrontEndFilterExtension.h"
#include "RPGIdCategoryService.h"
#include "RPGIdCategoryFilter.generated.h"

struct FAssetData;

struct FRPGIdCategoryFilterChoice
{
public:
	FName AssetType;
	/** None denotes unclassified, never an unclaimed Id. */
	FName CategoryKey;

public:
	bool operator==(const FRPGIdCategoryFilterChoice& Other) const
	{
		return AssetType == Other.AssetType && CategoryKey == Other.CategoryKey;
	}
};

/** A prepared, per-browser snapshot. Predicate queries never read config, audit references or load assets. */
class FRPGIdCategoryFilterModel
{
public:
	bool Refresh(const FRPGIdCategoryStore& Store);
	void Toggle(FRPGIdCategoryFilterChoice Choice);
	void Clear() { Selected.Reset(); }
	bool IsSelected(FRPGIdCategoryFilterChoice Choice) const { return Selected.Contains(Choice); }
	bool Matches(const FAssetData& Asset, bool bRegistryReady, bool& bUnknown) const;
	const TMap<FName, TArray<FRPGIdCategoryScope>>& GetScopes() const { return ScopesByType; }
	const TArray<FRPGIdCategoryFilterChoice>& GetSelected() const { return Selected; }
	void SetSelected(TArray<FRPGIdCategoryFilterChoice> Choices) { Selected = MoveTemp(Choices); }
	FString GetErrors() const;

private:
	TArray<FRPGIdCategoryFilterChoice> Selected;
	TMap<FName, FName> Prefixes;
	TMap<FTopLevelAssetPath, FName> ClassTypes;
	TMap<FName, TArray<FRPGIdCategoryScope>> ScopesByType;
	TMap<FName, FString> ErrorsByType;
};

UCLASS()
class URPGIdCategoryFilterExtension : public UContentBrowserFrontEndFilterExtension
{
	GENERATED_BODY()

public:
	virtual void AddFrontEndFilterExtensions(TSharedPtr<FFrontendFilterCategory> DefaultCategory,
		TArray<TSharedRef<FFrontendFilter>>& InOutFilterList) const override;
};
