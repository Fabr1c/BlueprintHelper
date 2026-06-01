// BlueprintHelper Review panel command service.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "Systems/Review/BlueprintHelperReviewPendingIndex.h"
#include "UI/Review/BlueprintHelperReviewPanelData.h"

class FBlueprintHelperReviewStoreService;

struct FBlueprintHelperReviewCommandResult
{
	bool bCascade = false;
	FBlueprintHelperReviewActionResult ActionResult;
	FBlueprintHelperReviewCascadeActionResult CascadeActionResult;
};

struct FBlueprintHelperReviewCommandBatchResult
{
	FBlueprintHelperReviewBatchActionResult BatchActionResult;
	FBlueprintHelperReviewStoreChangedEvent StoreChangedEvent;
};

class FBlueprintHelperReviewPanelCommandService
{
public:
	explicit FBlueprintHelperReviewPanelCommandService(
		const FBlueprintHelperReviewActionService* InReviewActionService,
		const FBlueprintHelperReviewStoreService* InReviewStoreService = nullptr);

	FBlueprintHelperReviewCommandResult ExecuteActionIntent(
		const FBlueprintHelperReviewActionIntent& Intent,
		const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges,
		const FBlueprintHelperReviewRejectOptions& RejectOptions = FBlueprintHelperReviewRejectOptions()) const;

	FBlueprintHelperReviewActionResult AcceptVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Change) const;

	FBlueprintHelperReviewActionResult RejectVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Change,
		const FBlueprintHelperReviewRejectOptions& Options) const;

	FBlueprintHelperReviewCascadeActionResult RejectLifecycleRootVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Root,
		const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges,
		const FBlueprintHelperReviewRejectOptions& Options) const;

	FBlueprintHelperReviewCommandBatchResult AcceptVisibleChangesBatch(
		const TArray<FBlueprintHelperReviewVisibleChange>& Changes) const;

	FBlueprintHelperReviewCommandBatchResult RejectVisibleChangesBatch(
		const TArray<FBlueprintHelperReviewVisibleChange>& Changes,
		const FBlueprintHelperReviewRejectOptions& Options) const;

	FBlueprintHelperReviewCommandBatchResult AcceptPendingVisibleChangesForAsset(
		const FString& AssetPath) const;

	FBlueprintHelperReviewCommandBatchResult RejectPendingVisibleChangesForAsset(
		const FString& AssetPath,
		const FBlueprintHelperReviewRejectOptions& Options) const;

private:
	void NotifyStoreChangedIfSucceeded(
		const FBlueprintHelperReviewActionResult& Result,
		const FBlueprintHelperReviewStoreChangedEvent& Event) const;
	void NotifyStoreChangedIfSucceeded(
		const FBlueprintHelperReviewCascadeActionResult& Result,
		const FBlueprintHelperReviewStoreChangedEvent& Event) const;
	void NotifyStoreChangedIfSucceeded(const FBlueprintHelperReviewCommandBatchResult& Result) const;

	const FBlueprintHelperReviewActionService* ReviewActionService = nullptr;
	const FBlueprintHelperReviewStoreService* ReviewStoreService = nullptr;
};
