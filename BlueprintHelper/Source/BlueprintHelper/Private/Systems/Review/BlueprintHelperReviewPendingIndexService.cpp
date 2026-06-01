// BlueprintHelper Review pending index service implementation.

#include "Systems/Review/BlueprintHelperReviewPendingIndexService.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/Review/BlueprintHelperReviewEnumUtils.h"
#include "Systems/Review/BlueprintHelperReviewConfigResolver.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/Review/Utils/BlueprintHelperReviewStoreJsonUtils.h"
#include "Systems/Review/Utils/BlueprintHelperReviewStoreTargetUtils.h"

namespace BlueprintHelperReviewPendingIndex
{
	static bool IsPendingVisibleChange(const FBlueprintHelperReviewVisibleChange& Change)
	{
		return Change.Status == EBlueprintHelperReviewChangeStatus::Pending
			|| Change.Status == EBlueprintHelperReviewChangeStatus::NeedsAction
			|| Change.Status == EBlueprintHelperReviewChangeStatus::RejectFailed;
	}

	static FString MakeSortKey(const FBlueprintHelperReviewRecord& Record)
	{
		if (!Record.SourceReviewSummary.CreatedAtLast.IsEmpty())
		{
			return Record.SourceReviewSummary.CreatedAtLast;
		}
		if (!Record.SourceReviewSummary.CreatedAtFirst.IsEmpty())
		{
			return Record.SourceReviewSummary.CreatedAtFirst;
		}
		return Record.ArchiveSessionId;
	}

	static void StripSnapshotPayload(FBlueprintHelperReviewVisibleChange& Change)
	{
		Change.BeforeSnapshotJson.Reset();
		Change.AfterSnapshotJson.Reset();
		for (FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			Target.BeforeSnapshotJson.Reset();
			Target.AfterSnapshotJson.Reset();
		}
	}

	static FBlueprintHelperReviewPendingRecordSummary MakeRecordSummary(
		const FBlueprintHelperReviewRecord& Record)
	{
		FBlueprintHelperReviewPendingRecordSummary Summary;
		Summary.ReviewRecordId = Record.ReviewRecordId;
		Summary.ArchiveSessionId = Record.ArchiveSessionId;
		Summary.AssetPath = Record.AssetPath;
		Summary.SourceTaskRunIds = Record.SourceTaskRunIds;
		Summary.Status = Record.Status;
		Summary.StorageStatus = Record.StorageStatus;
		Summary.SourceReviewSummary = Record.SourceReviewSummary;

		const FString SortKey = MakeSortKey(Record);
		for (FBlueprintHelperReviewVisibleChange Change : Record.VisibleChanges)
		{
			if (!IsPendingVisibleChange(Change))
			{
				continue;
			}

			StripSnapshotPayload(Change);
			FBlueprintHelperReviewPendingVisibleChangeSummary ChangeSummary;
			ChangeSummary.ReviewRecordId = Record.ReviewRecordId;
			ChangeSummary.ArchiveSessionId = Record.ArchiveSessionId;
			ChangeSummary.RecordAssetPath = Record.AssetPath;
			ChangeSummary.SortKey = SortKey;
			ChangeSummary.Change = MoveTemp(Change);
			Summary.VisibleChanges.Add(MoveTemp(ChangeSummary));
		}

		return Summary;
	}

	static TArray<TSharedPtr<FJsonValue>> MakeStringArray(const TArray<FString>& Values)
	{
		return FBlueprintHelperReviewStoreJsonUtils::MakeReviewJsonStringArray(Values);
	}

	static void ReadStringArray(
		const TSharedPtr<FJsonObject>& Json,
		const TCHAR* FieldName,
		TArray<FString>& OutValues)
	{
		FBlueprintHelperReviewStoreJsonUtils::ReadReviewStringArray(Json, FieldName, OutValues);
	}

	static TSharedRef<FJsonObject> SourceSummaryToJson(
		const FBlueprintHelperReviewSourceSummary& Summary)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("evidence_count"), Summary.EvidenceCount);
		Json->SetArrayField(TEXT("task_run_ids"), MakeStringArray(Summary.TaskRunIds));
		Json->SetArrayField(TEXT("operation_kinds"), MakeStringArray(Summary.OperationKinds));
		Json->SetArrayField(TEXT("asset_paths"), MakeStringArray(Summary.AssetPaths));
		Json->SetArrayField(TEXT("evidence_ids"), MakeStringArray(Summary.EvidenceIds));
		if (!Summary.CreatedAtFirst.IsEmpty())
		{
			Json->SetStringField(TEXT("created_at_first"), Summary.CreatedAtFirst);
		}
		if (!Summary.CreatedAtLast.IsEmpty())
		{
			Json->SetStringField(TEXT("created_at_last"), Summary.CreatedAtLast);
		}
		Json->SetStringField(TEXT("final_review_status"),
			BlueprintHelperReviewChangeStatusToString(Summary.FinalReviewStatus));
		return Json;
	}

	static void ReadSourceSummaryFromJson(
		const TSharedPtr<FJsonObject>& Json,
		FBlueprintHelperReviewSourceSummary& OutSummary)
	{
		if (!Json.IsValid())
		{
			return;
		}
		OutSummary.EvidenceCount = static_cast<int32>(Json->GetNumberField(TEXT("evidence_count")));
		ReadStringArray(Json, TEXT("task_run_ids"), OutSummary.TaskRunIds);
		ReadStringArray(Json, TEXT("operation_kinds"), OutSummary.OperationKinds);
		ReadStringArray(Json, TEXT("asset_paths"), OutSummary.AssetPaths);
		ReadStringArray(Json, TEXT("evidence_ids"), OutSummary.EvidenceIds);
		Json->TryGetStringField(TEXT("created_at_first"), OutSummary.CreatedAtFirst);
		Json->TryGetStringField(TEXT("created_at_last"), OutSummary.CreatedAtLast);

		FString FinalReviewStatus;
		Json->TryGetStringField(TEXT("final_review_status"), FinalReviewStatus);
		OutSummary.FinalReviewStatus =
			FBlueprintHelperReviewStoreJsonUtils::ParseReviewChangeStatus(FinalReviewStatus);
	}

	static TSharedRef<FJsonObject> ChangeSummaryToJson(
		const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("review_record_id"), Summary.ReviewRecordId);
		Json->SetStringField(TEXT("archive_session_id"), Summary.ArchiveSessionId);
		Json->SetStringField(TEXT("record_asset_path"), Summary.RecordAssetPath);
		if (!Summary.SortKey.IsEmpty())
		{
			Json->SetStringField(TEXT("sort_key"), Summary.SortKey);
		}
		Json->SetObjectField(TEXT("change"),
			FBlueprintHelperReviewStoreJsonUtils::ReviewVisibleChangeToJson(Summary.Change));
		return Json;
	}

	static bool ReadVisibleChangeFromJson(
		const TSharedPtr<FJsonObject>& ChangeJson,
		FBlueprintHelperReviewVisibleChange& OutChange)
	{
		if (!ChangeJson.IsValid())
		{
			return false;
		}

		TSharedRef<FJsonObject> FakeRecordJson = MakeShared<FJsonObject>();
		FakeRecordJson->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.ReviewRecord.v2"));
		FakeRecordJson->SetStringField(TEXT("review_record_id"), TEXT("pending_index_read"));
		TArray<TSharedPtr<FJsonValue>> Changes;
		Changes.Add(MakeShared<FJsonValueObject>(ChangeJson.ToSharedRef()));
		FakeRecordJson->SetArrayField(TEXT("visible_changes"), Changes);

		FBlueprintHelperReviewRecord FakeRecord;
		if (!FBlueprintHelperReviewStoreJsonUtils::ReadReviewRecordFromJson(FakeRecordJson, FakeRecord)
			|| FakeRecord.VisibleChanges.Num() == 0)
		{
			return false;
		}

		OutChange = FakeRecord.VisibleChanges[0];
		return true;
	}

	static bool ReadChangeSummaryFromJson(
		const TSharedPtr<FJsonObject>& Json,
		FBlueprintHelperReviewPendingVisibleChangeSummary& OutSummary)
	{
		if (!Json.IsValid())
		{
			return false;
		}

		Json->TryGetStringField(TEXT("review_record_id"), OutSummary.ReviewRecordId);
		Json->TryGetStringField(TEXT("archive_session_id"), OutSummary.ArchiveSessionId);
		Json->TryGetStringField(TEXT("record_asset_path"), OutSummary.RecordAssetPath);
		Json->TryGetStringField(TEXT("sort_key"), OutSummary.SortKey);

		const TSharedPtr<FJsonObject>* ChangeJson = nullptr;
		if (!Json->TryGetObjectField(TEXT("change"), ChangeJson) || !ChangeJson || !ChangeJson->IsValid())
		{
			return false;
		}
		return ReadVisibleChangeFromJson(*ChangeJson, OutSummary.Change);
	}

	static TSharedRef<FJsonObject> RecordSummaryToJson(
		const FBlueprintHelperReviewPendingRecordSummary& Summary)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("review_record_id"), Summary.ReviewRecordId);
		Json->SetStringField(TEXT("archive_session_id"), Summary.ArchiveSessionId);
		Json->SetStringField(TEXT("asset_path"), Summary.AssetPath);
		Json->SetArrayField(TEXT("source_task_run_ids"), MakeStringArray(Summary.SourceTaskRunIds));
		Json->SetStringField(TEXT("status"), BlueprintHelperReviewChangeStatusToString(Summary.Status));
		Json->SetStringField(TEXT("storage_status"),
			BlueprintHelperReviewStorageStatusToString(Summary.StorageStatus));
		Json->SetObjectField(TEXT("source_review_summary"),
			SourceSummaryToJson(Summary.SourceReviewSummary));

		TArray<TSharedPtr<FJsonValue>> Changes;
		for (const FBlueprintHelperReviewPendingVisibleChangeSummary& Change : Summary.VisibleChanges)
		{
			Changes.Add(MakeShared<FJsonValueObject>(ChangeSummaryToJson(Change)));
		}
		Json->SetArrayField(TEXT("visible_changes"), Changes);
		return Json;
	}

	static bool ReadRecordSummaryFromJson(
		const TSharedPtr<FJsonObject>& Json,
		FBlueprintHelperReviewPendingRecordSummary& OutSummary)
	{
		if (!Json.IsValid())
		{
			return false;
		}

		Json->TryGetStringField(TEXT("review_record_id"), OutSummary.ReviewRecordId);
		Json->TryGetStringField(TEXT("archive_session_id"), OutSummary.ArchiveSessionId);
		Json->TryGetStringField(TEXT("asset_path"), OutSummary.AssetPath);
		ReadStringArray(Json, TEXT("source_task_run_ids"), OutSummary.SourceTaskRunIds);

		FString Status;
		Json->TryGetStringField(TEXT("status"), Status);
		OutSummary.Status = FBlueprintHelperReviewStoreJsonUtils::ParseReviewChangeStatus(Status);

		FString StorageStatus;
		Json->TryGetStringField(TEXT("storage_status"), StorageStatus);
		OutSummary.StorageStatus = FBlueprintHelperReviewEnumUtils::ParseStorageStatus(StorageStatus);

		const TSharedPtr<FJsonObject>* SourceSummaryJson = nullptr;
		if (Json->TryGetObjectField(TEXT("source_review_summary"), SourceSummaryJson)
			&& SourceSummaryJson
			&& SourceSummaryJson->IsValid())
		{
			ReadSourceSummaryFromJson(*SourceSummaryJson, OutSummary.SourceReviewSummary);
		}

		const TArray<TSharedPtr<FJsonValue>>* Changes = nullptr;
		if (Json->TryGetArrayField(TEXT("visible_changes"), Changes) && Changes)
		{
			for (const TSharedPtr<FJsonValue>& ChangeValue : *Changes)
			{
				const TSharedPtr<FJsonObject> ChangeJson = ChangeValue.IsValid()
					? ChangeValue->AsObject()
					: nullptr;
				FBlueprintHelperReviewPendingVisibleChangeSummary ChangeSummary;
				if (ReadChangeSummaryFromJson(ChangeJson, ChangeSummary))
				{
					OutSummary.VisibleChanges.Add(ChangeSummary);
				}
			}
		}

		return !OutSummary.ReviewRecordId.IsEmpty();
	}

	static TSharedRef<FJsonObject> IndexToJson(const FBlueprintHelperReviewPendingIndex& Index)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), Index.Schema);
		if (!Index.BuiltAtUtc.IsEmpty())
		{
			Json->SetStringField(TEXT("built_at_utc"), Index.BuiltAtUtc);
		}

		TArray<TSharedPtr<FJsonValue>> Records;
		for (const FBlueprintHelperReviewPendingRecordSummary& Record : Index.Records)
		{
			Records.Add(MakeShared<FJsonValueObject>(RecordSummaryToJson(Record)));
		}
		Json->SetArrayField(TEXT("records"), Records);
		return Json;
	}

	static bool ReadIndexFromJson(
		const TSharedPtr<FJsonObject>& Json,
		FBlueprintHelperReviewPendingIndex& OutIndex)
	{
		if (!Json.IsValid())
		{
			return false;
		}

		FString Schema;
		Json->TryGetStringField(TEXT("schema"), Schema);
		if (Schema != TEXT("BlueprintHelper.ReviewPendingIndex.v1"))
		{
			return false;
		}

		OutIndex = FBlueprintHelperReviewPendingIndex();
		OutIndex.Schema = Schema;
		Json->TryGetStringField(TEXT("built_at_utc"), OutIndex.BuiltAtUtc);

		const TArray<TSharedPtr<FJsonValue>>* Records = nullptr;
		if (Json->TryGetArrayField(TEXT("records"), Records) && Records)
		{
			for (const TSharedPtr<FJsonValue>& RecordValue : *Records)
			{
				const TSharedPtr<FJsonObject> RecordJson = RecordValue.IsValid()
					? RecordValue->AsObject()
					: nullptr;
				FBlueprintHelperReviewPendingRecordSummary RecordSummary;
				if (ReadRecordSummaryFromJson(RecordJson, RecordSummary))
				{
					OutIndex.Records.Add(RecordSummary);
				}
			}
		}
		return true;
	}

	static bool RecordMatchesQuery(
		const FBlueprintHelperReviewPendingRecordSummary& Record,
		const FBlueprintHelperReviewPendingIndexQuery& Query)
	{
		if (!Query.ArchiveSessionIdFilter.IsEmpty() && Record.ArchiveSessionId != Query.ArchiveSessionIdFilter)
		{
			return false;
		}
		if (!Query.AssetPathFilter.IsEmpty() && Record.AssetPath != Query.AssetPathFilter)
		{
			return false;
		}
		if (!Query.TaskRunIdFilter.IsEmpty()
			&& !Record.SourceTaskRunIds.Contains(Query.TaskRunIdFilter)
			&& !Record.SourceReviewSummary.TaskRunIds.Contains(Query.TaskRunIdFilter))
		{
			return false;
		}
		if (Query.bPendingOnly
			&& (Record.Status == EBlueprintHelperReviewChangeStatus::Accepted
				|| Record.Status == EBlueprintHelperReviewChangeStatus::Rejected))
		{
			return false;
		}
		if (Record.StorageStatus != EBlueprintHelperReviewStorageStatus::Active)
		{
			return false;
		}
		return true;
	}

	static bool ChangeMatchesQuery(
		const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary,
		const FBlueprintHelperReviewPendingIndexQuery& Query)
	{
		if (!Query.bPendingOnly)
		{
			return true;
		}
		return IsPendingVisibleChange(Summary.Change);
	}

	static int32 ComparePendingSummaryForPage(
		const FBlueprintHelperReviewPendingVisibleChangeSummary& Left,
		const FBlueprintHelperReviewPendingIndexPageCursor& Cursor)
	{
		if (Left.SortKey != Cursor.SortKey)
		{
			return Left.SortKey < Cursor.SortKey ? -1 : 1;
		}
		if (Left.ReviewRecordId != Cursor.ReviewRecordId)
		{
			return Left.ReviewRecordId < Cursor.ReviewRecordId ? -1 : 1;
		}
		if (Left.Change.ChangeId != Cursor.ChangeId)
		{
			return Left.Change.ChangeId < Cursor.ChangeId ? -1 : 1;
		}
		return 0;
	}

	static FBlueprintHelperReviewPendingIndexPageCursor MakePendingPageCursor(
		const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary)
	{
		FBlueprintHelperReviewPendingIndexPageCursor Cursor;
		Cursor.SortKey = Summary.SortKey;
		Cursor.ReviewRecordId = Summary.ReviewRecordId;
		Cursor.ChangeId = Summary.Change.ChangeId;
		return Cursor;
	}
}

FString FBlueprintHelperReviewPendingIndexService::GetIndexPath() const
{
	return FBlueprintHelperReviewConfigResolver::Load().GetReviewRootDir()
		/ TEXT("PendingReviewIndex.json");
}

bool FBlueprintHelperReviewPendingIndexService::LoadIndex(
	FBlueprintHelperReviewPendingIndex& OutIndex,
	FString& OutError) const
{
	const FString Path = GetIndexPath();
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Path))
	{
		OutError = FString::Printf(TEXT("pending index not found: %s"), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Json)
		|| !BlueprintHelperReviewPendingIndex::ReadIndexFromJson(Json, OutIndex))
	{
		OutError = FString::Printf(TEXT("failed to parse pending index: %s"), *Path);
		return false;
	}

	OutError.Reset();
	return true;
}

bool FBlueprintHelperReviewPendingIndexService::SaveIndex(
	const FBlueprintHelperReviewPendingIndex& Index,
	FString& OutError) const
{
	const FString Path = GetIndexPath();
	const FString Directory = FPaths::GetPath(Path);
	if (!IFileManager::Get().DirectoryExists(*Directory))
	{
		IFileManager::Get().MakeDirectory(*Directory, true);
	}

	FString JsonText;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(BlueprintHelperReviewPendingIndex::IndexToJson(Index), Writer))
	{
		OutError = FString::Printf(TEXT("failed to serialize pending index: %s"), *Path);
		return false;
	}

	if (!FFileHelper::SaveStringToFile(JsonText, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("failed to write pending index: %s"), *Path);
		return false;
	}

	OutError.Reset();
	return true;
}

bool FBlueprintHelperReviewPendingIndexService::RebuildIndex(
	FBlueprintHelperReviewPendingIndex& OutIndex,
	FString& OutError) const
{
	OutIndex = FBlueprintHelperReviewPendingIndex();
	OutIndex.BuiltAtUtc = FDateTime::UtcNow().ToIso8601();

	const FString RecordsDir = FBlueprintHelperReviewConfigResolver::Load().GetReviewRecordsDir();
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(RecordsDir / TEXT("*.json")), true, false);
	for (const FString& File : Files)
	{
		const FString Path = RecordsDir / File;
		FString Content;
		if (!FFileHelper::LoadFileToString(Content, *Path))
		{
			continue;
		}

		TSharedPtr<FJsonObject> Json;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		FBlueprintHelperReviewRecord Record;
		if (!FJsonSerializer::Deserialize(Reader, Json)
			|| !FBlueprintHelperReviewStoreJsonUtils::ReadReviewRecordFromJson(Json, Record))
		{
			continue;
		}

		FBlueprintHelperReviewPendingRecordSummary Summary =
			BlueprintHelperReviewPendingIndex::MakeRecordSummary(Record);
		if (Summary.VisibleChanges.Num() > 0)
		{
			OutIndex.Records.Add(MoveTemp(Summary));
		}
	}

	OutIndex.Records.Sort([](
		const FBlueprintHelperReviewPendingRecordSummary& Left,
		const FBlueprintHelperReviewPendingRecordSummary& Right)
	{
		const FString LeftSort = !Left.SourceReviewSummary.CreatedAtLast.IsEmpty()
			? Left.SourceReviewSummary.CreatedAtLast
			: (!Left.SourceReviewSummary.CreatedAtFirst.IsEmpty() ? Left.SourceReviewSummary.CreatedAtFirst : Left.ArchiveSessionId);
		const FString RightSort = !Right.SourceReviewSummary.CreatedAtLast.IsEmpty()
			? Right.SourceReviewSummary.CreatedAtLast
			: (!Right.SourceReviewSummary.CreatedAtFirst.IsEmpty() ? Right.SourceReviewSummary.CreatedAtFirst : Right.ArchiveSessionId);
		if (LeftSort != RightSort)
		{
			return LeftSort < RightSort;
		}
		return Left.ReviewRecordId < Right.ReviewRecordId;
	});

	return SaveIndex(OutIndex, OutError);
}

bool FBlueprintHelperReviewPendingIndexService::QueryPendingVisibleChanges(
	const FBlueprintHelperReviewPendingIndexQuery& Query,
	TArray<FBlueprintHelperReviewPendingVisibleChangeSummary>& OutChanges,
	FString& OutError) const
{
	OutChanges.Reset();

	FBlueprintHelperReviewPendingIndex Index;
	if (!LoadOrRebuildIndex(Index, OutError))
	{
		return false;
	}

	const bool bSkipMissingAssetRecords = Query.bSkipMissingAssetRecords
		&& Query.AssetPathFilter.IsEmpty();
	for (const FBlueprintHelperReviewPendingRecordSummary& Record : Index.Records)
	{
		if (!BlueprintHelperReviewPendingIndex::RecordMatchesQuery(Record, Query))
		{
			continue;
		}
		if (bSkipMissingAssetRecords
			&& !FBlueprintHelperReviewStoreTargetUtils::DoesReviewAssetPackageExist(Record.AssetPath))
		{
			continue;
		}

		for (const FBlueprintHelperReviewPendingVisibleChangeSummary& Change : Record.VisibleChanges)
		{
			if (!BlueprintHelperReviewPendingIndex::ChangeMatchesQuery(Change, Query))
			{
				continue;
			}
			if (bSkipMissingAssetRecords
				&& !Change.Change.AssetPath.IsEmpty()
				&& Change.Change.AssetPath != Record.AssetPath
				&& !FBlueprintHelperReviewStoreTargetUtils::DoesReviewAssetPackageExist(Change.Change.AssetPath))
			{
				continue;
			}
			OutChanges.Add(Change);
		}
	}

	OutChanges.Sort([](
		const FBlueprintHelperReviewPendingVisibleChangeSummary& Left,
		const FBlueprintHelperReviewPendingVisibleChangeSummary& Right)
	{
		if (Left.SortKey != Right.SortKey)
		{
			return Left.SortKey < Right.SortKey;
		}
		if (Left.ReviewRecordId != Right.ReviewRecordId)
		{
			return Left.ReviewRecordId < Right.ReviewRecordId;
		}
		return Left.Change.ChangeId < Right.Change.ChangeId;
	});

	OutError.Reset();
	return true;
}

bool FBlueprintHelperReviewPendingIndexService::QueryPendingVisibleChangePage(
	const FBlueprintHelperReviewPendingIndexPageRequest& Request,
	FBlueprintHelperReviewPendingIndexPage& OutPage,
	FString& OutError) const
{
	OutPage = FBlueprintHelperReviewPendingIndexPage();

	TArray<FBlueprintHelperReviewPendingVisibleChangeSummary> AllMatches;
	if (!QueryPendingVisibleChanges(Request.Query, AllMatches, OutError))
	{
		return false;
	}

	OutPage.TotalMatchingCount = AllMatches.Num();
	const int32 ClampedPageSize = FMath::Clamp(Request.PageSize, 1, 1000);
	int32 StartIndex = 0;
	if (Request.Cursor.IsSet())
	{
		while (StartIndex < AllMatches.Num()
			&& BlueprintHelperReviewPendingIndex::ComparePendingSummaryForPage(AllMatches[StartIndex], Request.Cursor) <= 0)
		{
			++StartIndex;
		}
	}

	for (int32 Index = StartIndex; Index < AllMatches.Num() && OutPage.Changes.Num() < ClampedPageSize; ++Index)
	{
		OutPage.Changes.Add(AllMatches[Index]);
	}

	const int32 NextIndex = StartIndex + OutPage.Changes.Num();
	OutPage.bHasMore = NextIndex < AllMatches.Num();
	if (OutPage.Changes.Num() > 0)
	{
		OutPage.NextCursor = BlueprintHelperReviewPendingIndex::MakePendingPageCursor(OutPage.Changes.Last());
	}
	OutError.Reset();
	return true;
}

bool FBlueprintHelperReviewPendingIndexService::ApplyRecordSaved(
	const FBlueprintHelperReviewRecord& Record,
	FString& OutError) const
{
	FBlueprintHelperReviewPendingIndex Index;
	if (!LoadIndex(Index, OutError))
	{
		Index = FBlueprintHelperReviewPendingIndex();
		OutError.Reset();
	}

	Index.Records.RemoveAll([&Record](const FBlueprintHelperReviewPendingRecordSummary& Summary)
	{
		return Summary.ReviewRecordId == Record.ReviewRecordId;
	});

	FBlueprintHelperReviewPendingRecordSummary Summary =
		BlueprintHelperReviewPendingIndex::MakeRecordSummary(Record);
	if (Summary.VisibleChanges.Num() > 0)
	{
		Index.Records.Add(MoveTemp(Summary));
	}
	Index.BuiltAtUtc = FDateTime::UtcNow().ToIso8601();
	return SaveIndex(Index, OutError);
}

bool FBlueprintHelperReviewPendingIndexService::ApplyRecordDeleted(
	const FString& ReviewRecordId,
	FString& OutError) const
{
	if (ReviewRecordId.IsEmpty())
	{
		OutError = TEXT("review_record_id is required");
		return false;
	}

	FBlueprintHelperReviewPendingIndex Index;
	if (!LoadIndex(Index, OutError))
	{
		Index = FBlueprintHelperReviewPendingIndex();
		OutError.Reset();
	}

	const int32 RemovedCount = Index.Records.RemoveAll(
		[&ReviewRecordId](const FBlueprintHelperReviewPendingRecordSummary& Summary)
		{
			return Summary.ReviewRecordId == ReviewRecordId;
		});
	if (RemovedCount == 0)
	{
		OutError.Reset();
		return true;
	}

	Index.BuiltAtUtc = FDateTime::UtcNow().ToIso8601();
	return SaveIndex(Index, OutError);
}

bool FBlueprintHelperReviewPendingIndexService::LoadOrRebuildIndex(
	FBlueprintHelperReviewPendingIndex& OutIndex,
	FString& OutError) const
{
	if (!LoadIndex(OutIndex, OutError) || IsIndexStale(OutIndex))
	{
		return RebuildIndex(OutIndex, OutError);
	}
	OutError.Reset();
	return true;
}

bool FBlueprintHelperReviewPendingIndexService::IsIndexStale(
	const FBlueprintHelperReviewPendingIndex& Index) const
{
	const FString RecordsDir = FBlueprintHelperReviewConfigResolver::Load().GetReviewRecordsDir();
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(RecordsDir / TEXT("*.json")), true, false);

	TSet<FString> RecordFileIds;
	for (const FString& File : Files)
	{
		RecordFileIds.Add(FPaths::GetBaseFilename(File));
	}

	for (const FBlueprintHelperReviewPendingRecordSummary& Summary : Index.Records)
	{
		if (Summary.ReviewRecordId.IsEmpty())
		{
			return true;
		}
		if (!RecordFileIds.Contains(Summary.ReviewRecordId))
		{
			return true;
		}
	}

	const FDateTime IndexTimestamp = IFileManager::Get().GetTimeStamp(*GetIndexPath());
	if (IndexTimestamp == FDateTime::MinValue())
	{
		return true;
	}

	for (const FString& File : Files)
	{
		const FString Path = RecordsDir / File;
		const FDateTime RecordTimestamp = IFileManager::Get().GetTimeStamp(*Path);
		if (RecordTimestamp > IndexTimestamp)
		{
			return true;
		}
	}

	return false;
}
