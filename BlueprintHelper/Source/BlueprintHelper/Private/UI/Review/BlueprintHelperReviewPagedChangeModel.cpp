// BlueprintHelper ReviewPanel paged pending-change model implementation.

#include "UI/Review/BlueprintHelperReviewPagedChangeModel.h"

void FBlueprintHelperReviewPagedChangeModel::Reset()
{
	LoadedChanges.Reset();
	LoadedChangeIds.Reset();
	NextCursor = FBlueprintHelperReviewPendingIndexPageCursor();
	TotalMatchingCount = 0;
	bHasMorePages = false;
	bPageRequestInFlight = false;
}

void FBlueprintHelperReviewPagedChangeModel::MarkPageRequestStarted()
{
	bPageRequestInFlight = true;
}

void FBlueprintHelperReviewPagedChangeModel::MarkPageRequestFinished()
{
	bPageRequestInFlight = false;
}

void FBlueprintHelperReviewPagedChangeModel::ApplyPendingLoadResult(
	const FBlueprintHelperReviewPendingLoadResult& Result)
{
	bPageRequestInFlight = false;
	if (Result.Mode == EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage)
	{
		LoadedChanges.Reset();
		LoadedChangeIds.Reset();
	}
	else if (Result.Mode == EBlueprintHelperReviewPendingLoadMode::RefreshChanged)
	{
		TSet<FString> IncomingChangeIds;
		for (const FBlueprintHelperReviewVisibleChange& Incoming : Result.Changes)
		{
			if (!Incoming.ChangeId.IsEmpty())
			{
				IncomingChangeIds.Add(Incoming.ChangeId);
			}
		}

		LoadedChanges.RemoveAll([&Result, &IncomingChangeIds](const FBlueprintHelperReviewVisibleChange& Change)
		{
			if (!Change.ChangeId.IsEmpty() && IncomingChangeIds.Contains(Change.ChangeId))
			{
				return true;
			}
			const bool bChangeMatches = Result.SourceEvent.ChangeIds.Num() == 0
				|| Result.SourceEvent.ChangeIds.Contains(Change.ChangeId);
			const bool bAssetMatches = Result.SourceEvent.AssetPaths.Num() == 0
				|| Result.SourceEvent.AssetPaths.Contains(Change.AssetPath);
			return bChangeMatches && bAssetMatches;
		});
		RebuildLoadedChangeIds();
	}

	for (const FBlueprintHelperReviewVisibleChange& Change : Result.Changes)
	{
		AddLoadedChange(Change);
	}

	if (Result.Mode != EBlueprintHelperReviewPendingLoadMode::RefreshChanged)
	{
		NextCursor = Result.NextCursor;
		TotalMatchingCount = Result.TotalMatchingCount;
		bHasMorePages = Result.bHasMore;
	}
}

bool FBlueprintHelperReviewPagedChangeModel::PendingLoadResultContainsChange(
	const FBlueprintHelperReviewPendingLoadResult& Result,
	const FString& ChangeId)
{
	if (ChangeId.IsEmpty())
	{
		return false;
	}
	for (const FBlueprintHelperReviewVisibleChange& Change : Result.Changes)
	{
		if (Change.ChangeId == ChangeId)
		{
			return true;
		}
	}
	return false;
}

bool FBlueprintHelperReviewPagedChangeModel::ShouldRequestNextPage(
	double ScrollOffset,
	int32 GeneratedRowCount,
	int32 LoadedFlatRowCount,
	int32 PrefetchRows) const
{
	if (bPageRequestInFlight || !bHasMorePages || LoadedFlatRowCount <= 0)
	{
		return false;
	}
	return ScrollOffset + GeneratedRowCount + FMath::Max(0, PrefetchRows) >= LoadedFlatRowCount;
}

const TArray<FBlueprintHelperReviewVisibleChange>& FBlueprintHelperReviewPagedChangeModel::GetLoadedChanges() const
{
	return LoadedChanges;
}

bool FBlueprintHelperReviewPagedChangeModel::HasMorePages() const
{
	return bHasMorePages;
}

bool FBlueprintHelperReviewPagedChangeModel::IsPageRequestInFlight() const
{
	return bPageRequestInFlight;
}

int32 FBlueprintHelperReviewPagedChangeModel::GetTotalMatchingCount() const
{
	return TotalMatchingCount;
}

const FBlueprintHelperReviewPendingIndexPageCursor& FBlueprintHelperReviewPagedChangeModel::GetNextCursor() const
{
	return NextCursor;
}

void FBlueprintHelperReviewPagedChangeModel::RebuildLoadedChangeIds()
{
	LoadedChangeIds.Reset();
	for (const FBlueprintHelperReviewVisibleChange& Change : LoadedChanges)
	{
		if (!Change.ChangeId.IsEmpty())
		{
			LoadedChangeIds.Add(Change.ChangeId);
		}
	}
}

void FBlueprintHelperReviewPagedChangeModel::AddLoadedChange(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	if (!Change.ChangeId.IsEmpty() && LoadedChangeIds.Contains(Change.ChangeId))
	{
		return;
	}
	if (!Change.ChangeId.IsEmpty())
	{
		LoadedChangeIds.Add(Change.ChangeId);
	}
	LoadedChanges.Add(Change);
}
