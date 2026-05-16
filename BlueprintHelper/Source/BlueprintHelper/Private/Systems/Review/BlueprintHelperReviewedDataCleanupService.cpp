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

namespace
{
static bool IsBlueprintHelperReviewedTerminalStatus(EBlueprintHelperReviewChangeStatus Status)
{
	return Status == EBlueprintHelperReviewChangeStatus::Accepted
		|| Status == EBlueprintHelperReviewChangeStatus::Rejected
		|| Status == EBlueprintHelperReviewChangeStatus::Superseded;
}

static bool IsBlueprintHelperOpenReviewStatusText(const FString& Status)
{
	return Status.Equals(TEXT("pending"), ESearchCase::IgnoreCase)
		|| Status.Equals(TEXT("needs_action"), ESearchCase::IgnoreCase)
		|| Status.Equals(TEXT("reject_failed"), ESearchCase::IgnoreCase);
}

static void AddBlueprintHelperReviewTransactionId(TSet<FString>& TransactionIds, const FString& TransactionId)
{
	if (!TransactionId.IsEmpty())
	{
		TransactionIds.Add(TransactionId);
	}
}

static void AddBlueprintHelperRollbackRefTransactionId(TSet<FString>& TransactionIds, const FString& RollbackDataRef)
{
	const FString Prefix = TEXT("transaction://");
	const FString Suffix = TEXT("/rollback_data");
	if (!RollbackDataRef.StartsWith(Prefix) || !RollbackDataRef.EndsWith(Suffix))
	{
		return;
	}
	FString TransactionId = RollbackDataRef.Mid(Prefix.Len());
	TransactionId.LeftChopInline(Suffix.Len());
	AddBlueprintHelperReviewTransactionId(TransactionIds, TransactionId);
}

static void CollectBlueprintHelperRetainedTransactionIds(
	const FBlueprintHelperReviewVisibleChange& Change,
	TSet<FString>& TransactionIds)
{
	AddBlueprintHelperReviewTransactionId(TransactionIds, Change.LatestTransactionId);
	for (const FString& TransactionId : Change.LatestTransactionIds)
	{
		AddBlueprintHelperReviewTransactionId(TransactionIds, TransactionId);
	}
	for (const FString& TransactionId : Change.SourceTransactionIds)
	{
		AddBlueprintHelperReviewTransactionId(TransactionIds, TransactionId);
	}
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		AddBlueprintHelperReviewTransactionId(TransactionIds, Target.FirstTransactionId);
		AddBlueprintHelperReviewTransactionId(TransactionIds, Target.LatestTransactionId);
		for (const FString& TransactionId : Target.SourceTransactionIds)
		{
			AddBlueprintHelperReviewTransactionId(TransactionIds, TransactionId);
		}
		AddBlueprintHelperRollbackRefTransactionId(TransactionIds, Target.RollbackDataRef);
	}
}

static void InspectBlueprintHelperDebugBundleChangeObject(
	const TSharedPtr<FJsonObject>& ChangeObject,
	bool& bHasReviewStatus,
	bool& bHasOpenReviewStatus)
{
	if (!ChangeObject.IsValid())
	{
		return;
	}

	bool bValidChange = true;
	ChangeObject->TryGetBoolField(TEXT("valid"), bValidChange);
	if (!bValidChange)
	{
		return;
	}

	FString Status;
	if (ChangeObject->TryGetStringField(TEXT("status"), Status) && !Status.IsEmpty())
	{
		bHasReviewStatus = true;
		if (IsBlueprintHelperOpenReviewStatusText(Status))
		{
			bHasOpenReviewStatus = true;
		}
	}
}

static bool IsBlueprintHelperCompletedReviewPanelBundle(const FString& BundlePath)
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *BundlePath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Bundle;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Bundle) || !Bundle.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
	if (!Bundle->TryGetArrayField(TEXT("events"), Events) || !Events)
	{
		return false;
	}

	bool bHasReviewStatus = false;
	bool bHasOpenReviewStatus = false;
	for (const TSharedPtr<FJsonValue>& EventValue : *Events)
	{
		const TSharedPtr<FJsonObject> EventObject = EventValue.IsValid()
			? EventValue->AsObject()
			: nullptr;
		if (!EventObject.IsValid())
		{
			continue;
		}

		const TSharedPtr<FJsonObject>* SelectedChangeObject = nullptr;
		if (EventObject->TryGetObjectField(TEXT("selected_change"), SelectedChangeObject)
			&& SelectedChangeObject)
		{
			InspectBlueprintHelperDebugBundleChangeObject(
				*SelectedChangeObject,
				bHasReviewStatus,
				bHasOpenReviewStatus);
		}

		const TSharedPtr<FJsonObject>* ChangeObject = nullptr;
		if (EventObject->TryGetObjectField(TEXT("change"), ChangeObject)
			&& ChangeObject)
		{
			InspectBlueprintHelperDebugBundleChangeObject(
				*ChangeObject,
				bHasReviewStatus,
				bHasOpenReviewStatus);
		}
	}

	return bHasReviewStatus && !bHasOpenReviewStatus;
}

static FString GetBlueprintHelperSavedRoot()
{
	return FPaths::ProjectSavedDir() / TEXT("BlueprintHelper");
}

static FString GetReviewPanelBundleDir()
{
	return GetBlueprintHelperSavedRoot() / TEXT("Debug") / TEXT("ReviewPanelBundles");
}

static void ScanBlueprintHelperReviewPanelBundles(
	FBlueprintHelperReviewedDataCleanupPlan& Plan)
{
	const FString BundleDir = GetReviewPanelBundleDir();
	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.DirectoryExists(*BundleDir))
	{
		return;
	}

	TArray<FString> FileNames;
	FileManager.FindFiles(FileNames, *(BundleDir / TEXT("*.json")), true, false);
	const FDateTime OldBundleCutoff = FDateTime::Now() - FTimespan::FromDays(7);
	for (const FString& FileName : FileNames)
	{
		const FString Path = BundleDir / FileName;
		const FDateTime Timestamp = FileManager.GetTimeStamp(*Path);
		const bool bOldBundle = Timestamp != FDateTime::MinValue()
			&& Timestamp < OldBundleCutoff;
		const bool bCompletedSessionBundle = IsBlueprintHelperCompletedReviewPanelBundle(Path);
		if (bOldBundle)
		{
			Plan.OldDebugBundlePathsToDelete.Add(Path);
		}
		else if (bCompletedSessionBundle)
		{
			Plan.CompletedDebugBundlePathsToDelete.Add(Path);
		}
	}
}

static void ScanBlueprintHelperUnreferencedJsonFiles(
	const FString& Directory,
	const TSet<FString>& RetainedIds,
	TArray<FString>& OutFilePaths)
{
	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.DirectoryExists(*Directory))
	{
		return;
	}

	TArray<FString> FileNames;
	FileManager.FindFiles(FileNames, *(Directory / TEXT("*.json")), true, false);
	for (const FString& FileName : FileNames)
	{
		const FString Id = FPaths::GetBaseFilename(FileName);
		if (RetainedIds.Contains(Id))
		{
			continue;
		}
		OutFilePaths.Add(Directory / FileName);
	}
}

static void DeleteBlueprintHelperFileList(
	const TArray<FString>& FilePaths,
	int32& DeletedCount,
	int32& FailureCount)
{
	IFileManager& FileManager = IFileManager::Get();
	for (const FString& Path : FilePaths)
	{
		if (FileManager.Delete(*Path, false, true))
		{
			++DeletedCount;
		}
		else
		{
			++FailureCount;
		}
	}
}
}

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
			if (IsBlueprintHelperReviewedTerminalStatus(Change.Status))
			{
				++Plan.ChangesRemoved;
				bRemovedTerminalChange = true;
				continue;
			}
			RetainedChanges.Add(Change);
			CollectBlueprintHelperRetainedTransactionIds(Change, RetainedTransactionIds);
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

	const FString BlueprintHelperSavedRoot = GetBlueprintHelperSavedRoot();
	ScanBlueprintHelperUnreferencedJsonFiles(
		BlueprintHelperSavedRoot / TEXT("Transactions") / TEXT("Active"),
		RetainedTransactionIds,
		Plan.TransactionFilePathsToDelete);
	ScanBlueprintHelperUnreferencedJsonFiles(
		BlueprintHelperSavedRoot / TEXT("Review"),
		RetainedTransactionIds,
		Plan.TransactionFilePathsToDelete);
	ScanBlueprintHelperUnreferencedJsonFiles(
		BlueprintHelperSavedRoot / TEXT("Review") / TEXT("Sessions"),
		RetainedArchiveSessionIds,
		Plan.SessionFilePathsToDelete);
	ScanBlueprintHelperReviewPanelBundles(Plan);

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

	DeleteBlueprintHelperFileList(
		Plan.TransactionFilePathsToDelete,
		Result.TransactionFilesDeleted,
		Result.Failures);
	DeleteBlueprintHelperFileList(
		Plan.SessionFilePathsToDelete,
		Result.SessionFilesDeleted,
		Result.Failures);
	DeleteBlueprintHelperFileList(
		Plan.OldDebugBundlePathsToDelete,
		Result.OldDebugBundlesDeleted,
		Result.Failures);
	DeleteBlueprintHelperFileList(
		Plan.CompletedDebugBundlePathsToDelete,
		Result.CompletedDebugBundlesDeleted,
		Result.Failures);

	return Result;
}
