// BlueprintHelper Review pending load application service tests.

#include "UI/Review/BlueprintHelperReviewPendingLoadApplicationService.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

class FBlueprintHelperReviewPendingLoadApplicationServiceTestUtils
{
public:
	static FBlueprintHelperReviewVisibleChange MakeChange(
		const FString& ChangeId,
		const FString& AssetPath,
		bool bIsAssetLifecycleRoot = false)
	{
		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = ChangeId;
		Change.AssetPath = AssetPath;
		Change.Status = EBlueprintHelperReviewChangeStatus::Pending;
		Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
		Change.DisplayLabel = ChangeId;
		Change.bIsAssetLifecycleRoot = bIsAssetLifecycleRoot;
		return Change;
	}

	static FBlueprintHelperReviewPendingLoadResult MakeSucceededResult(
		EBlueprintHelperReviewPendingLoadMode Mode,
		const TArray<FBlueprintHelperReviewVisibleChange>& Changes)
	{
		FBlueprintHelperReviewPendingLoadResult Result;
		Result.RequestId = 1;
		Result.bSucceeded = true;
		Result.Source = TEXT("test");
		Result.Mode = Mode;
		Result.Changes = Changes;
		Result.TotalMatchingCount = Changes.Num();
		return Result;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingLoadApplicationService_NormalizesEmptyIncrementalEvent,
	"BlueprintHelper.Review.Panel.PendingLoadApplicationService.NormalizesEmptyIncrementalEvent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewPendingLoadApplicationService_NormalizesEmptyIncrementalEvent::RunTest(const FString&)
{
	FBlueprintHelperReviewStoreChangedEvent SourceEvent;
	SourceEvent.bRequiresFullReload = false;

	const FBlueprintHelperReviewStoreChangedEvent Normalized =
		FBlueprintHelperReviewPendingLoadApplicationService::NormalizeStoreChangedEvent(SourceEvent);

	TestTrue(TEXT("empty incremental event becomes full reload"), Normalized.bRequiresFullReload);
	TestEqual(
		TEXT("full reload resolves reset mode"),
		static_cast<int32>(FBlueprintHelperReviewPendingLoadApplicationService::ResolveLoadMode(Normalized)),
		static_cast<int32>(EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingLoadApplicationService_SkipsAppendWhenInFlight,
	"BlueprintHelper.Review.Panel.PendingLoadApplicationService.SkipsAppendWhenInFlight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewPendingLoadApplicationService_SkipsAppendWhenInFlight::RunTest(const FString&)
{
	FBlueprintHelperReviewPagedChangeModel PagedModel;
	PagedModel.MarkPageRequestStarted();

	const FBlueprintHelperReviewPendingLoadRequestApplication Application =
		FBlueprintHelperReviewPendingLoadApplicationService::BuildRequestApplication(
			TEXT("scroll_append"),
			EBlueprintHelperReviewPendingLoadMode::AppendNextPage,
			FBlueprintHelperReviewStoreChangedEvent::FullReload(),
			PagedModel,
			100);

	TestFalse(TEXT("append should not request while in flight"), Application.bShouldRequestLoad);
	TestFalse(TEXT("append should not cancel existing load"), Application.bShouldCancelPendingLoads);
	TestTrue(TEXT("skip debug message"), Application.DebugMessage.Contains(TEXT("in_flight_append")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingLoadApplicationService_CancelsInFlightForRefresh,
	"BlueprintHelper.Review.Panel.PendingLoadApplicationService.CancelsInFlightForRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewPendingLoadApplicationService_CancelsInFlightForRefresh::RunTest(const FString&)
{
	FBlueprintHelperReviewPagedChangeModel PagedModel;
	PagedModel.MarkPageRequestStarted();

	FBlueprintHelperReviewStoreChangedEvent Event;
	Event.ChangeIds.Add(TEXT("change_1"));
	const FBlueprintHelperReviewPendingLoadRequestApplication Application =
		FBlueprintHelperReviewPendingLoadApplicationService::BuildRequestApplication(
			TEXT("store_changed"),
			EBlueprintHelperReviewPendingLoadMode::RefreshChanged,
			Event,
			PagedModel,
			64);

	TestTrue(TEXT("refresh should request load"), Application.bShouldRequestLoad);
	TestTrue(TEXT("refresh cancels old in-flight load"), Application.bShouldCancelPendingLoads);
	TestTrue(TEXT("refresh finishes local in-flight state"), Application.bShouldFinishInFlightRequest);
	TestTrue(TEXT("store timing"), Application.bShouldRecordStoreTiming);
	TestEqual(TEXT("page size"), Application.Request.PageSize, 64);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingLoadApplicationService_PreservesSelectionWithinSameAsset,
	"BlueprintHelper.Review.Panel.PendingLoadApplicationService.PreservesSelectionWithinSameAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewPendingLoadApplicationService_PreservesSelectionWithinSameAsset::RunTest(const FString&)
{
	FBlueprintHelperReviewVisibleChange OldRoot =
		FBlueprintHelperReviewPendingLoadApplicationServiceTestUtils::MakeChange(
			TEXT("old_root"),
			TEXT("/Game/A"),
			true);
	FBlueprintHelperReviewVisibleChange OldLeaf =
		FBlueprintHelperReviewPendingLoadApplicationServiceTestUtils::MakeChange(
			TEXT("old_leaf"),
			TEXT("/Game/A"));
	TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> CurrentItems;
	CurrentItems.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(OldRoot));
	CurrentItems.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(OldLeaf));

	TArray<FBlueprintHelperReviewVisibleChange> NextChanges;
	NextChanges.Add(FBlueprintHelperReviewPendingLoadApplicationServiceTestUtils::MakeChange(
		TEXT("new_root"),
		TEXT("/Game/A"),
		true));
	NextChanges.Add(FBlueprintHelperReviewPendingLoadApplicationServiceTestUtils::MakeChange(
		TEXT("new_leaf"),
		TEXT("/Game/A")));

	FBlueprintHelperReviewPagedChangeModel PagedModel;
	const FBlueprintHelperReviewPendingLoadResult Result =
		FBlueprintHelperReviewPendingLoadApplicationServiceTestUtils::MakeSucceededResult(
			EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage,
			NextChanges);

	const FBlueprintHelperReviewPendingLoadResultApplication Application =
		FBlueprintHelperReviewPendingLoadApplicationService::ApplyResult(
			Result,
			PagedModel,
			CurrentItems,
			CurrentItems[1],
			TEXT("old_signature"),
			100);

	TestTrue(TEXT("apply changes"), Application.bShouldApplyVisibleChanges);
	TestEqual(TEXT("prefer non-root same asset after removal"), Application.RecommendedSelectedChangeId, FString(TEXT("new_leaf")));
	TestTrue(TEXT("selection change refreshes workspace"), Application.bShouldRefreshMainWorkspace);
	return true;
}

#endif
