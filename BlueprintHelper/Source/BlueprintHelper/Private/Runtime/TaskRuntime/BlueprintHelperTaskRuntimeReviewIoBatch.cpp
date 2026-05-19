// BlueprintHelper TaskRuntime Review IO batch builder.

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeReviewIoBatch.h"

void FBlueprintHelperTaskRuntimeReviewIoBatch::SetArchiveSession(
	const FBlueprintHelperReviewArchiveSession& InArchiveSession)
{
	Batch.ArchiveSession = InArchiveSession;
}

void FBlueprintHelperTaskRuntimeReviewIoBatch::AddReviewEvidence(
	const FBlueprintHelperWriteReviewEvidence& Evidence)
{
	Batch.ReviewEvidences.Add(Evidence);
}

void FBlueprintHelperTaskRuntimeReviewIoBatch::SetTaskRunJournal(
	const FString& InTaskRunId,
	const TSharedPtr<FJsonObject>& Journal)
{
	Batch.TaskRunId = InTaskRunId;
	Batch.TaskRunJournal = Journal;
}

void FBlueprintHelperTaskRuntimeReviewIoBatch::SetDebugEvent(
	const FBlueprintHelperDebugEntryEventInput& Input,
	bool bInAttachDebugCaseToFailure)
{
	Batch.DebugEventInput = Input;
	Batch.bAttachDebugCaseToFailure = bInAttachDebugCaseToFailure;
}

void FBlueprintHelperTaskRuntimeReviewIoBatch::EnablePendingReviewNotification()
{
	Batch.bNotifyPendingReviewChanged = true;
}

const FBlueprintHelperTaskRuntimePostIoBatch& FBlueprintHelperTaskRuntimeReviewIoBatch::GetBatch() const
{
	return Batch;
}

bool FBlueprintHelperTaskRuntimeReviewIoBatch::HasWork() const
{
	return Batch.ArchiveSession.IsSet() ||
		Batch.ReviewEvidences.Num() > 0 ||
		Batch.TaskRunJournal.IsValid() ||
		Batch.DebugEventInput.IsSet() ||
		Batch.bNotifyPendingReviewChanged;
}
