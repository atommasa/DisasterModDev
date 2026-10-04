// Copyright Ironic Studio. All Rights Reserved.
#include "RPGIdCategoryService.h"

#include "Assets/RPGAssetManager.h"
#include "Assets/RPGPrimaryAsset.h"
#include "RPGSettings.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#else
#include <cstdio>
#endif

namespace
{
	const TCHAR* CategorySection = TEXT("/Script/RPGEditor.RPGIdCategorySettings");
#if WITH_DEV_AUTOMATION_TESTS
	FRPGIdCategoryStore* CategoryTestOverride = nullptr;
#endif

	bool Fail(FString& Error, const FString& Message)
	{
		Error = Message;
		return false;
	}

	bool ReadSource(const FString& Filename, FString& Text, bool& bExists, FString& Error)
	{
		Text.Reset();
		bExists = IFileManager::Get().FileExists(*Filename);
		return !bExists || FFileHelper::LoadFileToString(Text, *Filename) || Fail(Error, TEXT("Cannot read category config: ") + Filename);
	}

	class FCategoryImportErrors : public FOutputDevice
	{
	public:
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			bError = true;
			Details += Message;
		}
	public:
		bool bError = false;
		FString Details;
	};

	bool Parse(const FString& Text, FRPGIdCategoryRules& Out, FString& Error)
	{
		Out = {};
		FConfigFile Config;
		Config.ProcessInputFileContents(Text, TEXT("RPGIdCategories"));
		const FConfigSection* Section = Config.FindSection(CategorySection);
		if (!Section) { return true; }
		if (Section->Num() != 2 || !Section->Find(TEXT("SchemaVersion")) || !Section->Find(TEXT("Rules")))
		{
			return Fail(Error, TEXT("Category section must contain exactly SchemaVersion and Rules. Reload after repairing the config."));
		}
		FString Version, Serialized;
		Config.GetString(CategorySection, TEXT("SchemaVersion"), Version);
		Config.GetString(CategorySection, TEXT("Rules"), Serialized);
		if ((Version != TEXT("1") && Version != TEXT("2")) || Serialized.IsEmpty())
		{
			return Fail(Error, TEXT("Unsupported or incomplete category config."));
		}
		FCategoryImportErrors Errors;
		FRPGIdCategoryLegacyRules Legacy;
		const TCHAR* End = Version == TEXT("1")
			? FRPGIdCategoryLegacyRules::StaticStruct()->ImportText(*Serialized, &Legacy, nullptr, PPF_None, &Errors, TEXT("CategoryRulesV1"))
			: FRPGIdCategoryRules::StaticStruct()->ImportText(*Serialized, &Out, nullptr, PPF_None, &Errors, TEXT("CategoryRulesV2"));
		if (!End || Errors.bError || !FString(End).TrimStartAndEnd().IsEmpty())
		{
			Out = {};
			return Fail(Error, TEXT("Malformed category rules. No previous rules are being used. ") + Errors.Details);
		}
		for (const auto& Category : Legacy.Categories)
		{
			Out.AssetTypes.FindOrAdd(Category.AssetType).Categories.Add(static_cast<const FRPGIdCategoryDefinition&>(Category));
		}
		return true;
	}

	FString ReplaceSection(const FString& Source, const FRPGIdCategoryRules& Rules)
	{
		// Keep unrelated sections verbatim rather than regenerating the user's entire config.
		FString Result;
		bool bSkipping = false;
		int32 Offset = 0;
		while (Offset < Source.Len())
		{
			int32 End = Source.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Offset);
			End = End == INDEX_NONE ? Source.Len() : End + 1;
			const FString Line = Source.Mid(Offset, End - Offset);
			const FString Trimmed = Line.TrimStartAndEnd();
			if (Trimmed.StartsWith(TEXT("[")) && Trimmed.EndsWith(TEXT("]")))
			{
				bSkipping = Trimmed == FString::Printf(TEXT("[%s]"), CategorySection);
			}
			if (!bSkipping) { Result += Line; }
			Offset = End;
		}
		if (!Result.IsEmpty() && !Result.EndsWith(TEXT("\n"))) { Result += TEXT("\r\n"); }
		Result += FString::Printf(TEXT("\r\n[%s]\r\nSchemaVersion=2\r\nRules=%s\r\n"), CategorySection, *FRPGIdCategoryResolver::Export(Rules));
		return Result;
	}

	bool AtomicReplace(const FString& Destination, const FString& Temporary)
	{
#if PLATFORM_WINDOWS
		return MoveFileExW(*Temporary, *Destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
		return std::rename(TCHAR_TO_UTF8(*Temporary), TCHAR_TO_UTF8(*Destination)) == 0;
#endif
	}
}

TArray<FString> FRPGIdCategoryResolver::Validate(const FRPGIdCategoryRules& Rules, const TMap<FName, FName>& Prefixes, FName OnlyType)
{
	TArray<FString> Errors;
	TMap<FName, TSet<FName>> Keys;
	TMap<FName, TSet<FString>> Names;
	TMap<FName, TArray<TPair<FRPGIdCategoryRange, FName>>> Seen;
	for (const auto& Entry : Rules.AssetTypes)
	{
		const FName Type = Entry.Key;
		if (!OnlyType.IsNone() && !Type.IsNone() && Type != OnlyType) { continue; }
		if (!Prefixes.Contains(Type)) { Errors.Add(Type.ToString() + TEXT(": Unknown or inconsistent native AssetType/prefix.")); }
		for (const FRPGIdCategoryDefinition& Category : Entry.Value.Categories)
		{
			const FString Context = Type.ToString() + TEXT(" / ") + Category.CategoryKey.ToString() + TEXT(": ");
			if (Category.CategoryKey.IsNone() || Keys.FindOrAdd(Type).Contains(Category.CategoryKey))
			{
				Errors.Add(Context + TEXT("CategoryKey must be non-None and unique within its AssetType."));
			}
			Keys.FindOrAdd(Type).Add(Category.CategoryKey);
			const FString Name = Category.DisplayName.TrimStartAndEnd().ToLower();
			if (Name.IsEmpty() || Names.FindOrAdd(Type).Contains(Name))
			{
				Errors.Add(Context + TEXT("DisplayName must be nonempty and unique within its AssetType."));
			}
			Names.FindOrAdd(Type).Add(Name);
			if (Category.Ranges.IsEmpty()) { Errors.Add(Context + TEXT("At least one range is required.")); }
			for (const FRPGIdCategoryRange& Range : Category.Ranges)
			{
				if (Range.Start < 0 || Range.End > 9999 || Range.Start > Range.End)
				{
					Errors.Add(Context + FString::Printf(TEXT("Invalid range %d-%d; expected 0 <= Start <= End <= 9999."), Range.Start, Range.End));
					continue;
				}
				for (const auto& Prior : Seen.FindOrAdd(Type))
				{
					if (Range.Start <= Prior.Key.End && Prior.Key.Start <= Range.End)
					{
						Errors.Add(Context + FString::Printf(TEXT("Range %d-%d overlaps %s (%d-%d)."),
							Range.Start, Range.End, *Prior.Value.ToString(), Prior.Key.Start, Prior.Key.End));
					}
				}
				Seen.FindOrAdd(Type).Emplace(Range, Category.CategoryKey);
			}
		}
	}
	return Errors;
}

bool FRPGIdCategoryResolver::ListScopes(const FRPGIdCategoryRules& Rules, const TMap<FName, FName>& Prefixes, int32 NumericLen,
	FName Type, TArray<FRPGIdCategoryScope>& OutScopes, FString& Error)
{
	OutScopes.Reset();
	Error.Reset();
	if (NumericLen != 4) { return Fail(Error, TEXT("Categories support NumericLen=4 only. Identity settings were not changed.")); }
	if (!Prefixes.Contains(Type)) { return Fail(Error, TEXT("Unknown or inconsistent native AssetType/prefix.")); }
	const TArray<FString> Errors = Validate(Rules, Prefixes, Type);
	if (!Errors.IsEmpty()) { return Fail(Error, FString::Join(Errors, TEXT("\n"))); }
	TArray<FRPGIdCategoryRange> Covered;
	const auto* TypeRules = Rules.AssetTypes.Find(Type);
	const TArray<FRPGIdCategoryDefinition> Empty;
	for (const FRPGIdCategoryDefinition& Category : TypeRules ? TypeRules->Categories : Empty)
	{
		FRPGIdCategoryScope Scope;
		Scope.Kind = ERPGIdSuggestionScope::Category;
		Scope.CategoryKey = Category.CategoryKey;
		Scope.DisplayName = Category.DisplayName;
		Scope.Ranges = Category.Ranges;
		Scope.Ranges.Sort([](const auto& A, const auto& B) { return A.Start < B.Start; });
		Covered.Append(Scope.Ranges);
		OutScopes.Add(MoveTemp(Scope));
	}
	if (OutScopes.IsEmpty())
	{
		FRPGIdCategoryScope Scope;
		Scope.DisplayName = TEXT("All numbers");
		Scope.Ranges.Add({0, 9999});
		OutScopes.Add(MoveTemp(Scope));
		return true;
	}
	OutScopes.Sort([](const auto& A, const auto& B) { return A.Ranges[0].Start < B.Ranges[0].Start; });
	Covered.Sort([](const auto& A, const auto& B) { return A.Start < B.Start; });
	FRPGIdCategoryScope Remaining;
	Remaining.Kind = ERPGIdSuggestionScope::Unclassified;
	Remaining.DisplayName = TEXT("None (Unclassified)");
	int32 Next = 0;
	for (const auto& Range : Covered)
	{
		if (Next < Range.Start) { Remaining.Ranges.Add({Next, Range.Start - 1}); }
		Next = Range.End + 1;
	}
	if (Next <= 9999) { Remaining.Ranges.Add({Next, 9999}); }
	if (!Remaining.Ranges.IsEmpty()) { OutScopes.Add(MoveTemp(Remaining)); }
	return true;
}

FRPGIdCategoryResolution FRPGIdCategoryResolver::Resolve(const FRPGIdCategoryRules& Rules, const TMap<FName, FName>& Prefixes,
	int32 NumericLen, FName Type, FName Id)
{
	FRPGIdCategoryResolution Result;
	if (NumericLen != 4)
	{
		Result.State = ERPGIdCategoryState::UnsupportedFormat;
		Result.Error = TEXT("Categories support NumericLen=4 only.");
		return Result;
	}
	TArray<FRPGIdCategoryScope> Scopes;
	if (!ListScopes(Rules, Prefixes, NumericLen, Type, Scopes, Result.Error)) { return Result; }
	return ResolveInScopes(Prefixes.FindChecked(Type), Id, Scopes);
}

FRPGIdCategoryResolution FRPGIdCategoryResolver::ResolveInScopes(FName InPrefix, FName Id, const TArray<FRPGIdCategoryScope>& Scopes)
{
	FRPGIdCategoryResolution Result;
	if (Id.IsNone()) { Result.State = ERPGIdCategoryState::Unclaimed; return Result; }
	const FString Prefix = InPrefix.ToString();
	const FString Text = Id.ToString();
	const FString Number = Text.Right(4);
	bool bDigits = true;
	for (const TCHAR Character : Number) { bDigits &= Character >= TEXT('0') && Character <= TEXT('9'); }
	if (Text.Len() != Prefix.Len() + 4 || !Text.StartsWith(Prefix, ESearchCase::IgnoreCase)
		|| !bDigits)
	{
		Result.State = ERPGIdCategoryState::InvalidId;
		return Result;
	}
	const int32 Value = FCString::Atoi(*Number);
	Result.State = ERPGIdCategoryState::Unclassified;
	for (const auto& Scope : Scopes)
	{
		if (Scope.Kind == ERPGIdSuggestionScope::Category && Scope.Ranges.ContainsByPredicate([Value](const auto& R)
			{ return Value >= R.Start && Value <= R.End; }))
		{
			Result.State = ERPGIdCategoryState::Classified;
			Result.CategoryKey = Scope.CategoryKey;
			Result.DisplayName = Scope.DisplayName;
			break;
		}
	}
	return Result;
}

FString FRPGIdCategoryResolver::Export(const FRPGIdCategoryRules& Rules)
{
	FRPGIdCategoryRules Sorted = Rules;
	Sorted.AssetTypes.KeySort(FNameLexicalLess());
	FString Text;
	FRPGIdCategoryRules::StaticStruct()->ExportText(Text, &Sorted, nullptr, nullptr, PPF_None, nullptr);
	return Text;
}

void FRPGIdCategoryResolver::IncludeRegisteredTypes(FRPGIdCategoryRules& Rules, const TMap<FName, FName>& Prefixes)
{
	for (const auto& Entry : Prefixes) { Rules.AssetTypes.FindOrAdd(Entry.Key); }
	Rules.AssetTypes.KeySort(FNameLexicalLess());
}

TMap<FName, FName> FRPGIdCategoryResolver::ReadNativePrefixes()
{
	TMap<FName, FName> Result;
	const URPGAssetManager* Manager = GEngine ? Cast<URPGAssetManager>(GEngine->AssetManager) : nullptr;
	if (!Manager) { return Result; }
	const URPGSettings* Settings = GetDefault<URPGSettings>();
	const TArray<FName> RegisteredTypes = Manager->GetAllAssetTypes();
	for (const FName RegisteredType : RegisteredTypes)
	{
		const UClass* Class = Manager->GetAssetTypeClass(RegisteredType);
		if (!Class || !Class->HasAnyClassFlags(CLASS_Native)) { continue; }
		const URPGPrimaryAsset* Owner = Cast<URPGPrimaryAsset>(Class->GetDefaultObject(false));
		if (!Owner || Owner->GetAssetType() != RegisteredType) { continue; }
		const FName Type = Owner->GetAssetType();
		const FName Prefix = Owner->GetAssetIdPrefix();
		const FString PrefixText = Prefix.ToString();
		bool bLetters = !Prefix.IsNone() && PrefixText.Len() <= Settings->MaxPrefixLen;
		for (const TCHAR Character : PrefixText)
		{
			bLetters &= (Character >= TEXT('a') && Character <= TEXT('z')) || (Character >= TEXT('A') && Character <= TEXT('Z'));
		}
		if (!Type.IsNone() && bLetters && RegisteredTypes.FilterByPredicate([Type](FName Entry) { return Entry == Type; }).Num() == 1
			&& Manager->GetAssetTypeClass(Type) == Class
			&& Manager->GetIdPrefix(Type) == Prefix && Manager->GetIdTypeFromPrefix(Prefix) == Type)
		{
			Result.Add(Type, Prefix);
		}
	}
	return Result;
}

FRPGIdCategoryStore::FRPGIdCategoryStore(FString InFilename, TFunction<bool(const FString&, const FString&)> InReplace)
	: Filename(FPaths::ConvertRelativePathToFull(InFilename)), Replace(MoveTemp(InReplace)) {}

FRPGIdCategoryStore& FRPGIdCategoryStore::Get()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (CategoryTestOverride) { return *CategoryTestOverride; }
#endif
	static FRPGIdCategoryStore Store(FPaths::ProjectConfigDir() / TEXT("DefaultEditor.ini"));
	if (Store.Revision == 0) { FString Error; Store.Reload(Error); }
	return Store;
}

bool FRPGIdCategoryStore::Reload(FString& Error)
{
	check(IsInGameThread());
	Error.Reset();
	Rules = {};
	bSourceKnown = ReadSource(Filename, SourceText, bSourceExists, Error);
	const bool bParsed = bSourceKnown && Parse(SourceText, Rules, Error);
	if (bParsed) { FRPGIdCategoryResolver::IncludeRegisteredTypes(Rules, FRPGIdCategoryResolver::ReadNativePrefixes()); }
	ReadError = Error;
	++Revision;
	Changed.Broadcast();
	if (!bParsed) { return false; }
	const auto Errors = FRPGIdCategoryResolver::Validate(Rules, FRPGIdCategoryResolver::ReadNativePrefixes());
	Error = FString::Join(Errors, TEXT("\n"));
	return Errors.IsEmpty();
}

bool FRPGIdCategoryStore::Save(const FRPGIdCategoryRules& Draft, uint64 ExpectedRevision, FString& Error)
{
	check(IsInGameThread());
	Error.Reset();
	if (!bSourceKnown || Revision != ExpectedRevision) { return Fail(Error, TEXT("Settings changed or were unreadable. Reload before saving.")); }
	if (!ReadError.IsEmpty())
	{
		return Fail(Error, TEXT("Category config could not be parsed. Repair the file and Reload before saving. ") + ReadError);
	}
	if (GetDefault<URPGSettings>()->NumericLen != 4) { return Fail(Error, TEXT("Categories support NumericLen=4 only.")); }
	const auto Errors = FRPGIdCategoryResolver::Validate(Draft, FRPGIdCategoryResolver::ReadNativePrefixes());
	if (!Errors.IsEmpty()) { return Fail(Error, FString::Join(Errors, TEXT("\n"))); }
	FString Current;
	bool bExists;
	if (!ReadSource(Filename, Current, bExists, Error)) { return false; }
	if (bExists != bSourceExists || Current != SourceText) { return Fail(Error, TEXT("Config changed on disk. Reload before saving.")); }
	if (bExists && IFileManager::Get().IsReadOnly(*Filename)) { return Fail(Error, TEXT("Category config is read-only.")); }
	FRPGIdCategoryRules CompleteDraft = Draft;
	FRPGIdCategoryResolver::IncludeRegisteredTypes(CompleteDraft, FRPGIdCategoryResolver::ReadNativePrefixes());
	const FString Output = ReplaceSection(Current, CompleteDraft);
	const FString Temporary = Filename + TEXT(".categories-") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
	ON_SCOPE_EXIT { IFileManager::Get().Delete(*Temporary); };
	if (!FFileHelper::SaveStringToFile(Output, *Temporary, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return Fail(Error, TEXT("Cannot write category settings temporary file."));
	}
	FString Verified;
	FRPGIdCategoryRules VerifiedRules;
	if (!FFileHelper::LoadFileToString(Verified, *Temporary) || Verified != Output || !Parse(Verified, VerifiedRules, Error)
		|| FRPGIdCategoryResolver::Export(VerifiedRules) != FRPGIdCategoryResolver::Export(CompleteDraft))
	{
		return Fail(Error, TEXT("Category settings verification failed. Original file was not replaced."));
	}
	if (!ReadSource(Filename, Current, bExists, Error)) { return false; }
	if (bExists != bSourceExists || Current != SourceText) { return Fail(Error, TEXT("Config changed while saving. Reload before saving.")); }
	const bool bReplaced = Replace ? Replace(Filename, Temporary) : AtomicReplace(Filename, Temporary);
	if (!bReplaced) { return Fail(Error, TEXT("Cannot replace category settings. Original file was preserved.")); }
	SourceText = Output;
	bSourceExists = true;
	Rules = MoveTemp(CompleteDraft);
	ReadError.Reset();
	++Revision;
	Changed.Broadcast();
	return true;
}

FRPGIdCategoryResolution FRPGIdCategoryStore::Resolve(FName Type, FName Id) const
{
	if (!ReadError.IsEmpty()) { FRPGIdCategoryResolution Result; Result.Error = ReadError; return Result; }
	return FRPGIdCategoryResolver::Resolve(Rules, FRPGIdCategoryResolver::ReadNativePrefixes(), GetDefault<URPGSettings>()->NumericLen, Type, Id);
}

bool FRPGIdCategoryStore::ListScopes(FName Type, TArray<FRPGIdCategoryScope>& OutScopes, FString& Error) const
{
	OutScopes.Reset();
	if (!ReadError.IsEmpty()) { return Fail(Error, ReadError); }
	if (!FRPGIdCategoryResolver::ListScopes(Rules, FRPGIdCategoryResolver::ReadNativePrefixes(),
		GetDefault<URPGSettings>()->NumericLen, Type, OutScopes, Error)) { return false; }
	for (auto& Scope : OutScopes) { Scope.Revision = Revision; }
	return true;
}

bool FRPGIdCategoryStore::ResolveSelection(FName Type, const FRPGIdCategorySelection* Selection,
	FRPGIdCategoryScope& OutScope, FString& Error) const
{
	OutScope = {};
	TArray<FRPGIdCategoryScope> Scopes;
	if (!ListScopes(Type, Scopes, Error)) { return false; }
	if (Selection && Selection->Revision != Revision) { return Fail(Error, TEXT("Category rules changed. Open Suggest again.")); }
	const auto* Found = Scopes.FindByPredicate([Selection](const auto& Scope)
	{
		return Selection ? Scope.Kind == Selection->Kind && Scope.CategoryKey == Selection->CategoryKey
			: Scope.Kind == ERPGIdSuggestionScope::AllNumbers;
	});
	if (!Found) { return Fail(Error, TEXT("Choose a Category or None (Unclassified) from Suggest.")); }
	OutScope = *Found;
	return true;
}

bool FRPGIdCategoryStore::FindAvailable(const FRPGIdCategoryScope& Scope, FName Prefix, const TSet<FName>& Used, FName& OutId)
{
	OutId = NAME_None;
	for (const auto& Range : Scope.Ranges)
	{
		for (int32 Number = Range.Start; Number <= Range.End; ++Number)
		{
			const FName Candidate(*FString::Printf(TEXT("%s%04d"), *Prefix.ToString(), Number));
			if (!Used.Contains(Candidate)) { OutId = Candidate; return true; }
		}
	}
	return false;
}

#if WITH_DEV_AUTOMATION_TESTS
FRPGIdCategoryStore* FRPGIdCategoryStore::SetTestOverride(FRPGIdCategoryStore* Store)
{
	FRPGIdCategoryStore* Previous = CategoryTestOverride;
	CategoryTestOverride = Store;
	return Previous;
}
#endif
