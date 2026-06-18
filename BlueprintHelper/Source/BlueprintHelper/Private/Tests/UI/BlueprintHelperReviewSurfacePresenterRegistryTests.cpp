// BlueprintHelper Review surface presenter registry tests.

#include "UI/Review/BlueprintHelperReviewSurfacePresenterRegistry.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewSurfacePresenterRegistry_DefaultRegistryRoutesOverlaySurfaces,
	"BlueprintHelper.Review.Panel.SurfacePresenterRegistry.DefaultRegistryRoutesOverlaySurfaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewSurfacePresenterRegistry_DefaultRegistryRoutesOverlaySurfaces::RunTest(const FString&)
{
	const TSharedRef<FBlueprintHelperReviewSurfacePresenterRegistry> Registry =
		FBlueprintHelperReviewSurfacePresenterRegistry::CreateDefault();

	const EBlueprintHelperReviewSurface OverlaySurfaces[] =
	{
		EBlueprintHelperReviewSurface::Components,
		EBlueprintHelperReviewSurface::MyBlueprint,
		EBlueprintHelperReviewSurface::Details,
		EBlueprintHelperReviewSurface::UMGWidgetTree,
		EBlueprintHelperReviewSurface::DataTable,
		EBlueprintHelperReviewSurface::DataAsset,
		EBlueprintHelperReviewSurface::Material
	};
	for (const EBlueprintHelperReviewSurface Surface : OverlaySurfaces)
	{
		TestTrue(
			FString::Printf(TEXT("%s has a registered predicate"), BlueprintHelperReviewSurfaceToString(Surface)),
			Registry->FindPredicate(Surface) != nullptr);
		TestTrue(
			FString::Printf(TEXT("%s has a registered overlay builder"), BlueprintHelperReviewSurfaceToString(Surface)),
			Registry->CanBuildOverlay(Surface));
	}

	TestTrue(
		TEXT("graph remains a registered route predicate"),
		Registry->FindPredicate(EBlueprintHelperReviewSurface::Graph) != nullptr);
	TestFalse(
		TEXT("graph remains route-only and does not own a row-overlay builder"),
		Registry->CanBuildOverlay(EBlueprintHelperReviewSurface::Graph));
	TestTrue(
		TEXT("unknown surface has no predicate"),
		Registry->FindPredicate(EBlueprintHelperReviewSurface::Unknown) == nullptr);
	TestFalse(
		TEXT("unknown surface has no overlay builder"),
		Registry->CanBuildOverlay(EBlueprintHelperReviewSurface::Unknown));

	return true;
}

#endif
