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
	struct FBlueprintHelperReviewAssetWorkspaceRoute
	{
		EBlueprintHelperReviewAssetKind AssetKind;
		EBlueprintHelperReviewSurface Surface;
	};

	static const FBlueprintHelperReviewAssetWorkspaceRoute Routes[] =
	{
		{ EBlueprintHelperReviewAssetKind::DataTable, EBlueprintHelperReviewSurface::DataTable },
		{ EBlueprintHelperReviewAssetKind::DataAsset, EBlueprintHelperReviewSurface::DataAsset },
		{ EBlueprintHelperReviewAssetKind::Structure, EBlueprintHelperReviewSurface::DataAsset },
		{ EBlueprintHelperReviewAssetKind::GenericObject, EBlueprintHelperReviewSurface::DataAsset },
		{ EBlueprintHelperReviewAssetKind::Blueprint, EBlueprintHelperReviewSurface::Graph },
		{ EBlueprintHelperReviewAssetKind::WidgetBlueprint, EBlueprintHelperReviewSurface::Graph },
		{ EBlueprintHelperReviewAssetKind::Unknown, EBlueprintHelperReviewSurface::Graph }
	};

	for (const FBlueprintHelperReviewAssetWorkspaceRoute& Route : Routes)
	{
		if (Route.AssetKind == AssetKind)
		{
			return Route.Surface;
		}
	}
	return EBlueprintHelperReviewSurface::Graph;
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
	struct FBlueprintHelperReviewSurfaceDebugName
	{
		EBlueprintHelperReviewSurface Surface;
		const TCHAR* Name;
	};

	static const FBlueprintHelperReviewSurfaceDebugName SurfaceNames[] =
	{
		{ EBlueprintHelperReviewSurface::Graph, TEXT("Graph") },
		{ EBlueprintHelperReviewSurface::Components, TEXT("Components") },
		{ EBlueprintHelperReviewSurface::MyBlueprint, TEXT("MyBlueprint") },
		{ EBlueprintHelperReviewSurface::Details, TEXT("Details") },
		{ EBlueprintHelperReviewSurface::UMGWidgetTree, TEXT("UMGWidgetTree") },
		{ EBlueprintHelperReviewSurface::DataTable, TEXT("DataTable") },
		{ EBlueprintHelperReviewSurface::DataAsset, TEXT("DataAsset") }
	};

	for (const FBlueprintHelperReviewSurfaceDebugName& Entry : SurfaceNames)
	{
		if (Entry.Surface == Surface)
		{
			return Entry.Name;
		}
	}
	return TEXT("Unknown");
}

bool FBlueprintHelperReviewSurfacePresenterRouter::LegacyFallbackMatchesSurface(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface)
{
	const FString Location = BlueprintHelperReviewNormalizeLocation(Change);
	using FSurfacePredicate = TFunction<bool()>;
	const TArray<TPair<EBlueprintHelperReviewSurface, FSurfacePredicate>> SurfacePredicates =
	{
		TPair<EBlueprintHelperReviewSurface, FSurfacePredicate>(
			EBlueprintHelperReviewSurface::Graph,
			[&Change, &Location]()
			{
				return !Change.GraphName.IsEmpty()
					|| Location.Contains(TEXT("graph:"))
					|| Location.Contains(TEXT("node:"))
					|| Location.Contains(TEXT("pin:"));
			}),
		TPair<EBlueprintHelperReviewSurface, FSurfacePredicate>(
			EBlueprintHelperReviewSurface::Components,
			[&Location]()
			{
				return Location.Contains(TEXT("component"));
			}),
		TPair<EBlueprintHelperReviewSurface, FSurfacePredicate>(
			EBlueprintHelperReviewSurface::MyBlueprint,
			[&Location]()
			{
				return !Location.Contains(TEXT("component"))
					&& (Location.Contains(TEXT("my_blueprint"))
						|| Location.Contains(TEXT("function"))
						|| Location.Contains(TEXT("macro"))
						|| Location.Contains(TEXT("variable"))
						|| Location.Contains(TEXT("dispatcher"))
						|| Location.Contains(TEXT("delegate")));
			}),
		TPair<EBlueprintHelperReviewSurface, FSurfacePredicate>(
			EBlueprintHelperReviewSurface::Details,
			[&Change, &Location]()
			{
				return Change.ChangeKind == EBlueprintHelperReviewChangeKind::VariableModified
					|| Change.ChangeKind == EBlueprintHelperReviewChangeKind::SignatureModified
					|| Location.Contains(TEXT("property"))
					|| Location.Contains(TEXT("variable"))
					|| Location.Contains(TEXT("signature"))
					|| Location.Contains(TEXT("dispatcher"));
			}),
		TPair<EBlueprintHelperReviewSurface, FSurfacePredicate>(
			EBlueprintHelperReviewSurface::UMGWidgetTree,
			[&Change]()
			{
				return BlueprintHelperReviewShouldShowInUMGWidgetTree(Change);
			}),
		TPair<EBlueprintHelperReviewSurface, FSurfacePredicate>(
			EBlueprintHelperReviewSurface::DataTable,
			[&Change]()
			{
				return BlueprintHelperReviewShouldShowInDataTable(Change);
			}),
		TPair<EBlueprintHelperReviewSurface, FSurfacePredicate>(
			EBlueprintHelperReviewSurface::DataAsset,
			[&Change]()
			{
				return BlueprintHelperReviewShouldShowInDataAsset(Change);
			})
	};

	for (const TPair<EBlueprintHelperReviewSurface, FSurfacePredicate>& Predicate : SurfacePredicates)
	{
		if (Predicate.Key == Surface)
		{
			return Predicate.Value();
		}
	}
	return false;
}
