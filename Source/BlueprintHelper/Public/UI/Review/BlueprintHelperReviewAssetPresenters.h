// BlueprintHelper Review asset-specific presenters.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenter.h"
#include "Widgets/SWidget.h"

struct FBlueprintHelperReviewPanelSurfacePresenterArgs;
class IPropertyRowGenerator;
class IDetailTreeNode;
struct FDataTableEditorColumnHeaderData;
struct FDataTableEditorRowListViewData;
template <typename ItemType> class STreeView;
template <typename ItemType> class SListView;

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

struct BLUEPRINTHELPER_API FBlueprintHelperReviewDataTablePresenterState
{
	TArray<TSharedPtr<FDataTableEditorColumnHeaderData>> Columns;
	TArray<TSharedPtr<FDataTableEditorRowListViewData>> Rows;
	TSharedPtr<SListView<TSharedPtr<FDataTableEditorRowListViewData>>> ListView;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewDataAssetRowItem
{
	FString Label;
	FString Value;
	FString SearchText;
	int32 Depth = 0;
	bool bIsSection = false;
	TSharedPtr<IDetailTreeNode> DetailNode;
	TWeakPtr<SWidget> RowWidget;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewDataAssetPresenterState
{
	TArray<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>> Rows;
	TSharedPtr<SListView<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>> ListView;
	TSharedPtr<IPropertyRowGenerator> PropertyRowGenerator;
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
		FBlueprintHelperReviewDataTablePresenterState& State,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated = FBlueprintHelperReviewGeometryInvalidated());
	static TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args);
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewDataAssetPresenter
{
public:
	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(
		const FBlueprintHelperReviewAssetContext& Context,
		FBlueprintHelperReviewDataAssetPresenterState& State,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated = FBlueprintHelperReviewGeometryInvalidated());
	static TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args);
};
