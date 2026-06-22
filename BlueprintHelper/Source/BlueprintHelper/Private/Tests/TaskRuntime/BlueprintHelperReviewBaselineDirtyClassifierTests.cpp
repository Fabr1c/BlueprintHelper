#include "Runtime/TaskRuntime/BlueprintHelperReviewBaselineDirtyClassifier.h"
#include "Runtime/TaskRuntime/BlueprintHelperReviewBaselineDirtyDebugEvidenceProjection.h"
#include "Runtime/TaskRuntime/BlueprintHelperReviewBaselineDirtyEvidenceProvider.h"
#include "Runtime/TaskRuntime/BlueprintHelperSequentialReviewSessionService.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRunJournalStoreService.h"
#include "Systems/Review/BlueprintHelperReviewConfigResolver.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/Guid.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

class FBlueprintHelperReviewBaselineDirtyClassifierTestsLocal
{
public:
	static FBlueprintHelperReviewBaselineDirtyClassifyRequest MakeDirtyRequest()
	{
		FBlueprintHelperReviewBaselineDirtyClassifyRequest Request;
		Request.TargetAssets.Add(TEXT("/Game/Test/BP_Test"));
		Request.DirtyAssets.Add(TEXT("/Game/Test/BP_Test"));
		return Request;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBaselineDirtyClassifier_ActiveSequentialSessionAllowsContinuation,
	"BlueprintHelper.TaskRuntime.ReviewBaselineDirtyClassifier.ActiveSequentialSessionAllowsContinuation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewBaselineDirtyClassifier_ActiveSequentialSessionAllowsContinuation::RunTest(const FString&)
{
	FBlueprintHelperReviewBaselineDirtyClassifyRequest Request =
		FBlueprintHelperReviewBaselineDirtyClassifierTestsLocal::MakeDirtyRequest();
	Request.ActiveReviewEvidenceRefs.Add(TEXT("review://evidence/test"));
	Request.ActiveSequentialReviewSessionId = TEXT("seq_review_test");
	Request.ActiveSequentialReviewArchiveSessionId = TEXT("archive_test");
	Request.bDirtyFromActiveSequentialReviewSession = true;
	Request.bSequentialReviewSessionHasLastGoodSnapshot = true;

	const FBlueprintHelperReviewBaselineDirtyDecision Decision =
		FBlueprintHelperReviewBaselineDirtyClassifier().Classify(Request);

	TestEqual(TEXT("state"), ToString(Decision.State), TEXT("dirty_with_active_sequential_review_session"));
	TestEqual(TEXT("category"), Decision.Category, FString(TEXT("runtime_state")));
	TestEqual(
		TEXT("safe next action"),
		Decision.SafeNextAction,
		FString(TEXT("continue_active_sequential_review_session")));
	TestEqual(TEXT("session id"), Decision.SequentialReviewSessionId, FString(TEXT("seq_review_test")));
	TestEqual(
		TEXT("archive id"),
		Decision.SequentialReviewSessionArchiveSessionId,
		FString(TEXT("archive_test")));
	TestTrue(TEXT("task execute allowed"), Decision.AllowedRecoveryActions.Contains(TEXT("task.execute")));
	TestFalse(TEXT("active session does not block execution"), Decision.bBlocksExecution);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBaselineDirtyClassifier_UnresolvedSequentialFailureBlocks,
	"BlueprintHelper.TaskRuntime.ReviewBaselineDirtyClassifier.UnresolvedSequentialFailureBlocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewBaselineDirtyClassifier_UnresolvedSequentialFailureBlocks::RunTest(const FString&)
{
	FBlueprintHelperReviewBaselineDirtyClassifyRequest Request =
		FBlueprintHelperReviewBaselineDirtyClassifierTestsLocal::MakeDirtyRequest();
	Request.ActiveSequentialReviewSessionId = TEXT("seq_review_test");
	Request.ActiveSequentialReviewArchiveSessionId = TEXT("archive_test");
	Request.bDirtyFromActiveSequentialReviewSession = true;
	Request.bSequentialReviewSessionHasLastGoodSnapshot = true;
	Request.bSequentialReviewSessionHasUnresolvedFailedExecute = true;

	const FBlueprintHelperReviewBaselineDirtyDecision Decision =
		FBlueprintHelperReviewBaselineDirtyClassifier().Classify(Request);

	TestEqual(TEXT("state"), ToString(Decision.State), TEXT("dirty_with_unresolved_failed_session_execute"));
	TestEqual(
		TEXT("safe next action"),
		Decision.SafeNextAction,
		FString(TEXT("review.reject_or_restore_last_good_snapshot_then_retry")));
	TestTrue(TEXT("review reject allowed"), Decision.AllowedRecoveryActions.Contains(TEXT("review.reject")));
	TestFalse(TEXT("task execute not allowed"), Decision.AllowedRecoveryActions.Contains(TEXT("task.execute")));
	TestTrue(TEXT("unresolved failed session blocks execution"), Decision.bBlocksExecution);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBaselineDirtyClassifier_SequentialSessionServicePersistsAndCloses,
	"BlueprintHelper.TaskRuntime.ReviewBaselineDirtyClassifier.SequentialSessionServicePersistsAndCloses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewBaselineDirtyClassifier_SequentialSessionServicePersistsAndCloses::RunTest(const FString&)
{
	const FString SessionId = FString::Printf(
		TEXT("seq_review_test_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString TargetAsset = FString::Printf(
		TEXT("/Game/Test/BP_Seq_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString ReviewRecordId = FString::Printf(
		TEXT("review_archive_test_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));

	FBlueprintHelperSequentialReviewSessionExecuteUpdate Update;
	Update.SequentialReviewSessionId = SessionId;
	Update.TaskRunId = TEXT("task_test");
	Update.ArchiveSessionId = TEXT("archive_test");
	Update.TargetAssets.Add(TargetAsset);
	Update.ReviewRecordIds.Add(ReviewRecordId);
	Update.bSucceeded = true;

	FBlueprintHelperSequentialReviewSession PersistedSession;
	FString Error;
	TestTrue(
		TEXT("record execute update"),
		FBlueprintHelperSequentialReviewSessionService().RecordExecuteUpdate(
			Update,
			PersistedSession,
			Error));
	TestTrue(TEXT("last good snapshot"), PersistedSession.bHasLastGoodSnapshot);

	TArray<FString> LookupTargets;
	LookupTargets.Add(TargetAsset);
	const FBlueprintHelperSequentialReviewSessionLookup Lookup =
		FBlueprintHelperSequentialReviewSessionService().FindOpenSessionForTargetAssets(LookupTargets);
	TestTrue(TEXT("lookup found active session"), Lookup.bFound);
	TestEqual(TEXT("lookup id"), Lookup.Session.SequentialReviewSessionId, SessionId);

	const FBlueprintHelperSequentialReviewSessionCloseResult CloseResult =
		FBlueprintHelperSequentialReviewSessionService().CloseSessionsForReviewRecord(
			ReviewRecordId,
			EBlueprintHelperSequentialReviewSessionStatus::Accepted);
	TestTrue(TEXT("close session"), CloseResult.bSucceeded);
	TestTrue(TEXT("close matched session"), CloseResult.bMatched);
	TestTrue(TEXT("close affected session id"), CloseResult.AffectedSessionIds.Contains(SessionId));
	const FBlueprintHelperSequentialReviewSessionLookup ClosedLookup =
		FBlueprintHelperSequentialReviewSessionService().FindOpenSessionForTargetAssets(LookupTargets);
	TestFalse(TEXT("closed session no longer active"), ClosedLookup.bFound);

	const FString SessionPath =
		FBlueprintHelperReviewConfigResolver::Load().GetReviewRootDir() /
		TEXT("SequentialSessions") /
		FString::Printf(TEXT("%s.json"), *SessionId);
	if (IFileManager::Get().FileExists(*SessionPath))
	{
		TestTrue(TEXT("cleanup session artifact"), IFileManager::Get().Delete(*SessionPath, false, true));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBaselineDirtyClassifier_SequentialSessionRequiresExactTargetSet,
	"BlueprintHelper.TaskRuntime.ReviewBaselineDirtyClassifier.SequentialSessionRequiresExactTargetSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewBaselineDirtyClassifier_SequentialSessionRequiresExactTargetSet::RunTest(const FString&)
{
	const FString SessionId = FString::Printf(
		TEXT("seq_review_test_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString TargetAsset = FString::Printf(
		TEXT("/Game/Test/BP_SeqExact_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString ExtraTargetAsset = TargetAsset + TEXT("_Extra");

	FBlueprintHelperSequentialReviewSessionExecuteUpdate Update;
	Update.SequentialReviewSessionId = SessionId;
	Update.TaskRunId = TEXT("task_test");
	Update.ArchiveSessionId = TEXT("archive_test");
	Update.TargetAssets.Add(TargetAsset);
	Update.bSucceeded = true;

	FBlueprintHelperSequentialReviewSession PersistedSession;
	FString Error;
	TestTrue(
		TEXT("record exact-set session"),
		FBlueprintHelperSequentialReviewSessionService().RecordExecuteUpdate(
			Update,
			PersistedSession,
			Error));

	TArray<FString> ExactTargets;
	ExactTargets.Add(TargetAsset);
	TestTrue(
		TEXT("exact target lookup succeeds"),
		FBlueprintHelperSequentialReviewSessionService().FindOpenSessionForTargetAssets(ExactTargets).bFound);

	TArray<FString> ExpandedTargets;
	ExpandedTargets.Add(TargetAsset);
	ExpandedTargets.Add(ExtraTargetAsset);
	TestFalse(
		TEXT("expanded target lookup does not inherit partial baseline"),
		FBlueprintHelperSequentialReviewSessionService().FindOpenSessionForTargetAssets(ExpandedTargets).bFound);

	const FString SessionPath =
		FBlueprintHelperReviewConfigResolver::Load().GetReviewRootDir() /
		TEXT("SequentialSessions") /
		FString::Printf(TEXT("%s.json"), *SessionId);
	if (IFileManager::Get().FileExists(*SessionPath))
	{
		TestTrue(TEXT("cleanup exact-set session artifact"), IFileManager::Get().Delete(*SessionPath, false, true));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBaselineDirtyClassifier_CleanAssetsReturnClean,
	"BlueprintHelper.TaskRuntime.ReviewBaselineDirtyClassifier.CleanAssetsReturnClean",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewBaselineDirtyClassifier_CleanAssetsReturnClean::RunTest(const FString&)
{
	FBlueprintHelperReviewBaselineDirtyClassifyRequest Request;
	Request.TargetAssets.Add(TEXT("/Game/Test/BP_Test"));

	const FBlueprintHelperReviewBaselineDirtyDecision Decision =
		FBlueprintHelperReviewBaselineDirtyClassifier().Classify(Request);

	TestEqual(TEXT("state"), ToString(Decision.State), TEXT("clean"));
	TestEqual(TEXT("safe next action"), Decision.SafeNextAction, FString(TEXT("continue")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBaselineDirtyClassifier_OpenReviewPrefersReviewReject,
	"BlueprintHelper.TaskRuntime.ReviewBaselineDirtyClassifier.OpenReviewPrefersReviewReject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewBaselineDirtyClassifier_OpenReviewPrefersReviewReject::RunTest(const FString&)
{
	FBlueprintHelperReviewBaselineDirtyClassifyRequest Request =
		FBlueprintHelperReviewBaselineDirtyClassifierTestsLocal::MakeDirtyRequest();
	Request.ActiveReviewEvidenceRefs.Add(TEXT("review://evidence/test"));
	Request.DiagnosticEvidenceRefs.Add(TEXT("source_control:/Game/Test/BP_Test:modified"));

	const FBlueprintHelperReviewBaselineDirtyDecision Decision =
		FBlueprintHelperReviewBaselineDirtyClassifier().Classify(Request);

	TestEqual(TEXT("state"), ToString(Decision.State), TEXT("dirty_with_open_review"));
	TestTrue(TEXT("review.reject allowed"), Decision.AllowedRecoveryActions.Contains(TEXT("review.reject")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBaselineDirtyClassifier_DiagnosticEvidenceDoesNotCreateOpenReview,
	"BlueprintHelper.TaskRuntime.ReviewBaselineDirtyClassifier.DiagnosticEvidenceDoesNotCreateOpenReview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewBaselineDirtyClassifier_DiagnosticEvidenceDoesNotCreateOpenReview::RunTest(const FString&)
{
	FBlueprintHelperReviewBaselineDirtyClassifyRequest Request =
		FBlueprintHelperReviewBaselineDirtyClassifierTestsLocal::MakeDirtyRequest();
	Request.DiagnosticEvidenceRefs.Add(TEXT("source_control:/Game/Test/BP_Test:modified"));

	const FBlueprintHelperReviewBaselineDirtyDecision Decision =
		FBlueprintHelperReviewBaselineDirtyClassifier().Classify(Request);

	TestEqual(TEXT("state"), ToString(Decision.State), TEXT("unknown_dirty_origin"));
	TestTrue(
		TEXT("diagnostic evidence is retained"),
		Decision.EvidenceRefs.Contains(TEXT("source_control:/Game/Test/BP_Test:modified")));
	TestFalse(
		TEXT("diagnostic evidence must not allow review reject"),
		Decision.AllowedRecoveryActions.Contains(TEXT("review.reject")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBaselineDirtyClassifier_DebugEvidenceProjectionKeepsDirtyRoles,
	"BlueprintHelper.TaskRuntime.ReviewBaselineDirtyClassifier.DebugEvidenceProjectionKeepsDirtyRoles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewBaselineDirtyClassifier_DebugEvidenceProjectionKeepsDirtyRoles::RunTest(const FString&)
{
	TArray<FString> EvidenceRefs;
	EvidenceRefs.Add(TEXT("source_control:/Game/Test/BP_Test:checked_out_by_other"));
	EvidenceRefs.Add(TEXT("task_run:task_failed:partial_failure"));
	EvidenceRefs.Add(TEXT("pre_run_dirty:/Game/Test/BP_Test"));

	const TArray<FBlueprintHelperDebugEvidenceLink> Links =
		FBlueprintHelperReviewBaselineDirtyDebugEvidenceProjection::MakeEvidenceLinksFromRefs(EvidenceRefs);

	TestEqual(TEXT("link count"), Links.Num(), 3);
	if (Links.Num() == 3)
	{
		TestEqual(TEXT("source-control role"), Links[0].Role, FString(TEXT("source_control")));
		TestEqual(TEXT("failed task role"), Links[1].Role, FString(TEXT("failed_task_run")));
		TestEqual(TEXT("pre-run dirty role"), Links[2].Role, FString(TEXT("pre_run_dirty")));
		TestEqual(TEXT("projection source"), Links[0].Source, FString(TEXT("review_baseline_dirty_classifier")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBaselineDirtyClassifier_ProviderMarksPreRunDirty,
	"BlueprintHelper.TaskRuntime.ReviewBaselineDirtyClassifier.ProviderMarksPreRunDirty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewBaselineDirtyClassifier_ProviderMarksPreRunDirty::RunTest(const FString&)
{
	TArray<FString> TargetAssets;
	TargetAssets.Add(TEXT("/Game/Test/BP_Test"));
	TArray<FString> DirtyAssets;
	DirtyAssets.Add(TEXT("/Game/Test/BP_Test"));

	const FBlueprintHelperReviewBaselineDirtyClassifyRequest Request =
		FBlueprintHelperReviewBaselineDirtyEvidenceProvider().BuildClassifyRequest(TargetAssets, DirtyAssets);

	TestTrue(TEXT("pre-run dirty flag"), Request.bDirtyPreexistingBeforeRun);
	TestTrue(
		TEXT("pre-run diagnostic evidence"),
		Request.DiagnosticEvidenceRefs.Contains(TEXT("pre_run_dirty:/Game/Test/BP_Test")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBaselineDirtyClassifier_ProviderMarksFailedTaskRunDirty,
	"BlueprintHelper.TaskRuntime.ReviewBaselineDirtyClassifier.ProviderMarksFailedTaskRunDirty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewBaselineDirtyClassifier_ProviderMarksFailedTaskRunDirty::RunTest(const FString&)
{
	TArray<FString> TargetAssets;
	TargetAssets.Add(TEXT("/Game/Test/BP_Test"));
	TArray<FString> DirtyAssets;
	DirtyAssets.Add(TEXT("/Game/Test/BP_Test"));

	TSharedRef<FJsonObject> FailedJournal = MakeShared<FJsonObject>();
	FailedJournal->SetStringField(TEXT("task_run_id"), TEXT("task_failed"));
	FailedJournal->SetStringField(TEXT("status"), TEXT("partial_failure"));
	TArray<TSharedPtr<FJsonValue>> JournalTargets;
	JournalTargets.Add(MakeShared<FJsonValueString>(TEXT("/Game/Test/BP_Test")));
	FailedJournal->SetArrayField(TEXT("target_assets"), JournalTargets);

	TMap<FString, TSharedPtr<FJsonObject>> TaskRunJournals;
	TaskRunJournals.Add(TEXT("task_failed"), FailedJournal);

	const FBlueprintHelperReviewBaselineDirtyClassifyRequest Request =
		FBlueprintHelperReviewBaselineDirtyEvidenceProvider().BuildClassifyRequest(
			TargetAssets,
			DirtyAssets,
			TaskRunJournals);
	const FBlueprintHelperReviewBaselineDirtyDecision Decision =
		FBlueprintHelperReviewBaselineDirtyClassifier().Classify(Request);

	TestTrue(TEXT("failed execute flag"), Request.bDirtyFromFailedExecute);
	TestEqual(TEXT("failed task run id"), Request.FailedTaskRunId, FString(TEXT("task_failed")));
	TestEqual(TEXT("state"), ToString(Decision.State), TEXT("dirty_after_failed_execute"));
	TestTrue(
		TEXT("failed task diagnostic evidence"),
		Decision.EvidenceRefs.Contains(TEXT("task_run:task_failed:partial_failure")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBaselineDirtyClassifier_FailedExecuteSuggestsAgentOwnedRecovery,
	"BlueprintHelper.TaskRuntime.ReviewBaselineDirtyClassifier.FailedExecuteSuggestsAgentOwnedRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewBaselineDirtyClassifier_FailedExecuteSuggestsAgentOwnedRecovery::RunTest(const FString&)
{
	FBlueprintHelperReviewBaselineDirtyClassifyRequest Request =
		FBlueprintHelperReviewBaselineDirtyClassifierTestsLocal::MakeDirtyRequest();
	Request.bDirtyFromFailedExecute = true;
	Request.bEditorSessionOwnershipKnown = true;
	Request.bEditorSessionAgentOwned = true;

	const FBlueprintHelperReviewBaselineDirtyDecision Decision =
		FBlueprintHelperReviewBaselineDirtyClassifier().Classify(Request);

	TestEqual(TEXT("state"), ToString(Decision.State), TEXT("dirty_after_failed_execute"));
	TestTrue(
		TEXT("close without save allowed for agent-owned session"),
		Decision.AllowedRecoveryActions.Contains(TEXT("editor.close_without_save_when_agent_owned")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBaselineDirtyClassifier_ProviderMarksPersistedFailedTaskRunDirty,
	"BlueprintHelper.TaskRuntime.ReviewBaselineDirtyClassifier.ProviderMarksPersistedFailedTaskRunDirty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewBaselineDirtyClassifier_ProviderMarksPersistedFailedTaskRunDirty::RunTest(const FString&)
{
	const FString TaskRunId = FString::Printf(
		TEXT("task_persisted_failed_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString TargetAsset = FString::Printf(
		TEXT("/Game/Test/BP_PersistedFailed_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));

	TSharedRef<FJsonObject> FailedJournal = MakeShared<FJsonObject>();
	FailedJournal->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskRunJournal.v1"));
	FailedJournal->SetStringField(TEXT("task_run_id"), TaskRunId);
	FailedJournal->SetStringField(TEXT("status"), TEXT("partial_failure"));
	TArray<TSharedPtr<FJsonValue>> JournalTargets;
	JournalTargets.Add(MakeShared<FJsonValueString>(TargetAsset));
	FailedJournal->SetArrayField(TEXT("target_assets"), JournalTargets);

	const FBlueprintHelperTaskRunJournalStoreService JournalStore;
	FString StoreError;
	TestTrue(
		TEXT("save persisted failed journal"),
		JournalStore.SaveTaskRunJournal(TaskRunId, FailedJournal, StoreError));

	TArray<FString> TargetAssets;
	TargetAssets.Add(TargetAsset);
	TArray<FString> DirtyAssets;
	DirtyAssets.Add(TargetAsset);
	const TMap<FString, TSharedPtr<FJsonObject>> EmptyTaskRunJournals;
	const FBlueprintHelperReviewBaselineDirtyClassifyRequest Request =
		FBlueprintHelperReviewBaselineDirtyEvidenceProvider().BuildClassifyRequest(
			TargetAssets,
			DirtyAssets,
			EmptyTaskRunJournals);
	const FBlueprintHelperReviewBaselineDirtyDecision Decision =
		FBlueprintHelperReviewBaselineDirtyClassifier().Classify(Request);

	TestTrue(TEXT("persisted failed execute flag"), Request.bDirtyFromFailedExecute);
	TestEqual(TEXT("persisted failed task run id"), Request.FailedTaskRunId, TaskRunId);
	TestEqual(TEXT("persisted state"), ToString(Decision.State), TEXT("dirty_after_failed_execute"));
	TestTrue(
		TEXT("persisted failed task diagnostic evidence"),
		Decision.EvidenceRefs.Contains(FString::Printf(TEXT("task_run:%s:partial_failure"), *TaskRunId)));

	FString DeleteError;
	TestTrue(
		TEXT("cleanup persisted failed journal"),
		JournalStore.DeleteTaskRunJournal(TaskRunId, DeleteError));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBaselineDirtyClassifier_PreexistingDirtyRequiresUserDecision,
	"BlueprintHelper.TaskRuntime.ReviewBaselineDirtyClassifier.PreexistingDirtyRequiresUserDecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewBaselineDirtyClassifier_PreexistingDirtyRequiresUserDecision::RunTest(const FString&)
{
	FBlueprintHelperReviewBaselineDirtyClassifyRequest Request =
		FBlueprintHelperReviewBaselineDirtyClassifierTestsLocal::MakeDirtyRequest();
	Request.bDirtyPreexistingBeforeRun = true;

	const FBlueprintHelperReviewBaselineDirtyDecision Decision =
		FBlueprintHelperReviewBaselineDirtyClassifier().Classify(Request);

	TestEqual(TEXT("state"), ToString(Decision.State), TEXT("dirty_preexisting"));
	TestTrue(
		TEXT("user save allowed"),
		Decision.AllowedRecoveryActions.Contains(TEXT("user_save_then_retry")));
	TestFalse(
		TEXT("preexisting dirty must not discard"),
		Decision.AllowedRecoveryActions.Contains(TEXT("editor.close_without_save_when_agent_owned")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBaselineDirtyClassifier_ExternalUserDirtyDoesNotDiscard,
	"BlueprintHelper.TaskRuntime.ReviewBaselineDirtyClassifier.ExternalUserDirtyDoesNotDiscard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewBaselineDirtyClassifier_ExternalUserDirtyDoesNotDiscard::RunTest(const FString&)
{
	FBlueprintHelperReviewBaselineDirtyClassifyRequest Request =
		FBlueprintHelperReviewBaselineDirtyClassifierTestsLocal::MakeDirtyRequest();
	Request.bDirtyPreexistingBeforeRun = true;
	Request.bDirtyFromExternalUserChange = true;

	const FBlueprintHelperReviewBaselineDirtyDecision Decision =
		FBlueprintHelperReviewBaselineDirtyClassifier().Classify(Request);

	TestEqual(TEXT("state"), ToString(Decision.State), TEXT("dirty_external_user_change"));
	TestFalse(
		TEXT("external user dirty must not discard"),
		Decision.AllowedRecoveryActions.Contains(TEXT("editor.close_without_save_when_agent_owned")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBaselineDirtyClassifier_UnknownDirtyIsConservative,
	"BlueprintHelper.TaskRuntime.ReviewBaselineDirtyClassifier.UnknownDirtyIsConservative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewBaselineDirtyClassifier_UnknownDirtyIsConservative::RunTest(const FString&)
{
	const FBlueprintHelperReviewBaselineDirtyClassifyRequest Request =
		FBlueprintHelperReviewBaselineDirtyClassifierTestsLocal::MakeDirtyRequest();

	const FBlueprintHelperReviewBaselineDirtyDecision Decision =
		FBlueprintHelperReviewBaselineDirtyClassifier().Classify(Request);

	TestEqual(TEXT("state"), ToString(Decision.State), TEXT("unknown_dirty_origin"));
	TestFalse(
		TEXT("unknown dirty must not discard"),
		Decision.AllowedRecoveryActions.Contains(TEXT("editor.close_without_save_when_agent_owned")));
	return true;
}

#endif
