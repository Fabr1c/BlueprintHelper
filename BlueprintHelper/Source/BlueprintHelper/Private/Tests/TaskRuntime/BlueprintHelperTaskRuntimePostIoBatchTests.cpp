#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimePostIoBatch.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeReviewIoBatch.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeReviewIoBatch_CollectsReviewEvidenceInOrder,
	"BlueprintHelper.TaskRuntime.PostIO.ReviewIoBatchCollectsEvidenceInOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeReviewIoBatch_CollectsReviewEvidenceInOrder::RunTest(const FString& Parameters)
{
	FBlueprintHelperWriteReviewEvidence ParentEvidence;
	ParentEvidence.EvidenceId = TEXT("parent_evidence");
	ParentEvidence.ArchiveSessionId = TEXT("archive_post_io");
	ParentEvidence.TaskRunId = TEXT("task_post_io");

	FBlueprintHelperWriteReviewEvidence ChildEvidence;
	ChildEvidence.EvidenceId = TEXT("child_evidence");
	ChildEvidence.ArchiveSessionId = TEXT("archive_post_io");
	ChildEvidence.TaskRunId = TEXT("task_post_io");

	FBlueprintHelperTaskRuntimeReviewIoBatch BatchBuilder;
	BatchBuilder.AddReviewEvidence(ParentEvidence);
	BatchBuilder.AddReviewEvidence(ChildEvidence);

	const FBlueprintHelperTaskRuntimePostIoBatch& Batch = BatchBuilder.GetBatch();
	TestTrue(TEXT("batch has work"), BatchBuilder.HasWork());
	TestEqual(TEXT("two evidence entries collected"), Batch.ReviewEvidences.Num(), 2);
	TestEqual(TEXT("parent evidence remains first"),
		Batch.ReviewEvidences[0].EvidenceId,
		FString(TEXT("parent_evidence")));
	TestEqual(TEXT("child evidence remains second"),
		Batch.ReviewEvidences[1].EvidenceId,
		FString(TEXT("child_evidence")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimePostIoFlushResult_ReportsDiagnosticsWithoutMutationFailure,
	"BlueprintHelper.TaskRuntime.PostIO.FlushResultReportsDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimePostIoFlushResult_ReportsDiagnosticsWithoutMutationFailure::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskRuntimePostIoFlushResult Result;
	Result.bOk = false;
	FBlueprintHelperTaskRuntimePostIoDiagnostic Diagnostic;
	Diagnostic.Code = TEXT("review_record_write_failed");
	Diagnostic.Message = TEXT("disk write failed");
	Diagnostic.Field = TEXT("review.records");
	Result.Diagnostics.Add(Diagnostic);

	const TSharedRef<FJsonObject> Json = Result.ToJson();
	bool bOk = true;
	Json->TryGetBoolField(TEXT("ok"), bOk);
	TestFalse(TEXT("post IO reports own failure"), bOk);

	const TArray<TSharedPtr<FJsonValue>>* Diagnostics = nullptr;
	TestTrue(TEXT("diagnostics are returned"),
		Json->TryGetArrayField(TEXT("diagnostics"), Diagnostics) && Diagnostics && Diagnostics->Num() == 1);
	return true;
}

#endif
