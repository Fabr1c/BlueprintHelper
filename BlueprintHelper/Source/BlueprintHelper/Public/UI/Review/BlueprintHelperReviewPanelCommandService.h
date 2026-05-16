// BlueprintHelper Review panel command service.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"

class FBlueprintHelperReviewPanelCommandService
{
public:
	explicit FBlueprintHelperReviewPanelCommandService(
		const FBlueprintHelperReviewActionService* InReviewActionService);

	FBlueprintHelperReviewActionResult AcceptVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Change) const;

	FBlueprintHelperReviewActionResult RejectVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Change,
		const FBlueprintHelperReviewRejectOptions& Options) const;

	FBlueprintHelperReviewCascadeActionResult RejectLifecycleRootVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Root,
		const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges,
		const FBlueprintHelperReviewRejectOptions& Options) const;

private:
	const FBlueprintHelperReviewActionService* ReviewActionService = nullptr;
};
