// BlueprintHelper Review reject mutation application service tests.

#include "UI/Review/BlueprintHelperReviewRejectMutationApplicationService.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "UI/Review/BlueprintHelperReviewRejectMutationPresenter.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectMutationApplicationService_AppliesPresentationCallbacks,
	"BlueprintHelper.Review.Panel.RejectMutationApplicationService.AppliesPresentationCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewRejectMutationApplicationService_AppliesPresentationCallbacks::RunTest(const FString&)
{
	FBlueprintHelperReviewRejectMutationPresentation Presentation;
	Presentation.bCancelWaitingForStoreRefresh = true;
	Presentation.bSetPresenterError = true;
	Presentation.PresenterErrorStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
	Presentation.PresenterErrorMessage = TEXT("needs_action");
	Presentation.bRefreshAfterFailure = true;
	Presentation.RefreshReason = TEXT("reject_failed");
	Presentation.DebugMessages.Add(TEXT("debug_message"));
	Presentation.DebugBundleEvents.Add(MakeShared<FJsonObject>());
	Presentation.bShowNotification = true;
	Presentation.NotificationKey = TEXT("reject:change");
	Presentation.NotificationText = TEXT("Reject failed");
	Presentation.NotificationState = EBlueprintHelperReviewActionNotificationState::Fail;
	Presentation.bHasBatchResult = true;
	Presentation.bBatchSucceeded = false;
	Presentation.FeedbackStage = TEXT("failure_feedback_shown");
	Presentation.FeedbackDetail = TEXT("detail");

	bool bCancelled = false;
	bool bPresenterErrorSet = false;
	bool bRefreshed = false;
	int32 DebugMessageCount = 0;
	int32 DebugBundleEventCount = 0;
	bool bNotificationShown = false;
	bool bFeedbackRecorded = false;
	bool bBatchRecorded = false;

	FBlueprintHelperReviewRejectMutationApplicationCallbacks Callbacks;
	Callbacks.CancelWaitingForStoreRefresh = [&bCancelled]()
	{
		bCancelled = true;
	};
	Callbacks.SetPresenterError = [&bPresenterErrorSet](
		EBlueprintHelperReviewChangeStatus,
		const FString&)
	{
		bPresenterErrorSet = true;
	};
	Callbacks.RefreshAfterFailure = [&bRefreshed](const FString&)
	{
		bRefreshed = true;
	};
	Callbacks.AddDebugMessage = [&DebugMessageCount](const FString&)
	{
		++DebugMessageCount;
	};
	Callbacks.AppendDebugBundleEvent = [&DebugBundleEventCount](const TSharedRef<FJsonObject>&)
	{
		++DebugBundleEventCount;
	};
	Callbacks.ShowNotification = [&bNotificationShown](
		const FString&,
		const FString&,
		EBlueprintHelperReviewActionNotificationState,
		bool,
		bool)
	{
		bNotificationShown = true;
	};
	Callbacks.RecordFeedbackStage = [&bFeedbackRecorded](const FString&, const FString&)
	{
		bFeedbackRecorded = true;
	};
	Callbacks.RecordBatchResult = [&bBatchRecorded](bool)
	{
		bBatchRecorded = true;
	};

	FBlueprintHelperReviewRejectMutationApplicationService::ApplyPresentation(
		Presentation,
		true,
		Callbacks);

	TestTrue(TEXT("cancel waiting"), bCancelled);
	TestTrue(TEXT("presenter error"), bPresenterErrorSet);
	TestTrue(TEXT("refresh after failure"), bRefreshed);
	TestEqual(TEXT("debug messages"), DebugMessageCount, 1);
	TestEqual(TEXT("debug bundle events"), DebugBundleEventCount, 1);
	TestTrue(TEXT("notification"), bNotificationShown);
	TestTrue(TEXT("feedback"), bFeedbackRecorded);
	TestTrue(TEXT("batch result"), bBatchRecorded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectMutationApplicationService_FinishesMutationCallbacks,
	"BlueprintHelper.Review.Panel.RejectMutationApplicationService.FinishesMutationCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewRejectMutationApplicationService_FinishesMutationCallbacks::RunTest(const FString&)
{
	bool bFinishedStageRecorded = false;
	bool bTransientCleared = false;
	bool bWorkflowFinished = false;
	bool bTimingCompleted = false;

	FBlueprintHelperReviewRejectMutationApplicationCallbacks Callbacks;
	Callbacks.RecordFinishedStage = [&bFinishedStageRecorded]()
	{
		bFinishedStageRecorded = true;
	};
	Callbacks.ClearTransientActionState = [&bTransientCleared]()
	{
		bTransientCleared = true;
	};
	Callbacks.FinishWorkflow = [&bWorkflowFinished]()
	{
		bWorkflowFinished = true;
	};
	Callbacks.IsWaitingForStoreRefresh = []()
	{
		return false;
	};
	Callbacks.CompleteTiming = [&bTimingCompleted]()
	{
		bTimingCompleted = true;
	};

	FBlueprintHelperReviewRejectMutationApplicationService::FinishMutation(Callbacks);

	TestTrue(TEXT("finished stage"), bFinishedStageRecorded);
	TestTrue(TEXT("transient cleared"), bTransientCleared);
	TestTrue(TEXT("workflow finished"), bWorkflowFinished);
	TestTrue(TEXT("timing completed"), bTimingCompleted);
	return true;
}

#endif
