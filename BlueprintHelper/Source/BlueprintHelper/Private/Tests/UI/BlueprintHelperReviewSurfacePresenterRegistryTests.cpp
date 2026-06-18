// BlueprintHelper Review surface presenter registry tests.

#include "UI/Review/BlueprintHelperReviewSurfacePresenterRegistry.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

static bool LoadBlueprintHelperReviewPanelSourceForGuard(
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
		Test.AddError(FString::Printf(TEXT("Unable to load ReviewPanel source for guard: %s"), *RelativePath));
		return false;
	}
	return true;
}

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
	TestTrue(
		TEXT("graph owns route-driven main workspace content"),
		Registry->CanBuildContent(EBlueprintHelperReviewSurface::Graph));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewSurfacePresenterRegistry_DefaultRegistryRoutesContentSurfaces,
	"BlueprintHelper.Review.Panel.SurfacePresenterRegistry.DefaultRegistryRoutesContentSurfaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewSurfacePresenterRegistry_DefaultRegistryRoutesContentSurfaces::RunTest(const FString&)
{
	const TSharedRef<FBlueprintHelperReviewSurfacePresenterRegistry> Registry =
		FBlueprintHelperReviewSurfacePresenterRegistry::CreateDefault();

	const EBlueprintHelperReviewSurface ContentSurfaces[] =
	{
		EBlueprintHelperReviewSurface::Graph,
		EBlueprintHelperReviewSurface::Components,
		EBlueprintHelperReviewSurface::UMGWidgetTree,
		EBlueprintHelperReviewSurface::DataTable,
		EBlueprintHelperReviewSurface::DataAsset,
		EBlueprintHelperReviewSurface::Material
	};
	for (const EBlueprintHelperReviewSurface Surface : ContentSurfaces)
	{
		TestTrue(
			FString::Printf(TEXT("%s has a registered content builder"), BlueprintHelperReviewSurfaceToString(Surface)),
			Registry->CanBuildContent(Surface));
	}

	TestFalse(
		TEXT("unknown surface has no content builder"),
		Registry->CanBuildContent(EBlueprintHelperReviewSurface::Unknown));
	TestFalse(
		TEXT("my blueprint remains fixed-panel content and is not route-driven content"),
		Registry->CanBuildContent(EBlueprintHelperReviewSurface::MyBlueprint));
	TestFalse(
		TEXT("details remains fixed-panel content and is not route-driven content"),
		Registry->CanBuildContent(EBlueprintHelperReviewSurface::Details));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewSurfacePresenterRegistry_DefaultRegistryRoutesDetailsObjectPolicy,
	"BlueprintHelper.Review.Panel.SurfacePresenterRegistry.DefaultRegistryRoutesDetailsObjectPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewSurfacePresenterRegistry_DefaultRegistryRoutesDetailsObjectPolicy::RunTest(const FString&)
{
	const TSharedRef<FBlueprintHelperReviewSurfacePresenterRegistry> Registry =
		FBlueprintHelperReviewSurfacePresenterRegistry::CreateDefault();

	TestFalse(
		TEXT("widget tree details object policy is descriptor-driven"),
		Registry->UsesDetailsObject(EBlueprintHelperReviewSurface::UMGWidgetTree));
	TestFalse(
		TEXT("data table details object policy is descriptor-driven"),
		Registry->UsesDetailsObject(EBlueprintHelperReviewSurface::DataTable));
	TestFalse(
		TEXT("data asset details object policy is descriptor-driven"),
		Registry->UsesDetailsObject(EBlueprintHelperReviewSurface::DataAsset));
	TestTrue(
		TEXT("graph keeps details object fallback"),
		Registry->UsesDetailsObject(EBlueprintHelperReviewSurface::Graph));
	TestTrue(
		TEXT("material keeps details object fallback"),
		Registry->UsesDetailsObject(EBlueprintHelperReviewSurface::Material));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewSurfacePresenterRegistry_PanelContentHasNoSurfaceBranches,
	"BlueprintHelper.Review.Panel.SurfacePresenterRegistry.PanelContentHasNoSurfaceBranches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewSurfacePresenterRegistry_PanelContentHasNoSurfaceBranches::RunTest(const FString&)
{
	FString PanelSource;
	FString LayoutSource;
	if (!LoadBlueprintHelperReviewPanelSourceForGuard(
			*this,
			TEXT("Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp"),
			PanelSource)
		|| !LoadBlueprintHelperReviewPanelSourceForGuard(
			*this,
			TEXT("Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanelLayout.cpp"),
			LayoutSource))
	{
		return false;
	}

	TestFalse(
		TEXT("main workspace no longer branches on DataTable surface"),
		LayoutSource.Contains(TEXT("MainSurface == EBlueprintHelperReviewSurface::DataTable")));
	TestFalse(
		TEXT("main workspace no longer branches on DataAsset surface"),
		LayoutSource.Contains(TEXT("MainSurface == EBlueprintHelperReviewSurface::DataAsset")));
	TestFalse(
		TEXT("main workspace no longer branches on Material surface"),
		LayoutSource.Contains(TEXT("MainSurface == EBlueprintHelperReviewSurface::Material")));
	TestFalse(
		TEXT("main workspace no longer falls back to BuildGraphEditorWidget"),
		LayoutSource.Contains(TEXT("return BuildGraphEditorWidget();")));
	TestFalse(
		TEXT("structure panel no longer branches on UMGWidgetTree surface"),
		PanelSource.Contains(TEXT("StructureRoute.Surface == EBlueprintHelperReviewSurface::UMGWidgetTree")));
	TestFalse(
		TEXT("structure panel no longer directly builds UMG content"),
		PanelSource.Contains(TEXT("FBlueprintHelperReviewUMGWidgetTreePresenter::BuildContent")));
	TestFalse(
		TEXT("structure panel no longer directly builds Components content"),
		PanelSource.Contains(TEXT("FBlueprintHelperReviewBlueprintComponentsPresenter::BuildContent")));

	return true;
}

#endif
