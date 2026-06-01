// BlueprintHelper Review paged change model automation tests.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/Review/BlueprintHelperReviewPagedChangeModel.h"

namespace BlueprintHelperReviewPagedChangeModelTests
{
	static FBlueprintHelperReviewVisibleChange MakeVisibleChange(
		const FString& ChangeId,
		const FString& AssetPath,
		const FString& TargetKey)
	{
		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = ChangeId;
		Change.AssetPath = AssetPath;
		Change.GraphName = TEXT("EventGraph");
		Change.LocationKey = TargetKey;
		Change.LatestEvidenceId = ChangeId;
		Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
		Change.Status = EBlueprintHelperReviewChangeStatus::Pending;
		Change.DisplayLabel = TargetKey;
		return Change;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPagedChangeModelAppendDedupesTest,
	"BlueprintHelper.Review.PagedChangeModel.AppendDedupes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPagedChangeModelAppendDedupesTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewPagedChangeModel Model;

	FBlueprintHelperReviewPendingLoadResult FirstPage;
	FirstPage.Mode = EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage;
	FirstPage.Changes.Add(BlueprintHelperReviewPagedChangeModelTests::MakeVisibleChange(
		TEXT("change_a"),
		TEXT("/Game/BP_PageModel"),
		TEXT("graph_node:A")));
	FirstPage.Changes.Add(BlueprintHelperReviewPagedChangeModelTests::MakeVisibleChange(
		TEXT("change_b"),
		TEXT("/Game/BP_PageModel"),
		TEXT("graph_node:B")));
	FirstPage.TotalMatchingCount = 3;
	FirstPage.bHasMore = true;

	Model.ApplyPendingLoadResult(FirstPage);
	TestEqual(TEXT("first page loads two changes"), Model.GetLoadedChanges().Num(), 2);
	TestTrue(TEXT("model reports more pages"), Model.HasMorePages());

	FBlueprintHelperReviewPendingLoadResult SecondPage;
	SecondPage.Mode = EBlueprintHelperReviewPendingLoadMode::AppendNextPage;
	SecondPage.Changes.Add(FirstPage.Changes[1]);
	SecondPage.Changes.Add(BlueprintHelperReviewPagedChangeModelTests::MakeVisibleChange(
		TEXT("change_c"),
		TEXT("/Game/BP_PageModel"),
		TEXT("graph_node:C")));
	SecondPage.TotalMatchingCount = 3;
	SecondPage.bHasMore = false;

	Model.ApplyPendingLoadResult(SecondPage);
	TestEqual(TEXT("append page dedupes existing change"), Model.GetLoadedChanges().Num(), 3);
	TestFalse(TEXT("model reports no more pages"), Model.HasMorePages());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPagedChangeModelScrollNearEndRequestsNextPageTest,
	"BlueprintHelper.Review.PagedChangeModel.ScrollNearEndRequestsNextPage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPagedChangeModelScrollNearEndRequestsNextPageTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewPagedChangeModel Model;

	FBlueprintHelperReviewPendingLoadResult FirstPage;
	FirstPage.Mode = EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage;
	FirstPage.TotalMatchingCount = 200;
	FirstPage.bHasMore = true;
	for (int32 Index = 0; Index < 100; ++Index)
	{
		FirstPage.Changes.Add(BlueprintHelperReviewPagedChangeModelTests::MakeVisibleChange(
			FString::Printf(TEXT("change_%03d"), Index),
			TEXT("/Game/BP_PageModel"),
			FString::Printf(TEXT("graph_node:%03d"), Index)));
	}
	Model.ApplyPendingLoadResult(FirstPage);

	TestFalse(TEXT("far from end does not request next page"),
		Model.ShouldRequestNextPage(10.0, 20, 100, 24));
	TestTrue(TEXT("near end requests next page"),
		Model.ShouldRequestNextPage(60.0, 20, 100, 24));

	Model.MarkPageRequestStarted();
	TestFalse(TEXT("no request when already loading"),
		Model.ShouldRequestNextPage(80.0, 20, 100, 24));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPagedChangeModelDetectsTouchedRefreshChangeTest,
	"BlueprintHelper.Review.PagedChangeModel.DetectsTouchedRefreshChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPagedChangeModelDetectsTouchedRefreshChangeTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewPendingLoadResult Result;
	Result.Mode = EBlueprintHelperReviewPendingLoadMode::RefreshChanged;
	Result.Changes.Add(BlueprintHelperReviewPagedChangeModelTests::MakeVisibleChange(
		TEXT("change_touched"),
		TEXT("/Game/BP_PageModel"),
		TEXT("graph_node:Touched")));

	TestTrue(TEXT("refresh result reports changed selected row"),
		FBlueprintHelperReviewPagedChangeModel::PendingLoadResultContainsChange(Result, TEXT("change_touched")));
	TestFalse(TEXT("refresh result does not report unrelated row"),
		FBlueprintHelperReviewPagedChangeModel::PendingLoadResultContainsChange(Result, TEXT("change_other")));
	return true;
}

#endif
