// BlueprintHelper reviewed data cleanup service.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

struct FBlueprintHelperReviewedDataCleanupPlan
{
	TArray<FBlueprintHelperReviewRecord> RecordsToSave;
	TArray<FString> ReviewRecordIdsToDelete;
	TArray<FString> SessionFilePathsToDelete;
	TArray<FString> OldDebugBundlePathsToDelete;
	TArray<FString> CompletedDebugBundlePathsToDelete;
	int32 ChangesRemoved = 0;
	int32 RecordsScanned = 0;
	int32 Failures = 0;
	FString Error;
};

struct FBlueprintHelperReviewedDataCleanupResult
{
	int32 RecordsSaved = 0;
	int32 RecordsDeleted = 0;
	int32 ChangesRemoved = 0;
	int32 SessionFilesDeleted = 0;
	int32 OldDebugBundlesDeleted = 0;
	int32 CompletedDebugBundlesDeleted = 0;
	int32 Failures = 0;
	FString Error;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewedDataCleanupService
{
public:
	static FBlueprintHelperReviewedDataCleanupPlan ScanCleanupPlan();
	static FBlueprintHelperReviewedDataCleanupResult ExecuteCleanupPlan(
		const FBlueprintHelperReviewedDataCleanupPlan& Plan);
};
