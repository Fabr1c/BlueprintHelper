// BlueprintHelper Review pending load coordinator automation tests.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "UI/BlueprintHelperUiSettings.h"
#include "UI/Review/BlueprintHelperReviewPendingLoadCoordinator.h"

namespace BlueprintHelperReviewPendingLoadCoordinatorTests
{
	struct FPendingLoadLatentState
	{
		bool bDone = false;
		double StartSeconds = FPlatformTime::Seconds();
		FBlueprintHelperReviewPendingLoadResult Result;
	};

	static FBlueprintHelperReviewVisibleChange MakeVisibleChange(
		const FString& ChangeId,
		const FString& AssetPath)
	{
		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = ChangeId;
		Change.AssetPath = AssetPath;
		Change.GraphName = TEXT("EventGraph");
		Change.LocationKey = TEXT("graph_node:") + ChangeId;
		Change.DisplayLabel = ChangeId;
		Change.LatestEvidenceId = ChangeId;
		Change.Status = EBlueprintHelperReviewChangeStatus::Pending;
		Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
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
		Record.SourceReviewSummary.TaskRunIds.Add(TaskRunId);
		Record.SourceReviewSummary.AssetPaths.Add(AssetPath);
		Record.SourceReviewSummary.CreatedAtFirst = ArchiveId;
		Record.SourceReviewSummary.CreatedAtLast = ArchiveId;
		Record.Status = EBlueprintHelperReviewChangeStatus::Pending;
		Record.StorageStatus = EBlueprintHelperReviewStorageStatus::Active;
		Record.VisibleChanges.Add(MakeVisibleChange(ChangeId, AssetPath));
		return Record;
	}

	static void CreatePendingRecords(
		FBlueprintHelperReviewStoreService& Store,
		FAutomationTestBase& Test,
		const FString& AssetPath,
		const FString& TaskRunId,
		int32 Count,
		TArray<FString>& OutReviewRecordIds)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FString ArchiveId = FString::Printf(
				TEXT("archive_pending_load_page_%02d_%s"),
				Index,
				*FGuid::NewGuid().ToString(EGuidFormats::Digits));
			const FString ChangeId = FString::Printf(TEXT("tx_pending_load_page_%02d"), Index);
			FBlueprintHelperReviewRecord Record = MakeRecord(
				ArchiveId,
				AssetPath,
				TaskRunId,
				ChangeId);
			OutReviewRecordIds.Add(Record.ReviewRecordId);

			FString SaveError;
			Test.TestTrue(TEXT("record saves before coordinator load"), Store.SaveReviewRecord(Record, SaveError));
		}
	}

	static void DeletePendingRecords(
		FBlueprintHelperReviewStoreService& Store,
		const TArray<FString>& ReviewRecordIds)
	{
		for (const FString& ReviewRecordId : ReviewRecordIds)
		{
			FString DeleteError;
			Store.DeleteReviewRecord(ReviewRecordId, DeleteError);
		}
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FWaitForBlueprintHelperPendingLoadResult,
	TSharedPtr<BlueprintHelperReviewPendingLoadCoordinatorTests::FPendingLoadLatentState>,
	State,
	FAutomationTestBase*,
	Test);

bool FWaitForBlueprintHelperPendingLoadResult::Update()
{
	if (State->bDone)
	{
		return true;
	}
	if ((FPlatformTime::Seconds() - State->StartSeconds) > 5.0)
	{
		Test->AddError(TEXT("pending load coordinator did not complete within timeout"));
		return true;
	}
	return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingLoadFullReloadReturnsFirstPageOnlyTest,
	"BlueprintHelper.Review.PendingLoad.FullReloadReturnsFirstPageOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPendingLoadFullReloadReturnsFirstPageOnlyTest::RunTest(const FString& Parameters)
{
	TSharedRef<FBlueprintHelperReviewStoreService> Store = MakeShared<FBlueprintHelperReviewStoreService>();
	const FString AssetPath = TEXT("/Game/BlueprintHelperReview/BP_PendingLoadPage");
	const FString TaskRunId = TEXT("task_pending_load_page");
	TArray<FString> ReviewRecordIds;
	BlueprintHelperReviewPendingLoadCoordinatorTests::CreatePendingRecords(
		Store.Get(),
		*this,
		AssetPath,
		TaskRunId,
		5,
		ReviewRecordIds);

	FBlueprintHelperReviewPerformanceSettings Settings;
	Settings.bValiditySweepEnabled = false;
	TSharedRef<FBlueprintHelperReviewPendingLoadCoordinator> Coordinator =
		MakeShared<FBlueprintHelperReviewPendingLoadCoordinator>(&Store.Get(), Settings);
	FBlueprintHelperReviewPendingLoadRequest Request;
	Request.AssetPathFilter = AssetPath;
	Request.Mode = EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage;
	Request.PageSize = 2;
	Request.Source = TEXT("test_full_reload_page");
	Request.SourceEvent = FBlueprintHelperReviewStoreChangedEvent::FullReload();

	TSharedPtr<BlueprintHelperReviewPendingLoadCoordinatorTests::FPendingLoadLatentState> State =
		MakeShared<BlueprintHelperReviewPendingLoadCoordinatorTests::FPendingLoadLatentState>();
	Coordinator->RequestLoad(
		Request,
		FBlueprintHelperReviewPendingLoadCompleted::CreateLambda(
			[State](const FBlueprintHelperReviewPendingLoadResult& Result)
			{
				State->Result = Result;
				State->bDone = true;
			}));

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForBlueprintHelperPendingLoadResult(State, this));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State, ReviewRecordIds, Store]()
	{
		TestTrue(TEXT("coordinator result succeeds"), State->Result.bSucceeded);
		TestEqual(TEXT("full reload returns first page only"), State->Result.Changes.Num(), 2);
		TestEqual(TEXT("full reload total count reports all rows"), State->Result.TotalMatchingCount, 5);
		TestTrue(TEXT("full reload reports more pages"), State->Result.bHasMore);
		TestTrue(TEXT("full reload emits next cursor"), State->Result.NextCursor.IsSet());
		BlueprintHelperReviewPendingLoadCoordinatorTests::DeletePendingRecords(Store.Get(), ReviewRecordIds);
		return true;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingLoadAppendNextPageUsesCursorTest,
	"BlueprintHelper.Review.PendingLoad.AppendNextPageUsesCursor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPendingLoadAppendNextPageUsesCursorTest::RunTest(const FString& Parameters)
{
	TSharedRef<FBlueprintHelperReviewStoreService> Store = MakeShared<FBlueprintHelperReviewStoreService>();
	const FString AssetPath = TEXT("/Game/BlueprintHelperReview/BP_PendingLoadAppendPage");
	const FString TaskRunId = TEXT("task_pending_load_append_page");
	TArray<FString> ReviewRecordIds;
	BlueprintHelperReviewPendingLoadCoordinatorTests::CreatePendingRecords(
		Store.Get(),
		*this,
		AssetPath,
		TaskRunId,
		5,
		ReviewRecordIds);

	FBlueprintHelperReviewPerformanceSettings Settings;
	Settings.bValiditySweepEnabled = false;
	TSharedRef<FBlueprintHelperReviewPendingLoadCoordinator> Coordinator =
		MakeShared<FBlueprintHelperReviewPendingLoadCoordinator>(&Store.Get(), Settings);

	FBlueprintHelperReviewPendingLoadRequest FirstRequest;
	FirstRequest.AssetPathFilter = AssetPath;
	FirstRequest.Mode = EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage;
	FirstRequest.PageSize = 2;
	FirstRequest.Source = TEXT("test_append_first_page");
	FirstRequest.SourceEvent = FBlueprintHelperReviewStoreChangedEvent::FullReload();

	TSharedPtr<BlueprintHelperReviewPendingLoadCoordinatorTests::FPendingLoadLatentState> FirstState =
		MakeShared<BlueprintHelperReviewPendingLoadCoordinatorTests::FPendingLoadLatentState>();
	TSharedPtr<BlueprintHelperReviewPendingLoadCoordinatorTests::FPendingLoadLatentState> SecondState =
		MakeShared<BlueprintHelperReviewPendingLoadCoordinatorTests::FPendingLoadLatentState>();
	Coordinator->RequestLoad(
		FirstRequest,
		FBlueprintHelperReviewPendingLoadCompleted::CreateLambda(
			[FirstState](const FBlueprintHelperReviewPendingLoadResult& Result)
			{
				FirstState->Result = Result;
				FirstState->bDone = true;
			}));

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForBlueprintHelperPendingLoadResult(FirstState, this));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Coordinator, FirstState, SecondState, AssetPath]()
	{
		FBlueprintHelperReviewPendingLoadRequest SecondRequest;
		SecondRequest.AssetPathFilter = AssetPath;
		SecondRequest.Mode = EBlueprintHelperReviewPendingLoadMode::AppendNextPage;
		SecondRequest.PageSize = 2;
		SecondRequest.Cursor = FirstState->Result.NextCursor;
		SecondRequest.Source = TEXT("test_append_second_page");
		SecondRequest.SourceEvent = FBlueprintHelperReviewStoreChangedEvent::FullReload();
		Coordinator->RequestLoad(
			SecondRequest,
			FBlueprintHelperReviewPendingLoadCompleted::CreateLambda(
				[SecondState](const FBlueprintHelperReviewPendingLoadResult& Result)
				{
					SecondState->Result = Result;
					SecondState->bDone = true;
				}));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForBlueprintHelperPendingLoadResult(SecondState, this));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, FirstState, SecondState, ReviewRecordIds, Store]()
	{
		TestEqual(TEXT("first page count"), FirstState->Result.Changes.Num(), 2);
		TestEqual(TEXT("second page count"), SecondState->Result.Changes.Num(), 2);
		if (FirstState->Result.Changes.Num() > 0 && SecondState->Result.Changes.Num() > 0)
		{
			TestNotEqual(
				TEXT("append page starts after first page"),
				SecondState->Result.Changes[0].ChangeId,
				FirstState->Result.Changes[0].ChangeId);
		}
		TestTrue(TEXT("second page still has more rows"), SecondState->Result.bHasMore);
		BlueprintHelperReviewPendingLoadCoordinatorTests::DeletePendingRecords(Store.Get(), ReviewRecordIds);
		return true;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingLoadResetDoesNotReturnAllPendingChangesTest,
	"BlueprintHelper.Review.PendingLoad.ResetDoesNotReturnAllPendingChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPendingLoadResetDoesNotReturnAllPendingChangesTest::RunTest(const FString& Parameters)
{
	const int32 PageSize = 3;
	const int32 TotalCreated = 7;
	TSharedRef<FBlueprintHelperReviewStoreService> Store = MakeShared<FBlueprintHelperReviewStoreService>();
	const FString AssetPath = TEXT("/Game/BlueprintHelperReview/BP_PendingLoadResetRegression");
	const FString TaskRunId = TEXT("task_pending_load_reset_regression");
	TArray<FString> ReviewRecordIds;
	BlueprintHelperReviewPendingLoadCoordinatorTests::CreatePendingRecords(
		Store.Get(),
		*this,
		AssetPath,
		TaskRunId,
		TotalCreated,
		ReviewRecordIds);

	FBlueprintHelperReviewPerformanceSettings Settings;
	Settings.bValiditySweepEnabled = false;
	TSharedRef<FBlueprintHelperReviewPendingLoadCoordinator> Coordinator =
		MakeShared<FBlueprintHelperReviewPendingLoadCoordinator>(&Store.Get(), Settings);

	FBlueprintHelperReviewPendingLoadRequest Request;
	Request.AssetPathFilter = AssetPath;
	Request.Mode = EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage;
	Request.PageSize = PageSize;
	Request.Source = TEXT("test_reset_regression");
	Request.SourceEvent = FBlueprintHelperReviewStoreChangedEvent::FullReload();

	TSharedPtr<BlueprintHelperReviewPendingLoadCoordinatorTests::FPendingLoadLatentState> State =
		MakeShared<BlueprintHelperReviewPendingLoadCoordinatorTests::FPendingLoadLatentState>();
	Coordinator->RequestLoad(
		Request,
		FBlueprintHelperReviewPendingLoadCompleted::CreateLambda(
			[State](const FBlueprintHelperReviewPendingLoadResult& Result)
			{
				State->Result = Result;
				State->bDone = true;
			}));

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForBlueprintHelperPendingLoadResult(State, this));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State, ReviewRecordIds, Store, PageSize, TotalCreated]()
	{
		TestEqual(TEXT("reset result is exactly page size"), State->Result.Changes.Num(), PageSize);
		TestTrue(TEXT("reset result reports more pages"), State->Result.bHasMore);
		TestEqual(TEXT("reset result reports full total"), State->Result.TotalMatchingCount, TotalCreated);
		BlueprintHelperReviewPendingLoadCoordinatorTests::DeletePendingRecords(Store.Get(), ReviewRecordIds);
		return true;
	}));
	return true;
}

#endif
