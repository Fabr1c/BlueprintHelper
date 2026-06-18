// BlueprintHelper Review surface presenter registry.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"
#include "UI/Review/BlueprintHelperReviewSurfaceContentPresenterTypes.h"
#include "Widgets/SWidget.h"

typedef bool (*FBlueprintHelperReviewSurfaceChangePredicate)(
	const FBlueprintHelperReviewVisibleChange& Change);

typedef TFunction<TSharedRef<SWidget>(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)> FBlueprintHelperReviewSurfaceOverlayBuilder;

typedef TFunction<TSharedRef<SWidget>(
	const FBlueprintHelperReviewPanelSurfaceContentArgs& Args)> FBlueprintHelperReviewSurfaceContentBuilder;

typedef TFunction<bool(
	const FBlueprintHelperReviewSurfaceRowsRefreshArgs& Args)> FBlueprintHelperReviewSurfaceRowsRefreshBuilder;

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
	void RegisterContentBuilder(
		EBlueprintHelperReviewSurface Surface,
		FBlueprintHelperReviewSurfaceContentBuilder ContentBuilder);
	void RegisterHostBinding(
		const FBlueprintHelperReviewSurfaceHostBinding& HostBinding);
	void RegisterRowsRefreshBuilder(
		EBlueprintHelperReviewSurface Surface,
		FBlueprintHelperReviewSurfaceRowsRefreshBuilder RowsRefreshBuilder);
	void RegisterDetailsObjectPolicy(
		EBlueprintHelperReviewSurface Surface,
		bool bUsesDetailsObject);

	FBlueprintHelperReviewSurfaceChangePredicate FindPredicate(
		EBlueprintHelperReviewSurface Surface) const;
	const FBlueprintHelperReviewSurfaceHostBinding* FindHostBinding(
		EBlueprintHelperReviewSurface Surface) const;
	TArray<FBlueprintHelperReviewSurfaceHostBinding> ListHostBindings() const;
	TMap<EBlueprintHelperReviewSurface, TFunction<bool()>> BuildRowsRefreshHandlers(
		const FBlueprintHelperReviewSurfaceRowsRefreshArgs& Args) const;
	bool UsesDetailsObject(EBlueprintHelperReviewSurface Surface) const;
	bool CanBuildOverlay(EBlueprintHelperReviewSurface Surface) const;
	TSharedRef<SWidget> BuildOverlayOrNull(
		EBlueprintHelperReviewSurface Surface,
		const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args) const;
	bool CanBuildContent(EBlueprintHelperReviewSurface Surface) const;
	TSharedRef<SWidget> BuildContentOrError(
		EBlueprintHelperReviewSurface Surface,
		const FBlueprintHelperReviewPanelSurfaceContentArgs& Args) const;

private:
	struct FSurfacePresenterDescriptor
	{
		FBlueprintHelperReviewSurfaceChangePredicate Predicate = nullptr;
		FBlueprintHelperReviewSurfaceOverlayBuilder OverlayBuilder;
		FBlueprintHelperReviewSurfaceContentBuilder ContentBuilder;
		FBlueprintHelperReviewSurfaceHostBinding HostBinding;
		bool bHasHostBinding = false;
		FBlueprintHelperReviewSurfaceRowsRefreshBuilder RowsRefreshBuilder;
		bool bUsesDetailsObject = true;
	};

	TMap<EBlueprintHelperReviewSurface, FSurfacePresenterDescriptor> SurfacePresenters;
};
