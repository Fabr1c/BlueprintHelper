// BlueprintHelper Review surface geometry coordinator tests.

#include "UI/Review/BlueprintHelperReviewSurfaceGeometryCoordinator.h"

#include "Misc/AutomationTest.h"
#include "Widgets/SNullWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewSurfaceGeometryCoordinator_RejectsMissingOverlay,
	"BlueprintHelper.Review.Panel.SurfaceGeometryCoordinator.RejectsMissingOverlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewSurfaceGeometryCoordinator_RejectsMissingOverlay::RunTest(const FString&)
{
	FBlueprintHelperReviewSurfaceGeometryCoordinator Coordinator;
	FBlueprintHelperReviewSurfaceGeometryResolutionContext Context;
	FBlueprintHelperReviewVisibleChange Change;
	Change.AssetPath = TEXT("/Game/BH/TestAsset");
	FBlueprintHelperReviewSurfaceGeometryAnchor Anchor;

	const bool bResolved = Coordinator.ResolveRowGeometry(
		Change,
		EBlueprintHelperReviewSurface::Components,
		Context,
		Anchor);

	TestFalse(TEXT("missing overlay fails"), bResolved);
	TestEqual(TEXT("missing overlay reason"), Anchor.Reason, FString(TEXT("unsupported_surface_geometry")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewSurfaceGeometryCoordinator_UsesSurfaceSpecificResolvers,
	"BlueprintHelper.Review.Panel.SurfaceGeometryCoordinator.UsesSurfaceSpecificResolvers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewSurfaceGeometryCoordinator_UsesSurfaceSpecificResolvers::RunTest(const FString&)
{
	FBlueprintHelperReviewSurfaceGeometryCoordinator Coordinator;
	FBlueprintHelperReviewSurfaceGeometryResolutionContext Context;
	Context.ResolveOverlayWidget =
		[](EBlueprintHelperReviewSurface) -> TSharedPtr<SWidget>
		{
			TSharedPtr<SWidget> Widget = SNullWidget::NullWidget;
			return Widget;
		};
	int32 ComponentResolverCalls = 0;
	Context.ResolveComponentsRowGeometry =
		[&ComponentResolverCalls](
			const FBlueprintHelperReviewVisibleChange&,
			const TSharedPtr<SWidget>&,
			FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
		{
			++ComponentResolverCalls;
			OutAnchor.TargetText = TEXT("component");
			return true;
		};

	FBlueprintHelperReviewVisibleChange Change;
	Change.AssetPath = TEXT("/Game/BH/TestAsset");
	FBlueprintHelperReviewSurfaceGeometryAnchor Anchor;
	const bool bResolved = Coordinator.ResolveRowGeometry(
		Change,
		EBlueprintHelperReviewSurface::Components,
		Context,
		Anchor);

	TestTrue(TEXT("component resolver resolves"), bResolved);
	TestEqual(TEXT("component resolver called once"), ComponentResolverCalls, 1);
	TestEqual(TEXT("component target text"), Anchor.TargetText, FString(TEXT("component")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewSurfaceGeometryCoordinator_FiltersAndDispatchesGeometryEvents,
	"BlueprintHelper.Review.Panel.SurfaceGeometryCoordinator.FiltersAndDispatchesGeometryEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewSurfaceGeometryCoordinator_FiltersAndDispatchesGeometryEvents::RunTest(const FString&)
{
	FBlueprintHelperReviewSurfaceGeometryCoordinator Coordinator;
	int32 DebugMessages = 0;
	int32 RefreshCalls = 0;
	int32 FocusEvents = 0;
	FBlueprintHelperReviewSurfaceGeometryEventCallbacks Callbacks;
	Callbacks.GetReviewAssetPath = []()
	{
		return FString(TEXT("/Game/BH/TestAsset"));
	};
	Callbacks.AddDebugMessage = [&DebugMessages](const FString&)
	{
		++DebugMessages;
	};
	Callbacks.RefreshOverlay = [&RefreshCalls](EBlueprintHelperReviewSurface)
	{
		++RefreshCalls;
		return true;
	};
	Callbacks.ProcessDebugFocusTraversalGeometryEvent = [&FocusEvents]()
	{
		++FocusEvents;
	};

	Coordinator.HandleRegisteredRowGeometryChanged(
		TEXT("/Game/BH/OtherAsset"),
		EBlueprintHelperReviewSurface::Components,
		Callbacks);
	TestEqual(TEXT("foreign row ignored"), RefreshCalls, 0);

	Coordinator.HandleRegisteredRowGeometryChanged(
		TEXT("/Game/BH/TestAsset"),
		EBlueprintHelperReviewSurface::Components,
		Callbacks);
	TestEqual(TEXT("registered row refresh"), RefreshCalls, 1);
	TestEqual(TEXT("registered row debug"), DebugMessages, 1);
	TestEqual(TEXT("registered row focus event"), FocusEvents, 1);

	Coordinator.HandleSurfaceGeometryInvalidated(
		EBlueprintHelperReviewSurface::Components,
		Callbacks);
	TestEqual(TEXT("invalidated refresh"), RefreshCalls, 2);
	TestEqual(TEXT("invalidated debug"), DebugMessages, 2);
	TestEqual(TEXT("invalidated focus event"), FocusEvents, 2);
	return true;
}

#endif
