// BlueprintHelper Review surface geometry coordinator.

#include "UI/Review/BlueprintHelperReviewSurfaceGeometryCoordinator.h"

#include "UI/Review/BlueprintHelperReviewSlateRowGeometryRegistry.h"
#include "UI/Review/BlueprintHelperReviewSurfaceFrameBuilder.h"

bool FBlueprintHelperReviewSurfaceGeometryCoordinator::ResolveRowGeometry(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface,
	const FBlueprintHelperReviewSurfaceGeometryResolutionContext& Context,
	FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor) const
{
	TSharedPtr<SWidget> OverlayWidget;
	if (Context.ResolveOverlayWidget)
	{
		OverlayWidget = Context.ResolveOverlayWidget(Surface);
	}
	if (!OverlayWidget.IsValid())
	{
		OutAnchor.Reason = TEXT("unsupported_surface_geometry");
		return false;
	}

	if (Surface == EBlueprintHelperReviewSurface::Components && Context.ResolveComponentsRowGeometry)
	{
		if (Context.ResolveComponentsRowGeometry(Change, OverlayWidget, OutAnchor))
		{
			return true;
		}
	}
	else if (Surface == EBlueprintHelperReviewSurface::MyBlueprint && Context.ResolveMyBlueprintRowGeometry)
	{
		if (Context.ResolveMyBlueprintRowGeometry(Change, OverlayWidget, OutAnchor))
		{
			return true;
		}
	}
	else if (Surface == EBlueprintHelperReviewSurface::Details && Context.ResolveDetailsRowGeometry)
	{
		if (Context.ResolveDetailsRowGeometry(Change, OverlayWidget, OutAnchor))
		{
			return true;
		}
	}

	const FString TargetText = FBlueprintHelperReviewSurfaceFrameBuilder::GetReviewTargetText(Change, Surface);
	if (TargetText.IsEmpty())
	{
		OutAnchor.Reason = TEXT("missing_geometry_target");
		return false;
	}

	const FString PrimaryAssetPath = Change.AssetPath.IsEmpty() ? Context.ReviewAssetPath : Change.AssetPath;
	if (FBlueprintHelperReviewSlateRowGeometryRegistry::ResolveRowGeometry(
		PrimaryAssetPath,
		Surface,
		TargetText,
		OverlayWidget,
		OutAnchor))
	{
		return true;
	}

	if (PrimaryAssetPath != Context.ReviewAssetPath)
	{
		return FBlueprintHelperReviewSlateRowGeometryRegistry::ResolveRowGeometry(
			Context.ReviewAssetPath,
			Surface,
			TargetText,
			OverlayWidget,
			OutAnchor);
	}

	return false;
}

void FBlueprintHelperReviewSurfaceGeometryCoordinator::HandleRegisteredRowGeometryChanged(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FBlueprintHelperReviewSurfaceGeometryEventCallbacks& Callbacks) const
{
	if (Surface == EBlueprintHelperReviewSurface::Unknown)
	{
		return;
	}

	const FString ReviewAssetPath = Callbacks.GetReviewAssetPath ? Callbacks.GetReviewAssetPath() : FString();
	if (!ReviewAssetPath.IsEmpty()
		&& !AssetPath.IsEmpty()
		&& AssetPath != ReviewAssetPath)
	{
		return;
	}

	if (Callbacks.AddDebugMessage)
	{
		Callbacks.AddDebugMessage(FString::Printf(
			TEXT("ReviewRowLifecycle surface=%s event=row_registered asset=\"%s\" result=refresh"),
			BlueprintHelperReviewSurfaceToString(Surface),
			*AssetPath));
	}
	if (Callbacks.RefreshOverlay)
	{
		Callbacks.RefreshOverlay(Surface);
	}
	if (Callbacks.ProcessDebugFocusTraversalGeometryEvent)
	{
		Callbacks.ProcessDebugFocusTraversalGeometryEvent();
	}
}

void FBlueprintHelperReviewSurfaceGeometryCoordinator::HandleSurfaceGeometryInvalidated(
	EBlueprintHelperReviewSurface Surface,
	const FBlueprintHelperReviewSurfaceGeometryEventCallbacks& Callbacks) const
{
	if (Surface == EBlueprintHelperReviewSurface::Unknown)
	{
		return;
	}

	if (Callbacks.AddDebugMessage)
	{
		Callbacks.AddDebugMessage(FString::Printf(
			TEXT("ReviewRowLifecycle surface=%s event=geometry_changed result=refresh"),
			BlueprintHelperReviewSurfaceToString(Surface)));
	}
	if (Callbacks.RefreshOverlay)
	{
		Callbacks.RefreshOverlay(Surface);
	}
	if (Callbacks.ProcessDebugFocusTraversalGeometryEvent)
	{
		Callbacks.ProcessDebugFocusTraversalGeometryEvent();
	}
}
