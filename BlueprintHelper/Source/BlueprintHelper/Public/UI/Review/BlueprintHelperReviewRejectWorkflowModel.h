// BlueprintHelper Review reject workflow state model.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewRejectWorkflowModel
{
public:
	void EnqueueReject(const FString& ChangeId);
	bool IsPrepareActive() const;
	bool TryPopNextPending(FString& OutChangeId);
	bool HasPendingRejects() const;
	void MarkPrepareStarted(const FString& ChangeId);
	void MarkPrepareFinished(const FString& ChangeId, const FBlueprintHelperReviewRejectOptions& PreparedOptions);
	const FBlueprintHelperReviewRejectOptions* FindPreparedOptions(const FString& ChangeId) const;
	void FinishReject(const FString& ChangeId);

private:
	TArray<FString> PendingRejectChangeIds;
	TMap<FString, FBlueprintHelperReviewRejectOptions> PreparedRejectOptionsByChangeId;
	FString ActiveRejectChangeId;
	bool bPrepareActive = false;
};
