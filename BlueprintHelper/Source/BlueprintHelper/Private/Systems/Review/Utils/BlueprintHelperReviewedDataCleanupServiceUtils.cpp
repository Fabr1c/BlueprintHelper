// BlueprintHelper reviewed data cleanup service utilities implementation.

#include "Systems/Review/Utils/BlueprintHelperReviewedDataCleanupServiceUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/Review/BlueprintHelperReviewEnumUtils.h"

bool FBlueprintHelperReviewedDataCleanupServiceUtils::IsReviewedTerminalStatus(
	EBlueprintHelperReviewChangeStatus Status)
{
	return Status == EBlueprintHelperReviewChangeStatus::Accepted
		|| Status == EBlueprintHelperReviewChangeStatus::Rejected
		|| Status == EBlueprintHelperReviewChangeStatus::Superseded;
}

bool FBlueprintHelperReviewedDataCleanupServiceUtils::IsOpenReviewStatusText(const FString& Status)
{
	const EBlueprintHelperReviewChangeStatus ParsedStatus =
		FBlueprintHelperReviewEnumUtils::ParseChangeStatus(Status);
	return ParsedStatus == EBlueprintHelperReviewChangeStatus::Pending
		|| ParsedStatus == EBlueprintHelperReviewChangeStatus::NeedsAction
		|| ParsedStatus == EBlueprintHelperReviewChangeStatus::RejectFailed;
}

void FBlueprintHelperReviewedDataCleanupServiceUtils::AddReviewTransactionId(
	TSet<FString>& TransactionIds,
	const FString& TransactionId)
{
	if (!TransactionId.IsEmpty())
	{
		TransactionIds.Add(TransactionId);
	}
}

void FBlueprintHelperReviewedDataCleanupServiceUtils::AddRollbackRefTransactionId(
	TSet<FString>& TransactionIds,
	const FString& RollbackDataRef)
{
	const FString Prefix = TEXT("transaction://");
	const FString Suffix = TEXT("/rollback_data");
	if (!RollbackDataRef.StartsWith(Prefix) || !RollbackDataRef.EndsWith(Suffix))
	{
		return;
	}
	FString TransactionId = RollbackDataRef.Mid(Prefix.Len());
	TransactionId.LeftChopInline(Suffix.Len());
	AddReviewTransactionId(TransactionIds, TransactionId);
}

void FBlueprintHelperReviewedDataCleanupServiceUtils::CollectRetainedTransactionIds(
	const FBlueprintHelperReviewVisibleChange& Change,
	TSet<FString>& TransactionIds)
{
	AddReviewTransactionId(TransactionIds, Change.LatestTransactionId);
	for (const FString& TransactionId : Change.LatestTransactionIds)
	{
		AddReviewTransactionId(TransactionIds, TransactionId);
	}
	for (const FString& TransactionId : Change.SourceTransactionIds)
	{
		AddReviewTransactionId(TransactionIds, TransactionId);
	}
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		AddReviewTransactionId(TransactionIds, Target.FirstTransactionId);
		AddReviewTransactionId(TransactionIds, Target.LatestTransactionId);
		for (const FString& TransactionId : Target.SourceTransactionIds)
		{
			AddReviewTransactionId(TransactionIds, TransactionId);
		}
		AddRollbackRefTransactionId(TransactionIds, Target.RollbackDataRef);
	}
}

void FBlueprintHelperReviewedDataCleanupServiceUtils::InspectDebugBundleChangeObject(
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
		if (IsOpenReviewStatusText(Status))
		{
			bHasOpenReviewStatus = true;
		}
	}
}

bool FBlueprintHelperReviewedDataCleanupServiceUtils::IsCompletedReviewPanelBundle(
	const FString& BundlePath)
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
			InspectDebugBundleChangeObject(*SelectedChangeObject, bHasReviewStatus, bHasOpenReviewStatus);
		}

		const TSharedPtr<FJsonObject>* ChangeObject = nullptr;
		if (EventObject->TryGetObjectField(TEXT("change"), ChangeObject)
			&& ChangeObject)
		{
			InspectDebugBundleChangeObject(
				*ChangeObject,
				bHasReviewStatus,
				bHasOpenReviewStatus);
		}
	}

	return bHasReviewStatus && !bHasOpenReviewStatus;
}

FString FBlueprintHelperReviewedDataCleanupServiceUtils::GetSavedRoot()
{
	return FPaths::ProjectSavedDir() / TEXT("BlueprintHelper");
}

FString FBlueprintHelperReviewedDataCleanupServiceUtils::GetReviewPanelBundleDir()
{
	return GetSavedRoot() / TEXT("Debug") / TEXT("ReviewPanelBundles");
}

void FBlueprintHelperReviewedDataCleanupServiceUtils::ScanReviewPanelBundles(
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
		const bool bCompletedSessionBundle = IsCompletedReviewPanelBundle(Path);
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

void FBlueprintHelperReviewedDataCleanupServiceUtils::ScanUnreferencedJsonFiles(
	const FString& Directory,
	const TSet<FString>& RetainedTransactionIds,
	TArray<FString>& OutPaths)
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
		if (RetainedTransactionIds.Contains(Id))
		{
			continue;
		}
		OutPaths.Add(Directory / FileName);
	}
}

void FBlueprintHelperReviewedDataCleanupServiceUtils::DeleteFileList(
	const TArray<FString>& Paths,
	int32& DeletedCount,
	int32& FailureCount)
{
	IFileManager& FileManager = IFileManager::Get();
	for (const FString& Path : Paths)
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
