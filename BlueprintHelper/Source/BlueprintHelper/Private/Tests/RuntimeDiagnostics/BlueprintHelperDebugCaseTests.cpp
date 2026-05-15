#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/Debug/BlueprintHelperDebugTypes.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/Debug/BlueprintHelperCompileAssetService.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"

class FBlueprintHelperDebugCaseTestsLocalUtils
{
public:
static FString MakeDebugTestId(const FString& Prefix)
{
	return Prefix + TEXT("_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
}

static FString SerializeJsonObject(const TSharedRef<FJsonObject>& Json)
{
	FString Serialized;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	FJsonSerializer::Serialize(Json, Writer);
	return Serialized;
}

static FString ExpectedDebugCasePath(const FString& DebugCaseId)
{
	return FPaths::ProjectSavedDir()
		/ TEXT("BlueprintHelper")
		/ TEXT("Debug")
		/ TEXT("Cases")
		/ FString::Printf(TEXT("%s.json"), *DebugCaseId);
}

static FString ExpectedDebugRootDir()
{
	return FPaths::ProjectSavedDir() / TEXT("BlueprintHelper") / TEXT("Debug");
}

static FString ExpectedReviewRecordPath(const FString& ReviewRecordId)
{
	return FPaths::ProjectSavedDir()
		/ TEXT("BlueprintHelper")
		/ TEXT("Review")
		/ TEXT("Records")
		/ FString::Printf(TEXT("%s.json"), *ReviewRecordId);
}

static FString ExpectedArchiveSessionPath(const FString& ArchiveSessionId)
{
	return FPaths::ProjectSavedDir()
		/ TEXT("BlueprintHelper")
		/ TEXT("Review")
		/ TEXT("ArchiveSessions")
		/ FString::Printf(TEXT("%s.json"), *ArchiveSessionId);
}

static void CleanupDebugCaseFile(const FString& DebugCaseId)
{
	IFileManager::Get().Delete(*ExpectedDebugCasePath(DebugCaseId), false, true);
}

static void CleanupReviewRecordFile(const FString& ReviewRecordId)
{
	IFileManager::Get().Delete(*ExpectedReviewRecordPath(ReviewRecordId), false, true);
}

static void CleanupArchiveSessionFile(const FString& ArchiveSessionId)
{
	IFileManager::Get().Delete(*ExpectedArchiveSessionPath(ArchiveSessionId), false, true);
}

static FString ExpectedReviewSnapshotDirectory(const FString& ArchiveSessionId)
{
	return FPaths::ProjectSavedDir()
		/ TEXT("BlueprintHelper")
		/ TEXT("Review")
		/ TEXT("Snapshots")
		/ ArchiveSessionId;
}

static void CleanupReviewSnapshotDirectory(const FString& ArchiveSessionId)
{
	IFileManager::Get().DeleteDirectory(
		*ExpectedReviewSnapshotDirectory(ArchiveSessionId),
		false,
		true);
}

static void CleanupDebugBundleDirectory(const FString& BundleId)
{
	IFileManager::Get().DeleteDirectory(
		*(FBlueprintHelperDebugCaseStoreService::GetBundleDirectory(BundleId)),
		false,
		true);
}

};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDebugCaseIdsFailureOnlyTest,
	"BlueprintHelper.RuntimeDiagnostics.Debug.ToolResultDebugCaseIdsAreFailureOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDebugCaseIdsFailureOnlyTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperToolError Error;
	Error.Code = TEXT("execute_failed");
	Error.Message = TEXT("Execute failed.");
	FBlueprintHelperToolResultBase Failure = FBlueprintHelperToolResultBuilder::Failure(
		TEXT("execute_task_plan"),
		TEXT("trace_failure"),
		Error);
	Failure.DebugCaseIds.Add(TEXT("dbg_failure"));
	TSharedRef<FJsonObject> FailureJson = Failure.ToJson();
	const TArray<TSharedPtr<FJsonValue>>* DebugCaseIds = nullptr;
	TestTrue(TEXT("failure result exposes debug_case_ids"), FailureJson->TryGetArrayField(TEXT("debug_case_ids"), DebugCaseIds));
	TestTrue(TEXT("failure result exposes one debug case id"), DebugCaseIds && DebugCaseIds->Num() == 1);

	FBlueprintHelperToolResultBase Success = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("execute_task_plan"),
		TEXT("trace_success"));
	Success.DebugCaseIds.Add(TEXT("dbg_should_not_escape"));
	TestFalse(TEXT("success result hides debug_case_ids"),
		Success.ToJson()->HasField(TEXT("debug_case_ids")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDebugDtoSchemaTest,
	"BlueprintHelper.RuntimeDiagnostics.Debug.DTOsUseV1Schemas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDebugDtoSchemaTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperDebugEvent Event;
	Event.DebugEventId = TEXT("ev_test");
	Event.DebugCaseId = TEXT("dbg_test");
	Event.CreatedAt = TEXT("2026-05-08T00:00:00Z");
	Event.SourceLayer = TEXT("task_runtime");
	Event.Operation = TEXT("execute_task");
	Event.Stage = TEXT("preview");
	Event.Severity = EBlueprintHelperDebugSeverity::Error;
	Event.Status = EBlueprintHelperDebugEventStatus::Captured;
	Event.TraceId = TEXT("trace_debug");
	Event.AssetPaths.Add(TEXT("/Game/BP_Door"));
	Event.Error.Code = TEXT("preview_blocked");
	Event.Error.Message = TEXT("Preview blocked.");
	Event.Redaction.Profile = TEXT("standard");
	Event.Redaction.bRequiresRedaction = true;

	TSharedRef<FJsonObject> EventJson = Event.ToJson();
	TestEqual(TEXT("event schema is stable"), EventJson->GetStringField(TEXT("schema")),
		FString(TEXT("BlueprintHelper.DebugEvent.v1")));
	TestEqual(TEXT("event severity serializes as snake case"), EventJson->GetStringField(TEXT("severity")),
		FString(TEXT("error")));
	TestTrue(TEXT("event error is present"), EventJson->HasTypedField<EJson::Object>(TEXT("error")));

	FBlueprintHelperDebugCase DebugCase;
	DebugCase.DebugCaseId = TEXT("dbg_test");
	DebugCase.CreatedAt = Event.CreatedAt;
	DebugCase.UpdatedAt = Event.CreatedAt;
	DebugCase.Source = TEXT("task_preview_blocker");
	DebugCase.Severity = EBlueprintHelperDebugSeverity::Error;
	DebugCase.Status = EBlueprintHelperDebugCaseStatus::Open;
	DebugCase.Operation = Event.Operation;
	DebugCase.Stage = Event.Stage;
	DebugCase.TraceIds.Add(Event.TraceId);
	DebugCase.AssetPaths.Add(TEXT("/Game/BP_Door"));
	DebugCase.Error = Event.Error;

	TSharedRef<FJsonObject> CaseJson = DebugCase.ToJson();
	TestEqual(TEXT("case schema is stable"), CaseJson->GetStringField(TEXT("schema")),
		FString(TEXT("BlueprintHelper.DebugCase.v1")));
	TestEqual(TEXT("case status serializes as open"), CaseJson->GetStringField(TEXT("status")),
		FString(TEXT("open")));

	FBlueprintHelperDebugBundleManifest Manifest;
	Manifest.BundleId = TEXT("bundle_test");
	Manifest.DebugCaseId = DebugCase.DebugCaseId;
	Manifest.CreatedAt = Event.CreatedAt;
	Manifest.Contents.Add(TEXT("manifest.json"));
	Manifest.Contents.Add(FPaths::ProjectSavedDir() / TEXT("BlueprintHelper") / TEXT("Debug") / TEXT("raw.json"));
	Manifest.Contents.Add(TEXT("../escape.json"));
	Manifest.Contents.Add(TEXT("artifacts/debug_export_refs.json"));

	TSharedRef<FJsonObject> ManifestJson = Manifest.ToJson();
	TestEqual(TEXT("manifest schema is stable"), ManifestJson->GetStringField(TEXT("schema")),
		FString(TEXT("BlueprintHelper.DebugBundleManifest.v1")));
	const TArray<TSharedPtr<FJsonValue>>* Contents = nullptr;
	TestTrue(TEXT("manifest exposes contents"), ManifestJson->TryGetArrayField(TEXT("contents"), Contents));
	TestTrue(TEXT("manifest contents are relative only"), Contents && Contents->Num() == 1);
	TestTrue(TEXT("manifest privacy is present"), ManifestJson->HasTypedField<EJson::Object>(TEXT("privacy")));
	const TSharedPtr<FJsonObject>* Privacy = nullptr;
	TestTrue(TEXT("manifest privacy says legacy debug export refs are absent"),
		ManifestJson->TryGetObjectField(TEXT("privacy"), Privacy)
		&& Privacy
		&& Privacy->IsValid()
		&& (*Privacy)->GetBoolField(TEXT("contains_legacy_debug_export_refs")) == false);
	const TSharedPtr<FJsonObject>* ArtifactSummary = nullptr;
	TestTrue(TEXT("manifest exposes artifact summary"),
		ManifestJson->TryGetObjectField(TEXT("artifact_summary"), ArtifactSummary)
		&& ArtifactSummary
		&& ArtifactSummary->IsValid());
	if (ArtifactSummary && ArtifactSummary->IsValid())
	{
		TestEqual(TEXT("artifact summary counts only safe contents"),
			static_cast<int32>((*ArtifactSummary)->GetNumberField(TEXT("content_count"))),
			1);
		TestFalse(TEXT("artifact summary says legacy debug export refs are absent"),
			(*ArtifactSummary)->GetBoolField(TEXT("contains_legacy_debug_export_refs")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDebugCaseStoreWritesUnderSavedDebugTest,
	"BlueprintHelper.RuntimeDiagnostics.Debug.CaseStoreWritesOnlyUnderSavedDebug",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDebugCaseStoreWritesUnderSavedDebugTest::RunTest(const FString& Parameters)
{
	const FString DebugCaseId = FBlueprintHelperDebugCaseTestsLocalUtils::MakeDebugTestId(TEXT("dbg_store"));
	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugCaseFile(DebugCaseId);

	FBlueprintHelperDebugCase DebugCase;
	DebugCase.DebugCaseId = DebugCaseId;
	DebugCase.CreatedAt = TEXT("2026-05-08T00:00:00Z");
	DebugCase.UpdatedAt = DebugCase.CreatedAt;
	DebugCase.Source = TEXT("compile_failure");
	DebugCase.Severity = EBlueprintHelperDebugSeverity::Error;
	DebugCase.Status = EBlueprintHelperDebugCaseStatus::Open;
	DebugCase.Operation = TEXT("compile_blueprint_asset");
	DebugCase.Stage = TEXT("compile");
	DebugCase.TraceIds.Add(TEXT("trace_store"));
	DebugCase.AssetPaths.Add(TEXT("/Game/BP_Door"));
	DebugCase.Error.Code = TEXT("compile_failed");
	DebugCase.Error.Message = TEXT("Compile failed.");

	FBlueprintHelperDebugCaseStoreService Store;
	FString Error;
	TestTrue(TEXT("case is saved"), Store.SaveCase(DebugCase, &Error));
	if (!Error.IsEmpty())
	{
		AddError(FString::Printf(TEXT("Unexpected save error: %s"), *Error));
	}

	const FString CasePath = FBlueprintHelperDebugCaseTestsLocalUtils::ExpectedDebugCasePath(DebugCaseId);
	TestTrue(TEXT("case file exists at Saved/BlueprintHelper/Debug/Cases"), FPaths::FileExists(CasePath));

	FString NormalizedCasePath = CasePath;
	FString NormalizedDebugRoot = FBlueprintHelperDebugCaseTestsLocalUtils::ExpectedDebugRootDir();
	FPaths::NormalizeFilename(NormalizedCasePath);
	FPaths::NormalizeDirectoryName(NormalizedDebugRoot);
	TestTrue(TEXT("case file is inside Saved/BlueprintHelper/Debug"),
		NormalizedCasePath.StartsWith(NormalizedDebugRoot));

	FBlueprintHelperDebugCaseSummary Summary;
	TestTrue(TEXT("case summary query succeeds"), Store.QueryCaseSummary(DebugCaseId, Summary, &Error));
	TSharedRef<FJsonObject> SummaryJson = Summary.ToJson();
	const FString SerializedSummary = FBlueprintHelperDebugCaseTestsLocalUtils::SerializeJsonObject(SummaryJson);
	TestFalse(TEXT("summary does not expose saved file paths"), SerializedSummary.Contains(TEXT("Saved/BlueprintHelper/Debug")));
	TestFalse(TEXT("summary does not expose local path fields"), SerializedSummary.Contains(TEXT("local_path")));
	TestFalse(TEXT("summary does not expose raw payload fields"), SerializedSummary.Contains(TEXT("raw_payload")));
	TestFalse(TEXT("summary does not expose artifact contents"), SerializedSummary.Contains(TEXT("artifact_contents")));
	TestFalse(TEXT("summary does not expose full event payloads"), SummaryJson->HasField(TEXT("events")));

	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugCaseFile(DebugCaseId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDebugEntryBestEffortEventRecordingTest,
	"BlueprintHelper.RuntimeDiagnostics.Debug.EntryRecordsEventBestEffortAndQueriesSummary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDebugEntryBestEffortEventRecordingTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperDebugCaseStoreService Store;
	FBlueprintHelperDebugEntryService Entry(Store);

	FBlueprintHelperDebugEntryEventInput Input;
	Input.SourceLayer = TEXT("task_runtime");
	Input.Source = TEXT("task_execute_failure");
	Input.Operation = TEXT("execute_task");
	Input.Stage = TEXT("execute");
	Input.Severity = EBlueprintHelperDebugSeverity::Error;
	Input.TraceId = TEXT("trace_entry");
	Input.TaskRunId = TEXT("task_entry");
	Input.AssetPaths.Add(TEXT("/Game/BP_Door"));
	Input.ReviewRecordIds.Add(TEXT("review_entry_record"));
	FBlueprintHelperDebugTransactionLink TransactionLink;
	TransactionLink.TransactionId = TEXT("tx_debug_entry");
	TransactionLink.Role = TEXT("task_source");
	TransactionLink.Source = TEXT("task_runtime");
	TransactionLink.Summary = TEXT("source transaction summary only");
	Input.TransactionLinks.Add(TransactionLink);
	Input.Error.Code = TEXT("execute_failed");
	Input.Error.Message = TEXT("Execute failed.");
	Input.RecommendedNext = TEXT("inspect_debug_case");

	TSharedRef<FJsonObject> ToolSummary = MakeShared<FJsonObject>();
	ToolSummary->SetStringField(TEXT("summary"), TEXT("safe compact summary"));
	ToolSummary->SetStringField(TEXT("raw_payload"), TEXT("must not leave summary query"));
	Input.ToolResultSummary = ToolSummary;

	FBlueprintHelperDebugEntryRecordResult RecordResult = Entry.RecordEventBestEffort(Input);
	TestTrue(TEXT("best-effort entry records the event"), RecordResult.bRecorded);
	TestFalse(TEXT("record result has a debug case id"), RecordResult.DebugCaseId.IsEmpty());
	TestFalse(TEXT("record result has a debug event id"), RecordResult.DebugEventId.IsEmpty());

	FBlueprintHelperDebugCase LoadedCase;
	FString Error;
	TestTrue(TEXT("recorded case can be loaded"), Store.LoadCase(RecordResult.DebugCaseId, LoadedCase, &Error));
	TestEqual(TEXT("full stored case keeps the event for local diagnostics"), LoadedCase.Events.Num(), 1);

	FBlueprintHelperDebugCaseSummary Summary;
	TestTrue(TEXT("summary query succeeds"), Store.QueryCaseSummary(RecordResult.DebugCaseId, Summary, &Error));
	TestEqual(TEXT("summary reports event count only"), Summary.EventCount, 1);
	TestEqual(TEXT("summary keeps error code"), Summary.Error.Code, FString(TEXT("execute_failed")));
	TestEqual(TEXT("summary keeps operation"), Summary.Operation, FString(TEXT("execute_task")));
	TestTrue(TEXT("summary keeps review record ids"),
		Summary.ReviewRecordIds.Contains(TEXT("review_entry_record")));
	TestEqual(TEXT("summary keeps transaction link count"), Summary.TransactionLinks.Num(), 1);
	if (Summary.TransactionLinks.Num() == 1)
	{
		TestEqual(TEXT("summary keeps transaction id"),
			Summary.TransactionLinks[0].TransactionId,
			FString(TEXT("tx_debug_entry")));
		TestEqual(TEXT("summary keeps transaction role"),
			Summary.TransactionLinks[0].Role,
			FString(TEXT("task_source")));
	}

	const FString SerializedSummary = FBlueprintHelperDebugCaseTestsLocalUtils::SerializeJsonObject(Summary.ToJson());
	TestFalse(TEXT("summary hides raw payload fields from event evidence"),
		SerializedSummary.Contains(TEXT("raw_payload")));
	TestFalse(TEXT("summary hides local saved paths"),
		SerializedSummary.Contains(TEXT("Saved/BlueprintHelper/Debug")));
	TestTrue(TEXT("summary serializes transaction_links"),
		SerializedSummary.Contains(TEXT("transaction_links")));

	TSharedRef<FJsonObject> QueryPayload = MakeShared<FJsonObject>();
	QueryPayload->SetStringField(TEXT("debug_case_id"), RecordResult.DebugCaseId);
	const FBlueprintHelperToolResultBase QueryResult = Entry.GetDebugCaseSummaryResult(QueryPayload);
	TestTrue(TEXT("get_debug_case result succeeds"), QueryResult.bOk);
	TestNotNull(TEXT("get_debug_case returns data"), QueryResult.Data.Get());
	if (QueryResult.Data.IsValid())
	{
		const TSharedPtr<FJsonObject>* DebugCaseJson = nullptr;
		TestTrue(TEXT("get_debug_case data has debug_case summary"),
			QueryResult.Data->TryGetObjectField(TEXT("debug_case"), DebugCaseJson));
		if (DebugCaseJson && DebugCaseJson->IsValid())
		{
			TestEqual(TEXT("get_debug_case returns summary schema"),
				(*DebugCaseJson)->GetStringField(TEXT("schema")),
				FString(TEXT("BlueprintHelper.DebugCaseSummary.v1")));
			TestFalse(TEXT("get_debug_case summary omits events"),
				(*DebugCaseJson)->HasField(TEXT("events")));
		}
	}

	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugCaseFile(RecordResult.DebugCaseId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDebugBundleExportsReviewSummaryArtifactTest,
	"BlueprintHelper.RuntimeDiagnostics.Debug.BundleSummaryExportIncludesReviewSummaryArtifact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDebugBundleExportsReviewSummaryArtifactTest::RunTest(const FString& Parameters)
{
	const FString DebugCaseId = FBlueprintHelperDebugCaseTestsLocalUtils::MakeDebugTestId(TEXT("dbg_review_bundle"));
	const FString ReviewRecordId = FBlueprintHelperReviewStoreService::MakeReviewRecordId(
		TEXT("archive_debug_bundle_review"),
		TEXT("/Game/BP_DebugReview"));
	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugCaseFile(DebugCaseId);
	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupReviewRecordFile(ReviewRecordId);
	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupArchiveSessionFile(TEXT("archive_debug_bundle_review"));
	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupReviewSnapshotDirectory(TEXT("archive_debug_bundle_review"));

	FBlueprintHelperReviewRecord ReviewRecord;
	ReviewRecord.ReviewRecordId = ReviewRecordId;
	ReviewRecord.ArchiveSessionId = TEXT("archive_debug_bundle_review");
	ReviewRecord.AssetPath = TEXT("/Game/BP_DebugReview");
	ReviewRecord.Status = EBlueprintHelperReviewChangeStatus::NeedsAction;
	ReviewRecord.SourceTaskRunIds.Add(TEXT("task_review_summary"));
	ReviewRecord.DebugCaseIds.Add(DebugCaseId);
	ReviewRecord.SourceTransactionSummary.TransactionCount = 1;
	ReviewRecord.SourceTransactionSummary.TaskRunIds.Add(TEXT("task_review_summary"));
	ReviewRecord.SourceTransactionSummary.OperationKinds.Add(TEXT("graph_write"));
	ReviewRecord.SourceTransactionSummary.AssetPaths.Add(TEXT("/Game/BP_DebugReview"));
	ReviewRecord.SourceTransactionSummary.TransactionIds.Add(TEXT("tx_review_summary"));
	ReviewRecord.SourceTransactionSummary.FinalReviewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;

	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_review_summary");
	Change.AssetPath = TEXT("/Game/BP_DebugReview");
	Change.GraphName = TEXT("EventGraph");
	Change.LocationKey = TEXT("graph:EventGraph:block:DoorFlow");
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	Change.Status = EBlueprintHelperReviewChangeStatus::NeedsAction;
	Change.DisplayLabel = TEXT("Door Flow");
	Change.NeedsActionReason = TEXT("current_state_changed");
	ReviewRecord.VisibleChanges.Add(Change);

	FBlueprintHelperReviewStoreService ReviewStore;
	FString ReviewError;
	FBlueprintHelperReviewArchiveSession ArchiveSession;
	ArchiveSession.ArchiveSessionId = ReviewRecord.ArchiveSessionId;
	ArchiveSession.TaskRunId = TEXT("task_review_summary");
	ArchiveSession.AllowedTargetAssets.Add(TEXT("/Game/BP_DebugReview"));
	ArchiveSession.BaselineDirtyAssetPolicy = TEXT("allow_stale_disk_snapshot");
	ArchiveSession.BaselineSnapshotTrust = TEXT("stale_disk_copy");
	ArchiveSession.DirtyTargetAssets.Add(TEXT("/Game/BP_DebugReview"));
	ArchiveSession.BaselineSnapshotRefs.Add(TEXT("review://archive/archive_debug_bundle_review/baseline/_Game_BP_DebugReview.uasset"));
	ArchiveSession.BaselineSemanticSnapshotRefs.Add(TEXT("review://archive/archive_debug_bundle_review/baseline/_Game_BP_DebugReview_semantic/baseline.semantic.json"));
	ArchiveSession.BaselineWarnings.Add(TEXT("Review baseline snapshot copied from disk while target asset was dirty in editor."));

	const FString SemanticSnapshotDir =
		FBlueprintHelperDebugCaseTestsLocalUtils::ExpectedReviewSnapshotDirectory(TEXT("archive_debug_bundle_review"))
		/ TEXT("_Game_BP_DebugReview_semantic");
	IFileManager::Get().MakeDirectory(*SemanticSnapshotDir, true);
	TSharedRef<FJsonObject> SemanticSnapshot = MakeShared<FJsonObject>();
	SemanticSnapshot->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.ReviewBaselineSemanticSnapshot.v1"));
	SemanticSnapshot->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_DebugReview"));
	FString SemanticSnapshotText;
	TSharedRef<TJsonWriter<>> SemanticSnapshotWriter = TJsonWriterFactory<>::Create(&SemanticSnapshotText);
	FJsonSerializer::Serialize(SemanticSnapshot, SemanticSnapshotWriter);
	TestTrue(TEXT("semantic baseline snapshot fixture writes"),
		FFileHelper::SaveStringToFile(
			SemanticSnapshotText,
			*(SemanticSnapshotDir / TEXT("baseline.semantic.json")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	TestTrue(TEXT("archive session saves before bundle export"),
		ReviewStore.SaveArchiveSession(ArchiveSession, ReviewError));
	TestTrue(TEXT("review record saves before bundle export"),
		ReviewStore.SaveReviewRecord(ReviewRecord, ReviewError));

	FBlueprintHelperDebugCase DebugCase;
	DebugCase.DebugCaseId = DebugCaseId;
	DebugCase.CreatedAt = TEXT("2026-05-09T00:00:00Z");
	DebugCase.UpdatedAt = DebugCase.CreatedAt;
	DebugCase.Source = TEXT("review_reject_needs_action");
	DebugCase.Severity = EBlueprintHelperDebugSeverity::Error;
	DebugCase.Status = EBlueprintHelperDebugCaseStatus::Open;
	DebugCase.Operation = TEXT("reject_review_targets");
	DebugCase.Stage = TEXT("reject");
	DebugCase.AssetPaths.Add(TEXT("/Game/BP_DebugReview"));
	DebugCase.ReviewRecordIds.Add(ReviewRecordId);
	DebugCase.Error.Code = TEXT("review_reject_needs_action");
	DebugCase.Error.Message = TEXT("current_state_changed");

	FBlueprintHelperDebugCaseStoreService Store;
	FString Error;
	TestTrue(TEXT("debug case with review link saves"), Store.SaveCase(DebugCase, &Error));

	FBlueprintHelperDebugBundleManifest Manifest;
	const bool bExported = Store.ExportDebugBundleSummary(DebugCaseId, &ReviewStore, Manifest, &Error);
	TestTrue(TEXT("summary bundle exports review summary artifact"), bExported);
	if (!bExported)
	{
		FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugCaseFile(DebugCaseId);
		FBlueprintHelperDebugCaseTestsLocalUtils::CleanupReviewRecordFile(ReviewRecordId);
		return false;
	}
	TestEqual(TEXT("one review summary ref is exported"), Manifest.ReviewSummaryRefs.Num(), 1);
	if (Manifest.ReviewSummaryRefs.Num() != 1)
	{
		FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugBundleDirectory(Manifest.BundleId);
		FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugCaseFile(DebugCaseId);
		FBlueprintHelperDebugCaseTestsLocalUtils::CleanupReviewRecordFile(ReviewRecordId);
		FBlueprintHelperDebugCaseTestsLocalUtils::CleanupArchiveSessionFile(TEXT("archive_debug_bundle_review"));
		FBlueprintHelperDebugCaseTestsLocalUtils::CleanupReviewSnapshotDirectory(TEXT("archive_debug_bundle_review"));
		return false;
	}
	TestTrue(TEXT("review summary ref is relative"),
		FPaths::IsRelative(Manifest.ReviewSummaryRefs[0]));
	TestTrue(TEXT("review summary ref is under artifacts/review"),
		Manifest.ReviewSummaryRefs[0].StartsWith(TEXT("artifacts/review/")));

	FString ReviewSummaryText;
	const FString ReviewSummaryPath =
		FBlueprintHelperDebugCaseStoreService::GetBundleDirectory(Manifest.BundleId) / Manifest.ReviewSummaryRefs[0];
	TestTrue(TEXT("review summary file exists"), FPaths::FileExists(ReviewSummaryPath));
	TestTrue(TEXT("review summary file is readable"),
		FFileHelper::LoadFileToString(ReviewSummaryText, *ReviewSummaryPath));
	TestTrue(TEXT("review summary artifact carries schema"),
		ReviewSummaryText.Contains(TEXT("BlueprintHelper.ReviewSummaryArtifact.v1")));
	TestTrue(TEXT("review summary artifact carries review record id"),
		ReviewSummaryText.Contains(ReviewRecordId));
	TestFalse(TEXT("review summary artifact omits legacy debug export refs"),
		ReviewSummaryText.Contains(TEXT("debug_export_refs")));
	TestFalse(TEXT("review summary artifact omits local paths"),
		ReviewSummaryText.Contains(TEXT("Saved/BlueprintHelper")));
	TestTrue(TEXT("review summary artifact carries baseline dirty policy"),
		ReviewSummaryText.Contains(TEXT("allow_stale_disk_snapshot")));
	TestTrue(TEXT("review summary artifact carries baseline trust"),
		ReviewSummaryText.Contains(TEXT("stale_disk_copy")));
	TestTrue(TEXT("review summary artifact carries disk snapshot refs"),
		ReviewSummaryText.Contains(TEXT("disk_snapshot_refs")));
	bool bFoundSemanticSnapshotArtifact = false;
	for (const FString& ContentRef : Manifest.Contents)
	{
		if (ContentRef.Contains(TEXT("baseline.semantic")))
		{
			bFoundSemanticSnapshotArtifact = true;
			const FString SemanticArtifactPath =
				FBlueprintHelperDebugCaseStoreService::GetBundleDirectory(Manifest.BundleId) / ContentRef;
			FString SemanticArtifactText;
			TestTrue(TEXT("semantic baseline artifact is readable"),
				FFileHelper::LoadFileToString(SemanticArtifactText, *SemanticArtifactPath));
			TestTrue(TEXT("semantic baseline artifact carries schema"),
				SemanticArtifactText.Contains(TEXT("BlueprintHelper.ReviewBaselineSemanticSnapshot.v1")));
			break;
		}
	}
	TestTrue(TEXT("debug bundle includes semantic baseline artifact"), bFoundSemanticSnapshotArtifact);

	FBlueprintHelperReviewRecord ReloadedReviewRecord;
	FString ReloadError;
	TestTrue(TEXT("review record reloads after bundle export"),
		ReviewStore.LoadReviewRecordById(ReviewRecordId, ReloadedReviewRecord, ReloadError));
	TestTrue(TEXT("bundle export keeps debug case id on ReviewRecord"),
		ReloadedReviewRecord.DebugCaseIds.Contains(DebugCaseId));

	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugBundleDirectory(Manifest.BundleId);
	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugCaseFile(DebugCaseId);
	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupReviewRecordFile(ReviewRecordId);
	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupArchiveSessionFile(TEXT("archive_debug_bundle_review"));
	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupReviewSnapshotDirectory(TEXT("archive_debug_bundle_review"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDebugCompileFailureProducesFailureResultTest,
	"BlueprintHelper.RuntimeDiagnostics.Debug.CompileDiagnosticsFailureProducesDebugVisibleFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDebugCompileFailureProducesFailureResultTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperCompileResult CompileResult;
	CompileResult.bSuccess = false;
	CompileResult.BlueprintStatus = 1;
	CompileResult.Diagnostics.Add(
		EBlueprintHelperDiagnosticSeverity::Error,
		TEXT("Bad node compile error."),
		TEXT("BadNode"),
		TEXT("compile_error"));

	const FBlueprintHelperToolResultBase Result =
		FBlueprintHelperCompileAssetService::BuildResultFromCompileResult(
			TEXT("trace_compile_failure"),
			TEXT("/Game/BP_DebugCompileFailure"),
			CompileResult,
			nullptr);

	TestFalse(TEXT("compile diagnostics failure is a ToolResult failure"), Result.bOk);
	TestTrue(TEXT("compile diagnostics failure preserves result data"), Result.Data.IsValid());
	TestTrue(TEXT("compile diagnostics failure has error"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("compile diagnostics failure uses compile_failed"),
			Result.Error->Code,
			FString(TEXT("compile_failed")));
	}
	if (Result.Data.IsValid())
	{
		const TSharedPtr<FJsonObject>* CompileResultJson = nullptr;
		TestTrue(TEXT("compile failure data includes compile_result"),
			Result.Data->TryGetObjectField(TEXT("compile_result"), CompileResultJson));
		if (CompileResultJson && CompileResultJson->IsValid())
		{
			TestFalse(TEXT("compile_result.success is false"),
				(*CompileResultJson)->GetBoolField(TEXT("success")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDebugRedactionAndBundleSummaryExportTest,
	"BlueprintHelper.RuntimeDiagnostics.Debug.BundleSummaryExportRedactsSensitiveArtifacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDebugRedactionAndBundleSummaryExportTest::RunTest(const FString& Parameters)
{
	const FString DebugCaseId = FBlueprintHelperDebugCaseTestsLocalUtils::MakeDebugTestId(TEXT("dbg_bundle"));
	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugCaseFile(DebugCaseId);

	FBlueprintHelperDebugCase DebugCase;
	DebugCase.DebugCaseId = DebugCaseId;
	DebugCase.CreatedAt = TEXT("2026-05-09T00:00:00Z");
	DebugCase.UpdatedAt = DebugCase.CreatedAt;
	DebugCase.Source = TEXT("task_execute_failure");
	DebugCase.Severity = EBlueprintHelperDebugSeverity::Error;
	DebugCase.Status = EBlueprintHelperDebugCaseStatus::Open;
	DebugCase.Operation = TEXT("execute_task_plan");
	DebugCase.Stage = TEXT("execute");
	DebugCase.TraceIds.Add(TEXT("trace_bundle"));
	DebugCase.AssetPaths.Add(TEXT("/Game/BP_DebugBundle"));
	DebugCase.AssetPaths.Add(FPaths::ProjectSavedDir() / TEXT("BlueprintHelper/Debug/raw.json"));
	DebugCase.Error.Code = TEXT("execute_failed");
	DebugCase.Error.Message = FString::Printf(TEXT("Token sk-test-secret at %s"),
		*(FPaths::ProjectSavedDir() / TEXT("BlueprintHelper/Debug/raw.json")));

	FBlueprintHelperDebugEvent Event;
	Event.DebugEventId = TEXT("ev_bundle");
	Event.DebugCaseId = DebugCaseId;
	Event.CreatedAt = DebugCase.CreatedAt;
	Event.SourceLayer = TEXT("task_runtime");
	Event.Source = DebugCase.Source;
	Event.Operation = DebugCase.Operation;
	Event.Stage = DebugCase.Stage;
	Event.TraceId = TEXT("trace_bundle");
	Event.Error = DebugCase.Error;
	Event.ToolResultSummary = MakeShared<FJsonObject>();
	Event.ToolResultSummary->SetStringField(TEXT("raw_payload"), TEXT("{\"secret\":\"sk-test-secret\"}"));
	Event.ToolResultSummary->SetStringField(TEXT("source_file"), TEXT("#include \"PrivateSecret.h\"\nclass FSecret {};"));
	DebugCase.Events.Add(Event);

	FBlueprintHelperDebugCaseStoreService Store;
	FString Error;
	TestTrue(TEXT("case saves before export"), Store.SaveCase(DebugCase, &Error));

	FBlueprintHelperDebugBundleManifest Manifest;
	TestTrue(TEXT("summary bundle exports"), Store.ExportDebugBundleSummary(DebugCaseId, Manifest, &Error));
	TestFalse(TEXT("bundle id is set"), Manifest.BundleId.IsEmpty());
	TestTrue(TEXT("bundle summary ref is relative"), FPaths::IsRelative(Manifest.SummaryRef));
	TestEqual(TEXT("bundle summary is markdown"), Manifest.SummaryRef, FString(TEXT("summary.md")));
	TestTrue(TEXT("bundle contains markdown summary"), Manifest.Contents.Contains(TEXT("summary.md")));
	TestTrue(TEXT("bundle contains debug case summary artifact"),
		Manifest.Contents.Contains(TEXT("artifacts/debug_case.summary.json")));
	TestTrue(TEXT("sensitive artifacts are reported as skipped"), Manifest.SkippedArtifacts.Num() > 0);

	const FString BundleDir = FBlueprintHelperDebugCaseStoreService::GetBundleDirectory(Manifest.BundleId);
	TestTrue(TEXT("artifacts directory exists"), IFileManager::Get().DirectoryExists(*(BundleDir / TEXT("artifacts"))));
	TestTrue(TEXT("review artifact directory exists"), IFileManager::Get().DirectoryExists(*(BundleDir / TEXT("artifacts/review"))));
	TestTrue(TEXT("transaction artifact directory exists"), IFileManager::Get().DirectoryExists(*(BundleDir / TEXT("artifacts/transactions"))));
	TestTrue(TEXT("asset artifact directory exists"), IFileManager::Get().DirectoryExists(*(BundleDir / TEXT("artifacts/assets"))));
	TestTrue(TEXT("log artifact directory exists"), IFileManager::Get().DirectoryExists(*(BundleDir / TEXT("artifacts/logs"))));

	FString SummaryText;
	const FString SummaryPath = BundleDir / Manifest.SummaryRef;
	TestTrue(TEXT("summary file exists"), FPaths::FileExists(SummaryPath));
	TestTrue(TEXT("summary file is readable"), FFileHelper::LoadFileToString(SummaryText, *SummaryPath));
	TestTrue(TEXT("summary is markdown"), SummaryText.StartsWith(TEXT("# BlueprintHelper Debug Bundle")));
	TestFalse(TEXT("summary does not contain raw payload"), SummaryText.Contains(TEXT("raw_payload")));
	TestFalse(TEXT("summary does not contain token"), SummaryText.Contains(TEXT("sk-test-secret")));
	TestFalse(TEXT("summary does not contain local debug path"),
		SummaryText.Contains(TEXT("Saved/BlueprintHelper/Debug")));
	TestFalse(TEXT("summary does not contain source content"), SummaryText.Contains(TEXT("#include")));

	FString DebugCaseSummaryText;
	const FString DebugCaseSummaryPath = BundleDir / TEXT("artifacts/debug_case.summary.json");
	TestTrue(TEXT("debug case summary artifact exists"), FPaths::FileExists(DebugCaseSummaryPath));
	TestTrue(TEXT("debug case summary artifact is readable"),
		FFileHelper::LoadFileToString(DebugCaseSummaryText, *DebugCaseSummaryPath));
	TestTrue(TEXT("debug case summary artifact carries summary schema"),
		DebugCaseSummaryText.Contains(TEXT("BlueprintHelper.DebugCaseSummary.v1")));

	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugBundleDirectory(Manifest.BundleId);
	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugCaseFile(DebugCaseId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDebugCleanupResolvedLowSeverityCasesTest,
	"BlueprintHelper.RuntimeDiagnostics.Debug.CleanupArchivesOnlyResolvedLowSeverityCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDebugCleanupResolvedLowSeverityCasesTest::RunTest(const FString& Parameters)
{
	const FString ResolvedId = FBlueprintHelperDebugCaseTestsLocalUtils::MakeDebugTestId(TEXT("dbg_resolved"));
	const FString NeedsActionId = FBlueprintHelperDebugCaseTestsLocalUtils::MakeDebugTestId(TEXT("dbg_needs_action"));
	const FString RollbackId = FBlueprintHelperDebugCaseTestsLocalUtils::MakeDebugTestId(TEXT("dbg_rollback"));
	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugCaseFile(ResolvedId);
	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugCaseFile(NeedsActionId);
	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugCaseFile(RollbackId);

	FBlueprintHelperDebugCase Resolved;
	Resolved.DebugCaseId = ResolvedId;
	Resolved.CreatedAt = TEXT("2026-05-09T00:00:00Z");
	Resolved.UpdatedAt = Resolved.CreatedAt;
	Resolved.Source = TEXT("diagnostic_notice");
	Resolved.Severity = EBlueprintHelperDebugSeverity::Warning;
	Resolved.Status = EBlueprintHelperDebugCaseStatus::Resolved;

	FBlueprintHelperDebugCase NeedsAction = Resolved;
	NeedsAction.DebugCaseId = NeedsActionId;
	NeedsAction.Status = EBlueprintHelperDebugCaseStatus::NeedsAction;

	FBlueprintHelperDebugCase Rollback = Resolved;
	Rollback.DebugCaseId = RollbackId;
	Rollback.Source = TEXT("transaction_rollback_failure");
	Rollback.Error.Code = TEXT("rollback_failed");

	FBlueprintHelperDebugCaseStoreService Store;
	FString Error;
	TestTrue(TEXT("resolved case saves"), Store.SaveCase(Resolved, &Error));
	TestTrue(TEXT("needs_action case saves"), Store.SaveCase(NeedsAction, &Error));
	TestTrue(TEXT("rollback case saves"), Store.SaveCase(Rollback, &Error));

	TArray<FString> ArchivedCaseIds;
	TestTrue(TEXT("cleanup policy runs"), Store.CleanupResolvedLowSeverityCases(ArchivedCaseIds, &Error));
	TestTrue(TEXT("resolved case is archived"), ArchivedCaseIds.Contains(ResolvedId));
	TestFalse(TEXT("needs_action case is preserved"), ArchivedCaseIds.Contains(NeedsActionId));
	TestFalse(TEXT("rollback case is preserved"), ArchivedCaseIds.Contains(RollbackId));

	FBlueprintHelperDebugCase LoadedResolved;
	FBlueprintHelperDebugCase LoadedNeedsAction;
	FBlueprintHelperDebugCase LoadedRollback;
	TestTrue(TEXT("resolved case still exists"), Store.LoadCase(ResolvedId, LoadedResolved, &Error));
	TestTrue(TEXT("needs_action case still exists"), Store.LoadCase(NeedsActionId, LoadedNeedsAction, &Error));
	TestTrue(TEXT("rollback case still exists"), Store.LoadCase(RollbackId, LoadedRollback, &Error));
	TestTrue(TEXT("resolved case is archived"),
		LoadedResolved.Status == EBlueprintHelperDebugCaseStatus::Archived);
	TestTrue(TEXT("needs_action case stays needs_action"),
		LoadedNeedsAction.Status == EBlueprintHelperDebugCaseStatus::NeedsAction);
	TestTrue(TEXT("rollback case stays resolved for later action"),
		LoadedRollback.Status == EBlueprintHelperDebugCaseStatus::Resolved);

	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugCaseFile(ResolvedId);
	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugCaseFile(NeedsActionId);
	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugCaseFile(RollbackId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDebugEntryDeveloperUiInternalsTest,
	"BlueprintHelper.RuntimeDiagnostics.Debug.EntryProvidesDeveloperUiInternals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDebugEntryDeveloperUiInternalsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperDebugCaseStoreService Store;
	FBlueprintHelperDebugEntryService Entry(Store);

	FBlueprintHelperDebugEntryEventInput Input;
	Input.SourceLayer = TEXT("task_runtime");
	Input.Source = TEXT("task_execute_failure");
	Input.Operation = TEXT("execute_task_plan");
	Input.Stage = TEXT("execute");
	Input.Severity = EBlueprintHelperDebugSeverity::Warning;
	Input.Error.Code = TEXT("execute_failed");
	Input.Error.Message = TEXT("Execute failed.");

	const FBlueprintHelperDebugEntryRecordResult RecordResult = Entry.RecordEventBestEffort(Input);
	TestTrue(TEXT("case is recorded"), RecordResult.bRecorded);

	const FBlueprintHelperToolResultBase ListResult = Entry.GetDebugCaseListResult(MakeShared<FJsonObject>());
	TestTrue(TEXT("developer list succeeds"), ListResult.bOk);
	TestTrue(TEXT("developer list has data"), ListResult.Data.IsValid());

	TSharedRef<FJsonObject> ExportPayload = MakeShared<FJsonObject>();
	ExportPayload->SetStringField(TEXT("debug_case_id"), RecordResult.DebugCaseId);
	const FBlueprintHelperToolResultBase ExportResult = Entry.ExportDebugBundleSummaryResult(ExportPayload);
	TestTrue(TEXT("developer export succeeds"), ExportResult.bOk);
	TestTrue(TEXT("developer export has data"), ExportResult.Data.IsValid());
	FString ExportedBundleId;
	if (ExportResult.Data.IsValid())
	{
		const TSharedPtr<FJsonObject>* ManifestJson = nullptr;
		if (ExportResult.Data->TryGetObjectField(TEXT("manifest"), ManifestJson) && ManifestJson && ManifestJson->IsValid())
		{
			(*ManifestJson)->TryGetStringField(TEXT("bundle_id"), ExportedBundleId);
		}
	}

	TSharedRef<FJsonObject> CleanupPayload = MakeShared<FJsonObject>();
	const FBlueprintHelperToolResultBase CleanupResult = Entry.CleanupDebugCasesResult(CleanupPayload);
	TestTrue(TEXT("developer cleanup succeeds"), CleanupResult.bOk);
	TestTrue(TEXT("developer cleanup has data"), CleanupResult.Data.IsValid());

	if (!ExportedBundleId.IsEmpty())
	{
		FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugBundleDirectory(ExportedBundleId);
	}
	FBlueprintHelperDebugCaseTestsLocalUtils::CleanupDebugCaseFile(RecordResult.DebugCaseId);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
