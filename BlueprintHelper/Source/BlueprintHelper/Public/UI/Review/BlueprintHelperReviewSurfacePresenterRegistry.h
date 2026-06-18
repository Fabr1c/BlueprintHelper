// BlueprintHelper Review surface presenter registry.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"
#include "Widgets/SWidget.h"

typedef bool (*FBlueprintHelperReviewSurfaceChangePredicate)(
	const FBlueprintHelperReviewVisibleChange& Change);

typedef TFunction<TSharedRef<SWidget>(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)> FBlueprintHelperReviewSurfaceOverlayBuilder;

class BLUEPRINTHELPER_API FBlueprintHelperReviewSurfacePresenterRegistry
{
public:
	static TSharedRef<FBlueprintHelperReviewSurfacePresenterRegistry> CreateDefault();

	void RegisterSurfacePresenter(
		EBlueprintHelperReviewSurface Surface,
		FBlueprintHelperReviewSurfaceChangePredicate Predicate);
	void RegisterSurfacePresenter(
		EBlueprintHelperReviewSurface Surface,
		FBlueprintHelperReviewSurfaceChangePredicate Predicate,
		FBlueprintHelperReviewSurfaceOverlayBuilder OverlayBuilder);
	void RegisterOverlayBuilder(
		EBlueprintHelperReviewSurface Surface,
		FBlueprintHelperReviewSurfaceOverlayBuilder OverlayBuilder);

	FBlueprintHelperReviewSurfaceChangePredicate FindPredicate(
		EBlueprintHelperReviewSurface Surface) const;
	bool CanBuildOverlay(EBlueprintHelperReviewSurface Surface) const;
	TSharedRef<SWidget> BuildOverlayOrNull(
		EBlueprintHelperReviewSurface Surface,
		const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args) const;

private:
	struct FSurfacePresenterDescriptor
	{
		FBlueprintHelperReviewSurfaceChangePredicate Predicate = nullptr;
		FBlueprintHelperReviewSurfaceOverlayBuilder OverlayBuilder;
	};

	TMap<EBlueprintHelperReviewSurface, FSurfacePresenterDescriptor> SurfacePresenters;
};
