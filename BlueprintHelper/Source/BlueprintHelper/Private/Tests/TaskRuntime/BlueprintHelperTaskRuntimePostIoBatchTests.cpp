#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimePostIoBatch.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimePostIoService.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeReviewIoBatch.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	static FBlueprintHelperWriteReviewEvidence MakeGraphWriteEvidence()
	{
		FBlueprintHelperWriteReviewEvidence Evidence;
		Evidence.ArchiveSessionId = TEXT("archive_post_io_graph_write");
		Evidence.TaskRunId = TEXT("task_post_io_graph_write");
		Evidence.EvidenceId = TEXT("task_step_task_post_io_graph_write_7");
		Evidence.CreatedAt = TEXT("2026-05-25T10:00:00Z");
		Evidence.AssetPath = TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter");
		Evidence.OperationKind = TEXT("append_blueprint_graph");
		Evidence.DisplayLabel = TEXT("append_blueprint_graph");
		Evidence.TaskStepIndex = 7;

		FBlueprintHelperReviewAtomicTarget Target;
		Target.AssetPath = Evidence.AssetPath;
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
		Target.GraphName = TEXT("EventGraph");
		Target.TargetKind = TEXT("graph_block");
		Target.TargetKey = TEXT("graph:EventGraph:block:EventGraph_CE_DumpGlobalStateForReview0");
		Target.VisualGroupKey = TEXT("graph_body|EventGraph");
		Target.DisplayLabel = TEXT("append_blueprint_graph EventGraph");
		Target.LatestEvidenceId = Evidence.EvidenceId;
		Target.SourceEvidenceIds.Add(Evidence.EvidenceId);
		Target.Ownership = TEXT("graph_write");
		Target.TaskStepIndex = Evidence.TaskStepIndex;
		Target.ExecutionOrder = Evidence.TaskStepIndex;
		Evidence.AtomicTargets.Add(Target);
		return Evidence;
	}
}

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimePostIoReviewStore_PreservesGraphWriteEvidenceFields,
	"BlueprintHelper.TaskRuntime.PostIO.ReviewStorePreservesGraphWriteEvidenceFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimePostIoReviewStore_PreservesGraphWriteEvidenceFields::RunTest(const FString& Parameters)
{
	const FBlueprintHelperWriteReviewEvidence Evidence = MakeGraphWriteEvidence();
	const FBlueprintHelperReviewStoreService Store;
	const TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence({Evidence});

	TestEqual(TEXT("one review record is built"), Records.Num(), 1);
	if (Records.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewRecord& Record = Records[0];
	TestEqual(TEXT("record asset path survives evidence construction"),
		Record.AssetPath,
		Evidence.AssetPath);
	TestEqual(TEXT("record source task run id survives evidence construction"),
		Record.SourceTaskRunIds.Num(),
		1);
	TestEqual(TEXT("record source task run id value survives evidence construction"),
		Record.SourceTaskRunIds.Num() == 1 ? Record.SourceTaskRunIds[0] : FString(),
		Evidence.TaskRunId);
	TestEqual(TEXT("record operation kind survives evidence construction"),
		Record.SourceReviewSummary.OperationKinds.Num(),
		1);
	TestEqual(TEXT("record operation kind value survives evidence construction"),
		Record.SourceReviewSummary.OperationKinds.Num() == 1 ? Record.SourceReviewSummary.OperationKinds[0] : FString(),
		Evidence.OperationKind);
	TestEqual(TEXT("record visible change count survives evidence construction"),
		Record.VisibleChanges.Num(),
		1);
	if (Record.VisibleChanges.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewVisibleChange& Change = Record.VisibleChanges[0];
	TestEqual(TEXT("change asset path survives evidence construction"),
		Change.AssetPath,
		Evidence.AssetPath);
	TestEqual(TEXT("change graph name survives evidence construction"),
		Change.GraphName,
		FString(TEXT("EventGraph")));
	TestEqual(TEXT("change task step index survives evidence construction"),
		Change.TaskStepIndex,
		Evidence.TaskStepIndex);
	TestEqual(TEXT("change atomic target count survives evidence construction"),
		Change.AtomicTargets.Num(),
		1);
	if (Change.AtomicTargets.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& Target = Change.AtomicTargets[0];
	TestEqual(TEXT("target asset path survives evidence construction"),
		Target.AssetPath,
		Evidence.AssetPath);
	TestEqual(TEXT("target graph name survives evidence construction"),
		Target.GraphName,
		FString(TEXT("EventGraph")));
	TestEqual(TEXT("target kind survives evidence construction"),
		Target.TargetKind,
		FString(TEXT("graph_block")));
	TestEqual(TEXT("target key preserves concrete graph block ref"),
		Target.TargetKey,
		FString(TEXT("graph:EventGraph:block:EventGraph_CE_DumpGlobalStateForReview0")));
	TestEqual(TEXT("target task step index survives evidence construction"),
		Target.TaskStepIndex,
		Evidence.TaskStepIndex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimePostIoService_ReportsDiagnosticsWhenEvidenceBuildsZeroRecords,
	"BlueprintHelper.TaskRuntime.PostIO.PostIoServiceReportsEvidenceZeroRecords",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimePostIoService_ReportsDiagnosticsWhenEvidenceBuildsZeroRecords::RunTest(const FString& Parameters)
{
	FBlueprintHelperWriteReviewEvidence InvalidEvidence = MakeGraphWriteEvidence();
	InvalidEvidence.AssetPath.Reset();

	FBlueprintHelperTaskRuntimePostIoBatch Batch;
	Batch.ReviewEvidences.Add(InvalidEvidence);

	TMap<FString, TSharedPtr<FJsonObject>> TaskRunJournals;
	const FBlueprintHelperTaskRuntimePostIoService Service;
	const FBlueprintHelperTaskRuntimePostIoFlushResult Result =
		Service.Flush(Batch, TaskRunJournals, nullptr);

	TestFalse(TEXT("zero review records becomes a post IO diagnostic"), Result.bOk);
	TestEqual(TEXT("one diagnostic is reported for zero review records"), Result.Diagnostics.Num(), 1);
	TestEqual(TEXT("zero review records diagnostic code is stable"),
		Result.Diagnostics.Num() == 1 ? Result.Diagnostics[0].Code : FString(),
		FString(TEXT("review_evidence_produced_zero_records")));
	TestEqual(TEXT("zero review records diagnostic points at review records"),
		Result.Diagnostics.Num() == 1 ? Result.Diagnostics[0].Field : FString(),
		FString(TEXT("review.records")));
	return true;
}

#endif
