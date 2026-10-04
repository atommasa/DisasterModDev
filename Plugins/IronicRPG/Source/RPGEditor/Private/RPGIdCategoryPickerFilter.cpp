// Copyright Ironic Studio. All Rights Reserved.

#include "RPGIdCategoryPickerFilter.h"

#include "SRPGIdCategoryPickerFilter.h"

#include "AssetRegistry/AssetData.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "RPGIdCategoryPickerFilter"

FRPGIdCategoryPickerFilter::FRPGIdCategoryPickerFilter(FName InLimitedType, FRPGIdCategoryStore* InStore)
	: Store(InStore ? *InStore : FRPGIdCategoryStore::Get())
	, LimitedType(InLimitedType)
{
	StoreChangedHandle = Store.OnChanged().AddRaw(this, &FRPGIdCategoryPickerFilter::HandleStoreChanged);
	Refresh();
}

FRPGIdCategoryPickerFilter::~FRPGIdCategoryPickerFilter()
{
	Store.OnChanged().Remove(StoreChangedHandle);
}

bool FRPGIdCategoryPickerFilter::ShouldFilterAsset(const FAssetData& Asset) const
{
	if (!Selection.IsSet())
	{
		return false;
	}

	const FPrimaryAssetId PrimaryAssetId = Asset.GetPrimaryAssetId();
	return !PrimaryAssetId.IsValid() || !Matches(PrimaryAssetId.PrimaryAssetType.GetName(), PrimaryAssetId.PrimaryAssetName);
}

bool FRPGIdCategoryPickerFilter::Matches(FName AssetType, FName Id) const
{
	if (!Selection.IsSet())
	{
		return true;
	}

	const FRPGIdCategoryPickerChoice& Choice = Selection.GetValue();
	if (AssetType != Choice.AssetType)
	{
		return false;
	}
	if (Choice.Kind == ERPGIdSuggestionScope::AllNumbers)
	{
		return true;
	}

	const FName* Prefix = Prefixes.Find(AssetType);
	const TArray<FRPGIdCategoryScope>* Scopes = ScopesByType.Find(AssetType);
	if (!Prefix || !Scopes)
	{
		return false;
	}

	const FRPGIdCategoryResolution Resolution = FRPGIdCategoryResolver::ResolveInScopes(*Prefix, Id, *Scopes);
	if (Choice.Kind == ERPGIdSuggestionScope::Category)
	{
		return Resolution.State == ERPGIdCategoryState::Classified && Resolution.CategoryKey == Choice.CategoryKey;
	}

	return Choice.Kind == ERPGIdSuggestionScope::Unclassified && Resolution.State == ERPGIdCategoryState::Unclassified;
}

bool FRPGIdCategoryPickerFilter::HasChoices() const
{
	if (LimitedType.IsNone())
	{
		return !ScopesByType.IsEmpty();
	}
	for (const auto& Entry : ScopesByType)
	{
		if (Entry.Value.ContainsByPredicate([](const FRPGIdCategoryScope& Scope)
			{ return Scope.Kind == ERPGIdSuggestionScope::Category || Scope.Kind == ERPGIdSuggestionScope::Unclassified; }))
		{
			return true;
		}
	}
	return false;
}

bool FRPGIdCategoryPickerFilter::IsEnabled() const
{
	return HasChoices() && (LimitedType.IsNone() || !ErrorsByType.Contains(LimitedType));
}

void FRPGIdCategoryPickerFilter::Select(TOptional<FRPGIdCategoryPickerChoice> NewSelection)
{
	if (NewSelection.IsSet() && !IsChoiceAvailable(NewSelection.GetValue()))
	{
		NewSelection.Reset();
		LastResetReason = TEXT("The selected category is no longer available.");
	}
	if (Selection == NewSelection)
	{
		return;
	}
	Selection = MoveTemp(NewSelection);
	Changed.Broadcast();
}

bool FRPGIdCategoryPickerFilter::IsSelected(const TOptional<FRPGIdCategoryPickerChoice>& Choice) const
{
	return Selection == Choice;
}

FString FRPGIdCategoryPickerFilter::GetStatus() const
{
	TArray<FString> Messages;
	if (!LastResetReason.IsEmpty())
	{
		Messages.Add(LastResetReason);
	}
	if (!LimitedType.IsNone())
	{
		if (const FString* Error = ErrorsByType.Find(LimitedType))
		{
			Messages.Add(*Error);
		}
		else if (!HasChoices())
		{
			Messages.Add(TEXT("No categories are defined for this AssetType."));
		}
	}
	else
	{
		for (const auto& Entry : ErrorsByType)
		{
			Messages.Add(Entry.Key.ToString() + TEXT(": ") + Entry.Value);
		}
		if (!HasChoices())
		{
			Messages.Add(TEXT("No RPG Id categories are currently available."));
		}
	}
	return FString::Join(Messages, TEXT("\n"));
}

FString FRPGIdCategoryPickerFilter::GetScopeLabel(FName AssetType, const FRPGIdCategoryScope& Scope) const
{
	const FName* Prefix = Prefixes.Find(AssetType);
	FString Ranges;
	for (const FRPGIdCategoryRange& Range : Scope.Ranges)
	{
		Ranges += (Ranges.IsEmpty() ? TEXT("") : TEXT(", ")) + FString::Printf(TEXT("%s%04d–%s%04d"),
			Prefix ? *Prefix->ToString() : TEXT(""), Range.Start, Prefix ? *Prefix->ToString() : TEXT(""), Range.End);
	}
	return Scope.DisplayName + TEXT(" (") + Ranges + TEXT(")");
}

void FRPGIdCategoryPickerFilter::Refresh()
{
	Prefixes = FRPGIdCategoryResolver::ReadNativePrefixes();
	ScopesByType.Reset();
	ErrorsByType.Reset();
	TArray<FName> Types;
	if (LimitedType.IsNone())
	{
		Prefixes.GetKeys(Types);
	}
	else
	{
		Types.Add(LimitedType);
	}
	Types.Sort(FNameLexicalLess());
	for (const FName Type : Types)
	{
		TArray<FRPGIdCategoryScope> Scopes;
		FString Error;
		if (Store.ListScopes(Type, Scopes, Error))
		{
			const int32 AllNumbersIndex = Scopes.IndexOfByPredicate([](const FRPGIdCategoryScope& Scope)
				{ return Scope.Kind == ERPGIdSuggestionScope::AllNumbers; });
			if (AllNumbersIndex == INDEX_NONE)
			{
				FRPGIdCategoryScope Scope;
				Scope.Kind = ERPGIdSuggestionScope::AllNumbers;
				Scope.Ranges.Add({0, 9999});
				Scopes.Insert(MoveTemp(Scope), 0);
			}
			else if (AllNumbersIndex != 0)
			{
				FRPGIdCategoryScope Scope = MoveTemp(Scopes[AllNumbersIndex]);
				Scopes.RemoveAt(AllNumbersIndex);
				Scopes.Insert(MoveTemp(Scope), 0);
			}
			Scopes[0].DisplayName = TEXT("All");
			ScopesByType.Add(Type, MoveTemp(Scopes));
		}
		else
		{
			ErrorsByType.Add(Type, MoveTemp(Error));
		}
	}
	ScopesByType.KeySort(FNameLexicalLess());
	if (Selection.IsSet() && !IsChoiceAvailable(Selection.GetValue()))
	{
		Selection.Reset();
		LastResetReason = TEXT("The selected category no longer exists or is unavailable; the picker returned to All.");
	}
	else if (Selection.IsSet())
	{
		LastResetReason.Reset();
	}
}

void FRPGIdCategoryPickerFilter::HandleStoreChanged()
{
	Refresh();
	Changed.Broadcast();
}

bool FRPGIdCategoryPickerFilter::IsChoiceAvailable(const FRPGIdCategoryPickerChoice& Choice) const
{
	return FindScope(Choice) != nullptr;
}

const FRPGIdCategoryScope* FRPGIdCategoryPickerFilter::FindScope(const FRPGIdCategoryPickerChoice& Choice) const
{
	const TArray<FRPGIdCategoryScope>* Scopes = ScopesByType.Find(Choice.AssetType);
	if (!Scopes)
	{
		return nullptr;
	}
	return Scopes->FindByPredicate([&Choice](const FRPGIdCategoryScope& Scope)
	{
		return Scope.Kind == Choice.Kind && (Scope.Kind != ERPGIdSuggestionScope::Category || Scope.CategoryKey == Choice.CategoryKey);
	});
}

void SRPGIdCategoryPickerFilter::Construct(const FArguments& InArgs)
{
	Filter = MakeUnique<FRPGIdCategoryPickerFilter>(InArgs._LimitedType);
	OnSelectionChanged = InArgs._OnSelectionChanged;
	FilterChangedHandle = Filter->OnChanged().AddRaw(this, &SRPGIdCategoryPickerFilter::HandleFilterChanged);
	ChildSlot
	[
		SAssignNew(ComboButton, SComboButton)
		.OnGetMenuContent(this, &SRPGIdCategoryPickerFilter::BuildMenu)
		.IsEnabled_Lambda([this]() { return Filter->IsEnabled(); })
		.ToolTipText(this, &SRPGIdCategoryPickerFilter::GetToolTipText)
		.ButtonContent()
		[
			SNew(STextBlock).Text(this, &SRPGIdCategoryPickerFilter::GetButtonText)
		]
	];
}

SRPGIdCategoryPickerFilter::~SRPGIdCategoryPickerFilter()
{
	if (Filter)
	{
		Filter->OnChanged().Remove(FilterChangedHandle);
	}
}

bool SRPGIdCategoryPickerFilter::ShouldFilterAsset(const FAssetData& Asset) const
{
	return Filter && Filter->ShouldFilterAsset(Asset);
}

TSharedRef<SWidget> SRPGIdCategoryPickerFilter::BuildMenu()
{
	// This selector is nested inside the Asset picker popup. Its actions close only this combo,
	// so the author can change categories repeatedly without dismissing the Asset picker.
	FMenuBuilder Menu(false, nullptr);
	Menu.BeginSection(NAME_None, LOCTEXT("Category", "RPG Id Category"));
	Menu.AddMenuEntry(LOCTEXT("All", "All"), LOCTEXT("AllTip", "Do not apply a Category filter."), FSlateIcon(), FUIAction(
		FExecuteAction::CreateSP(this, &SRPGIdCategoryPickerFilter::Select, TOptional<FRPGIdCategoryPickerChoice>()), FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() { return Filter->IsSelected(TOptional<FRPGIdCategoryPickerChoice>()); })),
		NAME_None, EUserInterfaceActionType::RadioButton);
	Menu.EndSection();
	if (Filter->GetLimitedType().IsNone())
	{
		for (const auto& Entry : Filter->GetScopesByType())
		{
			Menu.AddSubMenu(FText::FromName(Entry.Key), FText(),
				FNewMenuDelegate::CreateSP(this, &SRPGIdCategoryPickerFilter::BuildTypeMenu, Entry.Key), false, FSlateIcon(), false);
		}
	}
	else
	{
		BuildTypeMenu(Menu, Filter->GetLimitedType());
	}
	const FString Status = Filter->GetStatus();
	if (!Status.IsEmpty())
	{
		Menu.AddWidget(SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(Status)), FText());
	}
	return Menu.MakeWidget();
}

void SRPGIdCategoryPickerFilter::BuildTypeMenu(FMenuBuilder& Menu, FName AssetType)
{
	const TArray<FRPGIdCategoryScope>* Scopes = Filter->GetScopesByType().Find(AssetType);
	if (!Scopes)
	{
		return;
	}
	for (const FRPGIdCategoryScope& Scope : *Scopes)
	{
		if (Scope.Kind == ERPGIdSuggestionScope::AllNumbers && !Filter->GetLimitedType().IsNone())
		{
			continue;
		}
		const FRPGIdCategoryPickerChoice Choice{AssetType, Scope.Kind, Scope.CategoryKey};
		const FText Label = Scope.Kind == ERPGIdSuggestionScope::AllNumbers
			? LOCTEXT("AllType", "All") : FText::FromString(Filter->GetScopeLabel(AssetType, Scope));
		const FText ToolTip = Scope.Kind == ERPGIdSuggestionScope::AllNumbers
			? FText::Format(LOCTEXT("AllTypeTip", "Show every {0} asset."), FText::FromName(AssetType)) : FText();
		Menu.AddMenuEntry(Label, ToolTip, FSlateIcon(), FUIAction(
			FExecuteAction::CreateSP(this, &SRPGIdCategoryPickerFilter::Select, TOptional<FRPGIdCategoryPickerChoice>(Choice)), FCanExecuteAction(),
			FIsActionChecked::CreateLambda([this, Choice]() { return Filter->IsSelected(Choice); })),
			NAME_None, EUserInterfaceActionType::RadioButton);
	}
}

void SRPGIdCategoryPickerFilter::Select(TOptional<FRPGIdCategoryPickerChoice> Choice)
{
	ComboButton->SetIsOpen(false);
	Filter->Select(MoveTemp(Choice));
}

void SRPGIdCategoryPickerFilter::HandleFilterChanged()
{
	Invalidate(EInvalidateWidgetReason::Layout);
	OnSelectionChanged.ExecuteIfBound();
}

FText SRPGIdCategoryPickerFilter::GetButtonText() const
{
	if (!Filter->GetSelection().IsSet())
	{
		return LOCTEXT("AllLabel", "Category: All");
	}
	const FRPGIdCategoryPickerChoice& Choice = Filter->GetSelection().GetValue();
	const TArray<FRPGIdCategoryScope>* Scopes = Filter->GetScopesByType().Find(Choice.AssetType);
	const FRPGIdCategoryScope* Scope = Scopes ? Scopes->FindByPredicate([&Choice](const FRPGIdCategoryScope& Candidate)
	{
		return Candidate.Kind == Choice.Kind && (Choice.Kind != ERPGIdSuggestionScope::Category || Candidate.CategoryKey == Choice.CategoryKey);
	}) : nullptr;
	const FString TypePrefix = Filter->GetLimitedType().IsNone() ? Choice.AssetType.ToString() + TEXT(" / ") : TEXT("");
	return FText::FromString(TEXT("Category: ") + TypePrefix + (Scope ? Scope->DisplayName : TEXT("All")));
}

FText SRPGIdCategoryPickerFilter::GetToolTipText() const
{
	const FString Status = Filter->GetStatus();
	return Status.IsEmpty() ? LOCTEXT("Tip", "Temporarily filters reference candidates by RPG Id Category.") : FText::FromString(Status);
}

#undef LOCTEXT_NAMESPACE
