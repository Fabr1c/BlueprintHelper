// BlueprintHelper ReviewPanel paged pending-change model.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewPendingLoadCoordinator.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewPagedChangeModel
{
public:
	void Reset();
	void MarkPageRequestStarted();
	void MarkPageRequestFinished();
	void ApplyPendingLoadResult(const FBlueprintHelperReviewPendingLoadResult& Result);
	static bool PendingLoadResultContainsChange(
		const FBlueprintHelperReviewPendingLoadResult& Result,
		const FString& ChangeId);
	bool ShouldRequestNextPage(
		double ScrollOffset,
		int32 GeneratedRowCount,
		int32 LoadedFlatRowCount,
		int32 PrefetchRows) const;

	const TArray<FBlueprintHelperReviewVisibleChange>& GetLoadedChanges() const;
	bool HasMorePages() const;
	bool IsPageRequestInFlight() const;
	int32 GetTotalMatchingCount() const;
	const FBlueprintHelperReviewPendingIndexPageCursor& GetNextCursor() const;
	FText BuildPendingPageStatusText() const;

private:
	void RebuildLoadedChangeIds();
	void AddLoadedChange(const FBlueprintHelperReviewVisibleChange& Change);

	TArray<FBlueprintHelperReviewVisibleChange> LoadedChanges;
	TSet<FString> LoadedChangeIds;
	FBlueprintHelperReviewPendingIndexPageCursor NextCursor;
	int32 TotalMatchingCount = 0;
	bool bHasMorePages = false;
	bool bPageRequestInFlight = false;
};
