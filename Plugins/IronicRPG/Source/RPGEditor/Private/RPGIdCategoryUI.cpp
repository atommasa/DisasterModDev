// Copyright Ironic Studio. All Rights Reserved.
#include "RPGIdCategoryUI.h"
#include "RPGIdCategoryService.h"
#include "IDetailsView.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailGroup.h"
#include "PropertyHandle.h"
#include "Misc/MessageDialog.h"
#include "PropertyEditorModule.h"
#include "ToolMenus.h"
#include "Developer/Settings/Public/ISettingsModule.h"
#include "Developer/Settings/Public/ISettingsSection.h"
#include "Widgets/Layout/SBox.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "RPGIdCategoryUI"

namespace
{
	FDelegateHandle CategoryStartupHandle;

	class FCategorySettingsLayout : public IDetailCustomization
	{
	public:
		virtual void CustomizeDetails(IDetailLayoutBuilder& Builder) override
		{
			const auto Rules = Builder.GetProperty(GET_MEMBER_NAME_CHECKED(URPGIdCategorySettings, Rules));
			Builder.HideProperty(Rules);
			const auto Types = Rules->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRPGIdCategoryRules, AssetTypes));
			uint32 Count = 0;
			if (!Types || Types->GetNumChildren(Count) != FPropertyAccess::Success) { return; }
			auto& Category = Builder.EditCategory(TEXT("Rules"));
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				const auto Entry = Types->GetChildHandle(Index);
				FName Type;
				if (!Entry || !Entry->GetKeyHandle() || Entry->GetKeyHandle()->GetValue(Type) != FPropertyAccess::Success) { continue; }
				const auto Categories = Entry->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRPGIdCategoryTypeRules, Categories));
				if (Categories)
				{
					Category.AddGroup(Type, FText::FromName(Type), false, true).AddPropertyRow(Categories.ToSharedRef());
				}
			}
		}
	};

	class SRPGIdCategorySettingsEditor : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SRPGIdCategorySettingsEditor) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Draft.Reset(NewObject<URPGIdCategorySettings>(GetTransientPackage(), NAME_None, RF_Transient));
			FDetailsViewArgs DetailsArgs;
			DetailsArgs.bAllowSearch = true;
			DetailsArgs.bHideSelectionTip = true;
			DetailsArgs.bUpdatesFromSelection = false;
			Details = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateDetailView(DetailsArgs);
			Details->RegisterInstancedCustomPropertyLayout(URPGIdCategorySettings::StaticClass(),
				FOnGetDetailCustomizationInstance::CreateLambda([]() { return MakeShared<FCategorySettingsLayout>(); }));
			Details->SetObject(Draft.Get());
			ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(8)
				[
					SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("Help",
						"Asset types are registered by the RPG system. Under each type, add categories with "
						"a stable CategoryKey, a display name and inclusive ranges from 0 to 9999. "
						"Ranges must not overlap within a type. SubZone classification does not enable Claim authoring. "
						"Save commits project category settings. Cancel discards edits. Returning to this settings page reloads the saved rules."))
				]
				+ SVerticalBox::Slot().FillHeight(1)[Details.ToSharedRef()]
				+ SVerticalBox::Slot().AutoHeight().Padding(8)
				[
					SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(Message); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(8)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[SNew(SButton).Text(LOCTEXT("Save", "Save")).OnClicked(this, &SRPGIdCategorySettingsEditor::Save)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0)
					[SNew(SButton).Text(LOCTEXT("Reload", "Reload")).OnClicked(this, &SRPGIdCategorySettingsEditor::Reload)]
					+ SHorizontalBox::Slot().AutoWidth()
					[SNew(SButton).Text(LOCTEXT("Cancel", "Cancel")).OnClicked_Lambda([this]()
					{
						LoadFromDisk();
						return FReply::Handled();
					})]
				]
			];
			LoadFromDisk();
		}

	private:
		void LoadFromDisk()
		{
			FRPGIdCategoryStore& Store = FRPGIdCategoryStore::Get();
			const bool bValid = Store.Reload(Message);
			Draft->Rules = Store.GetRules();
			Revision = Store.GetRevision();
			Baseline = FRPGIdCategoryResolver::Export(Draft->Rules);
			Details->ForceRefresh();
			if (bValid) { Message = TEXT("Loaded: ") + Store.GetFilename() + TEXT(". Changes remain a draft until Save."); }
		}

		FReply Save()
		{
			FRPGIdCategoryStore& Store = FRPGIdCategoryStore::Get();
			if (Store.Save(Draft->Rules, Revision, Message))
			{
				Draft->Rules = Store.GetRules();
				Details->ForceRefresh();
				Revision = Store.GetRevision();
				Baseline = FRPGIdCategoryResolver::Export(Draft->Rules);
				Message = TEXT("Saved category rules. No asset, Level, Id or release data was changed.");
			}
			return FReply::Handled();
		}

		FReply Reload()
		{
			if (FRPGIdCategoryResolver::Export(Draft->Rules) != Baseline
				&& FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::No,
					LOCTEXT("Discard", "Discard unsaved category edits and reload from disk?")) != EAppReturnType::Yes)
			{
				return FReply::Handled();
			}
			LoadFromDisk();
			return FReply::Handled();
		}

	private:
		TStrongObjectPtr<URPGIdCategorySettings> Draft;
		TSharedPtr<IDetailsView> Details;
		FString Baseline;
		FString Message;
		uint64 Revision = 0;
	};

}

void FRPGIdCategoryUI::Register()
{
	if (IsRunningCommandlet()) { return; }
	CategoryStartupHandle = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		if (ISettingsModule* Settings = FModuleManager::LoadModulePtr<ISettingsModule>(TEXT("Settings")))
		{
			const TSharedRef<SBox> Host = SNew(SBox);
			const auto Section = Settings->RegisterSettings(TEXT("Project"), TEXT("Ironic"), TEXT("RPGIdCategoryEditorSettings"),
				LOCTEXT("Menu", "RPG Id Category Editor Settings"), LOCTEXT("Tooltip", "Project-wide RPG Id category ranges."),
				StaticCastSharedRef<SWidget>(Host));
			if (Section)
			{
				Section->OnSelect().BindLambda([Host]() { Host->SetContent(SNew(SRPGIdCategorySettingsEditor)); });
			}
		}
	}));
}

void FRPGIdCategoryUI::Unregister()
{
	if (IsRunningCommandlet()) { return; }
	if (ISettingsModule* Settings = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings")))
	{
		Settings->UnregisterSettings(TEXT("Project"), TEXT("Ironic"), TEXT("RPGIdCategoryEditorSettings"));
	}
	UToolMenus::UnRegisterStartupCallback(CategoryStartupHandle);
	UToolMenus::UnregisterOwner(TEXT("RPGIdCategories"));
}

#undef LOCTEXT_NAMESPACE
