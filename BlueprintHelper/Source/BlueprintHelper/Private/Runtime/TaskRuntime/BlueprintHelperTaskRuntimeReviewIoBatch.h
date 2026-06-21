// BlueprintHelper TaskRuntime Review IO batch builder.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimePostIoBatch.h"

class FBlueprintHelperTaskRuntimeReviewIoBatch
{
public:
	void SetArchiveSession(const FBlueprintHelperReviewArchiveSession& InArchiveSession);
	void SetSequentialReviewSessionUpdate(
		const FBlueprintHelperSequentialReviewSessionExecuteUpdate& Update);
	void AddReviewEvidence(const FBlueprintHelperWriteReviewEvidence& Evidence);
	void SetTaskRunJournal(const FString& InTaskRunId, const TSharedPtr<FJsonObject>& Journal);
	void SetDebugEvent(
		const FBlueprintHelperDebugEntryEventInput& Input,
		bool bInAttachDebugCaseToFailure);
	void EnablePendingReviewNotification();

	const FBlueprintHelperTaskRuntimePostIoBatch& GetBatch() const;
	bool HasWork() const;

private:
	FBlueprintHelperTaskRuntimePostIoBatch Batch;
};
