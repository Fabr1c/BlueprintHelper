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

	static FBlueprintHelperReviewVisibleChange MakeAssetFactoryRootChange(
		const FString& ChangeId,
		const FString& AssetPath)
	{
		FBlueprintHelperReviewVisibleChange Change = MakeVisibleChange(
			ChangeId,
			AssetPath,
			TEXT("asset_factory:create_asset"));
		Change.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
		Change.DisplayLabel = TEXT("blueprint_class");
		Change.bIsAssetLifecycleRoot = true;
		Change.bRejectRemovesChildren = true;
		Change.ParentChangeId.Reset();
		Change.AtomicTargets[0].TargetKind = TEXT("asset_factory");
		Change.AtomicTargets[0].TargetKey = TEXT("asset_factory:create_asset");
		Change.AtomicTargets[0].DisplayLabel = TEXT("blueprint_class");
		Change.AtomicTargets[0].Surface = EBlueprintHelperReviewSurface::Unknown;
		return Change;
	}

	static FBlueprintHelperReviewVisibleChange MakeVariableChange(
		const FString& ChangeId,
		const FString& AssetPath,
		const FString& VariableName)
	{
		FBlueprintHelperReviewVisibleChange Change = MakeVisibleChange(
			ChangeId,
			AssetPath,
			FString::Printf(TEXT("blueprint_variable:%s"), *VariableName));
		Change.DisplayLabel = FString::Printf(TEXT("variable %s"), *VariableName);
		Change.AtomicTargets[0].TargetKind = TEXT("blueprint_variable");
		Change.AtomicTargets[0].TargetKey = FString::Printf(TEXT("blueprint_variable:%s"), *VariableName);
		Change.AtomicTargets[0].DisplayLabel = Change.DisplayLabel;
		Change.AtomicTargets[0].Surface = EBlueprintHelperReviewSurface::MyBlueprint;
		return Change;
	}

	static FBlueprintHelperReviewVisibleChange MakeComponentRootChange(
		const FString& ChangeId,
		const FString& AssetPath,
		const FString& ComponentName)
	{
		FBlueprintHelperReviewVisibleChange Change = MakeVisibleChange(
			ChangeId,
			AssetPath,
			FString::Printf(TEXT("component:%s"), *ComponentName));
		Change.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
		Change.DisplayLabel = ComponentName;
		Change.bIsAssetLifecycleRoot = true;
		Change.bRejectRemovesChildren = true;
		Change.AtomicTargets[0].TargetKind = TEXT("component");
		Change.AtomicTargets[0].TargetKey = FString::Printf(TEXT("component:%s"), *ComponentName);
		Change.AtomicTargets[0].ComponentPath = ComponentName;
		Change.AtomicTargets[0].DisplayLabel = ComponentName;
		Change.AtomicTargets[0].Surface = EBlueprintHelperReviewSurface::Components;
		return Change;
	}

	static FBlueprintHelperReviewVisibleChange MakeComponentRootChangeWithParent(
		const FString& ChangeId,
		const FString& AssetPath,
		const FString& ComponentName,
		const FString& ParentComponentName)
	{
		FBlueprintHelperReviewVisibleChange Change = MakeComponentRootChange(ChangeId, AssetPath, ComponentName);
		const FString AfterSnapshot = FString::Printf(
			TEXT("{\"schema\":\"BlueprintHelper.ReviewTargetSnapshot.v2\",\"target_kind\":\"component\",\"target_key\":\"component:%s\",\"exists\":true,\"name\":\"%s\",\"parent_component\":\"%s\"}"),
			*ComponentName,
			*ComponentName,
			*ParentComponentName);
		Change.AfterSnapshotJson = AfterSnapshot;
		Change.AtomicTargets[0].AfterSnapshotJson = AfterSnapshot;
		Change.AtomicTargets[0].AnchorJson = FString::Printf(
			TEXT("{\"component_name\":\"%s\",\"parent_component\":\"%s\"}"),
			*ComponentName,
			*ParentComponentName);
		return Change;
	}

	static FBlueprintHelperReviewVisibleChange MakeWidgetRootChange(
		const FString& ChangeId,
		const FString& AssetPath,
		const FString& WidgetName)
	{
		FBlueprintHelperReviewVisibleChange Change = MakeVisibleChange(
			ChangeId,
			AssetPath,
			FString::Printf(TEXT("umg_widget:%s"), *WidgetName));
		Change.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
		Change.DisplayLabel = WidgetName;
		Change.bIsAssetLifecycleRoot = true;
		Change.bRejectRemovesChildren = true;
		Change.AtomicTargets[0].TargetKind = TEXT("umg_widget");
		Change.AtomicTargets[0].TargetKey = FString::Printf(TEXT("umg_widget:%s"), *WidgetName);
		Change.AtomicTargets[0].PropertyPath = WidgetName;
		Change.AtomicTargets[0].DisplayLabel = WidgetName;
		Change.AtomicTargets[0].Surface = EBlueprintHelperReviewSurface::UMGWidgetTree;
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

	static FBlueprintHelperReviewRecord MakeRecordWithChange(
		const FString& ArchiveId,
		const FString& AssetPath,
		const FString& TaskRunId,
		const FBlueprintHelperReviewVisibleChange& Change)
	{
		FBlueprintHelperReviewRecord Record = MakeRecord(
			ArchiveId,
			AssetPath,
			TaskRunId,
			Change.ChangeId);
		Record.VisibleChanges.Reset();
		Record.VisibleChanges.Add(Change);
		Record.SourceReviewSummary.EvidenceIds.Reset();
		Record.SourceReviewSummary.EvidenceIds.Add(Change.ChangeId);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingIndexQueryPageUsesStableCursorTest,
	"BlueprintHelper.Review.PendingIndex.QueryPageUsesStableCursor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPendingIndexQueryPageUsesStableCursorTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveId = BlueprintHelperReviewPendingIndexTests::MakeUniqueArchiveId(TEXT("archive_pending_index_page"));
	const FString AssetPath = BlueprintHelperReviewPendingIndexTests::MakeUniqueAssetPath(TEXT("BP_PendingIndexPage"));
	const FString TaskRunId = TEXT("task_pending_index_page");
	TArray<FString> ReviewRecordIds;

	for (int32 Index = 0; Index < 5; ++Index)
	{
		const FString ChangeId = FString::Printf(TEXT("tx_pending_index_page_%02d"), Index);
		FBlueprintHelperReviewRecord Record = BlueprintHelperReviewPendingIndexTests::MakeRecord(
			ArchiveId,
			AssetPath,
			TaskRunId,
			ChangeId);
		Record.ReviewRecordId = FBlueprintHelperReviewStoreService::MakeReviewRecordId(
			ArchiveId + FString::Printf(TEXT("_%02d"), Index),
			AssetPath);
		Record.VisibleChanges[0].ChangeId = ChangeId;
		Record.VisibleChanges[0].LatestEvidenceId = ChangeId;
		Record.VisibleChanges[0].DisplayLabel = ChangeId;
		ReviewRecordIds.Add(Record.ReviewRecordId);

		FString SaveError;
		TestTrue(TEXT("record saves before page query"), Store.SaveReviewRecord(Record, SaveError));
	}

	FBlueprintHelperReviewPendingIndexQuery Query;
	Query.AssetPathFilter = AssetPath;
	Query.TaskRunIdFilter = TaskRunId;
	Query.bSkipMissingAssetRecords = false;

	FBlueprintHelperReviewPendingIndexPageRequest FirstRequest;
	FirstRequest.Query = Query;
	FirstRequest.PageSize = 2;

	FBlueprintHelperReviewPendingIndexPage FirstPage;
	FString Error;
	FBlueprintHelperReviewPendingIndexService IndexService;
	TestTrue(TEXT("first page query succeeds"), IndexService.QueryPendingVisibleChangePage(FirstRequest, FirstPage, Error));
	TestEqual(TEXT("first page returns requested page size"), FirstPage.Changes.Num(), 2);
	TestTrue(TEXT("first page reports more rows"), FirstPage.bHasMore);
	TestTrue(TEXT("first page emits cursor"), FirstPage.NextCursor.IsSet());
	TestEqual(TEXT("first page total count is all matching rows"), FirstPage.TotalMatchingCount, 5);

	FBlueprintHelperReviewPendingIndexPageRequest SecondRequest = FirstRequest;
	SecondRequest.Cursor = FirstPage.NextCursor;
	FBlueprintHelperReviewPendingIndexPage SecondPage;
	TestTrue(TEXT("second page query succeeds"), IndexService.QueryPendingVisibleChangePage(SecondRequest, SecondPage, Error));
	TestEqual(TEXT("second page returns requested page size"), SecondPage.Changes.Num(), 2);
	TestNotEqual(TEXT("second page starts after first page cursor"),
		SecondPage.Changes[0].Change.ChangeId,
		FirstPage.Changes[0].Change.ChangeId);
	TestTrue(TEXT("second page still reports more rows"), SecondPage.bHasMore);

	FBlueprintHelperReviewPendingIndexPageRequest ThirdRequest = FirstRequest;
	ThirdRequest.Cursor = SecondPage.NextCursor;
	FBlueprintHelperReviewPendingIndexPage ThirdPage;
	TestTrue(TEXT("third page query succeeds"), IndexService.QueryPendingVisibleChangePage(ThirdRequest, ThirdPage, Error));
	TestEqual(TEXT("third page returns remaining row"), ThirdPage.Changes.Num(), 1);
	TestFalse(TEXT("third page has no more rows"), ThirdPage.bHasMore);

	for (const FString& ReviewRecordId : ReviewRecordIds)
	{
		FString DeleteError;
		Store.DeleteReviewRecord(ReviewRecordId, DeleteError);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingIndexPageLinksCrossRecordAssetRootTest,
	"BlueprintHelper.Review.PendingIndex.PageLinksCrossRecordAssetRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPendingIndexPageLinksCrossRecordAssetRootTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString AssetPath = BlueprintHelperReviewPendingIndexTests::MakeUniqueAssetPath(TEXT("BP_PendingIndexAssetRoot"));

	FBlueprintHelperReviewRecord RootRecord = BlueprintHelperReviewPendingIndexTests::MakeRecordWithChange(
		BlueprintHelperReviewPendingIndexTests::MakeUniqueArchiveId(TEXT("archive_pending_index_root")),
		AssetPath,
		TEXT("task_pending_index_root"),
		BlueprintHelperReviewPendingIndexTests::MakeAssetFactoryRootChange(TEXT("tx_pending_index_asset_root"), AssetPath));
	RootRecord.SourceReviewSummary.CreatedAtFirst.Reset();
	RootRecord.SourceReviewSummary.CreatedAtLast.Reset();

	FBlueprintHelperReviewRecord ChildRecord = BlueprintHelperReviewPendingIndexTests::MakeRecordWithChange(
		BlueprintHelperReviewPendingIndexTests::MakeUniqueArchiveId(TEXT("archive_pending_index_child")),
		AssetPath,
		TEXT("task_pending_index_child"),
		BlueprintHelperReviewPendingIndexTests::MakeVariableChange(
			TEXT("tx_pending_index_child_variable"),
			AssetPath,
			TEXT("PendingIndexChild")));
	ChildRecord.SourceReviewSummary.CreatedAtFirst = TEXT("2026-06-01T01:02:03Z");
	ChildRecord.SourceReviewSummary.CreatedAtLast = TEXT("2026-06-01T01:02:04Z");

	BlueprintHelperReviewPendingIndexTests::DeleteRecordFile(RootRecord.ReviewRecordId);
	BlueprintHelperReviewPendingIndexTests::DeleteRecordFile(ChildRecord.ReviewRecordId);

	FString SaveError;
	TestTrue(TEXT("root record saves"), Store.SaveReviewRecord(RootRecord, SaveError));
	TestTrue(TEXT("child record saves"), Store.SaveReviewRecord(ChildRecord, SaveError));

	FBlueprintHelperReviewPendingIndexQuery Query;
	Query.AssetPathFilter = AssetPath;
	Query.bSkipMissingAssetRecords = false;

	FBlueprintHelperReviewPendingIndexPageRequest Request;
	Request.Query = Query;
	Request.PageSize = 10;

	FBlueprintHelperReviewPendingIndexPage Page;
	FString Error;
	FBlueprintHelperReviewPendingIndexService IndexService;
	TestTrue(TEXT("page query succeeds"), IndexService.QueryPendingVisibleChangePage(Request, Page, Error));
	TestEqual(TEXT("page includes root and child"), Page.Changes.Num(), 2);
	if (Page.Changes.Num() == 2)
	{
		TestEqual(TEXT("asset lifecycle root sorts before same-asset child"),
			Page.Changes[0].Change.ChangeId,
			FString(TEXT("tx_pending_index_asset_root")));
		TestEqual(TEXT("child points to asset lifecycle root"),
			Page.Changes[1].Change.ParentChangeId,
			FString(TEXT("tx_pending_index_asset_root")));
	}

	FString DeleteError;
	Store.DeleteReviewRecord(RootRecord.ReviewRecordId, DeleteError);
	Store.DeleteReviewRecord(ChildRecord.ReviewRecordId, DeleteError);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingIndexPageLinksComponentAndWidgetRootsToAssetRootTest,
	"BlueprintHelper.Review.PendingIndex.PageLinksComponentAndWidgetRootsToAssetRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPendingIndexPageLinksComponentAndWidgetRootsToAssetRootTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString AssetPath = BlueprintHelperReviewPendingIndexTests::MakeUniqueAssetPath(TEXT("BP_PendingIndexStructureRoots"));

	FBlueprintHelperReviewRecord AssetRecord = BlueprintHelperReviewPendingIndexTests::MakeRecordWithChange(
		BlueprintHelperReviewPendingIndexTests::MakeUniqueArchiveId(TEXT("archive_pending_index_asset_root")),
		AssetPath,
		TEXT("task_pending_index_asset_root"),
		BlueprintHelperReviewPendingIndexTests::MakeAssetFactoryRootChange(TEXT("tx_pending_index_asset_root_structure"), AssetPath));
	AssetRecord.SourceReviewSummary.CreatedAtFirst.Reset();
	AssetRecord.SourceReviewSummary.CreatedAtLast.Reset();

	FBlueprintHelperReviewRecord ComponentRecord = BlueprintHelperReviewPendingIndexTests::MakeRecordWithChange(
		BlueprintHelperReviewPendingIndexTests::MakeUniqueArchiveId(TEXT("archive_pending_index_component_root")),
		AssetPath,
		TEXT("task_pending_index_component_root"),
		BlueprintHelperReviewPendingIndexTests::MakeComponentRootChange(
			TEXT("tx_pending_index_component_root"),
			AssetPath,
			TEXT("PendingIndexComp")));
	ComponentRecord.SourceReviewSummary.CreatedAtFirst = TEXT("2026-06-01T02:02:03Z");
	ComponentRecord.SourceReviewSummary.CreatedAtLast = TEXT("2026-06-01T02:02:04Z");

	FBlueprintHelperReviewRecord WidgetRecord = BlueprintHelperReviewPendingIndexTests::MakeRecordWithChange(
		BlueprintHelperReviewPendingIndexTests::MakeUniqueArchiveId(TEXT("archive_pending_index_widget_root")),
		AssetPath,
		TEXT("task_pending_index_widget_root"),
		BlueprintHelperReviewPendingIndexTests::MakeWidgetRootChange(
			TEXT("tx_pending_index_widget_root"),
			AssetPath,
			TEXT("PendingIndexWidget")));
	WidgetRecord.SourceReviewSummary.CreatedAtFirst = TEXT("2026-06-01T02:03:03Z");
	WidgetRecord.SourceReviewSummary.CreatedAtLast = TEXT("2026-06-01T02:03:04Z");

	BlueprintHelperReviewPendingIndexTests::DeleteRecordFile(AssetRecord.ReviewRecordId);
	BlueprintHelperReviewPendingIndexTests::DeleteRecordFile(ComponentRecord.ReviewRecordId);
	BlueprintHelperReviewPendingIndexTests::DeleteRecordFile(WidgetRecord.ReviewRecordId);

	FString SaveError;
	TestTrue(TEXT("asset root record saves"), Store.SaveReviewRecord(AssetRecord, SaveError));
	TestTrue(TEXT("component root record saves"), Store.SaveReviewRecord(ComponentRecord, SaveError));
	TestTrue(TEXT("widget root record saves"), Store.SaveReviewRecord(WidgetRecord, SaveError));

	FBlueprintHelperReviewPendingIndexQuery Query;
	Query.AssetPathFilter = AssetPath;
	Query.bSkipMissingAssetRecords = false;

	FBlueprintHelperReviewPendingIndexPageRequest Request;
	Request.Query = Query;
	Request.PageSize = 10;

	FBlueprintHelperReviewPendingIndexPage Page;
	FString Error;
	FBlueprintHelperReviewPendingIndexService IndexService;
	TestTrue(TEXT("page query succeeds"), IndexService.QueryPendingVisibleChangePage(Request, Page, Error));
	TestEqual(TEXT("page includes asset, component, and widget roots"), Page.Changes.Num(), 3);
	if (Page.Changes.Num() == 3)
	{
		TestEqual(TEXT("asset lifecycle root sorts first"),
			Page.Changes[0].Change.ChangeId,
			FString(TEXT("tx_pending_index_asset_root_structure")));
		TestEqual(TEXT("component root falls back to asset root parent"),
			Page.Changes[1].Change.ParentChangeId,
			FString(TEXT("tx_pending_index_asset_root_structure")));
		TestEqual(TEXT("widget root falls back to asset root parent"),
			Page.Changes[2].Change.ParentChangeId,
			FString(TEXT("tx_pending_index_asset_root_structure")));
	}

	FString DeleteError;
	Store.DeleteReviewRecord(AssetRecord.ReviewRecordId, DeleteError);
	Store.DeleteReviewRecord(ComponentRecord.ReviewRecordId, DeleteError);
	Store.DeleteReviewRecord(WidgetRecord.ReviewRecordId, DeleteError);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingIndexPageLinksCrossRecordComponentParentAfterSnapshotStripTest,
	"BlueprintHelper.Review.PendingIndex.PageLinksCrossRecordComponentParentAfterSnapshotStrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPendingIndexPageLinksCrossRecordComponentParentAfterSnapshotStripTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString AssetPath = BlueprintHelperReviewPendingIndexTests::MakeUniqueAssetPath(TEXT("BP_PendingIndexComponentParent"));

	FBlueprintHelperReviewRecord ParentRecord = BlueprintHelperReviewPendingIndexTests::MakeRecordWithChange(
		BlueprintHelperReviewPendingIndexTests::MakeUniqueArchiveId(TEXT("archive_pending_index_parent_component")),
		AssetPath,
		TEXT("task_pending_index_parent_component"),
		BlueprintHelperReviewPendingIndexTests::MakeComponentRootChange(
			TEXT("tx_pending_index_parent_component"),
			AssetPath,
			TEXT("ParentComp")));

	FBlueprintHelperReviewRecord ChildRecord = BlueprintHelperReviewPendingIndexTests::MakeRecordWithChange(
		BlueprintHelperReviewPendingIndexTests::MakeUniqueArchiveId(TEXT("archive_pending_index_child_component")),
		AssetPath,
		TEXT("task_pending_index_child_component"),
		BlueprintHelperReviewPendingIndexTests::MakeComponentRootChangeWithParent(
			TEXT("tx_pending_index_child_component"),
			AssetPath,
			TEXT("ChildComp"),
			TEXT("ParentComp")));

	BlueprintHelperReviewPendingIndexTests::DeleteRecordFile(ParentRecord.ReviewRecordId);
	BlueprintHelperReviewPendingIndexTests::DeleteRecordFile(ChildRecord.ReviewRecordId);

	FString SaveError;
	TestTrue(TEXT("parent record saves"), Store.SaveReviewRecord(ParentRecord, SaveError));
	TestTrue(TEXT("child record saves"), Store.SaveReviewRecord(ChildRecord, SaveError));

	FBlueprintHelperReviewPendingIndexQuery Query;
	Query.AssetPathFilter = AssetPath;
	Query.bSkipMissingAssetRecords = false;

	FBlueprintHelperReviewPendingIndexPageRequest Request;
	Request.Query = Query;
	Request.PageSize = 10;

	FBlueprintHelperReviewPendingIndexPage Page;
	FString Error;
	FBlueprintHelperReviewPendingIndexService IndexService;
	TestTrue(TEXT("page query succeeds"), IndexService.QueryPendingVisibleChangePage(Request, Page, Error));
	TestEqual(TEXT("page includes parent and child"), Page.Changes.Num(), 2);
	if (Page.Changes.Num() == 2)
	{
		const FBlueprintHelperReviewPendingVisibleChangeSummary* ParentSummary = Page.Changes.FindByPredicate(
			[](const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary)
			{
				return Summary.Change.DisplayLabel == TEXT("ParentComp");
			});
		const FBlueprintHelperReviewPendingVisibleChangeSummary* ChildSummary = Page.Changes.FindByPredicate(
			[](const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary)
			{
				return Summary.Change.DisplayLabel == TEXT("ChildComp");
			});
		const FBlueprintHelperReviewVisibleChange* Parent = ParentSummary ? &ParentSummary->Change : nullptr;
		const FBlueprintHelperReviewVisibleChange* Child = ChildSummary ? &ChildSummary->Change : nullptr;

		TestNotNull(TEXT("parent exists"), Parent);
		TestNotNull(TEXT("child exists"), Child);
		if (Parent && Child)
		{
			TestEqual(TEXT("child keeps parent change id after pending index snapshot strip"),
				Child->ParentChangeId,
				Parent->ChangeId);
		}
	}

	FString DeleteError;
	Store.DeleteReviewRecord(ParentRecord.ReviewRecordId, DeleteError);
	Store.DeleteReviewRecord(ChildRecord.ReviewRecordId, DeleteError);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingIndexPageLinksRejectFailedLifecycleRootTest,
	"BlueprintHelper.Review.PendingIndex.PageLinksRejectFailedLifecycleRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPendingIndexPageLinksRejectFailedLifecycleRootTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString AssetPath = BlueprintHelperReviewPendingIndexTests::MakeUniqueAssetPath(TEXT("BP_PendingIndexRejectFailedRoot"));

	FBlueprintHelperReviewRecord ParentRecord = BlueprintHelperReviewPendingIndexTests::MakeRecordWithChange(
		BlueprintHelperReviewPendingIndexTests::MakeUniqueArchiveId(TEXT("archive_pending_index_reject_failed_parent")),
		AssetPath,
		TEXT("task_pending_index_reject_failed_parent"),
		BlueprintHelperReviewPendingIndexTests::MakeComponentRootChange(
			TEXT("tx_pending_index_reject_failed_parent"),
			AssetPath,
			TEXT("ParentComp")));
	ParentRecord.Status = EBlueprintHelperReviewChangeStatus::RejectFailed;
	ParentRecord.VisibleChanges[0].Status = EBlueprintHelperReviewChangeStatus::RejectFailed;
	ParentRecord.VisibleChanges[0].AtomicTargets[0].Status = EBlueprintHelperReviewChangeStatus::RejectFailed;
	ParentRecord.VisibleChanges[0].NeedsActionReason = TEXT("snapshot_restore_component_has_children:ParentComp");

	FBlueprintHelperReviewRecord ChildRecord = BlueprintHelperReviewPendingIndexTests::MakeRecordWithChange(
		BlueprintHelperReviewPendingIndexTests::MakeUniqueArchiveId(TEXT("archive_pending_index_reject_failed_child")),
		AssetPath,
		TEXT("task_pending_index_reject_failed_child"),
		BlueprintHelperReviewPendingIndexTests::MakeComponentRootChangeWithParent(
			TEXT("tx_pending_index_reject_failed_child"),
			AssetPath,
			TEXT("ChildComp"),
			TEXT("ParentComp")));

	BlueprintHelperReviewPendingIndexTests::DeleteRecordFile(ParentRecord.ReviewRecordId);
	BlueprintHelperReviewPendingIndexTests::DeleteRecordFile(ChildRecord.ReviewRecordId);

	FString SaveError;
	TestTrue(TEXT("reject failed parent record saves"), Store.SaveReviewRecord(ParentRecord, SaveError));
	TestTrue(TEXT("child record saves"), Store.SaveReviewRecord(ChildRecord, SaveError));

	FBlueprintHelperReviewPendingIndexQuery Query;
	Query.AssetPathFilter = AssetPath;
	Query.bSkipMissingAssetRecords = false;

	FBlueprintHelperReviewPendingIndexPageRequest Request;
	Request.Query = Query;
	Request.PageSize = 10;

	FBlueprintHelperReviewPendingIndexPage Page;
	FString Error;
	FBlueprintHelperReviewPendingIndexService IndexService;
	TestTrue(TEXT("page query succeeds"), IndexService.QueryPendingVisibleChangePage(Request, Page, Error));
	TestEqual(TEXT("page includes reject failed parent and child"), Page.Changes.Num(), 2);
	if (Page.Changes.Num() == 2)
	{
		const FBlueprintHelperReviewPendingVisibleChangeSummary* ParentSummary = Page.Changes.FindByPredicate(
			[](const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary)
			{
				return Summary.Change.DisplayLabel == TEXT("ParentComp");
			});
		const FBlueprintHelperReviewPendingVisibleChangeSummary* ChildSummary = Page.Changes.FindByPredicate(
			[](const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary)
			{
				return Summary.Change.DisplayLabel == TEXT("ChildComp");
			});
		const FBlueprintHelperReviewVisibleChange* Parent = ParentSummary ? &ParentSummary->Change : nullptr;
		const FBlueprintHelperReviewVisibleChange* Child = ChildSummary ? &ChildSummary->Change : nullptr;

		TestNotNull(TEXT("reject failed parent exists"), Parent);
		TestNotNull(TEXT("child exists"), Child);
		if (Parent && Child)
		{
			TestEqual(TEXT("child links to reject failed lifecycle root"),
				Child->ParentChangeId,
				Parent->ChangeId);
		}
	}

	FString DeleteError;
	Store.DeleteReviewRecord(ParentRecord.ReviewRecordId, DeleteError);
	Store.DeleteReviewRecord(ChildRecord.ReviewRecordId, DeleteError);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingIndexPageLinksComponentParentFromSnapshotOnlyTest,
	"BlueprintHelper.Review.PendingIndex.PageLinksComponentParentFromSnapshotOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPendingIndexPageLinksComponentParentFromSnapshotOnlyTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString AssetPath = BlueprintHelperReviewPendingIndexTests::MakeUniqueAssetPath(TEXT("BP_PendingIndexSnapshotOnlyParent"));

	FBlueprintHelperReviewRecord ParentRecord = BlueprintHelperReviewPendingIndexTests::MakeRecordWithChange(
		BlueprintHelperReviewPendingIndexTests::MakeUniqueArchiveId(TEXT("archive_pending_index_snapshot_parent")),
		AssetPath,
		TEXT("task_pending_index_snapshot_parent"),
		BlueprintHelperReviewPendingIndexTests::MakeComponentRootChange(
			TEXT("tx_pending_index_snapshot_parent"),
			AssetPath,
			TEXT("ParentComp")));

	FBlueprintHelperReviewVisibleChange SnapshotOnlyChild =
		BlueprintHelperReviewPendingIndexTests::MakeComponentRootChangeWithParent(
			TEXT("tx_pending_index_snapshot_child"),
			AssetPath,
			TEXT("ChildComp"),
			TEXT("ParentComp"));
	SnapshotOnlyChild.AtomicTargets[0].AnchorJson.Reset();
	SnapshotOnlyChild.AtomicTargets[0].LifecycleObjectKey.Reset();
	SnapshotOnlyChild.AtomicTargets[0].LifecycleParentKey.Reset();
	FBlueprintHelperReviewRecord ChildRecord = BlueprintHelperReviewPendingIndexTests::MakeRecordWithChange(
		BlueprintHelperReviewPendingIndexTests::MakeUniqueArchiveId(TEXT("archive_pending_index_snapshot_child")),
		AssetPath,
		TEXT("task_pending_index_snapshot_child"),
		SnapshotOnlyChild);

	BlueprintHelperReviewPendingIndexTests::DeleteRecordFile(ParentRecord.ReviewRecordId);
	BlueprintHelperReviewPendingIndexTests::DeleteRecordFile(ChildRecord.ReviewRecordId);

	FString SaveError;
	TestTrue(TEXT("snapshot-only parent record saves"), Store.SaveReviewRecord(ParentRecord, SaveError));
	TestTrue(TEXT("snapshot-only child record saves"), Store.SaveReviewRecord(ChildRecord, SaveError));

	FBlueprintHelperReviewPendingIndexQuery Query;
	Query.AssetPathFilter = AssetPath;
	Query.bSkipMissingAssetRecords = false;

	FBlueprintHelperReviewPendingIndexPageRequest Request;
	Request.Query = Query;
	Request.PageSize = 10;

	FBlueprintHelperReviewPendingIndexPage Page;
	FString Error;
	FBlueprintHelperReviewPendingIndexService IndexService;
	TestTrue(TEXT("page query succeeds"), IndexService.QueryPendingVisibleChangePage(Request, Page, Error));
	TestEqual(TEXT("page includes snapshot-only parent and child"), Page.Changes.Num(), 2);
	if (Page.Changes.Num() == 2)
	{
		const FBlueprintHelperReviewPendingVisibleChangeSummary* ParentSummary = Page.Changes.FindByPredicate(
			[](const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary)
			{
				return Summary.Change.DisplayLabel == TEXT("ParentComp");
			});
		const FBlueprintHelperReviewPendingVisibleChangeSummary* ChildSummary = Page.Changes.FindByPredicate(
			[](const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary)
			{
				return Summary.Change.DisplayLabel == TEXT("ChildComp");
			});
		const FBlueprintHelperReviewVisibleChange* Parent = ParentSummary ? &ParentSummary->Change : nullptr;
		const FBlueprintHelperReviewVisibleChange* Child = ChildSummary ? &ChildSummary->Change : nullptr;

		TestNotNull(TEXT("snapshot-only parent exists"), Parent);
		TestNotNull(TEXT("snapshot-only child exists"), Child);
		if (Parent && Child)
		{
			TestEqual(TEXT("snapshot-only child links to parent after pending index strip"),
				Child->ParentChangeId,
				Parent->ChangeId);
		}
	}

	FString DeleteError;
	Store.DeleteReviewRecord(ParentRecord.ReviewRecordId, DeleteError);
	Store.DeleteReviewRecord(ChildRecord.ReviewRecordId, DeleteError);
	return true;
}

#endif
