// BlueprintHelper Review panel presenter implementation.

#include "UI/Review/BlueprintHelperReviewPanelPresenter.h"

#include "Systems/Review/BlueprintHelperReviewStoreService.h"

FBlueprintHelperReviewPanelVisualEvent FBlueprintHelperReviewPanelVisualEvent::AcceptVisibleChange(
	const FBlueprintHelperReviewVisibleChange& InChange)
{
	FBlueprintHelperReviewPanelVisualEvent Event;
	Event.Type = EBlueprintHelperReviewPanelVisualEventType::AcceptVisibleChange;
	Event.Change = InChange;
	return Event;
}

FBlueprintHelperReviewPanelVisualEvent FBlueprintHelperReviewPanelVisualEvent::RejectVisibleChange(
	const FBlueprintHelperReviewVisibleChange& InChange,
	const FBlueprintHelperReviewRejectOptions& InRejectOptions)
{
	FBlueprintHelperReviewPanelVisualEvent Event;
	Event.Type = EBlueprintHelperReviewPanelVisualEventType::RejectVisibleChange;
	Event.Change = InChange;
	Event.RejectOptions = InRejectOptions;
	return Event;
}

FBlueprintHelperReviewPanelVisualEvent
FBlueprintHelperReviewPanelVisualEvent::RejectLifecycleRootVisibleChange(
	const FBlueprintHelperReviewVisibleChange& InChange,
	const FBlueprintHelperReviewPanelDataSnapshot& InDataSnapshot,
	const FBlueprintHelperReviewRejectOptions& InRejectOptions)
{
	FBlueprintHelperReviewPanelVisualEvent Event;
	Event.Type = EBlueprintHelperReviewPanelVisualEventType::RejectLifecycleRootVisibleChange;
	Event.Change = InChange;
	Event.DataSnapshot = InDataSnapshot;
	Event.RejectOptions = InRejectOptions;
	return Event;
}

FBlueprintHelperReviewPanelPresenterEvent
FBlueprintHelperReviewPanelPresenterEvent::FromActionResult(
	const FBlueprintHelperReviewActionResult& InResult)
{
	FBlueprintHelperReviewPanelPresenterEvent Event;
	Event.Type = EBlueprintHelperReviewPanelPresenterEventType::ActionResult;
	Event.ActionResult = InResult;
	return Event;
}

FBlueprintHelperReviewPanelPresenterEvent
FBlueprintHelperReviewPanelPresenterEvent::FromCascadeActionResult(
	const FBlueprintHelperReviewCascadeActionResult& InResult)
{
	FBlueprintHelperReviewPanelPresenterEvent Event;
	Event.Type = EBlueprintHelperReviewPanelPresenterEventType::CascadeActionResult;
	Event.CascadeActionResult = InResult;
	return Event;
}

FBlueprintHelperReviewPanelPresenter::FBlueprintHelperReviewPanelPresenter(
	const FBlueprintHelperReviewStoreService* InReviewStoreService,
	const FBlueprintHelperReviewActionService* InReviewActionService)
	: ReviewStoreService(InReviewStoreService)
	, CommandService(InReviewActionService, InReviewStoreService)
{
}

TArray<FBlueprintHelperReviewVisibleChange>
FBlueprintHelperReviewPanelPresenter::LoadPendingVisibleChanges() const
{
	return ReviewStoreService
		? ReviewStoreService->LoadPendingVisibleChanges()
		: TArray<FBlueprintHelperReviewVisibleChange>();
}

FDelegateHandle FBlueprintHelperReviewPanelPresenter::AddPendingReviewChangedEventHandler(
	const FBlueprintHelperReviewStoreChangedMulticast::FDelegate& InDelegate) const
{
	return ReviewStoreService
		? ReviewStoreService->AddPendingReviewChangedEventHandler(InDelegate)
		: FDelegateHandle();
}

void FBlueprintHelperReviewPanelPresenter::RemovePendingReviewChangedEventHandler(
	FDelegateHandle& InHandle) const
{
	if (ReviewStoreService && InHandle.IsValid())
	{
		ReviewStoreService->RemovePendingReviewChangedEventHandler(InHandle);
		InHandle.Reset();
	}
}

FBlueprintHelperReviewPanelPresenterEvent FBlueprintHelperReviewPanelPresenter::HandleVisualEvent(
	const FBlueprintHelperReviewPanelVisualEvent& Event) const
{
	if (Event.Type == EBlueprintHelperReviewPanelVisualEventType::AcceptVisibleChange)
	{
		return HandleAcceptVisibleChange(Event.Change);
	}
	if (Event.Type == EBlueprintHelperReviewPanelVisualEventType::RejectVisibleChange)
	{
		return HandleRejectVisibleChange(Event.Change, Event.RejectOptions);
	}
	if (Event.Type == EBlueprintHelperReviewPanelVisualEventType::RejectLifecycleRootVisibleChange)
	{
		return HandleRejectLifecycleRootVisibleChange(
			Event.Change,
			Event.DataSnapshot,
			Event.RejectOptions);
	}

	FBlueprintHelperReviewActionResult Result;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
	Result.Message = TEXT("Unhandled ReviewPanel visual event.");
	return FBlueprintHelperReviewPanelPresenterEvent::FromActionResult(Result);
}

FBlueprintHelperReviewPanelPresenterEvent FBlueprintHelperReviewPanelPresenter::HandleActionIntent(
	const FBlueprintHelperReviewActionIntent& Intent,
	const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges,
	const FBlueprintHelperReviewRejectOptions& RejectOptions) const
{
	const FBlueprintHelperReviewCommandResult CommandResult =
		CommandService.ExecuteActionIntent(Intent, PendingChanges, RejectOptions);
	return CommandResult.bCascade
		? FBlueprintHelperReviewPanelPresenterEvent::FromCascadeActionResult(CommandResult.CascadeActionResult)
		: FBlueprintHelperReviewPanelPresenterEvent::FromActionResult(CommandResult.ActionResult);
}

FBlueprintHelperReviewCommandBatchResult
FBlueprintHelperReviewPanelPresenter::AcceptVisibleChangesBatch(
	const TArray<FBlueprintHelperReviewVisibleChange>& Changes) const
{
	return CommandService.AcceptVisibleChangesBatch(Changes);
}

FBlueprintHelperReviewCommandBatchResult
FBlueprintHelperReviewPanelPresenter::RejectVisibleChangesBatch(
	const TArray<FBlueprintHelperReviewVisibleChange>& Changes,
	const FBlueprintHelperReviewRejectOptions& RejectOptions) const
{
	return CommandService.RejectVisibleChangesBatch(Changes, RejectOptions);
}

FBlueprintHelperReviewCommandBatchResult
FBlueprintHelperReviewPanelPresenter::AcceptPendingVisibleChangesForAsset(
	const FString& AssetPath) const
{
	return CommandService.AcceptPendingVisibleChangesForAsset(AssetPath);
}

FBlueprintHelperReviewCommandBatchResult
FBlueprintHelperReviewPanelPresenter::RejectPendingVisibleChangesForAsset(
	const FString& AssetPath,
	const FBlueprintHelperReviewRejectOptions& RejectOptions) const
{
	return CommandService.RejectPendingVisibleChangesForAsset(AssetPath, RejectOptions);
}

FBlueprintHelperReviewPanelPresenterEvent
FBlueprintHelperReviewPanelPresenter::HandleAcceptVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Change) const
{
	return FBlueprintHelperReviewPanelPresenterEvent::FromActionResult(
		CommandService.AcceptVisibleChange(Change));
}

FBlueprintHelperReviewPanelPresenterEvent
FBlueprintHelperReviewPanelPresenter::HandleRejectVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Change,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	return FBlueprintHelperReviewPanelPresenterEvent::FromActionResult(
		CommandService.RejectVisibleChange(Change, Options));
}

FBlueprintHelperReviewPanelPresenterEvent
FBlueprintHelperReviewPanelPresenter::HandleRejectLifecycleRootVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Change,
	const FBlueprintHelperReviewPanelDataSnapshot& DataSnapshot,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	return FBlueprintHelperReviewPanelPresenterEvent::FromCascadeActionResult(
		CommandService.RejectLifecycleRootVisibleChange(Change, Options));
}
