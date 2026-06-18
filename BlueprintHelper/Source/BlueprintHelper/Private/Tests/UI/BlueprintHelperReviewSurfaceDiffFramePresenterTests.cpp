// BlueprintHelper Review surface diff frame presenter tests.

#include "UI/Review/BlueprintHelperReviewSurfaceDiffFramePresenter.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewSurfaceDiffFramePresenter_ResolvesStructurePanelRoute,
	"BlueprintHelper.Review.Panel.SurfaceDiffFramePresenter.ResolvesStructurePanelRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewSurfaceDiffFramePresenter_ResolvesStructurePanelRoute::RunTest(const FString&)
{
	const FBlueprintHelperReviewSurfaceDiffFrameRoute BlueprintRoute =
		FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveStructurePanelRoute(
			EBlueprintHelperReviewAssetKind::Blueprint);
	const FBlueprintHelperReviewSurfaceDiffFrameRoute WidgetRoute =
		FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveStructurePanelRoute(
			EBlueprintHelperReviewAssetKind::WidgetBlueprint);

	TestEqual(
		TEXT("blueprint uses components structure surface"),
		static_cast<int32>(BlueprintRoute.Surface),
		static_cast<int32>(EBlueprintHelperReviewSurface::Components));
	TestTrue(TEXT("blueprint route owns overlay"), BlueprintRoute.bShouldBuildOverlay);
	TestTrue(TEXT("blueprint route has predicate"), BlueprintRoute.ShouldShowChange != nullptr);
	TestEqual(
		TEXT("widget uses widget tree structure surface"),
		static_cast<int32>(WidgetRoute.Surface),
		static_cast<int32>(EBlueprintHelperReviewSurface::UMGWidgetTree));
	TestTrue(TEXT("widget route owns overlay"), WidgetRoute.bShouldBuildOverlay);
	TestTrue(TEXT("widget route has predicate"), WidgetRoute.ShouldShowChange != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewSurfaceDiffFramePresenter_ResolvesMainWorkspaceRoute,
	"BlueprintHelper.Review.Panel.SurfaceDiffFramePresenter.ResolvesMainWorkspaceRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewSurfaceDiffFramePresenter_ResolvesMainWorkspaceRoute::RunTest(const FString&)
{
	const FBlueprintHelperReviewSurfaceDiffFrameRoute BlueprintRoute =
		FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveMainWorkspaceRoute(
			EBlueprintHelperReviewAssetKind::Blueprint);
	const FBlueprintHelperReviewSurfaceDiffFrameRoute DataTableRoute =
		FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveMainWorkspaceRoute(
			EBlueprintHelperReviewAssetKind::DataTable);
	const FBlueprintHelperReviewSurfaceDiffFrameRoute DataAssetRoute =
		FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveMainWorkspaceRoute(
			EBlueprintHelperReviewAssetKind::DataAsset);
	const FBlueprintHelperReviewSurfaceDiffFrameRoute MaterialRoute =
		FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveMainWorkspaceRoute(
			EBlueprintHelperReviewAssetKind::Material);

	TestFalse(TEXT("blueprint graph surface is not main workspace overlay-owned"), BlueprintRoute.bShouldBuildOverlay);
	TestTrue(TEXT("blueprint graph route still has a predicate"), BlueprintRoute.ShouldShowChange != nullptr);
	TestEqual(
		TEXT("data table uses data table surface"),
		static_cast<int32>(DataTableRoute.Surface),
		static_cast<int32>(EBlueprintHelperReviewSurface::DataTable));
	TestTrue(TEXT("data table route owns overlay"), DataTableRoute.bShouldBuildOverlay);
	TestEqual(
		TEXT("data asset uses data asset surface"),
		static_cast<int32>(DataAssetRoute.Surface),
		static_cast<int32>(EBlueprintHelperReviewSurface::DataAsset));
	TestTrue(TEXT("data asset route owns overlay"), DataAssetRoute.bShouldBuildOverlay);
	TestEqual(
		TEXT("material uses material surface"),
		static_cast<int32>(MaterialRoute.Surface),
		static_cast<int32>(EBlueprintHelperReviewSurface::Material));
	TestTrue(TEXT("material route owns overlay"), MaterialRoute.bShouldBuildOverlay);
	return true;
}

#endif
