// BlueprintHelper Review details geometry resolver tests.

#include "UI/Review/BlueprintHelperReviewDetailsGeometryResolver.h"

#include "Misc/AutomationTest.h"
#include "Widgets/SNullWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewDetailsGeometryResolver_RejectsMissingInspector,
	"BlueprintHelper.Review.Panel.DetailsGeometryResolver.RejectsMissingInspector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewDetailsGeometryResolver_RejectsMissingInspector::RunTest(const FString&)
{
	FBlueprintHelperReviewDetailsGeometryResolver Resolver;
	FBlueprintHelperReviewDetailsGeometryResolutionContext Context;
	FBlueprintHelperReviewVisibleChange Change;
	FBlueprintHelperReviewSurfaceGeometryAnchor Anchor;
	TSharedPtr<SWidget> OverlayWidget = SNullWidget::NullWidget;

	const bool bResolved = Resolver.ResolveRowGeometry(Change, OverlayWidget, Context, Anchor);
	TestFalse(TEXT("missing inspector fails"), bResolved);
	TestEqual(TEXT("missing inspector reason"), Anchor.Reason, FString(TEXT("details_inspector_unavailable")));
	return true;
}

#endif
