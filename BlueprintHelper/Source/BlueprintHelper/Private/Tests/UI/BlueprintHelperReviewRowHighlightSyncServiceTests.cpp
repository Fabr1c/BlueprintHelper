// BlueprintHelper Review row highlight sync service tests.

#include "UI/Review/BlueprintHelperReviewRowHighlightSyncService.h"

#include "Misc/AutomationTest.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenter.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRowHighlightSyncService_SyncsDefaultSurfaces,
	"BlueprintHelper.Review.Panel.RowHighlightSyncService.SyncsDefaultSurfaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewRowHighlightSyncService_SyncsDefaultSurfaces::RunTest(const FString&)
{
	TArray<EBlueprintHelperReviewSurface> Surfaces =
		FBlueprintHelperReviewRowHighlightSyncService::GetDefaultSurfacesForTesting();
	TestEqual(TEXT("default surface count"), Surfaces.Num(), 7);
	TestTrue(TEXT("default includes components"), Surfaces.Contains(EBlueprintHelperReviewSurface::Components));
	TestTrue(TEXT("default includes widget tree"), Surfaces.Contains(EBlueprintHelperReviewSurface::UMGWidgetTree));
	TestTrue(TEXT("default includes my blueprint"), Surfaces.Contains(EBlueprintHelperReviewSurface::MyBlueprint));
	TestTrue(TEXT("default includes details"), Surfaces.Contains(EBlueprintHelperReviewSurface::Details));
	TestTrue(TEXT("default includes data table"), Surfaces.Contains(EBlueprintHelperReviewSurface::DataTable));
	TestTrue(TEXT("default includes data asset"), Surfaces.Contains(EBlueprintHelperReviewSurface::DataAsset));
	TestTrue(TEXT("default includes material"), Surfaces.Contains(EBlueprintHelperReviewSurface::Material));

	int32 BuildCalls = 0;
	int32 ConfigureCalls = 0;
	FBlueprintHelperReviewRowHighlightSyncCallbacks Callbacks;
	Callbacks.BuildSurfaceDiffModels =
		[&BuildCalls](const FBlueprintHelperReviewSurfaceDiffFrameRoute&)
		{
			++BuildCalls;
			return TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel>();
		};
	Callbacks.ConfigurePresenterArgs =
		[&ConfigureCalls](
			FBlueprintHelperReviewPanelSurfacePresenterArgs&,
			const TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel>&)
		{
			++ConfigureCalls;
		};

	FBlueprintHelperReviewRowHighlightSyncService Service;
	const int32 SyncedCount = Service.Sync(TEXT("/Game/BH/TestAsset"), Callbacks);
	TestEqual(TEXT("sync count"), SyncedCount, 7);
	TestEqual(TEXT("build calls"), BuildCalls, 7);
	TestEqual(TEXT("configure calls"), ConfigureCalls, 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRowHighlightSyncService_RejectsIncompleteCallbacks,
	"BlueprintHelper.Review.Panel.RowHighlightSyncService.RejectsIncompleteCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewRowHighlightSyncService_RejectsIncompleteCallbacks::RunTest(const FString&)
{
	FBlueprintHelperReviewRowHighlightSyncService Service;
	TestEqual(
		TEXT("missing callbacks do not sync"),
		Service.Sync(TEXT("/Game/BH/TestAsset"), FBlueprintHelperReviewRowHighlightSyncCallbacks()),
		0);
	return true;
}

#endif
