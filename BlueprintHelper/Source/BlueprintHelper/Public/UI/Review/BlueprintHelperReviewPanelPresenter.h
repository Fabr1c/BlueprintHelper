// BlueprintHelper Review panel presenter.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewPanelData.h"
#include "UI/Review/BlueprintHelperReviewPanelCommandService.h"

class FBlueprintHelperReviewStoreService;

enum class EBlueprintHelperReviewPanelVisualEventType : uint8
{
	AcceptVisibleChange,
	RejectVisibleChange,
	RejectLifecycleRootVisibleChange
};

enum class EBlueprintHelperReviewPanelPresenterEventType : uint8
{
	ActionResult,
	CascadeActionResult
};

struct FBlueprintHelperReviewPanelVisualEvent
{
	EBlueprintHelperReviewPanelVisualEventType Type =
		EBlueprintHelperReviewPanelVisualEventType::AcceptVisibleChange;
	FBlueprintHelperReviewVisibleChange Change;
	FBlueprintHelperReviewPanelDataSnapshot DataSnapshot;
	FBlueprintHelperReviewRejectOptions RejectOptions;

	static FBlueprintHelperReviewPanelVisualEvent AcceptVisibleChange(
		const FBlueprintHelperReviewVisibleChange& InChange);
	static FBlueprintHelperReviewPanelVisualEvent RejectVisibleChange(
		const FBlueprintHelperReviewVisibleChange& InChange,
		const FBlueprintHelperReviewRejectOptions& InRejectOptions);
	static FBlueprintHelperReviewPanelVisualEvent RejectLifecycleRootVisibleChange(
		const FBlueprintHelperReviewVisibleChange& InChange,
		const FBlueprintHelperReviewPanelDataSnapshot& InDataSnapshot,
		const FBlueprintHelperReviewRejectOptions& InRejectOptions);
};

struct FBlueprintHelperReviewPanelPresenterEvent
{
	EBlueprintHelperReviewPanelPresenterEventType Type =
		EBlueprintHelperReviewPanelPresenterEventType::ActionResult;
	FBlueprintHelperReviewActionResult ActionResult;
	FBlueprintHelperReviewCascadeActionResult CascadeActionResult;

	static FBlueprintHelperReviewPanelPresenterEvent FromActionResult(
		const FBlueprintHelperReviewActionResult& InResult);
	static FBlueprintHelperReviewPanelPresenterEvent FromCascadeActionResult(
		const FBlueprintHelperReviewCascadeActionResult& InResult);
};

class FBlueprintHelperReviewPanelPresenter
{
public:
	FBlueprintHelperReviewPanelPresenter(
		const FBlueprintHelperReviewStoreService* InReviewStoreService,
	const FBlueprintHelperReviewActionService* InReviewActionService);

	TArray<FBlueprintHelperReviewVisibleChange> LoadPendingVisibleChanges() const;
	FDelegateHandle AddPendingReviewChangedEventHandler(
		const FBlueprintHelperReviewStoreChangedMulticast::FDelegate& InDelegate) const;
	void RemovePendingReviewChangedEventHandler(FDelegateHandle& InHandle) const;
	FBlueprintHelperReviewPanelPresenterEvent HandleVisualEvent(
		const FBlueprintHelperReviewPanelVisualEvent& Event) const;
	FBlueprintHelperReviewPanelPresenterEvent HandleActionIntent(
		const FBlueprintHelperReviewActionIntent& Intent,
		const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges,
		const FBlueprintHelperReviewRejectOptions& RejectOptions = FBlueprintHelperReviewRejectOptions()) const;
	FBlueprintHelperReviewCommandBatchResult AcceptVisibleChangesBatch(
		const TArray<FBlueprintHelperReviewVisibleChange>& Changes) const;
	FBlueprintHelperReviewCommandBatchResult RejectVisibleChangesBatch(
		const TArray<FBlueprintHelperReviewVisibleChange>& Changes,
		const FBlueprintHelperReviewRejectOptions& RejectOptions = FBlueprintHelperReviewRejectOptions()) const;
	FBlueprintHelperReviewCommandBatchResult AcceptPendingVisibleChangesForAsset(
		const FString& AssetPath) const;
	FBlueprintHelperReviewCommandBatchResult RejectPendingVisibleChangesForAsset(
		const FString& AssetPath,
		const FBlueprintHelperReviewRejectOptions& RejectOptions = FBlueprintHelperReviewRejectOptions()) const;

private:
	FBlueprintHelperReviewPanelPresenterEvent HandleAcceptVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Change) const;
	FBlueprintHelperReviewPanelPresenterEvent HandleRejectVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Change,
		const FBlueprintHelperReviewRejectOptions& Options) const;
	FBlueprintHelperReviewPanelPresenterEvent HandleRejectLifecycleRootVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Change,
		const FBlueprintHelperReviewPanelDataSnapshot& DataSnapshot,
		const FBlueprintHelperReviewRejectOptions& Options) const;

	const FBlueprintHelperReviewStoreService* ReviewStoreService = nullptr;
	FBlueprintHelperReviewPanelCommandService CommandService;
};
