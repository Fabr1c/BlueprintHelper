// BlueprintHelper Review Material surface tests.

#include "Misc/AutomationTest.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "UI/Review/BlueprintHelperReviewMaterialPresenter.h"
#include "UI/Review/BlueprintHelperReviewSurfaceRouter.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewMaterialSurfaceRoutingTest,
	"BlueprintHelper.Review.Material.SurfaceRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewMaterialSurfaceRoutingTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("material_expression routes to Material surface"),
		FBlueprintHelperReviewTargetKindRegistry::NormalizeSurfaceForTarget(
			EBlueprintHelperReviewSurface::Unknown,
			TEXT("material_expression"),
			TEXT("material_expression:SmokeColor")),
		EBlueprintHelperReviewSurface::Material);
	TestEqual(
		TEXT("material output links route to Material surface"),
		FBlueprintHelperReviewTargetKindRegistry::NormalizeSurfaceForTarget(
			EBlueprintHelperReviewSurface::Unknown,
			TEXT("material_output_link"),
			TEXT("material_link:SmokeColor:RGB:_material_output:BaseColor")),
		EBlueprintHelperReviewSurface::Material);
	TestEqual(
		TEXT("material asset factory roots route to Material surface"),
		FBlueprintHelperReviewTargetKindRegistry::ResolveAssetFactorySurface(TEXT("material")),
		EBlueprintHelperReviewSurface::Material);
	TestEqual(
		TEXT("Material assets own the main workspace"),
		FBlueprintHelperReviewSurfacePresenterRouter::GetMainWorkspaceSurfaceForAssetKind(
			EBlueprintHelperReviewAssetKind::Material),
		EBlueprintHelperReviewSurface::Material);
	TestTrue(
		TEXT("Material surface owns the main workspace overlay"),
		FBlueprintHelperReviewSurfacePresenterRouter::ShouldMainWorkspaceOwnOverlay(
			EBlueprintHelperReviewSurface::Material));

	FBlueprintHelperReviewVisibleChange Change;
	FBlueprintHelperReviewAtomicTarget MaterialTarget;
	MaterialTarget.Surface = EBlueprintHelperReviewSurface::Material;
	MaterialTarget.TargetKind = TEXT("material_expression");
	MaterialTarget.TargetKey = TEXT("material_expression:SmokeColor");
	Change.AtomicTargets.Add(MaterialTarget);

	TestTrue(
		TEXT("Material presenter accepts Material graph targets"),
		FBlueprintHelperReviewMaterialPresenter::ShouldShowChange(Change));
	TestTrue(
		TEXT("Material global predicate accepts Material graph targets"),
		BlueprintHelperReviewShouldShowInMaterial(Change));
	TestFalse(
		TEXT("Graph predicate does not consume Material-only visibility"),
		BlueprintHelperReviewShouldShowInGraph(Change));

	return true;
}

#endif
