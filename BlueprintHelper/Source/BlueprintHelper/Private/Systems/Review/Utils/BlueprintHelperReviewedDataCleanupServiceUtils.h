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
	static void AddReviewEvidenceId(TSet<FString>& EvidenceIds, const FString& EvidenceId);
	static void CollectRetainedEvidenceIds(
		const FBlueprintHelperReviewVisibleChange& Change,
		TSet<FString>& EvidenceIds);
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
		const TSet<FString>& RetainedEvidenceIds,
		TArray<FString>& OutPaths);
	static void DeleteFileList(
		const TArray<FString>& Paths,
		int32& DeletedCount,
		int32& FailureCount);
};
