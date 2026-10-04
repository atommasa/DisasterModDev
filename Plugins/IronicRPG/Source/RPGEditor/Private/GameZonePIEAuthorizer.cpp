// Copyright Ironic Studio. All Rights Reserved.

#include "GameZonePIEAuthorizer.h"

#include "GameZoneBindingCoordinator.h"
#include "Levels/GameZoneAsset.h"
#include "Levels/RPGWorldSettings.h"

#include "Editor.h"
#include "Features/IModularFeatures.h"
#include "Internationalization/Text.h"
#include "IO/IoHash.h"
#include "IPIEAuthorizer.h"
#include "Misc/App.h"
#include "Misc/MessageDialog.h"
#include "Templates/ValueOrError.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameZonePIE, Log, All);

namespace
{
    FString HashIssueIdentities(const TConstArrayView<FGameZonePIEIssue> Issues)
    {
        FString Canonical;
        for (const FGameZonePIEIssue& Issue : Issues)
        {
            Canonical.Appendf(TEXT("%d:"), Issue.Identity.Len());
            Canonical.Append(Issue.Identity);
            Canonical.AppendChar(TEXT('|'));
        }
        const FTCHARToUTF8 Utf8(*Canonical);
        return LexToString(FIoHash::HashBuffer(Utf8.Get(), Utf8.Length()));
    }

    FString JoinAuditIssues(const FGameZoneMapBakeAuditResult& Audit)
    {
        TArray<FString> Messages;
        Messages.Reserve(Audit.Issues.Num());
        for (const FText& Issue : Audit.Issues)
        {
            Messages.Add(Issue.ToString());
        }
        return FString::Join(Messages, TEXT(" "));
    }

    FText MakeDialogMessage(const FGameZonePIEGateResult& Result)
    {
        FString Message = TEXT("Game Zone data is not verified for PIE:\n\n");
        for (const FGameZonePIEIssue& Issue : Result.Issues)
        {
            Message.Append(TEXT("- "));
            Message.Append(Issue.Message.ToString());
            Message.AppendChar(TEXT('\n'));
        }
        Message.Append(TEXT("\nChoose Yes to Continue Once with the current in-memory data, or No to cancel PIE."));
        return FText::FromString(Message);
    }

    class FRegisteredGameZonePIEAuthorizer final : public IPIEAuthorizer
    {
    protected:
        virtual TValueOrError<bool, FText> IsPIEAuthorizedInternal(bool) const override
        {
            // Recoverable integrity issues require the interactive permission path.
            return MakeValue(true);
        }

        virtual TValueOrError<bool, FText> RequestPIEPermissionInternal(bool) const override
        {
            const FGameZonePIEGateResult Result = FGameZonePIEAuthorizer::AuditCurrentEditorState();
            if (Result.IsAllowed())
            {
                return MakeValue(true);
            }

            const FText Message = MakeDialogMessage(Result);
            if (FApp::IsUnattended())
            {
                return MakeError(Message);
            }

            const EAppReturnType::Type DialogResult = FMessageDialog::Open(
                EAppMsgType::YesNo,
                EAppReturnType::No,
                Message,
                FText::FromString(TEXT("Game Zone PIE Integrity")));
            const bool bContinue = FGameZonePIEAuthorizer::ShouldContinueOnce(Result, DialogResult);
            if (bContinue)
            {
                UE_LOG(
                    LogGameZonePIE,
                    Warning,
                    TEXT("Continuing PIE once despite Game Zone integrity issue %s."),
                    *Result.IssueFingerprint);
            }
            return MakeValue(bContinue);
        }
    };

    TUniquePtr<FRegisteredGameZonePIEAuthorizer> RegisteredAuthorizer;
}

FGameZonePIEGateResult FGameZonePIEAuthorizer::BuildGateResult(
    const TConstArrayView<FGameZonePIEIssue> CandidateIssues)
{
    TMap<FString, FGameZonePIEIssue> UniqueIssues;
    for (const FGameZonePIEIssue& Issue : CandidateIssues)
    {
        if (!Issue.Identity.IsEmpty())
        {
            UniqueIssues.FindOrAdd(Issue.Identity, Issue);
        }
    }

    FGameZonePIEGateResult Result;
    UniqueIssues.GenerateValueArray(Result.Issues);
    Result.Issues.Sort([](const FGameZonePIEIssue& First, const FGameZonePIEIssue& Second)
    {
        return First.Identity < Second.Identity;
    });
    if (!Result.Issues.IsEmpty())
    {
        Result.IssueFingerprint = HashIssueIdentities(Result.Issues);
    }
    return Result;
}

FGameZonePIEGateResult FGameZonePIEAuthorizer::AuditCurrentEditorState()
{
    TArray<FGameZonePIEIssue> Issues;
    const FGameZoneGlobalBindingAuditResult BindingAudit = FGameZoneBindingCoordinator::AuditProjectBindings();
    for (const FGameZoneGlobalBindingIssue& Issue : BindingAudit.Issues)
    {
        FGameZonePIEIssue& PIEIssue = Issues.AddDefaulted_GetRef();
        PIEIssue.Kind = EGameZonePIEIssueKind::Binding;
        PIEIssue.Identity = FString::Printf(
            TEXT("Binding|%d|%s|%s"),
            static_cast<uint8>(Issue.Issue),
            *Issue.AssetPath.ToString(),
            *Issue.LevelPackage.ToString());
        PIEIssue.Message = Issue.Message;
    }

    UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    const ARPGWorldSettings* Settings = EditorWorld
        ? Cast<ARPGWorldSettings>(EditorWorld->GetWorldSettings())
        : nullptr;
    const bool bHasLevelClaim = Settings
        && (Settings->GetGameZoneId().IsValid() || Settings->GetGameZoneBindingId().IsValid());
    if (!EditorWorld || !bHasLevelClaim)
    {
        return BuildGateResult(Issues);
    }

    FText BindingFailure;
    UGameZoneAsset* ZoneAsset = FGameZoneBindingCoordinator::FindExactBoundAsset(*EditorWorld, &BindingFailure);
    if (!ZoneAsset)
    {
        FGameZonePIEIssue& Issue = Issues.AddDefaulted_GetRef();
        Issue.Kind = EGameZonePIEIssueKind::Binding;
        Issue.Identity = TEXT("CurrentWorldBinding|") + EditorWorld->GetOutermost()->GetName();
        Issue.Message = BindingFailure;
        return BuildGateResult(Issues);
    }

    Issues.Append(BuildBakeIssues(ZoneAsset->GetPathName(), ZoneAsset->AuditMapDataForPIE()));

    return BuildGateResult(Issues);
}

TArray<FGameZonePIEIssue> FGameZonePIEAuthorizer::BuildBakeIssues(
    const FString& ZoneAssetPath,
    const FGameZoneMapBakeAuditResult& Audit)
{
    TArray<FGameZonePIEIssue> Issues;
    if (Audit.bHasUnsavedLevelChanges || Audit.bHasUnsavedAssetChanges)
    {
        FGameZonePIEIssue& Issue = Issues.AddDefaulted_GetRef();
        Issue.Kind = EGameZonePIEIssueKind::UnsavedAuthoring;
        Issue.Identity = TEXT("Unsaved|") + ZoneAssetPath + TEXT("|") + Audit.IssueFingerprint;
        Issue.Message = FText::FromString(JoinAuditIssues(Audit));
    }
    else if (!Audit.IsVerifiedForPIE())
    {
        FGameZonePIEIssue& Issue = Issues.AddDefaulted_GetRef();
        Issue.Kind = EGameZonePIEIssueKind::SavedBakeStale;
        Issue.Identity = TEXT("SavedStale|") + ZoneAssetPath + TEXT("|") + Audit.IssueFingerprint;
        Issue.Message = FText::FromString(JoinAuditIssues(Audit));
    }
    return Issues;
}

bool FGameZonePIEAuthorizer::ShouldContinueOnce(
    const FGameZonePIEGateResult& Result,
    const EAppReturnType::Type DialogResult)
{
    return Result.IsAllowed() || DialogResult == EAppReturnType::Yes;
}

void FGameZonePIEAuthorizer::Register()
{
    if (!RegisteredAuthorizer)
    {
        RegisteredAuthorizer = MakeUnique<FRegisteredGameZonePIEAuthorizer>();
        IModularFeatures::Get().RegisterModularFeature(IPIEAuthorizer::GetModularFeatureName(), RegisteredAuthorizer.Get());
    }
}

void FGameZonePIEAuthorizer::Unregister()
{
    if (RegisteredAuthorizer)
    {
        IModularFeatures::Get().UnregisterModularFeature(IPIEAuthorizer::GetModularFeatureName(), RegisteredAuthorizer.Get());
        RegisteredAuthorizer.Reset();
    }
}
