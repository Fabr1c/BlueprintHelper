#include "Runtime/TaskRuntime/BlueprintHelperReviewBaselineDirtyClassifier.h"
#include "Runtime/TaskRuntime/BlueprintHelperReviewBaselineDirtyDebugEvidenceProjection.h"
#include "Runtime/TaskRuntime/BlueprintHelperReviewBaselineDirtyEvidenceProvider.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRunJournalStoreService.h"

#include "Dom/JsonObject.h"
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
