// BlueprintHelper Review paged change model automation tests.

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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

	static FBlueprintHelperReviewPendingLoadResult MakeLoadResult(
		int32 LoadedCount,
		int32 TotalMatchingCount,
		bool bHasMore)
	{
		FBlueprintHelperReviewPendingLoadResult Result;
		Result.bSucceeded = true;
		Result.TotalMatchingCount = TotalMatchingCount;
		Result.bHasMore = bHasMore;
		for (int32 Index = 0; Index < LoadedCount; ++Index)
		{
			Result.Changes.Add(MakeVisibleChange(
				FString::Printf(TEXT("status_change_%d"), Index),
				TEXT("/Game/BP_PageModel"),
				FString::Printf(TEXT("graph_node:Status%d"), Index)));
		}
		return Result;
	}

	static bool LoadPluginSource(
		FAutomationTestBase& Test,
		const FString& RelativePath,
		FString& OutSource)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlueprintHelper"));
		const FString Path = Plugin.IsValid()
			? FPaths::Combine(Plugin->GetBaseDir(), RelativePath)
			: FString();
		if (Path.IsEmpty() || !FFileHelper::LoadFileToString(OutSource, *Path))
		{
			Test.AddError(FString::Printf(TEXT("Unable to load source file: %s"), *RelativePath));
			return false;
		}
		return true;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPagedChangeModelBuildsReadableChineseStatusTextTest,
	"BlueprintHelper.Review.PagedChangeModel.BuildsReadableChineseStatusText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPagedChangeModelBuildsReadableChineseStatusTextTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewPagedChangeModel LoadingModel;
	LoadingModel.ApplyPendingLoadResult(
		BlueprintHelperReviewPagedChangeModelTests::MakeLoadResult(3, 5, true));
	LoadingModel.MarkPageRequestStarted();
	TestEqual(
		TEXT("in-flight text is readable Chinese"),
		LoadingModel.BuildPendingPageStatusText().ToString(),
		FString(TEXT("\u6b63\u5728\u52a0\u8f7d 3 / 5")));

	FBlueprintHelperReviewPagedChangeModel HasMoreModel;
	HasMoreModel.ApplyPendingLoadResult(
		BlueprintHelperReviewPagedChangeModelTests::MakeLoadResult(2, 5, true));
	TestEqual(
		TEXT("has-more text is readable Chinese"),
		HasMoreModel.BuildPendingPageStatusText().ToString(),
		FString(TEXT("\u5df2\u52a0\u8f7d 2 / 5\uff0c\u6eda\u52a8\u5230\u5e95\u90e8\u7ee7\u7eed\u52a0\u8f7d")));

	FBlueprintHelperReviewPagedChangeModel CompleteModel;
	CompleteModel.ApplyPendingLoadResult(
		BlueprintHelperReviewPagedChangeModelTests::MakeLoadResult(2, 2, false));
	TestEqual(
		TEXT("complete text is readable Chinese"),
		CompleteModel.BuildPendingPageStatusText().ToString(),
		FString(TEXT("\u5df2\u52a0\u8f7d 2 / 2")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPagedChangeModelSourceHasNoKnownStatusTextMojibakeTest,
	"BlueprintHelper.Review.PagedChangeModel.SourceHasNoKnownStatusTextMojibake",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPagedChangeModelSourceHasNoKnownStatusTextMojibakeTest::RunTest(const FString& Parameters)
{
	FString PanelSource;
	FString ModelSource;
	if (!BlueprintHelperReviewPagedChangeModelTests::LoadPluginSource(
			*this,
			TEXT("Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp"),
			PanelSource)
		|| !BlueprintHelperReviewPagedChangeModelTests::LoadPluginSource(
			*this,
			TEXT("Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPagedChangeModel.cpp"),
			ModelSource))
	{
		return false;
	}

	const FString CombinedSource = PanelSource + TEXT("\n") + ModelSource;
	const TArray<FString> KnownMojibakeFragments = {
		TEXT("\u6fee\u6d93\u7d7d\u5a40"),
		TEXT("\u5d1d\u9418\u70d8\u7970"),
		TEXT("\u7039\u544a\u5f43\u6fee\u70b4"),
		TEXT("\u95bf\u6d98\u672c\u7eee\u64ae")
	};
	for (const FString& Fragment : KnownMojibakeFragments)
	{
		TestFalse(
			FString::Printf(TEXT("Review pending-page status source should not contain mojibake fragment %s"), *Fragment),
			CombinedSource.Contains(Fragment));
	}
	return true;
}

#endif
