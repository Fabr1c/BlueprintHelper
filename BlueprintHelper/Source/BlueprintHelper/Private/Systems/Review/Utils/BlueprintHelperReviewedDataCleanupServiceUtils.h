// BlueprintHelper reviewed data cleanup service utilities.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/Review/BlueprintHelperReviewedDataCleanupService.h"

class FJsonObject;

class FBlueprintHelperReviewedDataCleanupServiceUtils
{
public:
	static bool IsReviewedTerminalStatus(EBlueprintHelperReviewChangeStatus Status);
	static bool IsOpenReviewStatusText(const FString& Status);
	static void AddReviewTransactionId(TSet<FString>& TransactionIds, const FString& TransactionId);
	static void AddRollbackRefTransactionId(TSet<FString>& TransactionIds, const FString& RollbackDataRef);
	static void CollectRetainedTransactionIds(
		const FBlueprintHelperReviewVisibleChange& Change,
		TSet<FString>& TransactionIds);
	static void InspectDebugBundleChangeObject(
		const TSharedPtr<FJsonObject>& ChangeObject,
		bool& bHasReviewStatus,
		bool& bHasOpenReviewStatus);
	static bool IsCompletedReviewPanelBundle(const FString& BundlePath);
	static FString GetSavedRoot();
	static FString GetReviewPanelBundleDir();
	static void ScanReviewPanelBundles(FBlueprintHelperReviewedDataCleanupPlan& Plan);
	static void ScanUnreferencedJsonFiles(
		const FString& Directory,
		const TSet<FString>& RetainedTransactionIds,
		TArray<FString>& OutPaths);
	static void DeleteFileList(
		const TArray<FString>& Paths,
		int32& DeletedCount,
		int32& FailureCount);
};
