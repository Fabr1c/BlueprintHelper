#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/Review/BlueprintHelperReviewGenericSurfaceProjectionAdapter.h"
#include "UI/Review/BlueprintHelperReviewSurfaceProjectionRegistry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewSurfaceProjectionRegistryUsesAssetSurfaceTargetKeyTest,
	"BlueprintHelper.Review.SurfaceProjection.RegistryUsesAssetSurfaceTargetKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewSurfaceProjectionRegistryUsesAssetSurfaceTargetKeyTest::RunTest(const FString& Parameters)
{
	TSharedRef<FBlueprintHelperReviewSurfaceProjectionRegistry> Registry =
		MakeShared<FBlueprintHelperReviewSurfaceProjectionRegistry>();
	TArray<FBlueprintHelperDiagnosticItem> Diagnostics;
	const bool bRegistered = Registry->RegisterProjectionAdapter(
		MakeShared<FBlueprintHelperReviewGenericSurfaceProjectionAdapter>(
			TEXT("blueprint"),
			TEXT("my_blueprint"),
			TEXT("blueprint_variable")),
		Diagnostics);

	FBlueprintHelperReviewTargetIdentity Identity;
	Identity.AssetKind = TEXT("blueprint");
	Identity.SurfaceKind = TEXT("my_blueprint");
	Identity.TargetKind = TEXT("blueprint_variable");
	Identity.TargetKey = TEXT("blueprint_variable:SmokeFloat");

	const FBlueprintHelperReviewSurfaceProjectionLookup Lookup =
		Registry->FindProjectionAdapter(Identity);

	TestTrue(TEXT("projection adapter registers"), bRegistered);
	TestEqual(TEXT("registration diagnostics stay empty"), Diagnostics.Num(), 0);
	TestTrue(TEXT("projection adapter resolves by asset/surface/target"), Lookup.bAvailable);
	return true;
}

#endif
