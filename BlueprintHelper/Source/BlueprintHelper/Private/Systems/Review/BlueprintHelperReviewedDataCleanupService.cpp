// BlueprintHelper reviewed data cleanup service implementation.

#include "Systems/Review/BlueprintHelperReviewedDataCleanupService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/Review/Utils/BlueprintHelperReviewedDataCleanupServiceUtils.h"


FBlueprintHelperReviewedDataCleanupPlan FBlueprintHelperReviewedDataCleanupService::ScanCleanupPlan()
{
	FBlueprintHelperReviewedDataCleanupPlan Plan;
	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewRecordQuery Query;
	Query.bPendingOnly = false;
	TArray<FBlueprintHelperReviewRecord> Records = Store.QueryReviewRecords(Query);
	TSet<FString> RetainedTransactionIds;
	TSet<FString> RetainedArchiveSessionIds;

	Plan.RecordsScanned = Records.Num();
	for (FBlueprintHelperReviewRecord& Record : Records)
	{
		TArray<FBlueprintHelperReviewVisibleChange> RetainedChanges;
		bool bRemovedTerminalChange = false;
		for (const FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
		{
			if (FBlueprintHelperReviewedDataCleanupServiceUtils::IsReviewedTerminalStatus(Change.Status))
			{
				++Plan.ChangesRemoved;
				bRemovedTerminalChange = true;
				continue;
			}
			RetainedChanges.Add(Change);
			FBlueprintHelperReviewedDataCleanupServiceUtils::CollectRetainedTransactionIds(
				Change,
				RetainedTransactionIds);
		}

		if (RetainedChanges.Num() > 0)
		{
			RetainedArchiveSessionIds.Add(Record.ArchiveSessionId);
			if (bRemovedTerminalChange)
			{
				Record.VisibleChanges = MoveTemp(RetainedChanges);
				Plan.RecordsToSave.Add(MoveTemp(Record));
			}
			continue;
		}

		Plan.ReviewRecordIdsToDelete.Add(Record.ReviewRecordId);
	}

	const FString BlueprintHelperSavedRoot = FBlueprintHelperReviewedDataCleanupServiceUtils::GetSavedRoot();
	FBlueprintHelperReviewedDataCleanupServiceUtils::ScanUnreferencedJsonFiles(
		BlueprintHelperSavedRoot / TEXT("Transactions") / TEXT("Active"),
		RetainedTransactionIds,
		Plan.TransactionFilePathsToDelete);
	FBlueprintHelperReviewedDataCleanupServiceUtils::ScanUnreferencedJsonFiles(
		BlueprintHelperSavedRoot / TEXT("Review"),
		RetainedTransactionIds,
		Plan.TransactionFilePathsToDelete);
	FBlueprintHelperReviewedDataCleanupServiceUtils::ScanUnreferencedJsonFiles(
		BlueprintHelperSavedRoot / TEXT("Review") / TEXT("Sessions"),
		RetainedArchiveSessionIds,
		Plan.SessionFilePathsToDelete);
	FBlueprintHelperReviewedDataCleanupServiceUtils::ScanReviewPanelBundles(Plan);

	return Plan;
}

FBlueprintHelperReviewedDataCleanupResult FBlueprintHelperReviewedDataCleanupService::ExecuteCleanupPlan(
	const FBlueprintHelperReviewedDataCleanupPlan& Plan)
{
	FBlueprintHelperReviewedDataCleanupResult Result;
	Result.ChangesRemoved = Plan.ChangesRemoved;
	Result.Failures += Plan.Failures;
	Result.Error = Plan.Error;

	FBlueprintHelperReviewStoreService Store;
	for (const FBlueprintHelperReviewRecord& Record : Plan.RecordsToSave)
	{
		FString SaveError;
		if (Store.SaveReviewRecord(Record, SaveError))
		{
			++Result.RecordsSaved;
		}
		else
		{
			++Result.Failures;
			Result.Error = SaveError;
		}
	}

	for (const FString& ReviewRecordId : Plan.ReviewRecordIdsToDelete)
	{
		FString DeleteError;
		if (Store.DeleteReviewRecord(ReviewRecordId, DeleteError))
		{
			++Result.RecordsDeleted;
		}
		else
		{
			++Result.Failures;
			Result.Error = DeleteError;
		}
	}

	FBlueprintHelperReviewedDataCleanupServiceUtils::DeleteFileList(
		Plan.TransactionFilePathsToDelete,
		Result.TransactionFilesDeleted,
		Result.Failures);
	FBlueprintHelperReviewedDataCleanupServiceUtils::DeleteFileList(
		Plan.SessionFilePathsToDelete,
		Result.SessionFilesDeleted,
		Result.Failures);
	FBlueprintHelperReviewedDataCleanupServiceUtils::DeleteFileList(
		Plan.OldDebugBundlePathsToDelete,
		Result.OldDebugBundlesDeleted,
		Result.Failures);
	FBlueprintHelperReviewedDataCleanupServiceUtils::DeleteFileList(
		Plan.CompletedDebugBundlePathsToDelete,
		Result.CompletedDebugBundlesDeleted,
		Result.Failures);

	return Result;
}
