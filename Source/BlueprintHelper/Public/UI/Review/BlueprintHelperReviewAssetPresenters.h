// BlueprintHelper Review asset-specific presenters.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenter.h"
#include "Widgets/SWidget.h"

struct FBlueprintHelperReviewPanelSurfacePresenterArgs;
template <typename ItemType> class STreeView;

struct BLUEPRINTHELPER_API FBlueprintHelperReviewWidgetTreeRowItem
{
	FName WidgetName;
	FString WidgetClass;
	int32 Depth = 0;
	TArray<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>> Children;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewWidgetTreePresenterState
{
	TArray<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>> RootItems;
	TSharedPtr<STreeView<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>>> TreeView;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewUMGWidgetTreePresenter
{
public:
	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(
		const FBlueprintHelperReviewAssetContext& Context,
		FBlueprintHelperReviewWidgetTreePresenterState& State,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated = FBlueprintHelperReviewGeometryInvalidated());
	static TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args);
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewDataTablePresenter
{
public:
	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(
		const FBlueprintHelperReviewAssetContext& Context,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated = FBlueprintHelperReviewGeometryInvalidated());
	static TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args);
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewDataAssetPresenter
{
public:
	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(
		const FBlueprintHelperReviewAssetContext& Context,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated = FBlueprintHelperReviewGeometryInvalidated());
	static TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args);
};
