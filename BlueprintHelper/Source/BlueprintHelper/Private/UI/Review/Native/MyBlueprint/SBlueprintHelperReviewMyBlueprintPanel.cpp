// BlueprintHelper Review native My Blueprint panel.

#include "UI/Review/Native/MyBlueprint/SBlueprintHelperReviewMyBlueprintPanel.h"

#include "Styling/AppStyle.h"
#include "UI/Review/Native/MyBlueprint/SBlueprintHelperReviewMyBlueprintRow.h"
#include "Widgets/Layout/SBorder.h"

void SBlueprintHelperReviewMyBlueprintPanel::Construct(const FArguments& InArgs)
{
	RootItemsSource = InArgs._RootItemsSource;
	AssetPath = InArgs._AssetPath;
	OnGeometryInvalidated = InArgs._OnGeometryInvalidated;
	OnNavigateToGraph = InArgs._OnNavigateToGraph;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.Padding(4.0f)
		[
			SAssignNew(TreeView, STreeView<TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>>)
			.TreeItemsSource(RootItemsSource)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SBlueprintHelperReviewMyBlueprintPanel::OnGenerateRow)
			.OnGetChildren(this, &SBlueprintHelperReviewMyBlueprintPanel::OnGetChildren)
			.ItemHeight(25.0f)
		]
	];

	if (RootItemsSource && TreeView.IsValid())
	{
		for (const TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>& RootItem : *RootItemsSource)
		{
			ExpandItemRecursive(RootItem);
		}
	}
}

TSharedRef<ITableRow> SBlueprintHelperReviewMyBlueprintPanel::OnGenerateRow(
	TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SBlueprintHelperReviewMyBlueprintRow, OwnerTable)
		.Item(Item)
		.AssetPath(AssetPath)
		.OnGeometryInvalidated(OnGeometryInvalidated)
		.OnNavigateToGraph(OnNavigateToGraph);
}

void SBlueprintHelperReviewMyBlueprintPanel::OnGetChildren(
	TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem> Item,
	TArray<TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>>& OutChildren) const
{
	if (Item.IsValid())
	{
		OutChildren.Append(Item->Children);
	}
}

void SBlueprintHelperReviewMyBlueprintPanel::RequestRowsRefresh() const
{
	if (!TreeView.IsValid())
	{
		return;
	}

	TreeView->RequestTreeRefresh();
}

void SBlueprintHelperReviewMyBlueprintPanel::ExpandItemRecursive(
	TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem> Item) const
{
	if (!TreeView.IsValid() || !Item.IsValid())
	{
		return;
	}

	TreeView->SetItemExpansion(Item, true);
	for (const TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>& Child : Item->Children)
	{
		if (Child.IsValid() && Child->Children.Num() > 0)
		{
			ExpandItemRecursive(Child);
		}
	}
}
