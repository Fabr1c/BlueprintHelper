// BlueprintHelper Review pending index service.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewPendingIndex.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewPendingIndexService
{
public:
	FString GetIndexPath() const;

	bool LoadIndex(
		FBlueprintHelperReviewPendingIndex& OutIndex,
		FString& OutError) const;

	bool SaveIndex(
		const FBlueprintHelperReviewPendingIndex& Index,
		FString& OutError) const;

	bool RebuildIndex(
		FBlueprintHelperReviewPendingIndex& OutIndex,
		FString& OutError) const;

	bool QueryPendingVisibleChanges(
		const FBlueprintHelperReviewPendingIndexQuery& Query,
		TArray<FBlueprintHelperReviewPendingVisibleChangeSummary>& OutChanges,
		FString& OutError) const;

	bool QueryPendingVisibleChangePage(
		const FBlueprintHelperReviewPendingIndexPageRequest& Request,
		FBlueprintHelperReviewPendingIndexPage& OutPage,
		FString& OutError) const;

	bool ApplyRecordSaved(
		const FBlueprintHelperReviewRecord& Record,
		FString& OutError) const;

	bool ApplyRecordDeleted(
		const FString& ReviewRecordId,
		FString& OutError) const;

private:
	bool LoadOrRebuildIndex(
		FBlueprintHelperReviewPendingIndex& OutIndex,
		FString& OutError) const;

	bool IsIndexStale(
		const FBlueprintHelperReviewPendingIndex& Index) const;
};
