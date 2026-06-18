// BlueprintHelper Review surface router.

#include "UI/Review/BlueprintHelperReviewSurfaceRouter.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "Systems/Review/BlueprintHelperReviewTargetIdentity.h"
#include "UI/Review/BlueprintHelperReviewSurfaceProjectionRegistry.h"

FBlueprintHelperReviewSurfaceRouteDecision FBlueprintHelperReviewSurfacePresenterRouter::RouteChangeToSurface(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface)
{
	FBlueprintHelperReviewSurfaceRouteDecision Decision;
	Decision.bHasExplicitTargets = BlueprintHelperReviewHasExplicitTargets(Change);
	Decision.ExplicitTargetCount = Change.AtomicTargets.Num();
	Decision.MatchingTargetCount = 0;
	const TSharedRef<FBlueprintHelperReviewSurfaceProjectionRegistry> ProjectionRegistry =
		FBlueprintHelperReviewSurfaceProjectionRegistry::CreateDefault();
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		FBlueprintHelperReviewTargetIdentity Identity =
			FBlueprintHelperReviewTargetIdentity::FromAtomicTarget(Change, Target);
		Identity.AssetKind.Reset();
		Identity.SurfaceKind = BlueprintHelperReviewSurfaceToString(Surface);
		if (ProjectionRegistry->FindProjectionAdapter(Identity).bAvailable)
		{
			++Decision.MatchingTargetCount;
		}
	}

	if (Decision.bHasExplicitTargets)
	{
		Decision.bShouldShow = Decision.MatchingTargetCount > 0;
		Decision.Reason = Decision.bShouldShow ? TEXT("target_match") : TEXT("no_surface_anchor");
		return Decision;
	}

	Decision.bShouldShow = false;
	Decision.Reason = TEXT("missing_explicit_targets");
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
		{ EBlueprintHelperReviewAssetKind::Material, EBlueprintHelperReviewSurface::Material },
		{ EBlueprintHelperReviewAssetKind::MaterialInstance, EBlueprintHelperReviewSurface::Material },
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
		|| Surface == EBlueprintHelperReviewSurface::DataAsset
		|| Surface == EBlueprintHelperReviewSurface::Material;
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
		{ EBlueprintHelperReviewSurface::DataAsset, TEXT("DataAsset") },
		{ EBlueprintHelperReviewSurface::Material, TEXT("Material") }
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
