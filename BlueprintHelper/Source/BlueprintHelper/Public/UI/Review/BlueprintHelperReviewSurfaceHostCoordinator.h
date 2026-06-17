// BlueprintHelper Review surface host coordinator.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewSurfaceViewCoordinator.h"

struct BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceHostCoordinatorDelegates
{
	TFunction<bool()> StructureOverlayRefresh;
	TFunction<bool()> MyBlueprintOverlayRefresh;
	TFunction<bool()> DetailsOverlayRefresh;
	TFunction<bool()> MainWorkspaceOverlayRefresh;

	TFunction<bool()> ComponentsRowsRefresh;
	TFunction<bool()> WidgetTreeRowsRefresh;
	TFunction<bool()> MyBlueprintRowsRefresh;
	TFunction<bool()> DetailsRowsRefresh;
	TFunction<bool()> DataTableRowsRefresh;
	TFunction<bool()> DataAssetRowsRefresh;
	TFunction<bool()> MaterialRowsRefresh;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceHostCoordinator
{
public:
	void Configure(
		FBlueprintHelperReviewSurfaceViewCoordinator& SurfaceViewCoordinator,
		FBlueprintHelperReviewSurfaceHostCoordinatorDelegates Delegates) const;

private:
	static void RegisterSurface(
		FBlueprintHelperReviewSurfaceViewCoordinator& SurfaceViewCoordinator,
		EBlueprintHelperReviewSurface Surface,
		TFunction<bool()> OverlayRefresh,
		TFunction<bool()> RowsRefresh);
};
