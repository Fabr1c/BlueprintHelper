// BlueprintHelper Review reject mutation presentation application service implementation.

#include "UI/Review/BlueprintHelperReviewRejectMutationApplicationService.h"

#include "UI/Review/BlueprintHelperReviewRejectMutationPresenter.h"

void FBlueprintHelperReviewRejectMutationApplicationService::ApplyPresentation(
	const FBlueprintHelperReviewRejectMutationPresentation& Presentation,
	bool bHasChangeItem,
	const FBlueprintHelperReviewRejectMutationApplicationCallbacks& Callbacks)
{
	if (Presentation.bCancelWaitingForStoreRefresh && Callbacks.CancelWaitingForStoreRefresh)
	{
		Callbacks.CancelWaitingForStoreRefresh();
	}

	if (Presentation.bSetPresenterError && bHasChangeItem && Callbacks.SetPresenterError)
	{
		Callbacks.SetPresenterError(
			Presentation.PresenterErrorStatus,
			Presentation.PresenterErrorMessage);
	}

	if (Presentation.bRefreshAfterFailure && bHasChangeItem && Callbacks.RefreshAfterFailure)
	{
		Callbacks.RefreshAfterFailure(Presentation.RefreshReason);
	}

	if (Callbacks.AddDebugMessage)
	{
		for (const FString& DebugMessage : Presentation.DebugMessages)
		{
			Callbacks.AddDebugMessage(DebugMessage);
		}
	}

	if (Callbacks.AppendDebugBundleEvent)
	{
		for (const TSharedRef<FJsonObject>& DebugBundleEvent : Presentation.DebugBundleEvents)
		{
			Callbacks.AppendDebugBundleEvent(DebugBundleEvent);
		}
	}

	if (Presentation.bShowNotification && Callbacks.ShowNotification)
	{
		Callbacks.ShowNotification(
			Presentation.NotificationKey,
			Presentation.NotificationText,
			Presentation.NotificationState,
			Presentation.bNotificationExpires,
			Presentation.bNotificationUsesThrobber);
	}

	if (!Presentation.FeedbackStage.IsEmpty() && Callbacks.RecordFeedbackStage)
	{
		Callbacks.RecordFeedbackStage(
			Presentation.FeedbackStage,
			Presentation.FeedbackDetail);
	}

	if (Presentation.bHasBatchResult && Callbacks.RecordBatchResult)
	{
		Callbacks.RecordBatchResult(Presentation.bBatchSucceeded);
	}
}

void FBlueprintHelperReviewRejectMutationApplicationService::FinishMutation(
	const FBlueprintHelperReviewRejectMutationApplicationCallbacks& Callbacks)
{
	if (Callbacks.RecordFinishedStage)
	{
		Callbacks.RecordFinishedStage();
	}
	if (Callbacks.ClearTransientActionState)
	{
		Callbacks.ClearTransientActionState();
	}
	if (Callbacks.FinishWorkflow)
	{
		Callbacks.FinishWorkflow();
	}
	const bool bWaitingForStoreRefresh = Callbacks.IsWaitingForStoreRefresh
		? Callbacks.IsWaitingForStoreRefresh()
		: false;
	if (!bWaitingForStoreRefresh && Callbacks.CompleteTiming)
	{
		Callbacks.CompleteTiming();
	}
}
