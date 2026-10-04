// Copyright Ironic Studio. All Rights Reserved.

#include "GameZoneEntryIdCustomization.h"

#include "UObject/NoExportTypes.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

#include "Assets/RPGAssetLibrary.h"
#include "Levels/GameZoneAsset.h"
#include "Levels/GameZoneContext.h"

namespace GameZoneEntryIdCustomization
{
    const FName SourceWorldMetaName(TEXT("SourceWorld"));

    FString ExportGuid(const FGuid& Guid)
    {
        FString Result;
        TBaseStructure<FGuid>::Get()->ExportText(
            Result,
            &Guid,
            nullptr,
            nullptr,
            PPF_None,
            nullptr);
        return Result;
    }

    bool ImportGuid(const FString& Text, FGuid& OutGuid)
    {
        OutGuid.Invalidate();
        return TBaseStructure<FGuid>::Get()->ImportText(
            *Text,
            &OutGuid,
            nullptr,
            PPF_None,
            GLog,
            TEXT("FGameZoneEntryIdCustomization")) != nullptr;
    }
}

void FGameZoneEntryIdCustomization::CustomizeHeader(
    TSharedRef<IPropertyHandle> PropertyHandle,
    FDetailWidgetRow& HeaderRow,
    IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    Handler = PropertyHandle;
    EntryGuidHandle = PropertyHandle->GetChildHandle(
        GET_MEMBER_NAME_CHECKED(FGameZoneEntryId, EntryGuid));
    SourceWorldHandle = ResolveSourceWorldHandle();

    if (SourceWorldHandle.IsValid())
    {
        SourceWorldHandle->SetOnPropertyValueChanged(
            FSimpleDelegate::CreateSP(
                this,
                &FGameZoneEntryIdCustomization::UpdateEntryGuidComboBox));
    }

    HeaderRow
        .NameContent()
        [
            PropertyHandle->CreatePropertyNameWidget()
        ]
        .ValueContent()
        .MinDesiredWidth(300.0f)
        .MaxDesiredWidth(500.0f)
        [
            GenerateEntryGuidComboBox()
        ];
}

void FGameZoneEntryIdCustomization::CustomizeChildren(
    TSharedRef<IPropertyHandle> PropertyHandle,
    IDetailChildrenBuilder& ChildBuilder,
    IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    // The wrapped FGuid is edited by the combo box in the struct header.
}

TSharedRef<SWidget> FGameZoneEntryIdCustomization::GenerateEntryGuidComboBox()
{
    // Always keep a real option object for None. SComboBox does not generate a
    // row for a null TSharedPtr item.
    EntryGuidOptions.Reset();
    EntryGuidOptions.Add(MakeShared<FGuid>());
    EntryNames.Reset();
    EntryNames.Add(FGuid(), FText::FromName(NAME_None));

    FGuid InitialGuid;
    GetCurrentEntryGuid(InitialGuid);
    const TSharedPtr<FGuid> InitiallySelectedItem = FindEntryOption(InitialGuid);

    TSharedRef<SWidget> Widget =
        SAssignNew(EntryGuidComboBox, SComboBox<TSharedPtr<FGuid>>)
        .OptionsSource(&EntryGuidOptions)
        .InitiallySelectedItem(InitiallySelectedItem)
        .OnComboBoxOpening(this, &FGameZoneEntryIdCustomization::UpdateEntryGuidComboBox)
        .OnGenerateWidget_Lambda([this](const TSharedPtr<FGuid> InItem)
            {
                return SNew(STextBlock).Text(CreateEntryGuidText(InItem));
            })
        .OnSelectionChanged_Lambda(
            [this](const TSharedPtr<FGuid> NewSelection, ESelectInfo::Type SelectInfo)
            {
                // Refreshing the options selects an item directly; that must not
                // create a property edit or an Undo transaction.
                if (SelectInfo == ESelectInfo::Direct || !EntryGuidHandle.IsValid())
                {
                    return;
                }

                const FGuid NewGuid = NewSelection.IsValid() ? *NewSelection : FGuid();
                const FPropertyAccess::Result WriteResult =
                    EntryGuidHandle->SetValueFromFormattedString(
                        GameZoneEntryIdCustomization::ExportGuid(NewGuid));

                if (WriteResult != FPropertyAccess::Success)
                {
                    // Keep the UI synchronized with the value that is actually
                    // stored if the property editor rejects the write.
                    FGuid StoredGuid;
                    GetCurrentEntryGuid(StoredGuid);
                    RefreshEntryGuidComboBox(StoredGuid);
                    return;
                }

                if (ComboBoxButtonTextBlock.IsValid())
                {
                    ComboBoxButtonTextBlock->SetText(CreateEntryGuidText(NewSelection));
                }
            })
        [
            SAssignNew(ComboBoxButtonTextBlock, STextBlock)
                .Text(CreateEntryGuidText(InitiallySelectedItem))
        ];

    // Build Slate first so both the immediate reset and async completion can
    // safely refresh the combo box.
    UpdateEntryGuidComboBox();
    return Widget;
}

TSharedPtr<IPropertyHandle> FGameZoneEntryIdCustomization::ResolveSourceWorldHandle() const
{
    if (!Handler.IsValid() || !Handler->GetProperty())
    {
        return nullptr;
    }

    const FString SourcePropertyPath = Handler->GetProperty()->GetMetaData(
        GameZoneEntryIdCustomization::SourceWorldMetaName);
    if (SourcePropertyPath.IsEmpty())
    {
        return nullptr;
    }

    // Search each owning struct/object level. This supports both a direct
    // sibling (SourceWorld="ZoneId") and a property nested one level deeper.
    TSharedPtr<IPropertyHandle> OwnerHandle = Handler->GetParentHandle();
    while (OwnerHandle.IsValid())
    {
        TSharedPtr<IPropertyHandle> Candidate = OwnerHandle;
        TArray<FString> PathSegments;
        SourcePropertyPath.ParseIntoArray(PathSegments, TEXT("."), true);

        for (const FString& Segment : PathSegments)
        {
            Candidate = Candidate.IsValid()
                ? Candidate->GetChildHandle(FName(*Segment))
                : nullptr;
        }

        if (Candidate.IsValid())
        {
            return Candidate;
        }

        OwnerHandle = OwnerHandle->GetParentHandle();
    }

    return nullptr;
}

bool FGameZoneEntryIdCustomization::GetSourceZoneId(FRPGId& OutZoneId) const
{
    if (!SourceWorldHandle.IsValid())
    {
        return false;
    }

    TArray<void*> RawData;
    SourceWorldHandle->AccessRawData(RawData);
    if (RawData.IsEmpty() || RawData[0] == nullptr)
    {
        return false;
    }

    OutZoneId = *static_cast<const FRPGId*>(RawData[0]);

    // A multi-object selection with different source worlds is ambiguous.
    for (int32 Index = 1; Index < RawData.Num(); ++Index)
    {
        if (RawData[Index] == nullptr ||
            *static_cast<const FRPGId*>(RawData[Index]) != OutZoneId)
        {
            return false;
        }
    }

    return OutZoneId.IsValid();
}

void FGameZoneEntryIdCustomization::UpdateEntryGuidComboBox()
{
    FGuid CurrentGuid;
    GetCurrentEntryGuid(CurrentGuid);

    EntryGuidOptions.Reset();
    EntryGuidOptions.Add(MakeShared<FGuid>());
    EntryNames.Reset();
    EntryNames.Add(FGuid(), FText::FromName(NAME_None));
    RefreshEntryGuidComboBox(CurrentGuid);

    FRPGId RequestedZoneId;
    if (!GetSourceZoneId(RequestedZoneId))
    {
        return;
    }

    const TWeakPtr<FGameZoneEntryIdCustomization> WeakThis =
        StaticCastSharedRef<FGameZoneEntryIdCustomization>(AsShared());

    // Do not capture a temporary coroutine lambda. Parameters are copied into
    // the coroutine frame and remain valid after co_await resumes.
    [](TWeakPtr<FGameZoneEntryIdCustomization> InWeakThis,
        FRPGId InRequestedZoneId) -> TRPGCoroutine<>
        {
            const auto Result =
                co_await URPGAssetLibrary::LoadAssetByRPGIdAsync<UGameZoneAsset>(
                    InRequestedZoneId, {});

            const TSharedPtr<FGameZoneEntryIdCustomization> Self = InWeakThis.Pin();
            if (!Self.IsValid())
            {
                co_return;
            }

            FRPGId LatestZoneId;
            if (!Self->GetSourceZoneId(LatestZoneId) || LatestZoneId != InRequestedZoneId)
            {
                co_return;
            }

            Self->EntryGuidOptions.Reset();
            Self->EntryGuidOptions.Add(MakeShared<FGuid>());
            Self->EntryNames.Reset();
            Self->EntryNames.Add(FGuid(), FText::FromName(NAME_None));

            if (Result.IsSuccess() && Result.Value)
            {
                for (const TPair<FGuid, FGameZonePointData>& Pair : Result.Value->GetBakedPoints())
                {
                    // The all-zero Guid is reserved for the None option.
                    if (!Pair.Key.IsValid())
                    {
                        continue;
                    }

                    Self->EntryGuidOptions.Add(MakeShared<FGuid>(Pair.Key));
                    Self->EntryNames.Add(Pair.Key, Pair.Value.DisplayName);
                }

                Self->EntryGuidOptions.Sort(
                    [Self](const TSharedPtr<FGuid>& A, const TSharedPtr<FGuid>& B)
                    {
                        const bool bAIsNone = !A.IsValid() || !A->IsValid();
                        const bool bBIsNone = !B.IsValid() || !B->IsValid();
                        if (bAIsNone != bBIsNone)
                        {
                            return bAIsNone;
                        }

                        return Self->CreateEntryGuidText(A).ToString() <
                            Self->CreateEntryGuidText(B).ToString();
                    });
            }

            FGuid LatestGuid;
            Self->GetCurrentEntryGuid(LatestGuid);
            Self->RefreshEntryGuidComboBox(LatestGuid);
        }(WeakThis, RequestedZoneId);
}

void FGameZoneEntryIdCustomization::RefreshEntryGuidComboBox(const FGuid& CurrentGuid)
{
    const TSharedPtr<FGuid> MatchingItem = FindEntryOption(CurrentGuid);

    if (EntryGuidComboBox.IsValid())
    {
        EntryGuidComboBox->RefreshOptions();
        EntryGuidComboBox->SetSelectedItem(MatchingItem);
    }

    if (ComboBoxButtonTextBlock.IsValid())
    {
        ComboBoxButtonTextBlock->SetText(
            MatchingItem.IsValid()
            ? CreateEntryGuidText(MatchingItem)
            : (CurrentGuid.IsValid()
                ? FText::FromString(CurrentGuid.ToString())
                : FText::FromName(NAME_None)));
    }
}

TSharedPtr<FGuid> FGameZoneEntryIdCustomization::FindEntryOption(const FGuid& EntryGuid) const
{
    for (const TSharedPtr<FGuid>& Option : EntryGuidOptions)
    {
        if (Option.IsValid() && *Option == EntryGuid)
        {
            return Option;
        }
    }

    return nullptr;
}

bool FGameZoneEntryIdCustomization::GetCurrentEntryGuid(FGuid& OutEntryGuid) const
{
    OutEntryGuid.Invalidate();

    if (!EntryGuidHandle.IsValid())
    {
        return false;
    }

    FString FormattedValue;
    if (EntryGuidHandle->GetValueAsFormattedString(FormattedValue)
        != FPropertyAccess::Success)
    {
        return false;
    }

    const TCHAR* ImportResult =
        TBaseStructure<FGuid>::Get()->ImportText(
            *FormattedValue,
            &OutEntryGuid,
            nullptr,
            PPF_None,
            nullptr,
            TEXT("FGameZoneEntryIdCustomization")
        );

    return ImportResult != nullptr;
}

FText FGameZoneEntryIdCustomization::CreateEntryGuidText(
    const TSharedPtr<FGuid>& EntryGuid) const
{
    if (!EntryGuid.IsValid() || !EntryGuid->IsValid())
    {
        return FText::FromName(NAME_None);
    }

    const FText* DisplayName = EntryNames.Find(*EntryGuid);
    if (!DisplayName)
    {
        return FText::FromString(EntryGuid->ToString());
    }

    return FText::Format(
        NSLOCTEXT("FGameZoneEntryIdCustomization", "EntryIdText", "{0} [{1}]"),
        *DisplayName,
        FText::FromString(EntryGuid->ToString()));
}
