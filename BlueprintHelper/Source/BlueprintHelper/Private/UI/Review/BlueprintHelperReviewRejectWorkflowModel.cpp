// BlueprintHelper Review reject workflow state model implementation.

#include "UI/Review/BlueprintHelperReviewRejectWorkflowModel.h"

void FBlueprintHelperReviewRejectWorkflowModel::EnqueueReject(const FString& ChangeId)
{
	if (!ChangeId.IsEmpty())
	{
		PendingRejectChangeIds.Add(ChangeId);
	}
}

bool FBlueprintHelperReviewRejectWorkflowModel::IsPrepareActive() const
{
	return bPrepareActive;
}

bool FBlueprintHelperReviewRejectWorkflowModel::TryPopNextPending(FString& OutChangeId)
{
	while (PendingRejectChangeIds.Num() > 0)
	{
		OutChangeId = PendingRejectChangeIds[0];
		PendingRejectChangeIds.RemoveAt(0);
		if (!OutChangeId.IsEmpty())
		{
			return true;
		}
	}
	OutChangeId.Reset();
	return false;
}

bool FBlueprintHelperReviewRejectWorkflowModel::HasPendingRejects() const
{
	return PendingRejectChangeIds.Num() > 0;
}

void FBlueprintHelperReviewRejectWorkflowModel::MarkPrepareStarted(const FString& ChangeId)
{
	ActiveRejectChangeId = ChangeId;
	bPrepareActive = true;
}

void FBlueprintHelperReviewRejectWorkflowModel::MarkPrepareFinished(
	const FString& ChangeId,
	const FBlueprintHelperReviewRejectOptions& PreparedOptions)
{
	bPrepareActive = false;
	PreparedRejectOptionsByChangeId.Add(ChangeId, PreparedOptions);
}

const FBlueprintHelperReviewRejectOptions* FBlueprintHelperReviewRejectWorkflowModel::FindPreparedOptions(
	const FString& ChangeId) const
{
	return PreparedRejectOptionsByChangeId.Find(ChangeId);
}

void FBlueprintHelperReviewRejectWorkflowModel::FinishReject(const FString& ChangeId)
{
	PreparedRejectOptionsByChangeId.Remove(ChangeId);
	if (ActiveRejectChangeId == ChangeId)
	{
		ActiveRejectChangeId.Reset();
	}
	bPrepareActive = false;
}
