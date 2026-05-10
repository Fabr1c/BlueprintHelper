// BlueprintHelper Review UMG widget tree presenter.

#include "UI/Review/BlueprintHelperReviewWidgetTreePresenter.h"

#include "Blueprint/WidgetTree.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewPresenterWidgetUtils.h"
#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"
#include "WidgetBlueprint.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Views/STreeView.h"

bool FBlueprintHelperReviewUMGWidgetTreePresenter::ShouldShowChange(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	return FBlueprintHelperReviewPresenterWidgetUtils::ShouldShowIndependentSurfaceChange(
		Change,
		EBlueprintHelperReviewSurface::UMGWidgetTree,
		{TEXT("umg_widget"), TEXT("umg_widget_property"), TEXT("asset_factory")});
}

TSharedRef<SWidget> FBlueprintHelperReviewUMGWidgetTreePresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext& Context,
	FBlueprintHelperReviewWidgetTreePresenterState& State,
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
{
	State.RootItems.Reset();
	State.TreeView.Reset();

	TArray<FString> PlaceholderLines;
	PlaceholderLines.Add(FString::Printf(TEXT("Asset: %s"), *Context.AssetPath));
	PlaceholderLines.Add(FString::Printf(TEXT("Kind: %s"), BlueprintHelperReviewAssetKindToString(Context.AssetKind)));

	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Context.Blueprint.Get());
	if (!WidgetBlueprint)
	{
		PlaceholderLines.Add(TEXT("WidgetTree: unavailable"));
		return FBlueprintHelperReviewPresenterWidgetUtils::BuildSummaryPanel(
			TEXT("UMG Widget Tree"),
			PlaceholderLines,
			FString(),
			EBlueprintHelperReviewSurface::Unknown,
			OnGeometryInvalidated);
	}

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		PlaceholderLines.Add(TEXT("WidgetTree: unavailable"));
		return FBlueprintHelperReviewPresenterWidgetUtils::BuildSummaryPanel(
			TEXT("UMG Widget Tree"),
			PlaceholderLines,
			FString(),
			EBlueprintHelperReviewSurface::Unknown,
			OnGeometryInvalidated);
	}

	TSet<const UWidget*> ReachableWidgets;
	if (WidgetTree->RootWidget)
	{
		TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem> RootItem =
			FBlueprintHelperReviewPresenterWidgetUtils::BuildWidgetTreeRowItem(
				WidgetTree->RootWidget,
				0,
				ReachableWidgets);
		if (RootItem.IsValid())
		{
			State.RootItems.Add(RootItem);
		}
	}

	TArray<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>> UnparentedItems;
	FBlueprintHelperReviewPresenterWidgetUtils::CollectUnparentedWidgetTreeItems(
		WidgetTree,
		ReachableWidgets,
		UnparentedItems);
	if (UnparentedItems.Num() > 0)
	{
		TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem> UnparentedGroup =
			FBlueprintHelperReviewPresenterWidgetUtils::MakeWidgetTreeRowItem(
				FName(TEXT("Unparented Widgets")),
				FString(),
				0);
		UnparentedGroup->Children = MoveTemp(UnparentedItems);
		State.RootItems.Add(UnparentedGroup);
	}

	const FString AssetPath = Context.AssetPath;
	TSharedRef<STreeView<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>>> TreeView =
		SAssignNew(State.TreeView, STreeView<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>>)
		.TreeItemsSource(&State.RootItems)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow_Lambda([AssetPath, OnGeometryInvalidated](
			TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem> Item,
			const TSharedRef<STableViewBase>& OwnerTable)
		{
			return FBlueprintHelperReviewPresenterWidgetUtils::GenerateWidgetTreeRow(
				Item,
				OwnerTable,
				AssetPath,
				OnGeometryInvalidated);
		})
		.OnGetChildren_Lambda([](
			TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem> Item,
			TArray<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>>& OutChildren)
		{
			if (Item.IsValid())
			{
				OutChildren.Append(Item->Children);
			}
		});

	TreeView->RequestTreeRefresh();
	for (const TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>& RootItem : State.RootItems)
	{
		FBlueprintHelperReviewPresenterWidgetUtils::ExpandWidgetTreeRows(State.TreeView, RootItem);
	}

	return SNew(SBorder)
		.Padding(8.0f)
		[
			TreeView
		];
}

TSharedRef<SWidget> FBlueprintHelperReviewUMGWidgetTreePresenter::BuildOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
{
	return FBlueprintHelperReviewRowHighlightModel::BuildRowHighlightOverlay(
		Args,
		EBlueprintHelperReviewSurface::UMGWidgetTree,
		&FBlueprintHelperReviewUMGWidgetTreePresenter::ShouldShowChange);
}

