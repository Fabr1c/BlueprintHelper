// BlueprintHelper Review panel command service implementation.

#include "UI/Review/BlueprintHelperReviewPanelCommandService.h"

#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/Review/Utils/BlueprintHelperReviewActionTargetUtils.h"
#include "UI/Review/BlueprintHelperReviewPanelStateService.h"

FBlueprintHelperReviewPanelCommandService::FBlueprintHelperReviewPanelCommandService(
	const FBlueprintHelperReviewActionService* InReviewActionService,
	const FBlueprintHelperReviewStoreService* InReviewStoreService)
	: ReviewActionService(InReviewActionService)
	, ReviewStoreService(InReviewStoreService)
{
}

FBlueprintHelperReviewCommandResult FBlueprintHelperReviewPanelCommandService::ExecuteActionIntent(
	const FBlueprintHelperReviewActionIntent& Intent,
	const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges,
	const FBlueprintHelperReviewRejectOptions& RejectOptions) const
{
	FBlueprintHelperReviewCommandResult CommandResult;
	FBlueprintHelperReviewVisibleChange Change;
	if (!FBlueprintHelperReviewPanelStateService::TryFindChangeByIntent(Intent, PendingChanges, Change))
	{
		CommandResult.ActionResult.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
		CommandResult.ActionResult.Message = TEXT("review_action_intent_target_not_found");
		return CommandResult;
	}

	if (Intent.Action == EBlueprintHelperReviewActionIntentKind::Accept)
	{
		CommandResult.ActionResult = AcceptVisibleChange(Change);
		NotifyStoreChangedIfSucceeded(CommandResult.ActionResult);
		return CommandResult;
	}

	if (Change.bIsAssetLifecycleRoot)
	{
		CommandResult.bCascade = true;
		CommandResult.CascadeActionResult = RejectLifecycleRootVisibleChange(Change, PendingChanges, RejectOptions);
		NotifyStoreChangedIfSucceeded(CommandResult.CascadeActionResult);
		return CommandResult;
	}

	CommandResult.ActionResult = RejectVisibleChange(Change, RejectOptions);
	NotifyStoreChangedIfSucceeded(CommandResult.ActionResult);
	return CommandResult;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewPanelCommandService::AcceptVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Change) const
{
	if (ReviewActionService)
	{
		return ReviewActionService->AcceptVisibleChange(Change);
	}

	FBlueprintHelperReviewActionResult Result;
	Result.bSucceeded = true;
	Result.TargetEvidenceId = Change.LatestEvidenceId;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::Accepted;
	Result.Message = TEXT("Accepted visible change.");
	return Result;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewPanelCommandService::RejectVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Change,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	if (ReviewActionService)
	{
		const TArray<FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch> Matches =
			FBlueprintHelperReviewActionTargetUtils::ResolvePersistedReviewTargetMatches(Change);
		if (Matches.Num() == 0)
		{
			FBlueprintHelperReviewActionResult Result;
			Result.TargetEvidenceId = Change.LatestEvidenceId;
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Result.Message = TEXT("persisted_review_targets_not_found");
			return Result;
		}

		FBlueprintHelperReviewActionResult LastResult;
		for (const FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch& Match : Matches)
		{
			LastResult = ReviewActionService->RejectReviewTargets(Match.ReviewRecordId, Match.TargetKeys, Options);
			if (!LastResult.bSucceeded)
			{
				return LastResult;
			}
		}
		LastResult.bSucceeded = true;
		LastResult.TargetEvidenceId = Change.LatestEvidenceId;
		LastResult.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
		LastResult.Message = TEXT("rejected");
		LastResult.bSupersededDataCompactionEligible = true;
		return LastResult;
	}

	FBlueprintHelperReviewActionResult Result;
	Result.bSucceeded = false;
	Result.TargetEvidenceId = Change.LatestEvidenceId;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
	Result.Message = TEXT("review_action_service_unavailable");
	return Result;
}

FBlueprintHelperReviewCascadeActionResult
FBlueprintHelperReviewPanelCommandService::RejectLifecycleRootVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Root,
	const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	if (ReviewActionService)
	{
		return ReviewActionService->RejectLifecycleRootVisibleChange(Root, PendingChanges, Options);
	}

	FBlueprintHelperReviewCascadeActionResult Result;
	Result.RootResult.bSucceeded = false;
	Result.RootResult.TargetEvidenceId = Root.LatestEvidenceId;
	Result.RootResult.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
	Result.RootResult.Message = TEXT("review_action_service_unavailable");
	return Result;
}

void FBlueprintHelperReviewPanelCommandService::NotifyStoreChangedIfSucceeded(
	const FBlueprintHelperReviewActionResult& Result) const
{
	if (ReviewStoreService && Result.bSucceeded)
	{
		ReviewStoreService->NotifyPendingReviewChanged();
	}
}

void FBlueprintHelperReviewPanelCommandService::NotifyStoreChangedIfSucceeded(
	const FBlueprintHelperReviewCascadeActionResult& Result) const
{
	if (ReviewStoreService && Result.RootResult.bSucceeded)
	{
		ReviewStoreService->NotifyPendingReviewChanged();
	}
}
