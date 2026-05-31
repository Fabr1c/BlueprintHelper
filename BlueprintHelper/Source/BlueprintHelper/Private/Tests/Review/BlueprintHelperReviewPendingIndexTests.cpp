// BlueprintHelper Review pending index automation tests.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Systems/Review/BlueprintHelperReviewConfigResolver.h"
#include "Systems/Review/BlueprintHelperReviewPendingIndexService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"

namespace BlueprintHelperReviewPendingIndexTests
{
	static FString MakeUniqueArchiveId(const FString& Prefix)
	{
		return Prefix + TEXT("_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}

	static FString MakeUniqueAssetPath(const FString& Prefix)
	{
		return FString::Printf(
			TEXT("/Game/BlueprintHelperReview/%s_%s"),
			*Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static FBlueprintHelperReviewVisibleChange MakeVisibleChange(
		const FString& ChangeId,
		const FString& AssetPath,
		const FString& TargetKey)
	{
		FBlueprintHelperReviewAtomicTarget Target;
		Target.AssetPath = AssetPath;
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
		Target.GraphName = TEXT("EventGraph");
		Target.TargetKey = TargetKey;
		Target.TargetKind = TEXT("graph_node");
		Target.VisualGroupKey = TargetKey;
		Target.DisplayLabel = TargetKey;
		Target.LatestEvidenceId = ChangeId;
		Target.SourceEvidenceIds.Add(ChangeId);
		Target.BaselineHash = TEXT("baseline_hash");
		Target.RecordedAfterHash = TEXT("after_hash");
		Target.BeforeSnapshotJson = TEXT("{\"schema\":\"BlueprintHelper.ReviewTargetSnapshot.v2\",\"before\":true}");
		Target.AfterSnapshotJson = TEXT("{\"schema\":\"BlueprintHelper.ReviewTargetSnapshot.v2\",\"after\":true}");
		Target.Status = EBlueprintHelperReviewChangeStatus::Pending;

		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = ChangeId;
		Change.AssetPath = AssetPath;
		Change.GraphName = TEXT("EventGraph");
		Change.LocationKey = TargetKey;
		Change.LatestEvidenceId = ChangeId;
		Change.LatestEvidenceIds.Add(ChangeId);
		Change.SourceEvidenceIds.Add(ChangeId);
		Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
		Change.Status = EBlueprintHelperReviewChangeStatus::Pending;
		Change.DisplayLabel = TargetKey;
		Change.BeforeSummary = TEXT("before");
		Change.AfterSummary = TEXT("after");
		Change.BeforeHash = TEXT("baseline_hash");
		Change.AfterHash = TEXT("after_hash");
		Change.BeforeSnapshotJson = TEXT("{\"schema\":\"BlueprintHelper.ReviewTargetSnapshot.v2\",\"change_before\":true}");
		Change.AfterSnapshotJson = TEXT("{\"schema\":\"BlueprintHelper.ReviewTargetSnapshot.v2\",\"change_after\":true}");
		Change.AtomicTargets.Add(Target);
		return Change;
	}

	static FBlueprintHelperReviewRecord MakeRecord(
		const FString& ArchiveId,
		const FString& AssetPath,
		const FString& TaskRunId,
		const FString& ChangeId)
	{
		FBlueprintHelperReviewRecord Record;
		Record.ReviewRecordId = FBlueprintHelperReviewStoreService::MakeReviewRecordId(ArchiveId, AssetPath);
		Record.ArchiveSessionId = ArchiveId;
		Record.AssetPath = AssetPath;
		Record.SourceTaskRunIds.Add(TaskRunId);
		Record.Status = EBlueprintHelperReviewChangeStatus::Pending;
		Record.StorageStatus = EBlueprintHelperReviewStorageStatus::Active;
		Record.SourceReviewSummary.EvidenceCount = 1;
		Record.SourceReviewSummary.TaskRunIds.Add(TaskRunId);
		Record.SourceReviewSummary.AssetPaths.Add(AssetPath);
		Record.SourceReviewSummary.EvidenceIds.Add(ChangeId);
		Record.SourceReviewSummary.CreatedAtFirst = TEXT("2026-05-31T01:02:03Z");
		Record.SourceReviewSummary.CreatedAtLast = TEXT("2026-05-31T01:02:04Z");
		Record.VisibleChanges.Add(MakeVisibleChange(ChangeId, AssetPath, TEXT("graph_node:PendingIndex")));
		return Record;
	}

	static void DeleteRecordFile(const FString& ReviewRecordId)
	{
		if (ReviewRecordId.IsEmpty())
		{
			return;
		}
		const FString Path = FBlueprintHelperReviewConfigResolver::Load().GetReviewRecordsDir()
			/ FString::Printf(TEXT("%s.json"), *ReviewRecordId);
		IFileManager::Get().Delete(*Path, false, true);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingIndexSaveUpdatesSummaryTest,
	"BlueprintHelper.Review.PendingIndex.SaveUpdatesSummary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPendingIndexSaveUpdatesSummaryTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveId = BlueprintHelperReviewPendingIndexTests::MakeUniqueArchiveId(TEXT("archive_pending_index_save"));
	const FString AssetPath = BlueprintHelperReviewPendingIndexTests::MakeUniqueAssetPath(TEXT("BP_PendingIndexSave"));
	const FString TaskRunId = TEXT("task_pending_index_save");
	FBlueprintHelperReviewRecord Record = BlueprintHelperReviewPendingIndexTests::MakeRecord(
		ArchiveId,
		AssetPath,
		TaskRunId,
		TEXT("tx_pending_index_save"));

	BlueprintHelperReviewPendingIndexTests::DeleteRecordFile(Record.ReviewRecordId);

	FString SaveError;
	TestTrue(TEXT("record saves and updates pending index"), Store.SaveReviewRecord(Record, SaveError));

	FBlueprintHelperReviewPendingIndexQuery Query;
	Query.AssetPathFilter = AssetPath;
	Query.TaskRunIdFilter = TaskRunId;
	Query.bSkipMissingAssetRecords = false;

	const TArray<FBlueprintHelperReviewPendingVisibleChangeSummary> Summaries =
		Store.QueryPendingVisibleChangeSummaries(Query);
	TestEqual(TEXT("one pending summary is indexed"), Summaries.Num(), 1);
	if (Summaries.Num() == 1)
	{
		TestEqual(TEXT("summary keeps review record id"), Summaries[0].ReviewRecordId, Record.ReviewRecordId);
		TestEqual(TEXT("summary keeps change id"), Summaries[0].Change.ChangeId, FString(TEXT("tx_pending_index_save")));
		TestEqual(TEXT("summary keeps target key"),
			Summaries[0].Change.AtomicTargets[0].TargetKey,
			FString(TEXT("graph_node:PendingIndex")));
		TestTrue(TEXT("summary strips change before snapshot"),
			Summaries[0].Change.BeforeSnapshotJson.IsEmpty());
		TestTrue(TEXT("summary strips target after snapshot"),
			Summaries[0].Change.AtomicTargets[0].AfterSnapshotJson.IsEmpty());
	}

	FBlueprintHelperReviewPendingIndexService IndexService;
	FString IndexText;
	TestTrue(TEXT("pending index file can be read"),
		FFileHelper::LoadFileToString(IndexText, *IndexService.GetIndexPath()));
	TestTrue(TEXT("pending index contains review record id"),
		IndexText.Contains(Record.ReviewRecordId));
	TestFalse(TEXT("pending index does not persist snapshot payload"),
		IndexText.Contains(TEXT("change_before")));

	FString DeleteError;
	Store.DeleteReviewRecord(Record.ReviewRecordId, DeleteError);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingIndexDeleteRemovesSummaryTest,
	"BlueprintHelper.Review.PendingIndex.DeleteRemovesSummary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPendingIndexDeleteRemovesSummaryTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveId = BlueprintHelperReviewPendingIndexTests::MakeUniqueArchiveId(TEXT("archive_pending_index_delete"));
	const FString AssetPath = BlueprintHelperReviewPendingIndexTests::MakeUniqueAssetPath(TEXT("BP_PendingIndexDelete"));
	FBlueprintHelperReviewRecord Record = BlueprintHelperReviewPendingIndexTests::MakeRecord(
		ArchiveId,
		AssetPath,
		TEXT("task_pending_index_delete"),
		TEXT("tx_pending_index_delete"));

	BlueprintHelperReviewPendingIndexTests::DeleteRecordFile(Record.ReviewRecordId);

	FString SaveError;
	TestTrue(TEXT("record saves before delete"), Store.SaveReviewRecord(Record, SaveError));

	FBlueprintHelperReviewPendingIndexQuery Query;
	Query.AssetPathFilter = AssetPath;
	Query.bSkipMissingAssetRecords = false;
	TestEqual(TEXT("summary exists before delete"), Store.QueryPendingVisibleChangeSummaries(Query).Num(), 1);

	FString DeleteError;
	TestTrue(TEXT("record delete succeeds"), Store.DeleteReviewRecord(Record.ReviewRecordId, DeleteError));
	TestEqual(TEXT("summary is removed after delete"), Store.QueryPendingVisibleChangeSummaries(Query).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingIndexLoadPendingUsesIndexTest,
	"BlueprintHelper.Review.PendingIndex.LoadPendingUsesIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPendingIndexLoadPendingUsesIndexTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveId = BlueprintHelperReviewPendingIndexTests::MakeUniqueArchiveId(TEXT("archive_pending_index_load"));
	const FString AssetPath = BlueprintHelperReviewPendingIndexTests::MakeUniqueAssetPath(TEXT("BP_PendingIndexLoad"));
	FBlueprintHelperReviewRecord Record = BlueprintHelperReviewPendingIndexTests::MakeRecord(
		ArchiveId,
		AssetPath,
		TEXT("task_pending_index_load"),
		TEXT("tx_pending_index_load"));

	BlueprintHelperReviewPendingIndexTests::DeleteRecordFile(Record.ReviewRecordId);

	FString SaveError;
	TestTrue(TEXT("record saves before indexed load"), Store.SaveReviewRecord(Record, SaveError));

	const TArray<FBlueprintHelperReviewVisibleChange> PendingChanges =
		Store.LoadPendingVisibleChanges(AssetPath);
	TestEqual(TEXT("explicit asset pending load returns indexed change"), PendingChanges.Num(), 1);
	if (PendingChanges.Num() == 1)
	{
		TestEqual(TEXT("indexed pending load keeps change id"),
			PendingChanges[0].ChangeId,
			FString(TEXT("tx_pending_index_load")));
		TestTrue(TEXT("indexed pending load stays lightweight"),
			PendingChanges[0].AfterSnapshotJson.IsEmpty());
	}

	FString DeleteError;
	Store.DeleteReviewRecord(Record.ReviewRecordId, DeleteError);
	return true;
}

#endif
