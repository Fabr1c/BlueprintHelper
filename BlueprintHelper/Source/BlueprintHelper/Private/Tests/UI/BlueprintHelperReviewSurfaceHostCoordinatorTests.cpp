// BlueprintHelper Review surface host coordinator tests.

#include "UI/Review/BlueprintHelperReviewSurfaceHostCoordinator.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenterRegistry.h"

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

class FBlueprintHelperReviewSurfaceHostCoordinatorTestUtils
{
public:
	static TFunction<bool()> Count(TMap<FString, int32>& Counters, const FString& Key)
	{
		return [&Counters, Key]()
		{
			Counters.FindOrAdd(Key) += 1;
			return true;
		};
	}

	static FBlueprintHelperReviewSurfaceHostCoordinatorDelegates MakeDelegates(
		TMap<FString, int32>& Counters)
	{
		FBlueprintHelperReviewSurfaceHostCoordinatorDelegates Delegates;
		Delegates.HostBindings = FBlueprintHelperReviewSurfacePresenterRegistry::CreateDefault()->ListHostBindings();
		Delegates.OverlayRefreshHandlers.Add(
			EBlueprintHelperReviewSurfaceHostSlot::Structure,
			Count(Counters, TEXT("structure_overlay")));
		Delegates.OverlayRefreshHandlers.Add(
			EBlueprintHelperReviewSurfaceHostSlot::MyBlueprint,
			Count(Counters, TEXT("my_blueprint_overlay")));
		Delegates.OverlayRefreshHandlers.Add(
			EBlueprintHelperReviewSurfaceHostSlot::Details,
			Count(Counters, TEXT("details_overlay")));
		Delegates.OverlayRefreshHandlers.Add(
			EBlueprintHelperReviewSurfaceHostSlot::MainWorkspace,
			Count(Counters, TEXT("main_workspace_overlay")));
		for (const FBlueprintHelperReviewSurfaceHostBinding& HostBinding : Delegates.HostBindings)
		{
			Delegates.RowRefreshHandlers.Add(
				HostBinding.Surface,
				Count(
					Counters,
					FString::Printf(
						TEXT("%s_rows"),
						BlueprintHelperReviewSurfaceToString(HostBinding.Surface))));
		}
		return Delegates;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewSurfaceHostCoordinator_MapsSurfacesToHostOverlays,
	"BlueprintHelper.Review.Panel.SurfaceHostCoordinator.MapsSurfacesToHostOverlays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewSurfaceHostCoordinator_MapsSurfacesToHostOverlays::RunTest(const FString&)
{
	TMap<FString, int32> Counters;
	FBlueprintHelperReviewSurfaceViewCoordinator SurfaceViewCoordinator;
	FBlueprintHelperReviewSurfaceHostCoordinator HostCoordinator;
	HostCoordinator.Configure(
		SurfaceViewCoordinator,
		FBlueprintHelperReviewSurfaceHostCoordinatorTestUtils::MakeDelegates(Counters));

	TestTrue(TEXT("components overlay handled"), SurfaceViewCoordinator.RefreshOverlay(EBlueprintHelperReviewSurface::Components));
	TestTrue(TEXT("widget tree overlay handled"), SurfaceViewCoordinator.RefreshOverlay(EBlueprintHelperReviewSurface::UMGWidgetTree));
	TestTrue(TEXT("my blueprint overlay handled"), SurfaceViewCoordinator.RefreshOverlay(EBlueprintHelperReviewSurface::MyBlueprint));
	TestTrue(TEXT("details overlay handled"), SurfaceViewCoordinator.RefreshOverlay(EBlueprintHelperReviewSurface::Details));
	TestTrue(TEXT("data table overlay handled"), SurfaceViewCoordinator.RefreshOverlay(EBlueprintHelperReviewSurface::DataTable));
	TestTrue(TEXT("data asset overlay handled"), SurfaceViewCoordinator.RefreshOverlay(EBlueprintHelperReviewSurface::DataAsset));
	TestTrue(TEXT("material overlay handled"), SurfaceViewCoordinator.RefreshOverlay(EBlueprintHelperReviewSurface::Material));

	TestEqual(TEXT("structure overlay shared by components and widget tree"), Counters.FindRef(TEXT("structure_overlay")), 2);
	TestEqual(TEXT("my blueprint overlay isolated"), Counters.FindRef(TEXT("my_blueprint_overlay")), 1);
	TestEqual(TEXT("details overlay isolated"), Counters.FindRef(TEXT("details_overlay")), 1);
	TestEqual(TEXT("main workspace overlay shared by table asset and material"), Counters.FindRef(TEXT("main_workspace_overlay")), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewSurfaceHostCoordinator_KeepsRowsSurfaceSpecific,
	"BlueprintHelper.Review.Panel.SurfaceHostCoordinator.KeepsRowsSurfaceSpecific",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewSurfaceHostCoordinator_KeepsRowsSurfaceSpecific::RunTest(const FString&)
{
	TMap<FString, int32> Counters;
	FBlueprintHelperReviewSurfaceViewCoordinator SurfaceViewCoordinator;
	FBlueprintHelperReviewSurfaceHostCoordinator HostCoordinator;
	HostCoordinator.Configure(
		SurfaceViewCoordinator,
		FBlueprintHelperReviewSurfaceHostCoordinatorTestUtils::MakeDelegates(Counters));

	TestTrue(TEXT("components rows handled"), SurfaceViewCoordinator.RefreshRows(EBlueprintHelperReviewSurface::Components));
	TestTrue(TEXT("widget tree rows handled"), SurfaceViewCoordinator.RefreshRows(EBlueprintHelperReviewSurface::UMGWidgetTree));
	TestTrue(TEXT("my blueprint rows handled"), SurfaceViewCoordinator.RefreshRows(EBlueprintHelperReviewSurface::MyBlueprint));
	TestTrue(TEXT("details rows handled"), SurfaceViewCoordinator.RefreshRows(EBlueprintHelperReviewSurface::Details));
	TestTrue(TEXT("data table rows handled"), SurfaceViewCoordinator.RefreshRows(EBlueprintHelperReviewSurface::DataTable));
	TestTrue(TEXT("data asset rows handled"), SurfaceViewCoordinator.RefreshRows(EBlueprintHelperReviewSurface::DataAsset));
	TestTrue(TEXT("material rows handled"), SurfaceViewCoordinator.RefreshRows(EBlueprintHelperReviewSurface::Material));
	TestFalse(TEXT("unknown rows not handled"), SurfaceViewCoordinator.RefreshRows(EBlueprintHelperReviewSurface::Unknown));

	TestEqual(TEXT("components rows count"), Counters.FindRef(TEXT("components_rows")), 1);
	TestEqual(TEXT("widget tree rows count"), Counters.FindRef(TEXT("umg_widget_tree_rows")), 1);
	TestEqual(TEXT("my blueprint rows count"), Counters.FindRef(TEXT("my_blueprint_rows")), 1);
	TestEqual(TEXT("details rows count"), Counters.FindRef(TEXT("details_rows")), 1);
	TestEqual(TEXT("data table rows count"), Counters.FindRef(TEXT("data_table_rows")), 1);
	TestEqual(TEXT("data asset rows count"), Counters.FindRef(TEXT("data_asset_rows")), 1);
	TestEqual(TEXT("material rows count"), Counters.FindRef(TEXT("material_rows")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelSurfaceBranchResidualGuardTest,
	"BlueprintHelper.Review.Panel.SurfaceHostCoordinator.PanelSurfaceBranchResidualGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewPanelSurfaceBranchResidualGuardTest::RunTest(const FString&)
{
	const FString PanelSourcePath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp"));
	FString PanelSource;
	if (!FFileHelper::LoadFileToString(PanelSource, *PanelSourcePath))
	{
		AddError(FString::Printf(TEXT("Could not read panel source: %s"), *PanelSourcePath));
		return false;
	}

	const TArray<FString> ForbiddenLiterals = {
		TEXT("DataTableRowsRefresh"),
		TEXT("DataAssetRowsRefresh"),
		TEXT("MaterialRowsRefresh"),
		TEXT("RoutedSurface == EBlueprintHelperReviewSurface::Material"),
		TEXT("RoutedSurface == EBlueprintHelperReviewSurface::DataTable"),
		TEXT("RoutedSurface == EBlueprintHelperReviewSurface::DataAsset"),
		TEXT("EBlueprintHelperReviewSurface::Material"),
		TEXT("EBlueprintHelperReviewSurface::DataTable"),
		TEXT("EBlueprintHelperReviewSurface::DataAsset"),
		TEXT("switch (HostSlot)")
	};
	for (const FString& ForbiddenLiteral : ForbiddenLiterals)
	{
		TestFalse(
			FString::Printf(TEXT("panel source should not contain %s"), *ForbiddenLiteral),
			PanelSource.Contains(ForbiddenLiteral));
	}
	return true;
}

#endif
