// BlueprintHelper Review baseline dirty classifier.

#include "Runtime/TaskRuntime/BlueprintHelperReviewBaselineDirtyClassifier.h"

static void BlueprintHelperReviewBaselineDirtyAddUnique(TArray<FString>& Values, const FString& Value)
{
	if (!Value.IsEmpty())
	{
		Values.AddUnique(Value);
	}
}

static void BlueprintHelperReviewBaselineDirtyApplyCommonFields(
	const FBlueprintHelperReviewBaselineDirtyClassifyRequest& Request,
	FBlueprintHelperReviewBaselineDirtyDecision& Decision)
{
	Decision.Code = TEXT("review_baseline_dirty_target_assets");
	Decision.Category = TEXT("runtime_state_error");
	Decision.Stage = TEXT("baseline_preflight");
	Decision.DirtyAssets = Request.DirtyAssets;
	Decision.EvidenceRefs = Request.DiagnosticEvidenceRefs;
	Decision.bBlocksExecution = true;
	for (const FString& ActiveReviewEvidenceRef : Request.ActiveReviewEvidenceRefs)
	{
		BlueprintHelperReviewBaselineDirtyAddUnique(
			Decision.EvidenceRefs,
			ActiveReviewEvidenceRef);
	}
	Decision.SequentialReviewSessionId = Request.ActiveSequentialReviewSessionId;
	Decision.SequentialReviewSessionArchiveSessionId = Request.ActiveSequentialReviewArchiveSessionId;
}

FBlueprintHelperReviewBaselineDirtyDecision FBlueprintHelperReviewBaselineDirtyClassifier::Classify(
	const FBlueprintHelperReviewBaselineDirtyClassifyRequest& Request) const
{
	FBlueprintHelperReviewBaselineDirtyDecision Decision;
	if (Request.DirtyAssets.Num() == 0)
	{
		Decision.State = EBlueprintHelperReviewBaselineDirtyState::Clean;
		Decision.Category = TEXT("runtime_state");
		Decision.Stage = TEXT("baseline_preflight");
		Decision.SafeNextAction = TEXT("continue");
		Decision.bBlocksExecution = false;
		return Decision;
	}

	BlueprintHelperReviewBaselineDirtyApplyCommonFields(Request, Decision);
	if (Request.bDirtyFromExternalUserChange ||
		Request.bSequentialReviewSessionHasExternalConflict)
	{
		Decision.State = EBlueprintHelperReviewBaselineDirtyState::DirtyExternalUserChange;
		Decision.SafeNextAction = TEXT("ask_user_to_resolve_external_dirty_assets_then_retry");
		BlueprintHelperReviewBaselineDirtyAddUnique(
			Decision.AllowedRecoveryActions,
			TEXT("user_resolve_then_retry"));
		return Decision;
	}

	if (Request.bDirtyFromActiveSequentialReviewSession &&
		Request.bSequentialReviewSessionHasUnresolvedFailedExecute)
	{
		Decision.State = EBlueprintHelperReviewBaselineDirtyState::DirtyWithUnresolvedFailedSessionExecute;
		Decision.SafeNextAction = TEXT("review.reject_or_restore_last_good_snapshot_then_retry");
		BlueprintHelperReviewBaselineDirtyAddUnique(
			Decision.AllowedRecoveryActions,
			TEXT("review.reject"));
		BlueprintHelperReviewBaselineDirtyAddUnique(
			Decision.AllowedRecoveryActions,
			TEXT("review.restore_last_good_snapshot"));
		return Decision;
	}

	if (Request.bDirtyFromActiveSequentialReviewSession &&
		Request.bSequentialReviewSessionHasLastGoodSnapshot &&
		!Request.ActiveSequentialReviewArchiveSessionId.IsEmpty())
	{
		Decision.State = EBlueprintHelperReviewBaselineDirtyState::DirtyWithActiveSequentialReviewSession;
		Decision.Category = TEXT("runtime_state");
		Decision.SafeNextAction = TEXT("continue_active_sequential_review_session");
		Decision.bBlocksExecution = false;
		BlueprintHelperReviewBaselineDirtyAddUnique(
			Decision.AllowedRecoveryActions,
			TEXT("task.preview"));
		BlueprintHelperReviewBaselineDirtyAddUnique(
			Decision.AllowedRecoveryActions,
			TEXT("task.execute"));
		BlueprintHelperReviewBaselineDirtyAddUnique(
			Decision.AllowedRecoveryActions,
			TEXT("review.accept"));
		BlueprintHelperReviewBaselineDirtyAddUnique(
			Decision.AllowedRecoveryActions,
			TEXT("review.reject"));
		return Decision;
	}

	if (Request.bDirtyFromFailedExecute)
	{
		Decision.State = EBlueprintHelperReviewBaselineDirtyState::DirtyAfterFailedExecute;
		Decision.SafeNextAction = TEXT("reject_or_close_without_save_agent_owned_changes_then_retry");
		BlueprintHelperReviewBaselineDirtyAddUnique(
			Decision.AllowedRecoveryActions,
			TEXT("review.reject"));
		if (!Request.bEditorSessionOwnershipKnown || Request.bEditorSessionAgentOwned)
		{
			BlueprintHelperReviewBaselineDirtyAddUnique(
				Decision.AllowedRecoveryActions,
				TEXT("editor.close_without_save_when_agent_owned"));
		}
		BlueprintHelperReviewBaselineDirtyAddUnique(
			Decision.RiskyRecoveryActions,
			TEXT("user_save_then_retry"));
		return Decision;
	}

	if (Request.ActiveReviewEvidenceRefs.Num() > 0)
	{
		Decision.State = EBlueprintHelperReviewBaselineDirtyState::DirtyWithOpenReview;
		Decision.SafeNextAction = TEXT("review.reject_or_accept_pending_changes_then_retry");
		BlueprintHelperReviewBaselineDirtyAddUnique(
			Decision.AllowedRecoveryActions,
			TEXT("review.reject"));
		BlueprintHelperReviewBaselineDirtyAddUnique(
			Decision.AllowedRecoveryActions,
			TEXT("review.accept"));
		return Decision;
	}

	if (Request.bDirtyPreexistingBeforeRun)
	{
		Decision.State = EBlueprintHelperReviewBaselineDirtyState::DirtyPreexisting;
		Decision.SafeNextAction = TEXT("ask_user_to_save_or_revert_preexisting_dirty_assets_then_retry");
		BlueprintHelperReviewBaselineDirtyAddUnique(
			Decision.AllowedRecoveryActions,
			TEXT("user_save_then_retry"));
		BlueprintHelperReviewBaselineDirtyAddUnique(
			Decision.AllowedRecoveryActions,
			TEXT("user_revert_then_retry"));
		return Decision;
	}

	Decision.State = EBlueprintHelperReviewBaselineDirtyState::UnknownDirtyOrigin;
	Decision.SafeNextAction = TEXT("ask_user_to_inspect_dirty_assets_before_retry");
	BlueprintHelperReviewBaselineDirtyAddUnique(
		Decision.AllowedRecoveryActions,
		TEXT("user_resolve_then_retry"));
	return Decision;
}
