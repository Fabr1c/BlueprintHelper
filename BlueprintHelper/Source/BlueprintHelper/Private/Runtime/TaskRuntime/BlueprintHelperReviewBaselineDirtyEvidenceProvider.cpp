// BlueprintHelper Review baseline dirty evidence provider implementation.

#include "Runtime/TaskRuntime/BlueprintHelperReviewBaselineDirtyEvidenceProvider.h"

#include "Runtime/TaskRuntime/BlueprintHelperTaskRunJournalStoreService.h"
#include "Runtime/TaskRuntime/BlueprintHelperSequentialReviewSessionService.h"
#include "Systems/SourceControl/BlueprintHelperSourceControlService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"

FBlueprintHelperReviewBaselineDirtyClassifyRequest
FBlueprintHelperReviewBaselineDirtyEvidenceProvider::BuildClassifyRequest(
	const TArray<FString>& TargetAssets,
	const TArray<FString>& DirtyAssets) const
{
	const TMap<FString, TSharedPtr<FJsonObject>> EmptyTaskRunJournals;
	return BuildClassifyRequest(TargetAssets, DirtyAssets, EmptyTaskRunJournals);
}

FBlueprintHelperReviewBaselineDirtyClassifyRequest
FBlueprintHelperReviewBaselineDirtyEvidenceProvider::BuildClassifyRequest(
	const TArray<FString>& TargetAssets,
	const TArray<FString>& DirtyAssets,
	const TMap<FString, TSharedPtr<FJsonObject>>& TaskRunJournals) const
{
	FBlueprintHelperReviewBaselineDirtyClassifyRequest Request;
	Request.TargetAssets = TargetAssets;
	Request.DirtyAssets = DirtyAssets;
	Request.ActiveReviewEvidenceRefs =
		CollectActiveReviewEvidenceRefsForTargetAssets(DirtyAssets);
	Request.DiagnosticEvidenceRefs = Request.ActiveReviewEvidenceRefs;
	Request.bDirtyPreexistingBeforeRun = DirtyAssets.Num() > 0;
	AddPreRunDirtyEvidenceRefs(DirtyAssets, Request.DiagnosticEvidenceRefs);
	ApplyFailedTaskRunEvidence(DirtyAssets, TaskRunJournals, Request);
	if (!Request.bDirtyFromFailedExecute)
	{
		const TArray<TSharedPtr<FJsonObject>> PersistedTaskRunJournals =
			FBlueprintHelperTaskRunJournalStoreService().QueryTaskRunJournalsForTargetAssets(DirtyAssets, 64);
		ApplyFailedTaskRunEvidence(DirtyAssets, PersistedTaskRunJournals, Request);
	}
	ApplySequentialReviewSessionEvidence(TargetAssets, DirtyAssets, Request);
	ApplySourceControlEvidence(DirtyAssets, Request);
	return Request;
}

TArray<FString>
FBlueprintHelperReviewBaselineDirtyEvidenceProvider::CollectActiveReviewEvidenceRefsForTargetAssets(
	const TArray<FString>& TargetAssets) const
{
	TArray<FString> EvidenceRefs;
	if (TargetAssets.Num() == 0)
	{
		return EvidenceRefs;
	}

	const FBlueprintHelperReviewStoreService ReviewStore;
	for (const FString& TargetAsset : TargetAssets)
	{
		if (TargetAsset.IsEmpty())
		{
			continue;
		}

		FBlueprintHelperReviewPendingIndexQuery Query;
		Query.AssetPathFilter = TargetAsset;
		Query.bPendingOnly = true;
		const TArray<FBlueprintHelperReviewPendingVisibleChangeSummary> PendingChanges =
			ReviewStore.QueryPendingVisibleChangeSummaries(Query);
		for (const FBlueprintHelperReviewPendingVisibleChangeSummary& PendingChange : PendingChanges)
		{
			AddUniqueNonEmptyString(EvidenceRefs, PendingChange.Change.LatestEvidenceId);
			AddUniqueNonEmptyString(EvidenceRefs, PendingChange.Change.ChangeId);
			AddUniqueNonEmptyString(EvidenceRefs, PendingChange.ReviewRecordId);
		}
	}
	return EvidenceRefs;
}

void FBlueprintHelperReviewBaselineDirtyEvidenceProvider::ApplySourceControlEvidence(
	const TArray<FString>& TargetAssets,
	FBlueprintHelperReviewBaselineDirtyClassifyRequest& InOutRequest) const
{
	if (TargetAssets.Num() == 0)
	{
		return;
	}

	const FBlueprintHelperSourceControlResult SourceControlResult =
		FBlueprintHelperSourceControlService().QueryStatus(TargetAssets, false);
	TArray<FString> Parts;
	AddUniqueNonEmptyString(
		Parts,
		FString::Printf(TEXT("provider=%s"), *SourceControlResult.Provider));
	AddUniqueNonEmptyString(
		Parts,
		FString::Printf(TEXT("status=%s"), *SourceControlResult.Status));
	AddUniqueNonEmptyString(
		Parts,
		FString::Printf(TEXT("recommended_action=%s"), *SourceControlResult.RecommendedAction));

	for (const FBlueprintHelperSourceControlFileState& FileState : SourceControlResult.Files)
	{
		AddUniqueNonEmptyString(
			InOutRequest.DiagnosticEvidenceRefs,
			FString::Printf(
				TEXT("source_control:%s:%s"),
				*FileState.Input,
				*FileState.Status));
	}

	InOutRequest.SourceControlStatus = FString::Join(Parts, TEXT(";"));
	if (SourceControlResult.Status == TEXT("checked_out_by_other") ||
		SourceControlResult.Status == TEXT("source_control_conflicted"))
	{
		InOutRequest.bDirtyFromExternalUserChange = true;
		InOutRequest.bEditorSessionOwnershipKnown = true;
		InOutRequest.bEditorSessionAgentOwned = false;
		AddUniqueNonEmptyString(
			InOutRequest.DiagnosticEvidenceRefs,
			FString::Printf(TEXT("external_dirty:source_control:%s"), *SourceControlResult.Status));
	}
}

void FBlueprintHelperReviewBaselineDirtyEvidenceProvider::AddPreRunDirtyEvidenceRefs(
	const TArray<FString>& DirtyAssets,
	TArray<FString>& InOutDiagnosticEvidenceRefs) const
{
	for (const FString& DirtyAsset : DirtyAssets)
	{
		AddUniqueNonEmptyString(
			InOutDiagnosticEvidenceRefs,
			FString::Printf(TEXT("pre_run_dirty:%s"), *DirtyAsset));
	}
}

void FBlueprintHelperReviewBaselineDirtyEvidenceProvider::ApplyFailedTaskRunEvidence(
	const TArray<FString>& DirtyAssets,
	const TMap<FString, TSharedPtr<FJsonObject>>& TaskRunJournals,
	FBlueprintHelperReviewBaselineDirtyClassifyRequest& InOutRequest) const
{
	if (DirtyAssets.Num() == 0 || TaskRunJournals.Num() == 0)
	{
		return;
	}

	TArray<TSharedPtr<FJsonObject>> JournalValues;
	for (const TPair<FString, TSharedPtr<FJsonObject>>& JournalPair : TaskRunJournals)
	{
		JournalValues.Add(JournalPair.Value);
	}
	ApplyFailedTaskRunEvidence(DirtyAssets, JournalValues, InOutRequest);
}

void FBlueprintHelperReviewBaselineDirtyEvidenceProvider::ApplyFailedTaskRunEvidence(
	const TArray<FString>& DirtyAssets,
	const TArray<TSharedPtr<FJsonObject>>& TaskRunJournals,
	FBlueprintHelperReviewBaselineDirtyClassifyRequest& InOutRequest) const
{
	if (DirtyAssets.Num() == 0 || TaskRunJournals.Num() == 0)
	{
		return;
	}

	for (const TSharedPtr<FJsonObject>& Journal : TaskRunJournals)
	{
		if (!Journal.IsValid())
		{
			continue;
		}

		FString Status;
		if (!Journal->TryGetStringField(TEXT("status"), Status) ||
			!Status.Equals(TEXT("partial_failure"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		TArray<FString> JournalTargetAssets;
		ReadStringArrayField(Journal, TEXT("target_assets"), JournalTargetAssets);
		if (!HasAnySharedString(DirtyAssets, JournalTargetAssets))
		{
			continue;
		}

		FString FailedTaskRunId;
		Journal->TryGetStringField(TEXT("task_run_id"), FailedTaskRunId);
		if (FailedTaskRunId.IsEmpty())
		{
			continue;
		}

		InOutRequest.FailedTaskRunId = FailedTaskRunId;
		InOutRequest.bDirtyFromFailedExecute = true;
		AddUniqueNonEmptyString(
			InOutRequest.DiagnosticEvidenceRefs,
			FString::Printf(TEXT("task_run:%s:partial_failure"), *FailedTaskRunId));
		return;
	}
}

void FBlueprintHelperReviewBaselineDirtyEvidenceProvider::ApplySequentialReviewSessionEvidence(
	const TArray<FString>& TargetAssets,
	const TArray<FString>& DirtyAssets,
	FBlueprintHelperReviewBaselineDirtyClassifyRequest& InOutRequest) const
{
	if (DirtyAssets.Num() == 0)
	{
		return;
	}

	const FBlueprintHelperSequentialReviewSessionLookup Lookup =
		FBlueprintHelperSequentialReviewSessionService().FindOpenSessionForTargetAssets(TargetAssets);
	if (!Lookup.bFound)
	{
		return;
	}

	const FBlueprintHelperSequentialReviewSession& Session = Lookup.Session;
	InOutRequest.ActiveSequentialReviewSessionId = Session.SequentialReviewSessionId;
	InOutRequest.ActiveSequentialReviewArchiveSessionId = Session.SessionStartArchiveSessionId;
	InOutRequest.bDirtyFromActiveSequentialReviewSession = true;
	InOutRequest.bSequentialReviewSessionHasLastGoodSnapshot = Session.bHasLastGoodSnapshot;
	InOutRequest.bSequentialReviewSessionHasUnresolvedFailedExecute = Session.bHasUnresolvedFailedExecute;
	InOutRequest.bSequentialReviewSessionHasExternalConflict = Session.bHasExternalConflict;
	AddUniqueNonEmptyString(
		InOutRequest.DiagnosticEvidenceRefs,
		FString::Printf(
			TEXT("sequential_review_session:%s"),
			*Session.SequentialReviewSessionId));
	AddUniqueNonEmptyString(
		InOutRequest.DiagnosticEvidenceRefs,
		FString::Printf(
			TEXT("sequential_review_session_archive:%s"),
			*Session.SessionStartArchiveSessionId));
	for (const FString& ReviewRecordId : Session.ReviewRecordIds)
	{
		AddUniqueNonEmptyString(
			InOutRequest.DiagnosticEvidenceRefs,
			FString::Printf(TEXT("sequential_review_session_review:%s"), *ReviewRecordId));
	}
}

void FBlueprintHelperReviewBaselineDirtyEvidenceProvider::AddUniqueNonEmptyString(
	TArray<FString>& Values,
	const FString& Value)
{
	if (!Value.IsEmpty())
	{
		Values.AddUnique(Value);
	}
}

bool FBlueprintHelperReviewBaselineDirtyEvidenceProvider::ReadStringArrayField(
	const TSharedPtr<FJsonObject>& Json,
	const FString& FieldName,
	TArray<FString>& OutValues)
{
	OutValues.Reset();
	const TArray<TSharedPtr<FJsonValue>>* RawValues = nullptr;
	if (!Json.IsValid() ||
		!Json->TryGetArrayField(FieldName, RawValues) ||
		!RawValues)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& RawValue : *RawValues)
	{
		if (!RawValue.IsValid() || RawValue->Type != EJson::String)
		{
			continue;
		}
		AddUniqueNonEmptyString(OutValues, RawValue->AsString());
	}
	return OutValues.Num() > 0;
}

bool FBlueprintHelperReviewBaselineDirtyEvidenceProvider::HasAnySharedString(
	const TArray<FString>& Left,
	const TArray<FString>& Right)
{
	for (const FString& Value : Left)
	{
		if (Right.Contains(Value))
		{
			return true;
		}
	}
	return false;
}
