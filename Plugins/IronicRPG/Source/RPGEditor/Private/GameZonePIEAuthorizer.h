// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FGameZoneMapBakeAuditResult;

enum class EGameZonePIEIssueKind : uint8
{
    Binding,
    SavedBakeStale,
    UnsavedAuthoring,
};

struct FGameZonePIEIssue
{
    EGameZonePIEIssueKind Kind = EGameZonePIEIssueKind::Binding;
    FString Identity;
    FText Message;
};

struct FGameZonePIEGateResult
{
    bool IsAllowed() const { return Issues.IsEmpty(); }

    FString IssueFingerprint;
    TArray<FGameZonePIEIssue> Issues;
};

class FGameZonePIEAuthorizer
{
public:
    static void Register();
    static void Unregister();

    static FGameZonePIEGateResult AuditCurrentEditorState();
    static FGameZonePIEGateResult BuildGateResult(TConstArrayView<FGameZonePIEIssue> CandidateIssues);
    static TArray<FGameZonePIEIssue> BuildBakeIssues(
        const FString& ZoneAssetPath,
        const FGameZoneMapBakeAuditResult& Audit);
    static bool ShouldContinueOnce(const FGameZonePIEGateResult& Result, EAppReturnType::Type DialogResult);
};
