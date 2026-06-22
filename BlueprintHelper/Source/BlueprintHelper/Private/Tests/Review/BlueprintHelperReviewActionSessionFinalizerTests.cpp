#include "Systems/Review/BlueprintHelperReviewActionSessionFinalizer.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewActionSessionFinalizerMarksPartialFailureTest,
	"BlueprintHelper.Review.Action.SessionFinalizer.MarksPartialFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewActionSessionFinalizerMarksPartialFailureTest::RunTest(const FString&)
{
	FBlueprintHelperReviewActionResult ActionResult;
	ActionResult.bSucceeded = true;
	ActionResult.bReviewActionCommitted = true;
	ActionResult.Message = TEXT("accepted");

	FBlueprintHelperSequentialReviewSessionCloseResult CloseResult;
	CloseResult.bSucceeded = false;
	CloseResult.ErrorCode = TEXT("sequential_review_session_close_save_failed");
	CloseResult.ErrorMessage = TEXT("save failed");
	CloseResult.AffectedSessionIds.Add(TEXT("seq_review_test"));

	FBlueprintHelperReviewActionSessionFinalizer::ApplyCloseResult(CloseResult, ActionResult);

	TestFalse(TEXT("action result is partial failure"), ActionResult.bSucceeded);
	TestTrue(TEXT("review action committed"), ActionResult.bReviewActionCommitted);
	TestFalse(TEXT("session close not committed"), ActionResult.bSessionCloseCommitted);
	TestEqual(
		TEXT("safe next action"),
		ActionResult.SafeNextAction,
		FString(TEXT("retry_close_sequential_review_session_or_query_debug_bundle")));
	TestEqual(
		TEXT("error code"),
		ActionResult.SessionCloseErrorCode,
		FString(TEXT("sequential_review_session_close_save_failed")));
	TestEqual(TEXT("error message"), ActionResult.SessionCloseErrorMessage, FString(TEXT("save failed")));
	TestEqual(
		TEXT("partial failure message"),
		ActionResult.Message,
		FString(TEXT("terminal_action_committed_with_session_close_failed")));
	TestEqual(TEXT("affected session count"), ActionResult.AffectedSequentialReviewSessionIds.Num(), 1);
	TestEqual(
		TEXT("affected session id"),
		ActionResult.AffectedSequentialReviewSessionIds[0],
		FString(TEXT("seq_review_test")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewActionSessionFinalizerKeepsSuccessCommittedTest,
	"BlueprintHelper.Review.Action.SessionFinalizer.KeepsSuccessCommitted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewActionSessionFinalizerKeepsSuccessCommittedTest::RunTest(const FString&)
{
	FBlueprintHelperReviewActionResult ActionResult;
	ActionResult.bSucceeded = true;
	ActionResult.bReviewActionCommitted = true;
	ActionResult.Message = TEXT("accepted");

	FBlueprintHelperSequentialReviewSessionCloseResult CloseResult;
	CloseResult.bSucceeded = true;
	CloseResult.bMatched = true;
	CloseResult.AffectedSessionIds.Add(TEXT("seq_review_success"));

	FBlueprintHelperReviewActionSessionFinalizer::ApplyCloseResult(CloseResult, ActionResult);

	TestTrue(TEXT("success remains success"), ActionResult.bSucceeded);
	TestTrue(TEXT("review action committed"), ActionResult.bReviewActionCommitted);
	TestTrue(TEXT("session close committed"), ActionResult.bSessionCloseCommitted);
	TestTrue(TEXT("safe next action stays empty"), ActionResult.SafeNextAction.IsEmpty());
	TestTrue(TEXT("error code stays empty"), ActionResult.SessionCloseErrorCode.IsEmpty());
	TestEqual(TEXT("message unchanged"), ActionResult.Message, FString(TEXT("accepted")));
	TestEqual(TEXT("affected session count"), ActionResult.AffectedSequentialReviewSessionIds.Num(), 1);
	TestEqual(
		TEXT("affected session id"),
		ActionResult.AffectedSequentialReviewSessionIds[0],
		FString(TEXT("seq_review_success")));
	return true;
}

#endif
