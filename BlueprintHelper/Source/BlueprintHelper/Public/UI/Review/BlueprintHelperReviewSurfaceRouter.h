// BlueprintHelper Review surface router.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"

/**
 * Surface 路由器，负责将 change 路由到正确的 surface。
 */
class BLUEPRINTHELPER_API FBlueprintHelperReviewSurfacePresenterRouter
{
public:
	static FBlueprintHelperReviewSurfaceRouteDecision RouteChangeToSurface(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface);

	static bool ShouldShowChangeOnSurface(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface);

	static FString BuildRouteDebugSummary(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface,
		const FBlueprintHelperReviewSurfaceRouteDecision& Decision,
		const TCHAR* AssetKindName);

	static EBlueprintHelperReviewSurface GetStructurePanelSurfaceForAssetKind(
		EBlueprintHelperReviewAssetKind AssetKind);

	static EBlueprintHelperReviewSurface GetMainWorkspaceSurfaceForAssetKind(
		EBlueprintHelperReviewAssetKind AssetKind);

	static bool ShouldDetailsPanelOwnOverlay(EBlueprintHelperReviewSurface Surface);

	static bool ShouldMainWorkspaceOwnOverlay(EBlueprintHelperReviewSurface Surface);

private:
	static const TCHAR* SurfaceDebugName(EBlueprintHelperReviewSurface Surface);
};
