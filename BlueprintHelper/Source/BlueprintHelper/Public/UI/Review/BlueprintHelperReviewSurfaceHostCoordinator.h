// BlueprintHelper Review surface host coordinator.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewSurfaceContentPresenterTypes.h"
#include "UI/Review/BlueprintHelperReviewSurfaceViewCoordinator.h"

struct BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceHostCoordinatorDelegates
{
	TArray<FBlueprintHelperReviewSurfaceHostBinding> HostBindings;
	TMap<EBlueprintHelperReviewSurfaceHostSlot, TFunction<bool()>> OverlayRefreshHandlers;
	TMap<EBlueprintHelperReviewSurface, TFunction<bool()>> RowRefreshHandlers;
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
