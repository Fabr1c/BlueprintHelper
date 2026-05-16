// BlueprintHelper Review panel command service implementation.

#include "UI/Review/BlueprintHelperReviewPanelCommandService.h"

FBlueprintHelperReviewPanelCommandService::FBlueprintHelperReviewPanelCommandService(
	const FBlueprintHelperReviewActionService* InReviewActionService)
	: ReviewActionService(InReviewActionService)
{
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
	Result.TargetTransactionId = Change.LatestTransactionId;
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
		return ReviewActionService->RejectVisibleChange(Change, Options);
	}

	FBlueprintHelperReviewActionResult Result;
	Result.bSucceeded = false;
	Result.TargetTransactionId = Change.LatestTransactionId;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
	Result.RollbackMode = TEXT("archive_baseline");
	Result.Message = TEXT("Reject requires archive-baseline rollback service.");
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
	Result.RootResult.TargetTransactionId = Root.LatestTransactionId;
	Result.RootResult.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
	Result.RootResult.RollbackMode = TEXT("archive_baseline");
	Result.RootResult.Message = TEXT("Reject requires archive-baseline rollback service.");
	return Result;
}
