// BlueprintHelper Review panel command service implementation.

#include "UI/Review/BlueprintHelperReviewPanelCommandService.h"

#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/Review/Utils/BlueprintHelperReviewActionTargetUtils.h"
#include "UI/Review/BlueprintHelperReviewPanelStateService.h"

namespace
{
	static void BlueprintHelperReviewAddUniqueNonEmpty(TArray<FString>& Values, const FString& Value)
	{
		if (!Value.IsEmpty())
		{
			Values.AddUnique(Value);
		}
	}

	static TMap<FString, TArray<FString>> BlueprintHelperReviewGroupTargetKeysByRecord(
		const TArray<FBlueprintHelperReviewVisibleChange>& Changes)
	{
		TMap<FString, TArray<FString>> TargetKeysByRecordId;
		const TArray<FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch> Matches =
			FBlueprintHelperReviewActionTargetUtils::ResolvePersistedReviewTargetMatchesBatch(Changes);
		for (const FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch& Match : Matches)
		{
			if (Match.ReviewRecordId.IsEmpty())
			{
				continue;
			}
			TArray<FString>& TargetKeys = TargetKeysByRecordId.FindOrAdd(Match.ReviewRecordId);
			for (const FString& TargetKey : Match.TargetKeys)
			{
				if (!TargetKey.IsEmpty())
				{
					TargetKeys.AddUnique(TargetKey);
				}
			}
		}
		return TargetKeysByRecordId;
	}

	static FBlueprintHelperReviewStoreChangedEvent BlueprintHelperReviewMakeChangedEventFromMatches(
		const FBlueprintHelperReviewVisibleChange& Change,
		const TArray<FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch>& Matches)
	{
		TArray<FString> ReviewRecordIds;
		for (const FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch& Match : Matches)
		{
			BlueprintHelperReviewAddUniqueNonEmpty(ReviewRecordIds, Match.ReviewRecordId);
		}
		return FBlueprintHelperReviewStoreChangedEvent::RecordsChanged(
			ReviewRecordIds,
			{ Change.ChangeId },
			{ Change.AssetPath });
	}

	static FBlueprintHelperReviewStoreChangedEvent BlueprintHelperReviewMakeChangedEventFromChanges(
		const TArray<FBlueprintHelperReviewVisibleChange>& Changes)
	{
		const TArray<FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch> Matches =
			FBlueprintHelperReviewActionTargetUtils::ResolvePersistedReviewTargetMatchesBatch(Changes);
		TArray<FString> ReviewRecordIds;
		TArray<FString> ChangeIds;
		TArray<FString> AssetPaths;
		for (const FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch& Match : Matches)
		{
			BlueprintHelperReviewAddUniqueNonEmpty(ReviewRecordIds, Match.ReviewRecordId);
		}
		for (const FBlueprintHelperReviewVisibleChange& Change : Changes)
		{
			BlueprintHelperReviewAddUniqueNonEmpty(ChangeIds, Change.ChangeId);
			BlueprintHelperReviewAddUniqueNonEmpty(AssetPaths, Change.AssetPath);
		}
		return FBlueprintHelperReviewStoreChangedEvent::RecordsChanged(
			ReviewRecordIds,
			ChangeIds,
			AssetPaths);
	}

	static FBlueprintHelperReviewStoreChangedEvent BlueprintHelperReviewMakeBatchChangedEvent(
		const TArray<FBlueprintHelperReviewVisibleChange>& Changes,
		const FBlueprintHelperReviewBatchActionResult& BatchResult)
	{
		TArray<FString> ChangeIds;
		TArray<FString> AssetPaths;
		for (const FBlueprintHelperReviewVisibleChange& Change : Changes)
		{
			BlueprintHelperReviewAddUniqueNonEmpty(ChangeIds, Change.ChangeId);
			BlueprintHelperReviewAddUniqueNonEmpty(AssetPaths, Change.AssetPath);
		}
		return FBlueprintHelperReviewStoreChangedEvent::RecordsChanged(
			BatchResult.ChangedReviewRecordIds,
			ChangeIds,
			AssetPaths);
	}

	static TArray<FBlueprintHelperReviewVisibleChange> BlueprintHelperReviewQueryPendingVisibleChangesForAsset(
		const FBlueprintHelperReviewStoreService& Store,
		const FString& AssetPath)
	{
		FBlueprintHelperReviewPendingIndexQuery Query;
		Query.AssetPathFilter = AssetPath;
		Query.bPendingOnly = true;
		Query.bSkipMissingAssetRecords = AssetPath.IsEmpty();

		TArray<FBlueprintHelperReviewVisibleChange> Changes;
		const TArray<FBlueprintHelperReviewPendingVisibleChangeSummary> Summaries =
			Store.QueryPendingVisibleChangeSummaries(Query);
		for (const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary : Summaries)
		{
			Changes.Add(Summary.Change);
		}
		return Changes;
	}
}

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
		const TArray<FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch> MatchesBeforeAction =
			FBlueprintHelperReviewActionTargetUtils::ResolvePersistedReviewTargetMatches(Change);
		CommandResult.ActionResult = AcceptVisibleChange(Change);
		NotifyStoreChangedIfSucceeded(
			CommandResult.ActionResult,
			BlueprintHelperReviewMakeChangedEventFromMatches(Change, MatchesBeforeAction));
		return CommandResult;
	}

	if (Change.bIsAssetLifecycleRoot)
	{
		CommandResult.bCascade = true;
		const FBlueprintHelperReviewStoreChangedEvent StoreChangedEvent =
			BlueprintHelperReviewMakeChangedEventFromChanges(PendingChanges);
		CommandResult.CascadeActionResult = RejectLifecycleRootVisibleChange(Change, PendingChanges, RejectOptions);
		NotifyStoreChangedIfSucceeded(CommandResult.CascadeActionResult, StoreChangedEvent);
		return CommandResult;
	}

	const TArray<FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch> MatchesBeforeAction =
		FBlueprintHelperReviewActionTargetUtils::ResolvePersistedReviewTargetMatches(Change);
	CommandResult.ActionResult = RejectVisibleChange(Change, RejectOptions);
	NotifyStoreChangedIfSucceeded(
		CommandResult.ActionResult,
		BlueprintHelperReviewMakeChangedEventFromMatches(Change, MatchesBeforeAction));
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

FBlueprintHelperReviewCommandBatchResult
FBlueprintHelperReviewPanelCommandService::AcceptVisibleChangesBatch(
	const TArray<FBlueprintHelperReviewVisibleChange>& Changes) const
{
	FBlueprintHelperReviewCommandBatchResult Result;
	if (!ReviewActionService)
	{
		Result.BatchActionResult.Message = TEXT("review_action_service_unavailable");
		return Result;
	}

	const TMap<FString, TArray<FString>> TargetKeysByRecordId =
		BlueprintHelperReviewGroupTargetKeysByRecord(Changes);
	Result.BatchActionResult = ReviewActionService->AcceptReviewTargetsBatch(TargetKeysByRecordId);
	Result.StoreChangedEvent = BlueprintHelperReviewMakeBatchChangedEvent(Changes, Result.BatchActionResult);
	NotifyStoreChangedIfSucceeded(Result);
	return Result;
}

FBlueprintHelperReviewCommandBatchResult
FBlueprintHelperReviewPanelCommandService::RejectVisibleChangesBatch(
	const TArray<FBlueprintHelperReviewVisibleChange>& Changes,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	FBlueprintHelperReviewCommandBatchResult Result;
	if (!ReviewActionService)
	{
		Result.BatchActionResult.Message = TEXT("review_action_service_unavailable");
		return Result;
	}

	const TMap<FString, TArray<FString>> TargetKeysByRecordId =
		BlueprintHelperReviewGroupTargetKeysByRecord(Changes);
	Result.BatchActionResult = ReviewActionService->RejectReviewTargetsBatch(TargetKeysByRecordId, Options);
	Result.StoreChangedEvent = BlueprintHelperReviewMakeBatchChangedEvent(Changes, Result.BatchActionResult);
	NotifyStoreChangedIfSucceeded(Result);
	return Result;
}

FBlueprintHelperReviewCommandBatchResult
FBlueprintHelperReviewPanelCommandService::AcceptPendingVisibleChangesForAsset(
	const FString& AssetPath) const
{
	FBlueprintHelperReviewCommandBatchResult Result;
	if (!ReviewStoreService)
	{
		Result.BatchActionResult.Message = TEXT("review_store_service_unavailable");
		return Result;
	}

	return AcceptVisibleChangesBatch(
		BlueprintHelperReviewQueryPendingVisibleChangesForAsset(*ReviewStoreService, AssetPath));
}

FBlueprintHelperReviewCommandBatchResult
FBlueprintHelperReviewPanelCommandService::RejectPendingVisibleChangesForAsset(
	const FString& AssetPath,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	FBlueprintHelperReviewCommandBatchResult Result;
	if (!ReviewStoreService)
	{
		Result.BatchActionResult.Message = TEXT("review_store_service_unavailable");
		return Result;
	}

	return RejectVisibleChangesBatch(
		BlueprintHelperReviewQueryPendingVisibleChangesForAsset(*ReviewStoreService, AssetPath),
		Options);
}

void FBlueprintHelperReviewPanelCommandService::NotifyStoreChangedIfSucceeded(
	const FBlueprintHelperReviewActionResult& Result,
	const FBlueprintHelperReviewStoreChangedEvent& Event) const
{
	if (ReviewStoreService
		&& Result.bSucceeded
		&& (Event.bRequiresFullReload
			|| Event.ReviewRecordIds.Num() > 0
			|| Event.ChangeIds.Num() > 0
			|| Event.AssetPaths.Num() > 0))
	{
		ReviewStoreService->NotifyPendingReviewChanged(Event);
	}
}

void FBlueprintHelperReviewPanelCommandService::NotifyStoreChangedIfSucceeded(
	const FBlueprintHelperReviewCommandBatchResult& Result) const
{
	if (ReviewStoreService && Result.BatchActionResult.SucceededCount > 0)
	{
		ReviewStoreService->NotifyPendingReviewChanged(Result.StoreChangedEvent);
	}
}

void FBlueprintHelperReviewPanelCommandService::NotifyStoreChangedIfSucceeded(
	const FBlueprintHelperReviewCascadeActionResult& Result,
	const FBlueprintHelperReviewStoreChangedEvent& Event) const
{
	if (ReviewStoreService
		&& Result.RootResult.bSucceeded
		&& (Event.bRequiresFullReload
			|| Event.ReviewRecordIds.Num() > 0
			|| Event.ChangeIds.Num() > 0
			|| Event.AssetPaths.Num() > 0))
	{
		ReviewStoreService->NotifyPendingReviewChanged(Event);
	}
}
