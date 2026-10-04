// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RPGIdCategoryService.h"

struct FAssetData;

/** A single temporary picker selection. All is represented by no selection. */
struct FRPGIdCategoryPickerChoice
{
public:
	FName AssetType;
	ERPGIdSuggestionScope Kind = ERPGIdSuggestionScope::Category;
	FName CategoryKey;

public:
	bool operator==(const FRPGIdCategoryPickerChoice& Other) const
	{
		return AssetType == Other.AssetType && Kind == Other.Kind && CategoryKey == Other.CategoryKey;
	}
};

/**
 * Editor-only, no-load category predicate shared by FRPGId Details and graph-pin pickers.
 * It owns only transient UI selection; it never mutates an Id, asset, graph or config.
 */
class FRPGIdCategoryPickerFilter
{
public:
	explicit FRPGIdCategoryPickerFilter(FName InLimitedType = NAME_None, FRPGIdCategoryStore* InStore = nullptr);
	~FRPGIdCategoryPickerFilter();

public:
	bool ShouldFilterAsset(const FAssetData& Asset) const;
	bool Matches(FName AssetType, FName Id) const;
	bool HasChoices() const;
	bool IsEnabled() const;
	void Select(TOptional<FRPGIdCategoryPickerChoice> NewSelection);
	bool IsSelected(const TOptional<FRPGIdCategoryPickerChoice>& Choice) const;
	const TOptional<FRPGIdCategoryPickerChoice>& GetSelection() const { return Selection; }
	const TMap<FName, TArray<FRPGIdCategoryScope>>& GetScopesByType() const { return ScopesByType; }
	const TMap<FName, FName>& GetPrefixes() const { return Prefixes; }
	FName GetLimitedType() const { return LimitedType; }
	FString GetStatus() const;
	FString GetScopeLabel(FName AssetType, const FRPGIdCategoryScope& Scope) const;
	FSimpleMulticastDelegate& OnChanged() { return Changed; }

private:
	void Refresh();
	void HandleStoreChanged();
	bool IsChoiceAvailable(const FRPGIdCategoryPickerChoice& Choice) const;
	const FRPGIdCategoryScope* FindScope(const FRPGIdCategoryPickerChoice& Choice) const;

private:
	FRPGIdCategoryStore& Store;
	FName LimitedType;
	TMap<FName, FName> Prefixes;
	TMap<FName, TArray<FRPGIdCategoryScope>> ScopesByType;
	TMap<FName, FString> ErrorsByType;
	TOptional<FRPGIdCategoryPickerChoice> Selection;
	FString LastResetReason;
	FDelegateHandle StoreChangedHandle;
	FSimpleMulticastDelegate Changed;
};
