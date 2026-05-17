// BlueprintHelper Review object details presenter.

#include "UI/Review/BlueprintHelperReviewObjectDetailsPresenter.h"

#include "SKismetInspector.h"
#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"
#include "UI/Review/BlueprintHelperReviewSurfaceRouter.h"

class FBlueprintHelperReviewObjectDetailsPresenterUtils
{
public:
	static bool IsReviewPropertyEditingEnabled();
};

bool FBlueprintHelperReviewObjectDetailsPresenterUtils::IsReviewPropertyEditingEnabled()
{
	return false;
}

bool FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change)
{
	if (FBlueprintHelperReviewSurfacePresenterRouter::ShouldShowChangeOnSurface(
		Change,
		EBlueprintHelperReviewSurface::Details))
	{
		return true;
	}

	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if (Target.Surface == EBlueprintHelperReviewSurface::DataAsset
			|| Target.Surface == EBlueprintHelperReviewSurface::DataTable
			|| Target.Surface == EBlueprintHelperReviewSurface::UMGWidgetTree)
		{
			continue;
		}

		if (Target.Surface == EBlueprintHelperReviewSurface::Details
			|| BlueprintHelperReviewTargetKindCanRouteToDetails(Target.TargetKind)
			|| !Target.PropertyPath.IsEmpty()
			|| !Target.ComponentPath.IsEmpty())
		{
			return true;
		}
	}

	return false;
}

TSharedRef<SWidget> FBlueprintHelperReviewObjectDetailsPresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext&,
	TSharedPtr<SKismetInspector>& OutKismetInspector)
{
	TSharedRef<SKismetInspector> Inspector = SAssignNew(OutKismetInspector, SKismetInspector)
		.HideNameArea(true)
		.ViewIdentifier(FName(TEXT("BlueprintHelperReviewInspector")))
		.IsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateStatic(
			&FBlueprintHelperReviewObjectDetailsPresenterUtils::IsReviewPropertyEditingEnabled))
		.ShowLocalVariables(true);
	return Inspector;
}

TSharedRef<SWidget> FBlueprintHelperReviewObjectDetailsPresenter::BuildOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
{
	return FBlueprintHelperReviewRowHighlightModel::BuildRowHighlightOverlay(
		Args,
		EBlueprintHelperReviewSurface::Details,
		&FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange);
}
