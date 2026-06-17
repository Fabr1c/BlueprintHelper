// BlueprintHelper ReviewPanel pending load application service implementation.

#include "UI/Review/BlueprintHelperReviewPendingLoadApplicationService.h"

FBlueprintHelperReviewStoreChangedEvent FBlueprintHelperReviewPendingLoadApplicationService::NormalizeStoreChangedEvent(
	const FBlueprintHelperReviewStoreChangedEvent& SourceEvent)
{
	if (!SourceEvent.bRequiresFullReload
		&& SourceEvent.ChangeIds.Num() == 0
		&& SourceEvent.AssetPaths.Num() == 0)
	{
		return FBlueprintHelperReviewStoreChangedEvent::FullReload();
	}
	return SourceEvent;
}

EBlueprintHelperReviewPendingLoadMode FBlueprintHelperReviewPendingLoadApplicationService::ResolveLoadMode(
	const FBlueprintHelperReviewStoreChangedEvent& NormalizedEvent)
{
	const bool bFullReload = NormalizedEvent.bRequiresFullReload
		|| (NormalizedEvent.ChangeIds.Num() == 0 && NormalizedEvent.AssetPaths.Num() == 0);
	return bFullReload
		? EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage
		: EBlueprintHelperReviewPendingLoadMode::RefreshChanged;
}

FBlueprintHelperReviewPendingLoadRequestApplication
FBlueprintHelperReviewPendingLoadApplicationService::BuildRequestApplication(
	const FString& Reason,
	EBlueprintHelperReviewPendingLoadMode Mode,
	const FBlueprintHelperReviewStoreChangedEvent& SourceEvent,
	const FBlueprintHelperReviewPagedChangeModel& PagedChangeModel,
	int32 PageSize)
{
	FBlueprintHelperReviewPendingLoadRequestApplication Application;
	if (PagedChangeModel.IsPageRequestInFlight())
	{
		if (Mode == EBlueprintHelperReviewPendingLoadMode::AppendNextPage)
		{
			Application.DebugMessage = FString::Printf(
				TEXT("Review pending load skipped reason=%s mode=%d cause=in_flight_append"),
				*Reason,
				static_cast<int32>(Mode));
			return Application;
		}
		Application.bShouldCancelPendingLoads = true;
		Application.bShouldFinishInFlightRequest = true;
	}

	Application.bShouldRequestLoad = true;
	Application.bShouldRecordStoreTiming = Reason == TEXT("store_changed");
	Application.Request.Source = Reason;
	Application.Request.SourceEvent = SourceEvent;
	Application.Request.Mode = Mode;
	Application.Request.PageSize = PageSize;
	Application.Request.Cursor = Mode == EBlueprintHelperReviewPendingLoadMode::AppendNextPage
		? PagedChangeModel.GetNextCursor()
		: FBlueprintHelperReviewPendingIndexPageCursor();
	Application.DebugMessage = FString::Printf(
		TEXT("Review pending load requested reason=%s mode=%d page_size=%d"),
		*Reason,
		static_cast<int32>(Mode),
		PageSize);
	return Application;
}

FBlueprintHelperReviewPendingLoadResultApplication
FBlueprintHelperReviewPendingLoadApplicationService::ApplyResult(
	const FBlueprintHelperReviewPendingLoadResult& Result,
	FBlueprintHelperReviewPagedChangeModel& PagedChangeModel,
	const TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>>& CurrentChangeItems,
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& CurrentSelectedChange,
	const FString& LastVisibleChangeRefreshSignature,
	int32 PageSize)
{
	FBlueprintHelperReviewPendingLoadResultApplication Application;
	PagedChangeModel.MarkPageRequestFinished();

	if (Result.bDiscarded)
	{
		Application.bShouldIgnore = true;
		Application.bShouldRecordTiming = true;
		Application.bTimingSucceeded = false;
		Application.TimingStage = TEXT("pending_load_discarded");
		Application.TimingSource = Result.Source;
		return Application;
	}

	if (!Result.bSucceeded)
	{
		Application.bShouldIgnore = true;
		Application.bShouldRecordTiming = true;
		Application.bTimingSucceeded = true;
		Application.TimingStage = TEXT("pending_load_failed");
		Application.TimingSource = Result.Source;
		Application.DebugMessage = FString::Printf(
			TEXT("Review pending load failed reason=%s error=%s"),
			*Result.Source,
			*Result.Error);
		return Application;
	}

	Application.bShouldDispatchValidityCandidates = Result.ValidityCandidates.Num() > 0;
	Application.bReturnedMoreThanPageSize =
		Result.Mode == EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage
		&& Result.Changes.Num() > PageSize;

	const FString PreviousSelectedChangeId = CurrentSelectedChange.IsValid()
		? CurrentSelectedChange->ChangeId
		: FString();
	const FString PreviousSelectedAssetPath = CurrentSelectedChange.IsValid()
		? CurrentSelectedChange->AssetPath
		: FString();
	const int32 PreviousSelectedIndex = FindChangeIndexById(
		CurrentChangeItems,
		PreviousSelectedChangeId);

	PagedChangeModel.ApplyPendingLoadResult(Result);
	Application.NextChanges = PagedChangeModel.GetLoadedChanges();
	Application.NewRefreshSignature = BuildVisibleChangeRefreshSignature(Application.NextChanges);
	if (Application.NewRefreshSignature == LastVisibleChangeRefreshSignature)
	{
		Application.bSignatureUnchanged = true;
		Application.bShouldRecordTiming = true;
		Application.bTimingSucceeded = true;
		Application.TimingStage = TEXT("panel_refresh_no_change");
		Application.TimingSource = Result.Source;
		Application.bShouldInvalidate = true;
		return Application;
	}

	Application.RecommendedSelectedChangeId = ResolveRecommendedSelectedChangeId(
		Application.NextChanges,
		PreviousSelectedChangeId,
		PreviousSelectedAssetPath,
		PreviousSelectedIndex == INDEX_NONE ? 0 : PreviousSelectedIndex);

	const bool bSelectionChanged = Application.RecommendedSelectedChangeId != PreviousSelectedChangeId;
	const bool bSelectedChangeRefreshed =
		Result.Mode == EBlueprintHelperReviewPendingLoadMode::RefreshChanged
		&& !Application.RecommendedSelectedChangeId.IsEmpty()
		&& Application.RecommendedSelectedChangeId == PreviousSelectedChangeId
		&& FBlueprintHelperReviewPagedChangeModel::PendingLoadResultContainsChange(
			Result,
			PreviousSelectedChangeId);

	Application.bShouldApplyVisibleChanges = true;
	Application.bShouldRefreshMainWorkspace = bSelectionChanged || bSelectedChangeRefreshed;
	Application.bShouldInvalidate = true;
	Application.bShouldRecordTiming = true;
	Application.bTimingSucceeded = true;
	Application.TimingStage = TEXT("panel_refresh_applied");
	Application.TimingSource = Result.Source;
	Application.DebugMessage = FString::Printf(
		TEXT("Review store refreshed dynamically source=%s loaded=%d total=%d has_more=%d"),
		*Result.Source,
		Application.NextChanges.Num(),
		PagedChangeModel.GetTotalMatchingCount(),
		PagedChangeModel.HasMorePages() ? 1 : 0);
	return Application;
}

FString FBlueprintHelperReviewPendingLoadApplicationService::BuildVisibleChangeRefreshSignature(
	const TArray<FBlueprintHelperReviewVisibleChange>& Changes)
{
	TArray<FString> Parts;
	Parts.Reserve(Changes.Num());
	for (const FBlueprintHelperReviewVisibleChange& Change : Changes)
	{
		Parts.Add(FString::Printf(
			TEXT("%s|%s|%s|%s|%s|%s|%s|%s|root=%d"),
			*Change.ChangeId,
			BlueprintHelperReviewChangeStatusToString(Change.Status),
			BlueprintHelperReviewChangeKindToString(Change.ChangeKind),
			*Change.AssetPath,
			*Change.ParentChangeId,
			*Change.LocationKey,
			*Change.LatestEvidenceId,
			*Change.DisplayLabel,
			Change.bIsAssetLifecycleRoot ? 1 : 0));
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			Parts.Add(FString::Printf(
				TEXT("target|%s|%s|%s|%s|%s|%s"),
				BlueprintHelperReviewSurfaceToString(Target.Surface),
				*Target.TargetKind,
				*Target.TargetKey,
				*Target.PropertyPath,
				*Target.ComponentPath,
				*Target.DisplayLabel));
		}
	}
	return FString::Join(Parts, TEXT("\n"));
}

int32 FBlueprintHelperReviewPendingLoadApplicationService::FindChangeIndexById(
	const TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>>& ChangeItems,
	const FString& ChangeId)
{
	if (ChangeId.IsEmpty())
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < ChangeItems.Num(); ++Index)
	{
		if (ChangeItems[Index].IsValid() && ChangeItems[Index]->ChangeId == ChangeId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool FBlueprintHelperReviewPendingLoadApplicationService::ContainsChangeId(
	const TArray<FBlueprintHelperReviewVisibleChange>& Changes,
	const FString& ChangeId)
{
	if (ChangeId.IsEmpty())
	{
		return false;
	}
	for (const FBlueprintHelperReviewVisibleChange& Change : Changes)
	{
		if (Change.ChangeId == ChangeId)
		{
			return true;
		}
	}
	return false;
}

FString FBlueprintHelperReviewPendingLoadApplicationService::ResolveRecommendedSelectedChangeId(
	const TArray<FBlueprintHelperReviewVisibleChange>& NextChanges,
	const FString& PreviousSelectedChangeId,
	const FString& PreviousSelectedAssetPath,
	int32 PreviousSelectedIndex)
{
	if (NextChanges.Num() == 0)
	{
		return FString();
	}
	if (ContainsChangeId(NextChanges, PreviousSelectedChangeId))
	{
		return PreviousSelectedChangeId;
	}

	if (!PreviousSelectedAssetPath.IsEmpty())
	{
		const int32 StartIndex = FMath::Clamp(PreviousSelectedIndex, 0, NextChanges.Num() - 1);
		for (int32 Offset = 0; Offset < NextChanges.Num(); ++Offset)
		{
			const int32 Index = (StartIndex + Offset) % NextChanges.Num();
			const FBlueprintHelperReviewVisibleChange& Change = NextChanges[Index];
			if (Change.AssetPath == PreviousSelectedAssetPath && !Change.bIsAssetLifecycleRoot)
			{
				return Change.ChangeId;
			}
		}

		for (const FBlueprintHelperReviewVisibleChange& Change : NextChanges)
		{
			if (Change.AssetPath == PreviousSelectedAssetPath)
			{
				return Change.ChangeId;
			}
		}
	}

	const int32 NextIndex = FMath::Clamp(PreviousSelectedIndex, 0, NextChanges.Num() - 1);
	return NextChanges[NextIndex].ChangeId;
}
