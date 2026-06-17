// BlueprintHelper Review reject mutation presentation model implementation.

#include "UI/Review/BlueprintHelperReviewRejectMutationPresenter.h"

#include "UI/Review/BlueprintHelperReviewDebugBundleService.h"

FBlueprintHelperReviewRejectMutationPresentation
FBlueprintHelperReviewRejectMutationPresenter::BuildMissingChange(
	const FString& ChangeId,
	bool bIsBatchChange)
{
	FBlueprintHelperReviewRejectMutationPresentation Presentation;
	Presentation.DebugMessages.Add(FString::Printf(
		TEXT("Reject mutation skipped id=%s reason=change_missing"),
		*ChangeId));
	if (!bIsBatchChange)
	{
		Presentation.bShowNotification = true;
		Presentation.NotificationKey = BuildRejectNotificationKey(ChangeId);
		Presentation.NotificationText = FString::Printf(
			TEXT("Reject failed: Review change no longer exists (%s)."),
			*ChangeId);
		Presentation.NotificationState = EBlueprintHelperReviewActionNotificationState::Fail;
	}
	Presentation.bHasBatchResult = true;
	Presentation.bBatchSucceeded = false;
	Presentation.FeedbackStage = TEXT("failure_feedback_shown");
	Presentation.FeedbackDetail = TEXT("change_missing");
	return Presentation;
}

FBlueprintHelperReviewRejectMutationPresentation
FBlueprintHelperReviewRejectMutationPresenter::BuildLifecycleRootResult(
	const FString& ChangeId,
	const FBlueprintHelperReviewVisibleChange& Change,
	const FBlueprintHelperReviewCascadeActionResult& Result,
	const FString& NotificationLabel,
	bool bIsBatchChange)
{
	FBlueprintHelperReviewRejectMutationPresentation Presentation;
	if (Result.RootResult.bSucceeded)
	{
		Presentation.DebugMessages.Add(FString::Printf(
			TEXT("Reject lifecycle root id=%s result=waiting_for_store_refresh"),
			*Change.ChangeId));
	}
	else
	{
		Presentation.bCancelWaitingForStoreRefresh = true;
		Presentation.bSetPresenterError = true;
		Presentation.PresenterErrorStatus = Result.RootResult.NewStatus;
		Presentation.PresenterErrorMessage = Result.RootResult.Message;
		Presentation.bRefreshAfterFailure = true;
		Presentation.RefreshReason = TEXT("reject_lifecycle_root_failed");
	}

	Presentation.DebugMessages.Add(FString::Printf(
		TEXT("Reject lifecycle root id=%s success=%d removedChildren=%d status=%s message=\"%s\""),
		*Change.ChangeId,
		Result.RootResult.bSucceeded ? 1 : 0,
		Result.RemovedChildChangeIds.Num(),
		BlueprintHelperReviewChangeStatusToString(Result.RootResult.NewStatus),
		*Result.RootResult.Message));

	if (!bIsBatchChange)
	{
		Presentation.bShowNotification = true;
		Presentation.NotificationKey = BuildRejectNotificationKey(ChangeId);
		Presentation.NotificationText = Result.RootResult.bSucceeded
			? FString::Printf(TEXT("Rejected: %s"), *NotificationLabel)
			: BuildFriendlyActionMessage(TEXT("Reject failed"), Result.RootResult.Message);
		Presentation.NotificationState = Result.RootResult.bSucceeded
			? EBlueprintHelperReviewActionNotificationState::Success
			: EBlueprintHelperReviewActionNotificationState::Fail;
	}

	Presentation.bHasBatchResult = true;
	Presentation.bBatchSucceeded = Result.RootResult.bSucceeded;
	Presentation.FeedbackStage = Result.RootResult.bSucceeded
		? TEXT("success_feedback_shown")
		: TEXT("failure_feedback_shown");
	Presentation.FeedbackDetail = Result.RootResult.Message;
	return Presentation;
}

FBlueprintHelperReviewRejectMutationPresentation
FBlueprintHelperReviewRejectMutationPresenter::BuildSingleResult(
	const FString& ChangeId,
	const FBlueprintHelperReviewVisibleChange& Change,
	const FBlueprintHelperReviewActionResult& Result,
	const FString& NotificationLabel,
	bool bIsBatchChange,
	const FString& DebugBundleSessionId,
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& SelectedChange)
{
	FBlueprintHelperReviewRejectMutationPresentation Presentation;
	if (Result.bSucceeded)
	{
		Presentation.DebugMessages.Add(FString::Printf(
			TEXT("Reject change id=%s result=waiting_for_store_refresh"),
			*Change.ChangeId));
	}
	else
	{
		Presentation.bCancelWaitingForStoreRefresh = true;
		Presentation.bSetPresenterError = true;
		Presentation.PresenterErrorStatus = Result.NewStatus;
		Presentation.PresenterErrorMessage = Result.Message;
		Presentation.bRefreshAfterFailure = true;
		Presentation.RefreshReason = TEXT("reject_change_failed");
	}

	Presentation.DebugMessages.Add(FString::Printf(
		TEXT("Reject change id=%s success=%d status=%s message=\"%s\""),
		*Change.ChangeId,
		Result.bSucceeded ? 1 : 0,
		BlueprintHelperReviewChangeStatusToString(Result.NewStatus),
		*Result.Message));

	if (!Result.bSucceeded && !Result.HashGuardTargetKey.IsEmpty())
	{
		Presentation.DebugMessages.Add(FString::Printf(
			TEXT("Reject hash guard target=%s expected=%s current=%s"),
			*Result.HashGuardTargetKey,
			*Result.HashGuardExpectedHash,
			*Result.HashGuardCurrentHash));
		Presentation.DebugBundleEvents.Add(
			FBlueprintHelperReviewDebugBundleService::BuildActionHashGuardEvent(
				DebugBundleSessionId,
				SelectedChange,
				SelectedChange.IsValid() ? SelectedChange->AssetPath : FString(),
				Result.HashGuardTargetKey,
				Result.HashGuardExpectedHash,
				Result.HashGuardCurrentHash,
				Result.HashGuardCurrentSnapshotJson,
				Result.HashGuardRecordedAfterSnapshotJson));
	}

	if (!bIsBatchChange)
	{
		Presentation.bShowNotification = true;
		Presentation.NotificationKey = BuildRejectNotificationKey(ChangeId);
		Presentation.NotificationText = Result.bSucceeded
			? FString::Printf(TEXT("Rejected: %s"), *NotificationLabel)
			: BuildFriendlyActionMessage(TEXT("Reject failed"), Result.Message);
		Presentation.NotificationState = Result.bSucceeded
			? EBlueprintHelperReviewActionNotificationState::Success
			: EBlueprintHelperReviewActionNotificationState::Fail;
	}

	Presentation.bHasBatchResult = true;
	Presentation.bBatchSucceeded = Result.bSucceeded;
	Presentation.FeedbackStage = Result.bSucceeded
		? TEXT("success_feedback_shown")
		: TEXT("failure_feedback_shown");
	Presentation.FeedbackDetail = Result.Message;
	return Presentation;
}

FString FBlueprintHelperReviewRejectMutationPresenter::BuildFriendlyActionMessage(
	const FString& Prefix,
	const FString& Detail)
{
	if (Detail.StartsWith(TEXT("struct_field_last_row_cannot_remove:")))
	{
		const FString FieldName = Detail.Mid(
			FString(TEXT("struct_field_last_row_cannot_remove:")).Len());
		return FString::Printf(
			TEXT("%s: cannot remove the last struct field (%s). Reject the whole struct asset instead."),
			*Prefix,
			*FieldName);
	}
	if (Detail.IsEmpty())
	{
		return Prefix;
	}
	return FString::Printf(TEXT("%s: %s"), *Prefix, *Detail);
}

FString FBlueprintHelperReviewRejectMutationPresenter::BuildRejectNotificationKey(
	const FString& ChangeId)
{
	return TEXT("reject:") + ChangeId;
}
