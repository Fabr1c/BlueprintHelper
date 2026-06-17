// BlueprintHelper Review surface geometry coordinator.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"

class SWidget;

struct BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceGeometryResolutionContext
{
	FString ReviewAssetPath;
	TFunction<TSharedPtr<SWidget>(EBlueprintHelperReviewSurface)> ResolveOverlayWidget;
	TFunction<bool(
		const FBlueprintHelperReviewVisibleChange&,
		const TSharedPtr<SWidget>&,
		FBlueprintHelperReviewSurfaceGeometryAnchor&)> ResolveComponentsRowGeometry;
	TFunction<bool(
		const FBlueprintHelperReviewVisibleChange&,
		const TSharedPtr<SWidget>&,
		FBlueprintHelperReviewSurfaceGeometryAnchor&)> ResolveMyBlueprintRowGeometry;
	TFunction<bool(
		const FBlueprintHelperReviewVisibleChange&,
		const TSharedPtr<SWidget>&,
		FBlueprintHelperReviewSurfaceGeometryAnchor&)> ResolveDetailsRowGeometry;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceGeometryEventCallbacks
{
	TFunction<FString()> GetReviewAssetPath;
	TFunction<void(const FString&)> AddDebugMessage;
	TFunction<bool(EBlueprintHelperReviewSurface)> RefreshOverlay;
	TFunction<void()> ProcessDebugFocusTraversalGeometryEvent;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceGeometryCoordinator
{
public:
	bool ResolveRowGeometry(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface,
		const FBlueprintHelperReviewSurfaceGeometryResolutionContext& Context,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor) const;

	void HandleRegisteredRowGeometryChanged(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FBlueprintHelperReviewSurfaceGeometryEventCallbacks& Callbacks) const;

	void HandleSurfaceGeometryInvalidated(
		EBlueprintHelperReviewSurface Surface,
		const FBlueprintHelperReviewSurfaceGeometryEventCallbacks& Callbacks) const;
};
