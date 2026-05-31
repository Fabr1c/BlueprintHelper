// BlueprintHelper Review pending index DTOs.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPendingVisibleChangeSummary
{
	FString ReviewRecordId;
	FString ArchiveSessionId;
	FString RecordAssetPath;
	FString SortKey;
	FBlueprintHelperReviewVisibleChange Change;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPendingRecordSummary
{
	FString ReviewRecordId;
	FString ArchiveSessionId;
	FString AssetPath;
	TArray<FString> SourceTaskRunIds;
	EBlueprintHelperReviewChangeStatus Status = EBlueprintHelperReviewChangeStatus::Pending;
	EBlueprintHelperReviewStorageStatus StorageStatus = EBlueprintHelperReviewStorageStatus::Active;
	FBlueprintHelperReviewSourceSummary SourceReviewSummary;
	TArray<FBlueprintHelperReviewPendingVisibleChangeSummary> VisibleChanges;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPendingIndex
{
	FString Schema = TEXT("BlueprintHelper.ReviewPendingIndex.v1");
	FString BuiltAtUtc;
	TArray<FBlueprintHelperReviewPendingRecordSummary> Records;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPendingIndexQuery
{
	FString ArchiveSessionIdFilter;
	FString AssetPathFilter;
	FString TaskRunIdFilter;
	bool bPendingOnly = true;
	bool bSkipMissingAssetRecords = true;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewStoreChangedEvent
{
	TArray<FString> ReviewRecordIds;
	TArray<FString> ChangeIds;
	TArray<FString> AssetPaths;
	bool bRequiresFullReload = false;

	static FBlueprintHelperReviewStoreChangedEvent FullReload();
	static FBlueprintHelperReviewStoreChangedEvent RecordsChanged(
		const TArray<FString>& InReviewRecordIds,
		const TArray<FString>& InChangeIds,
		const TArray<FString>& InAssetPaths);
};

DECLARE_MULTICAST_DELEGATE_OneParam(
	FBlueprintHelperReviewStoreChangedMulticast,
	const FBlueprintHelperReviewStoreChangedEvent&);
