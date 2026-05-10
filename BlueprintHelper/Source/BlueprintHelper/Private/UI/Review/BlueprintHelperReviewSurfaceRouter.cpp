// BlueprintHelper Review surface router.

#include "UI/Review/BlueprintHelperReviewSurfaceRouter.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"

FBlueprintHelperReviewSurfaceRouteDecision FBlueprintHelperReviewSurfacePresenterRouter::RouteChangeToSurface(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface)
{
	FBlueprintHelperReviewSurfaceRouteDecision Decision;
	Decision.bHasExplicitTargets = BlueprintHelperReviewHasExplicitTargets(Change);
	Decision.ExplicitTargetCount = Change.AtomicTargets.Num();
	Decision.MatchingTargetCount = Surface == EBlueprintHelperReviewSurface::Details
		? BlueprintHelperReviewCountDetailsTargets(Change)
		: BlueprintHelperReviewCountSurfaceTargets(Change, Surface);

	if (Decision.bHasExplicitTargets)
	{
		Decision.bShouldShow = Decision.MatchingTargetCount > 0;
		Decision.Reason = Decision.bShouldShow ? TEXT("target_match") : TEXT("no_surface_anchor");
		return Decision;
	}

	Decision.bShouldShow = LegacyFallbackMatchesSurface(Change, Surface);
	Decision.Reason = Decision.bShouldShow ? TEXT("legacy_fallback") : TEXT("legacy_no_match");
	return Decision;
}

bool FBlueprintHelperReviewSurfacePresenterRouter::ShouldShowChangeOnSurface(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface)
{
	return RouteChangeToSurface(Change, Surface).bShouldShow;
}

FString FBlueprintHelperReviewSurfacePresenterRouter::BuildRouteDebugSummary(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface,
	const FBlueprintHelperReviewSurfaceRouteDecision& Decision,
	const TCHAR* AssetKindName)
{
	return FString::Printf(
		TEXT("ReviewRoute change=%s surface=%s explicitTargets=%d graphTargets=%d result=%s reason=%s assetKind=%s matchingTargets=%d"),
		*Change.ChangeId,
		SurfaceDebugName(Surface),
		Decision.ExplicitTargetCount,
		BlueprintHelperReviewCountSurfaceTargets(Change, EBlueprintHelperReviewSurface::Graph),
		Decision.bShouldShow ? TEXT("shown") : TEXT("hidden"),
		*Decision.Reason,
		AssetKindName ? AssetKindName : TEXT("unknown"),
		Decision.MatchingTargetCount);
}

EBlueprintHelperReviewSurface FBlueprintHelperReviewSurfacePresenterRouter::GetStructurePanelSurfaceForAssetKind(
	EBlueprintHelperReviewAssetKind AssetKind)
{
	return AssetKind == EBlueprintHelperReviewAssetKind::WidgetBlueprint
		? EBlueprintHelperReviewSurface::UMGWidgetTree
		: EBlueprintHelperReviewSurface::Components;
}

EBlueprintHelperReviewSurface FBlueprintHelperReviewSurfacePresenterRouter::GetMainWorkspaceSurfaceForAssetKind(
	EBlueprintHelperReviewAssetKind AssetKind)
{
	switch (AssetKind)
	{
	case EBlueprintHelperReviewAssetKind::DataTable:
		return EBlueprintHelperReviewSurface::DataTable;
	case EBlueprintHelperReviewAssetKind::DataAsset:
	case EBlueprintHelperReviewAssetKind::Structure:
	case EBlueprintHelperReviewAssetKind::GenericObject:
		return EBlueprintHelperReviewSurface::DataAsset;
	case EBlueprintHelperReviewAssetKind::Blueprint:
	case EBlueprintHelperReviewAssetKind::WidgetBlueprint:
	case EBlueprintHelperReviewAssetKind::Unknown:
	default:
		return EBlueprintHelperReviewSurface::Graph;
	}
}

bool FBlueprintHelperReviewSurfacePresenterRouter::ShouldDetailsPanelOwnOverlay(EBlueprintHelperReviewSurface Surface)
{
	return false;
}

bool FBlueprintHelperReviewSurfacePresenterRouter::ShouldMainWorkspaceOwnOverlay(EBlueprintHelperReviewSurface Surface)
{
	return Surface == EBlueprintHelperReviewSurface::DataTable
		|| Surface == EBlueprintHelperReviewSurface::DataAsset;
}

const TCHAR* FBlueprintHelperReviewSurfacePresenterRouter::SurfaceDebugName(
	EBlueprintHelperReviewSurface Surface)
{
	switch (Surface)
	{
	case EBlueprintHelperReviewSurface::Graph:       return TEXT("Graph");
	case EBlueprintHelperReviewSurface::Components:  return TEXT("Components");
	case EBlueprintHelperReviewSurface::MyBlueprint: return TEXT("MyBlueprint");
	case EBlueprintHelperReviewSurface::Details:     return TEXT("Details");
	case EBlueprintHelperReviewSurface::UMGWidgetTree: return TEXT("UMGWidgetTree");
	case EBlueprintHelperReviewSurface::DataTable:   return TEXT("DataTable");
	case EBlueprintHelperReviewSurface::DataAsset:   return TEXT("DataAsset");
	default:                                         return TEXT("Unknown");
	}
}

bool FBlueprintHelperReviewSurfacePresenterRouter::LegacyFallbackMatchesSurface(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface)
{
	const FString Location = BlueprintHelperReviewNormalizeLocation(Change);
	switch (Surface)
	{
	case EBlueprintHelperReviewSurface::Graph:
		return !Change.GraphName.IsEmpty()
			|| Location.Contains(TEXT("graph:"))
			|| Location.Contains(TEXT("node:"))
			|| Location.Contains(TEXT("pin:"));
	case EBlueprintHelperReviewSurface::Components:
		return Location.Contains(TEXT("component"));
	case EBlueprintHelperReviewSurface::MyBlueprint:
		if (Location.Contains(TEXT("component")))
		{
			return false;
		}
		return Location.Contains(TEXT("my_blueprint"))
			|| Location.Contains(TEXT("function"))
			|| Location.Contains(TEXT("macro"))
			|| Location.Contains(TEXT("variable"))
			|| Location.Contains(TEXT("dispatcher"))
			|| Location.Contains(TEXT("delegate"));
	case EBlueprintHelperReviewSurface::Details:
		return Change.ChangeKind == EBlueprintHelperReviewChangeKind::VariableModified
			|| Change.ChangeKind == EBlueprintHelperReviewChangeKind::SignatureModified
			|| Location.Contains(TEXT("property"))
			|| Location.Contains(TEXT("variable"))
			|| Location.Contains(TEXT("signature"))
			|| Location.Contains(TEXT("dispatcher"));
	case EBlueprintHelperReviewSurface::UMGWidgetTree:
		return BlueprintHelperReviewShouldShowInUMGWidgetTree(Change);
	case EBlueprintHelperReviewSurface::DataTable:
		return BlueprintHelperReviewShouldShowInDataTable(Change);
	case EBlueprintHelperReviewSurface::DataAsset:
		return BlueprintHelperReviewShouldShowInDataAsset(Change);
	default:
		return false;
	}
}
