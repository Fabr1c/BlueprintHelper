// BlueprintHelper Review action sequential session finalizer implementation.

#include "Systems/Review/BlueprintHelperReviewActionSessionFinalizer.h"

FBlueprintHelperSequentialReviewSessionCloseResult
FBlueprintHelperReviewActionSessionFinalizer::CloseReviewRecordSessions(
	const FString& ReviewRecordId,
	EBlueprintHelperSequentialReviewSessionStatus FinalStatus) const
{
	return FBlueprintHelperSequentialReviewSessionService().CloseSessionsForReviewRecord(
		ReviewRecordId,
		FinalStatus);
}

void FBlueprintHelperReviewActionSessionFinalizer::ApplyCloseResult(
	const FBlueprintHelperSequentialReviewSessionCloseResult& CloseResult,
	FBlueprintHelperReviewActionResult& InOutActionResult)
{
	InOutActionResult.AffectedSequentialReviewSessionIds = CloseResult.AffectedSessionIds;
	if (CloseResult.bSucceeded)
	{
		InOutActionResult.bSessionCloseCommitted = true;
		return;
	}

	InOutActionResult.bSessionCloseCommitted = false;
	InOutActionResult.SessionCloseErrorCode = CloseResult.ErrorCode.IsEmpty()
		? TEXT("sequential_review_session_close_failed")
		: CloseResult.ErrorCode;
	InOutActionResult.SessionCloseErrorMessage = CloseResult.ErrorMessage;
	InOutActionResult.SafeNextAction = TEXT("retry_close_sequential_review_session_or_query_debug_bundle");
	InOutActionResult.Message = TEXT("terminal_action_committed_with_session_close_failed");
	InOutActionResult.bSucceeded = false;
}
