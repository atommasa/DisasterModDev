// Copyright Ironic Studio. All Rights Reserved.
#pragma once

#include "RPGIdCategorySettings.h"

enum class ERPGIdCategoryState : uint8
{
	Classified, Unclassified, Unclaimed, InvalidId, InvalidRules, UnsupportedFormat
};

struct FRPGIdCategoryResolution
{
	ERPGIdCategoryState State = ERPGIdCategoryState::InvalidRules;
	FName CategoryKey;
	FString DisplayName;
	FString Error;
};

enum class ERPGIdSuggestionScope : uint8 { AllNumbers, Category, Unclassified };

/** Identity of a menu choice, never caller-supplied number ranges. */
struct FRPGIdCategorySelection
{
	ERPGIdSuggestionScope Kind = ERPGIdSuggestionScope::AllNumbers;
	FName CategoryKey;
	uint64 Revision = 0;
};

struct FRPGIdCategoryScope
{
	ERPGIdSuggestionScope Kind = ERPGIdSuggestionScope::AllNumbers;
	FName CategoryKey;
	FString DisplayName;
	TArray<FRPGIdCategoryRange> Ranges;
	uint64 Revision = 0;
};

/** Pure rules. No asset loading, ownership inspection, mutation or configuration I/O. */
class FRPGIdCategoryResolver
{
public:
	static TArray<FString> Validate(const FRPGIdCategoryRules& Rules, const TMap<FName, FName>& Prefixes, FName OnlyType = NAME_None);
	static FRPGIdCategoryResolution Resolve(const FRPGIdCategoryRules& Rules, const TMap<FName, FName>& Prefixes,
		int32 NumericLen, FName Type, FName Id);
	static bool ListScopes(const FRPGIdCategoryRules& Rules, const TMap<FName, FName>& Prefixes, int32 NumericLen,
		FName Type, TArray<FRPGIdCategoryScope>& OutScopes, FString& Error);
	static FString Export(const FRPGIdCategoryRules& Rules);
	static FRPGIdCategoryResolution ResolveInScopes(FName Prefix, FName Id, const TArray<FRPGIdCategoryScope>& Scopes);
	static TMap<FName, FName> ReadNativePrefixes();
	static void IncludeRegisteredTypes(FRPGIdCategoryRules& Rules, const TMap<FName, FName>& Prefixes);
};

/** Explicit disk reload/save. Instances also allow isolated file tests without touching project config. */
class FRPGIdCategoryStore
{
public:
	explicit FRPGIdCategoryStore(FString InFilename, TFunction<bool(const FString&, const FString&)> InReplace = {});
	static FRPGIdCategoryStore& Get();
	bool Reload(FString& Error);
	bool Save(const FRPGIdCategoryRules& Draft, uint64 ExpectedRevision, FString& Error);
	FRPGIdCategoryResolution Resolve(FName Type, FName Id) const;
	bool ListScopes(FName Type, TArray<FRPGIdCategoryScope>& OutScopes, FString& Error) const;
	const FRPGIdCategoryRules& GetRules() const { return Rules; }
	uint64 GetRevision() const { return Revision; }
	const FString& GetFilename() const { return Filename; }
	FSimpleMulticastDelegate& OnChanged() { return Changed; }
	bool ResolveSelection(FName Type, const FRPGIdCategorySelection* Selection, FRPGIdCategoryScope& OutScope, FString& Error) const;
	static bool FindAvailable(const FRPGIdCategoryScope& Scope, FName Prefix, const TSet<FName>& Used, FName& OutId);

#if WITH_DEV_AUTOMATION_TESTS
	static FRPGIdCategoryStore* SetTestOverride(FRPGIdCategoryStore* Store);
#endif

private:
	FString Filename;
	FString SourceText;
	FString ReadError;
	bool bSourceExists = false;
	bool bSourceKnown = false;
	uint64 Revision = 0;
	FRPGIdCategoryRules Rules;
	FSimpleMulticastDelegate Changed;
	TFunction<bool(const FString&, const FString&)> Replace;
};
