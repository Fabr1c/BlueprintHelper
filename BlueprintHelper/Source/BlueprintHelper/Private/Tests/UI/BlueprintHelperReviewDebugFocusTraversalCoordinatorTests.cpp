// BlueprintHelper Review debug focus traversal coordinator tests.

#include "UI/Review/BlueprintHelperReviewDebugFocusTraversalCoordinator.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

class FBlueprintHelperReviewDebugFocusTraversalCoordinatorTestUtils
{
public:
	static TSharedPtr<FBlueprintHelperReviewVisibleChange> MakeChange(
		const FString& ChangeId,
		const FString& AssetPath)
	{
		TSharedPtr<FBlueprintHelperReviewVisibleChange> Change =
			MakeShared<FBlueprintHelperReviewVisibleChange>();
		Change->ChangeId = ChangeId;
		Change->AssetPath = AssetPath;
		Change->Status = EBlueprintHelperReviewChangeStatus::Pending;
		return Change;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewDebugFocusTraversalCoordinator_FiltersCurrentAsset,
	"BlueprintHelper.Review.Panel.DebugFocusTraversalCoordinator.FiltersCurrentAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewDebugFocusTraversalCoordinator_FiltersCurrentAsset::RunTest(const FString&)
{
	TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Items;
	Items.Add(FBlueprintHelperReviewDebugFocusTraversalCoordinatorTestUtils::MakeChange(
		TEXT("change_a"),
		TEXT("/Game/A")));
	Items.Add(FBlueprintHelperReviewDebugFocusTraversalCoordinatorTestUtils::MakeChange(
		TEXT("change_b"),
		TEXT("/Game/B")));

	FBlueprintHelperReviewDebugFocusTraversalCoordinator Coordinator;
	const FBlueprintHelperReviewDebugFocusTraversalStep StartStep =
		Coordinator.Start(Items, TEXT("/Game/B"), true);
	const FBlueprintHelperReviewDebugFocusTraversalStep FocusStep = Coordinator.Advance();

	TestEqual(
		TEXT("start kind"),
		static_cast<int32>(StartStep.Kind),
		static_cast<int32>(EBlueprintHelperReviewDebugFocusTraversalStepKind::Started));
	TestEqual(TEXT("filtered total"), StartStep.Total, 1);
	TestTrue(TEXT("focus item valid"), FocusStep.Item.IsValid());
	TestEqual(TEXT("focuses current asset item"), FocusStep.Item->ChangeId, FString(TEXT("change_b")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewDebugFocusTraversalCoordinator_WaitsAndAdvancesOnGeometry,
	"BlueprintHelper.Review.Panel.DebugFocusTraversalCoordinator.WaitsAndAdvancesOnGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewDebugFocusTraversalCoordinator_WaitsAndAdvancesOnGeometry::RunTest(const FString&)
{
	TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Items;
	Items.Add(FBlueprintHelperReviewDebugFocusTraversalCoordinatorTestUtils::MakeChange(
		TEXT("change_a"),
		TEXT("/Game/A")));

	FBlueprintHelperReviewDebugFocusTraversalCoordinator Coordinator;
	Coordinator.Start(Items, FString(), false);
	Coordinator.Advance();

	const FBlueprintHelperReviewDebugFocusTraversalStep WaitStep =
		Coordinator.ProcessGeometryEvent([](
			TSharedPtr<FBlueprintHelperReviewVisibleChange>,
			FString& OutReason)
		{
			OutReason = TEXT("geometry_not_ready");
			return false;
		});
	TestEqual(
		TEXT("wait kind"),
		static_cast<int32>(WaitStep.Kind),
		static_cast<int32>(EBlueprintHelperReviewDebugFocusTraversalStepKind::WaitGeometry));
	TestTrue(TEXT("still awaiting"), Coordinator.IsAwaitingGeometry());

	const FBlueprintHelperReviewDebugFocusTraversalStep ReadyStep =
		Coordinator.ProcessGeometryEvent([](
			TSharedPtr<FBlueprintHelperReviewVisibleChange>,
			FString& OutReason)
		{
			OutReason = TEXT("geometry_ready");
			return true;
		});
	TestEqual(
		TEXT("ready kind"),
		static_cast<int32>(ReadyStep.Kind),
		static_cast<int32>(EBlueprintHelperReviewDebugFocusTraversalStepKind::FocusReady));
	TestFalse(TEXT("no longer awaiting"), Coordinator.IsAwaitingGeometry());

	const FBlueprintHelperReviewDebugFocusTraversalStep CompleteStep = Coordinator.Advance();
	TestEqual(
		TEXT("complete kind"),
		static_cast<int32>(CompleteStep.Kind),
		static_cast<int32>(EBlueprintHelperReviewDebugFocusTraversalStepKind::Complete));
	TestFalse(TEXT("inactive after complete"), Coordinator.IsActive());
	return true;
}

#endif
