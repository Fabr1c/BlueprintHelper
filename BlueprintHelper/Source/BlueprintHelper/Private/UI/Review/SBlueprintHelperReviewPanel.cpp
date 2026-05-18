// BlueprintHelper fake Review panel implementation.

#include "UI/Review/SBlueprintHelperReviewPanel.h"

#include "Async/Async.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/DateTime.h"
#include "IDetailsView.h"
#include "PropertyEditorDelegates.h"
#include "PropertyPath.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "SKismetInspector.h"
#include "UI/Review/BlueprintHelperReviewDebugText.h"
#include "UI/Review/BlueprintHelperReviewPanelGeometryUtils.h"
#include "UI/Review/BlueprintHelperReviewPanelStyle.h"
#include "UI/Review/BlueprintHelperReviewPanelPresenter.h"
#include "UI/Review/BlueprintHelperReviewAssetPresenters.h"
#include "UI/Review/BlueprintHelperReviewDebugBundleService.h"
#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"
#include "UI/Review/BlueprintHelperReviewSlateRowGeometryRegistry.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenter.h"
#include "UI/Review/Utils/BlueprintHelperReviewPanelAsyncUtils.h"
#include "UI/Review/Utils/BlueprintHelperReviewPanelLocalUtils.h"
#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Styling/AppStyle.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

SBlueprintHelperReviewPanel::~SBlueprintHelperReviewPanel()
{
	if (ReviewPanelPresenter.IsValid() && PendingReviewChangedHandle.IsValid())
	{
		ReviewPanelPresenter->RemovePendingReviewChangedHandler(PendingReviewChangedHandle);
	}
	FBlueprintHelperReviewSlateRowGeometryRegistry::RemoveRowsChangedHandler(RowGeometryChangedHandle);
	FlushAsyncTasks();
}

void SBlueprintHelperReviewPanel::FlushAsyncTasks()
{
	FBlueprintHelperReviewPanelAsyncUtils::FlushTasks();
}

void SBlueprintHelperReviewPanel::ShutdownAsyncTasks()
{
	FBlueprintHelperReviewPanelAsyncUtils::ShutdownTasks();
}

void SBlueprintHelperReviewPanel::RefreshVisibleChanges(
	const TArray<FBlueprintHelperReviewVisibleChange>& SourceChanges)
{
	ChangeItems.Empty();
	for (const FBlueprintHelperReviewVisibleChange& Change : SourceChanges)
	{
		ChangeItems.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(Change));
	}
	RebuildChangeTreeItems();
}

void SBlueprintHelperReviewPanel::OnChangeSelectionChanged(FReviewChangeItem Item, ESelectInfo::Type SelectInfo)
{
	const bool bKeepGraphNavigationRequest =
		bAllowGraphNavigationWithoutGraphReview
		&& Item.IsValid()
		&& !RequestedGraphNavigationChangeId.IsEmpty()
		&& Item->ChangeId == RequestedGraphNavigationChangeId;

	if (!bKeepGraphNavigationRequest)
	{
		RequestedGraphNavigationChangeId.Reset();
		RequestedGraphNavigationGraphName.Reset();
		bAllowGraphNavigationWithoutGraphReview = false;
	}

	SelectedChange = Item;
	LoadReviewAssetFromSelection();
	RefreshMainWorkspaceAfterReviewStateChanged();
	StartFlash();
	if (Item.IsValid())
	{
		AddDebugMessage(FString::Printf(
			TEXT("Selected change id=%s label=\"%s\" asset=\"%s\" graph=\"%s\" latest=%s"),
			*Item->ChangeId,
			*Item->DisplayLabel,
			*Item->AssetPath,
			*Item->GraphName,
			*Item->LatestTransactionId));
	}
	if (SelectInfo != ESelectInfo::Direct)
	{
		RefreshChangeTreeWidget();
	}
}

TSharedRef<ITableRow> SBlueprintHelperReviewPanel::GenerateChangeTreeRow(
	FReviewTreeItemPtr Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	if (Item.IsValid() && Item->bIsAssetRoot)
	{
		return SNew(STableRow<FReviewTreeItemPtr>, OwnerTable)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
			.Padding(7.0f, 5.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->AssetPath.IsEmpty() ? TEXT("(unknown asset)") : Item->AssetPath))
			]
		];
	}

	return SNew(STableRow<FReviewTreeItemPtr>, OwnerTable)
	.Padding(0.0f)
	[
		BuildDiffRow(Item.IsValid() ? Item->Change : FReviewChangeItem(), true)
	];
}

void SBlueprintHelperReviewPanel::GetChangeTreeChildren(
	FReviewTreeItemPtr Item,
	TArray<FReviewTreeItemPtr>& OutChildren) const
{
	if (Item.IsValid())
	{
		OutChildren.Append(Item->Children);
	}
}

void SBlueprintHelperReviewPanel::OnChangeTreeSelectionChanged(
	FReviewTreeItemPtr Item,
	ESelectInfo::Type SelectInfo)
{
	if (!Item.IsValid())
	{
		return;
	}

	if (Item->bIsAssetRoot)
	{
		if (ChangeTreeView.IsValid())
		{
			ChangeTreeView->SetItemExpansion(Item, true);
		}
		if (Item->Children.Num() > 0)
		{
			OnChangeTreeSelectionChanged(Item->Children[0], SelectInfo);
		}
		return;
	}

	if (Item->Change.IsValid())
	{
		OnChangeSelectionChanged(Item->Change, ESelectInfo::Direct);
	}
}

void SBlueprintHelperReviewPanel::OnMyBlueprintGraphNavigationRequested(
	const FString& ChangeId,
	const FString& GraphName)
{
	FReviewChangeItem Item = FindChangeItemById(ChangeId);
	if (!Item.IsValid())
	{
		AddDebugMessage(FString::Printf(
			TEXT("MyBlueprintNavigate failed change=%s graph=\"%s\" reason=change_not_found"),
			*ChangeId,
			*GraphName));
		return;
	}

	if (!GraphName.IsEmpty())
	{
		Item->GraphName = GraphName;
		RequestedGraphNavigationChangeId = Item->ChangeId;
		RequestedGraphNavigationGraphName = GraphName;
		bAllowGraphNavigationWithoutGraphReview = true;
	}

	OnChangeSelectionChanged(Item, ESelectInfo::Direct);
	RefreshChangeTreeWidget();
	AddDebugMessage(FString::Printf(
		TEXT("MyBlueprintNavigate change=%s graph=\"%s\" label=\"%s\""),
		*Item->ChangeId,
		*Item->GraphName,
		*Item->DisplayLabel));
}

void SBlueprintHelperReviewPanel::RebuildChangeTreeItems()
{
	BuildChangeTreeItemsFromChangeItems(ChangeItems, ChangeTreeRootItems);
}

void SBlueprintHelperReviewPanel::BuildChangeTreeItemsFromChangeItems(
	const TArray<FReviewChangeItem>& SourceItems,
	TArray<FReviewTreeItemPtr>& OutRootItems)
{
	OutRootItems.Reset();
	TMap<FString, FReviewTreeItemPtr> AssetRootsByPath;
	TMap<FString, FReviewTreeItemPtr> LifecycleRootItemsByAssetAndChangeId;
	TArray<FReviewTreeItemPtr> LeafItems;
	for (const FReviewChangeItem& Item : SourceItems)
	{
		if (!Item.IsValid())
		{
			continue;
		}

		const FString AssetPath = Item->AssetPath.IsEmpty() ? TEXT("(unknown asset)") : Item->AssetPath;
		const FString AssetKey = FBlueprintHelperReviewPanelLocalUtils::MakeAssetTreeKey(AssetPath);
		FReviewTreeItemPtr* ExistingRoot = AssetRootsByPath.Find(AssetKey);
		if (!ExistingRoot)
		{
			FReviewTreeItemPtr Root = MakeShared<FReviewTreeItem>();
			Root->bIsAssetRoot = true;
			Root->AssetPath = AssetKey;
			AssetRootsByPath.Add(AssetKey, Root);
			OutRootItems.Add(Root);
			ExistingRoot = AssetRootsByPath.Find(AssetKey);
		}

		FReviewTreeItemPtr Leaf = MakeShared<FReviewTreeItem>();
		Leaf->AssetPath = AssetKey;
		Leaf->Change = Item;
		LeafItems.Add(Leaf);
		if (Item->bIsAssetLifecycleRoot && !Item->ChangeId.IsEmpty())
		{
			LifecycleRootItemsByAssetAndChangeId.Add(
				FString::Printf(TEXT("%s|%s"), *AssetKey, *Item->ChangeId),
				Leaf);
		}
	}

	for (const FReviewTreeItemPtr& Leaf : LeafItems)
	{
		if (!Leaf.IsValid() || !Leaf->Change.IsValid())
		{
			continue;
		}

		const FString AssetKey = Leaf->AssetPath.IsEmpty() ? TEXT("(unknown asset)") : Leaf->AssetPath;
		FReviewTreeItemPtr* ExistingRoot = AssetRootsByPath.Find(AssetKey);
		if (!ExistingRoot || !ExistingRoot->IsValid())
		{
			continue;
		}

		const FBlueprintHelperReviewVisibleChange& Change = *Leaf->Change;
		if (!Change.bIsAssetLifecycleRoot)
		{
			FReviewTreeItemPtr* ParentRoot = nullptr;
			if (!Change.ParentChangeId.IsEmpty())
			{
				ParentRoot = LifecycleRootItemsByAssetAndChangeId.Find(
					FString::Printf(TEXT("%s|%s"), *AssetKey, *Change.ParentChangeId));
			}
			if (ParentRoot)
			{
				if (ParentRoot->IsValid())
				{
					(*ParentRoot)->Children.Add(Leaf);
					continue;
				}
			}
		}

		(*ExistingRoot)->Children.Add(Leaf);
	}
}

void SBlueprintHelperReviewPanel::RefreshChangeTreeWidget()
{
	if (!ChangeTreeView.IsValid())
	{
		return;
	}

	ChangeTreeView->RequestTreeRefresh();
	for (const FReviewTreeItemPtr& Root : ChangeTreeRootItems)
	{
		if (Root.IsValid())
		{
			ExpandChangeTreeItemRecursive(Root);
		}
	}

	if (SelectedChange.IsValid())
	{
		if (FReviewTreeItemPtr TreeItem = FindTreeItemForChange(SelectedChange))
		{
			ChangeTreeView->SetSelection(TreeItem);
		}
	}
}

SBlueprintHelperReviewPanel::FReviewTreeItemPtr SBlueprintHelperReviewPanel::FindTreeItemForChange(
	FReviewChangeItem Item) const
{
	if (!Item.IsValid())
	{
		return nullptr;
	}
	return FindTreeItemForChangeRecursive(ChangeTreeRootItems, Item);
}

SBlueprintHelperReviewPanel::FReviewTreeItemPtr SBlueprintHelperReviewPanel::FindTreeItemForChangeRecursive(
	const TArray<FReviewTreeItemPtr>& Items,
	FReviewChangeItem ChangeItem)
{
	for (const FReviewTreeItemPtr& Item : Items)
	{
		if (!Item.IsValid())
		{
			continue;
		}
		if (Item->Change == ChangeItem)
		{
			return Item;
		}
		if (FReviewTreeItemPtr ChildMatch = FindTreeItemForChangeRecursive(Item->Children, ChangeItem))
		{
			return ChildMatch;
		}
	}
	return nullptr;
}

void SBlueprintHelperReviewPanel::ExpandChangeTreeItemRecursive(FReviewTreeItemPtr Item)
{
	if (!ChangeTreeView.IsValid() || !Item.IsValid())
	{
		return;
	}

	ChangeTreeView->SetItemExpansion(Item, true);
	for (const FReviewTreeItemPtr& Child : Item->Children)
	{
		ExpandChangeTreeItemRecursive(Child);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
TArray<SBlueprintHelperReviewPanel::FReviewTreeSnapshotEntry>
SBlueprintHelperReviewPanel::BuildReviewTreeSnapshotForTesting(
	const TArray<FBlueprintHelperReviewVisibleChange>& SourceChanges)
{
	TArray<FReviewChangeItem> SourceItems;
	for (const FBlueprintHelperReviewVisibleChange& SourceChange : SourceChanges)
	{
		SourceItems.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(SourceChange));
	}

	TArray<FReviewTreeItemPtr> RootItems;
	BuildChangeTreeItemsFromChangeItems(SourceItems, RootItems);

	TArray<FReviewTreeSnapshotEntry> Snapshot;
	TArray<TPair<FReviewTreeItemPtr, int32>> PendingItems;
	for (int32 Index = RootItems.Num() - 1; Index >= 0; --Index)
	{
		PendingItems.Emplace(RootItems[Index], 0);
	}
	while (PendingItems.Num() > 0)
	{
		const TPair<FReviewTreeItemPtr, int32> PendingItem = FBlueprintHelperVersionCompat::PopNoShrink(PendingItems);
		const FReviewTreeItemPtr Item = PendingItem.Key;
		const int32 Depth = PendingItem.Value;
		if (!Item.IsValid())
		{
			continue;
		}

		FReviewTreeSnapshotEntry Entry;
		Entry.bIsAssetHeader = Item->bIsAssetRoot;
		Entry.AssetPath = Item->AssetPath;
		Entry.Depth = Depth;
		Entry.ChangeId = Item->Change.IsValid() ? Item->Change->ChangeId : FString();
		Snapshot.Add(Entry);

		for (int32 ChildIndex = Item->Children.Num() - 1; ChildIndex >= 0; --ChildIndex)
		{
			PendingItems.Emplace(Item->Children[ChildIndex], Depth + 1);
		}
	}
	return Snapshot;
}
#endif


SBlueprintHelperReviewPanel::FReviewChangeItem SBlueprintHelperReviewPanel::FindChangeItemById(
	const FString& ChangeId) const
{
	for (const FReviewChangeItem& Item : ChangeItems)
	{
		if (Item.IsValid() && Item->ChangeId == ChangeId)
		{
			return Item;
		}
	}
	return FReviewChangeItem();
}

TArray<FBlueprintHelperReviewVisibleChange> SBlueprintHelperReviewPanel::BuildPendingChangeSnapshot() const
{
	TArray<FBlueprintHelperReviewVisibleChange> PendingChanges;
	for (const FReviewChangeItem& Item : ChangeItems)
	{
		if (Item.IsValid())
		{
			PendingChanges.Add(*Item);
		}
	}
	return PendingChanges;
}

FString SBlueprintHelperReviewPanel::BuildVisibleChangeRefreshSignature(
	const TArray<FBlueprintHelperReviewVisibleChange>& Changes)
{
	TArray<FString> Parts;
	Parts.Reserve(Changes.Num());
	for (const FBlueprintHelperReviewVisibleChange& Change : Changes)
	{
		Parts.Add(FString::Printf(
			TEXT("%s|%s|%s|%s|%s|%s"),
			*Change.ChangeId,
			BlueprintHelperReviewChangeStatusToString(Change.Status),
			*Change.AssetPath,
			*Change.ParentChangeId,
			*Change.LocationKey,
			*Change.LatestTransactionId));
	}
	return FString::Join(Parts, TEXT("\n"));
}

void SBlueprintHelperReviewPanel::SelectNextChangeAfterRemoval(
	const FString& PreferredAssetPath,
	int32 RemovedIndex)
{
	if (ChangeItems.Num() == 0)
	{
		SelectedChange.Reset();
		return;
	}

	if (!PreferredAssetPath.IsEmpty())
	{
		const int32 StartIndex = FMath::Clamp(RemovedIndex, 0, ChangeItems.Num() - 1);
		for (int32 Offset = 0; Offset < ChangeItems.Num(); ++Offset)
		{
			const int32 Index = (StartIndex + Offset) % ChangeItems.Num();
			const FReviewChangeItem& Item = ChangeItems[Index];
			if (Item.IsValid()
				&& Item->AssetPath == PreferredAssetPath
				&& !Item->bIsAssetLifecycleRoot)
			{
				SelectedChange = Item;
				return;
			}
		}

		for (const FReviewChangeItem& Item : ChangeItems)
		{
			if (Item.IsValid() && Item->AssetPath == PreferredAssetPath)
			{
				SelectedChange = Item;
				return;
			}
		}
	}

	const int32 NextIndex = FMath::Clamp(RemovedIndex, 0, ChangeItems.Num() - 1);
	SelectedChange = ChangeItems[NextIndex];
}

FReply SBlueprintHelperReviewPanel::OnAcceptChangeId(const FString& ChangeId)
{
	return ExecuteAcceptChange(FindChangeItemById(ChangeId));
}

FReply SBlueprintHelperReviewPanel::OnRejectChangeId(const FString& ChangeId)
{
	return ExecuteRejectChange(FindChangeItemById(ChangeId));
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildReadonlyComponentsWidget()
{
	if (FBlueprintHelperReviewSurfacePresenterRouter::GetStructurePanelSurfaceForAssetKind(ReviewAssetContext.AssetKind)
		== EBlueprintHelperReviewSurface::UMGWidgetTree)
	{
		return FBlueprintHelperReviewUMGWidgetTreePresenter::BuildContent(
			ReviewAssetContext,
			WidgetTreePresenterState,
			FBlueprintHelperReviewGeometryInvalidated::CreateSP(
				this,
				&SBlueprintHelperReviewPanel::OnSurfaceGeometryInvalidated));
	}

	return FBlueprintHelperReviewBlueprintComponentsPresenter::BuildContent(
		ReviewAssetContext,
		ComponentsPresenterState,
		FBlueprintHelperReviewGeometryInvalidated::CreateSP(
			this,
			&SBlueprintHelperReviewPanel::OnSurfaceGeometryInvalidated));
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildReadonlyMyBlueprintWidget()
{
	return FBlueprintHelperReviewMyBlueprintPresenter::BuildContent(
		ReviewAssetContext,
		MyBlueprintPresenterState,
		ChangeItems,
		FBlueprintHelperReviewGeometryInvalidated::CreateSP(
			this,
			&SBlueprintHelperReviewPanel::OnSurfaceGeometryInvalidated),
		FBlueprintHelperReviewMyBlueprintNavigateToGraph::CreateSP(
			this,
			&SBlueprintHelperReviewPanel::OnMyBlueprintGraphNavigationRequested));
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildReadonlyDetailsWidget()
{
	TSharedRef<SWidget> DetailsWidget = FBlueprintHelperReviewObjectDetailsPresenter::BuildContent(
		ReviewAssetContext,
		KismetInspector);
	if (KismetInspector.IsValid())
	{
		if (TSharedPtr<IDetailsView> PropertyView = KismetInspector->GetPropertyView())
		{
			PropertyView->SetOnDisplayedPropertiesChanged(
				FOnDisplayedPropertiesChanged::CreateSP(
					this,
					&SBlueprintHelperReviewPanel::OnDetailsDisplayedPropertiesChanged));
		}
	}
	return DetailsWidget;
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildStructurePanelDiffFrames()
{
	if (FBlueprintHelperReviewSurfacePresenterRouter::GetStructurePanelSurfaceForAssetKind(ReviewAssetContext.AssetKind)
		== EBlueprintHelperReviewSurface::UMGWidgetTree)
	{
		return BuildPanelDiffFrames(
			&FBlueprintHelperReviewUMGWidgetTreePresenter::ShouldShowChange,
			EBlueprintHelperReviewSurface::UMGWidgetTree);
	}

	return BuildPanelDiffFrames(
		&FBlueprintHelperReviewBlueprintComponentsPresenter::ShouldShowChange,
		EBlueprintHelperReviewSurface::Components);
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildMainWorkspaceDiffFrames()
{
	const EBlueprintHelperReviewSurface MainSurface =
		FBlueprintHelperReviewSurfacePresenterRouter::GetMainWorkspaceSurfaceForAssetKind(ReviewAssetContext.AssetKind);
	if (!FBlueprintHelperReviewSurfacePresenterRouter::ShouldMainWorkspaceOwnOverlay(MainSurface))
	{
		return SNullWidget::NullWidget;
	}
	if (MainSurface == EBlueprintHelperReviewSurface::DataTable)
	{
		return BuildPanelDiffFrames(
			&FBlueprintHelperReviewDataTablePresenter::ShouldShowChange,
			EBlueprintHelperReviewSurface::DataTable);
	}
	if (MainSurface == EBlueprintHelperReviewSurface::DataAsset)
	{
		return BuildPanelDiffFrames(
			&FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange,
			EBlueprintHelperReviewSurface::DataAsset);
	}
	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildScopedDiffStack(
	bool (*Predicate)(const FBlueprintHelperReviewVisibleChange&))
{
	TSharedRef<SScrollBox> Stack = SNew(SScrollBox);
	const FString CurrentAssetPath = SelectedChange.IsValid() ? SelectedChange->AssetPath : FString();
	const FString CurrentAssetKey = FBlueprintHelperReviewPanelLocalUtils::MakeAssetTreeKey(CurrentAssetPath);
	int32 VisibleRowCount = 0;
	for (const FReviewChangeItem& Item : ChangeItems)
	{
		if (!Item.IsValid() || !Predicate(*Item))
		{
			continue;
		}
		const FString ItemAssetKey = FBlueprintHelperReviewPanelLocalUtils::MakeAssetTreeKey(Item->AssetPath);
		if (!CurrentAssetKey.IsEmpty() && ItemAssetKey != CurrentAssetKey)
		{
			continue;
		}

		Stack->AddSlot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildDiffRow(Item, true)
		];
		++VisibleRowCount;
	}

	if (VisibleRowCount == 0)
	{
		return SNullWidget::NullWidget;
	}

	return Stack;
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildPanelDiffFrames(
	bool (*Predicate)(const FBlueprintHelperReviewVisibleChange&),
	EBlueprintHelperReviewSurface Surface)
{
	FBlueprintHelperReviewPanelSurfacePresenterArgs Args;
	Args.AssetContext = &ReviewAssetContext;
	Args.ChangeItems = &ChangeItems;
	Args.SelectedChange = SelectedChange;
	Args.AddDebugMessage = [this](const FString& Message)
	{
		AddDebugMessage(Message);
	};
	Args.OnAcceptChangeId = [this](const FString& ChangeId)
	{
		return OnAcceptChangeId(ChangeId);
	};
	Args.OnRejectChangeId = [this](const FString& ChangeId)
	{
		return OnRejectChangeId(ChangeId);
	};
	Args.GetChangeColor = [this](EBlueprintHelperReviewChangeKind Kind)
	{
		return GetChangeColor(Kind);
	};
	Args.GetSelectedDiffColor = [this]()
	{
		return GetSelectedDiffColor();
	};
	Args.ResolveRowGeometry.BindLambda([this](
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface RoutedSurface,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
	{
		return ResolveReviewRowGeometry(Change, RoutedSurface, OutAnchor);
	});
	Args.OnGeometryInvalidated = FBlueprintHelperReviewGeometryInvalidated::CreateSP(
		this,
		&SBlueprintHelperReviewPanel::OnSurfaceGeometryInvalidated);

	if (Surface == EBlueprintHelperReviewSurface::Components
		&& Predicate == &FBlueprintHelperReviewBlueprintComponentsPresenter::ShouldShowChange)
	{
		return FBlueprintHelperReviewBlueprintComponentsPresenter::BuildOverlay(Args);
	}
	if (Surface == EBlueprintHelperReviewSurface::MyBlueprint
		&& Predicate == &FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange)
	{
		return FBlueprintHelperReviewMyBlueprintPresenter::BuildOverlay(Args);
	}
	if (Surface == EBlueprintHelperReviewSurface::Details
		&& Predicate == &FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange)
	{
		return FBlueprintHelperReviewObjectDetailsPresenter::BuildOverlay(Args);
	}
	if (Surface == EBlueprintHelperReviewSurface::UMGWidgetTree
		&& Predicate == &FBlueprintHelperReviewUMGWidgetTreePresenter::ShouldShowChange)
	{
		return FBlueprintHelperReviewUMGWidgetTreePresenter::BuildOverlay(Args);
	}
	if (Surface == EBlueprintHelperReviewSurface::DataTable
		&& Predicate == &FBlueprintHelperReviewDataTablePresenter::ShouldShowChange)
	{
		return FBlueprintHelperReviewDataTablePresenter::BuildOverlay(Args);
	}
	if (Surface == EBlueprintHelperReviewSurface::DataAsset
		&& Predicate == &FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange)
	{
		return FBlueprintHelperReviewDataAssetPresenter::BuildOverlay(Args);
	}
	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildDetailsPanelDiffFrames()
{
	return BuildPanelDiffFrames(
		&FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange,
		EBlueprintHelperReviewSurface::Details);
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildDiffRow(FReviewChangeItem Item, bool bShowActions)
{
	const FString Label = Item.IsValid()
		? FBlueprintHelperReviewSurfaceFrameBuilder::BuildReadableChangeTitle(*Item)
		: TEXT("Invalid Change");
	const FString SubLabel = Item.IsValid()
		? FString::Printf(TEXT("%s  %s"),
			*FString(BlueprintHelperReviewChangeKindToString(Item->ChangeKind)),
			*Item->LatestTransactionId)
		: TEXT("");

	TSharedRef<SWidget> Content = SNew(SButton)
		.ContentPadding(6.0f)
		.OnClicked_Lambda([this, Item]()
		{
			OnChangeSelectionChanged(Item, ESelectInfo::OnMouseClick);
			return FReply::Handled();
		})
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 3.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.62f, 0.62f, 1.0f)))
				.Text(FText::FromString(SubLabel))
			]
		];

	return BuildDiffFrame(Item, Content, bShowActions);
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildDiffFrame(
	FReviewChangeItem Item,
	const TSharedRef<SWidget>& Content,
	bool bShowActions)
{
	return FBlueprintHelperReviewSurfaceFrameBuilder::BuildDiffFrame(
		Item,
		Content,
		bShowActions,
		true,
		Item.IsValid() ? GetChangeColor(Item->ChangeKind) : FSlateColor(FLinearColor::Transparent),
		[this](const FString& ChangeId)
		{
			return OnAcceptChangeId(ChangeId);
		},
		[this](const FString& ChangeId)
		{
			return OnRejectChangeId(ChangeId);
		},
		Item == SelectedChange);
}

FReply SBlueprintHelperReviewPanel::OnAcceptSelected()
{
	return ExecuteAcceptChange(SelectedChange);
}

FReply SBlueprintHelperReviewPanel::OnRejectSelected()
{
	return ExecuteRejectChange(SelectedChange);
}

FReply SBlueprintHelperReviewPanel::ExecuteAcceptChange(FReviewChangeItem Item)
{
	if (!Item.IsValid())
	{
		return FReply::Handled();
	}

	SelectedChange = Item;

	const FBlueprintHelperReviewPanelPresenterEvent PresenterEvent =
		ReviewPanelPresenter.IsValid()
			? ReviewPanelPresenter->HandleVisualEvent(
				FBlueprintHelperReviewPanelVisualEvent::AcceptVisibleChange(*Item))
			: FBlueprintHelperReviewPanelPresenterEvent::FromActionResult(
				FBlueprintHelperReviewActionResult());
	const FBlueprintHelperReviewActionResult& Result = PresenterEvent.ActionResult;

	if (Result.bSucceeded)
	{
		const FString PreferredAssetPath = Item->AssetPath;
		const int32 RemovedIndex = ChangeItems.IndexOfByKey(Item);
		ChangeItems.Remove(Item);
		SelectNextChangeAfterRemoval(PreferredAssetPath, RemovedIndex);
		FBlueprintHelperReviewRowHighlightModel::InvalidateAssetStates(PreferredAssetPath);
		RebuildChangeTreeItems();
		RefreshChangeTreeWidget();
		LoadReviewAssetFromSelection();
		RefreshMainWorkspaceAfterReviewStateChanged();
		LastVisibleChangeRefreshSignature.Reset();
		RefreshFromReviewStoreIfChanged();
	}

	AddDebugMessage(FString::Printf(
		TEXT("Accept change id=%s success=%d message=\"%s\""),
		*Item->ChangeId,
		Result.bSucceeded ? 1 : 0,
		*Result.Message));
	ShowReviewActionNotification(
		TEXT("accept:") + Item->ChangeId,
		FString::Printf(
			TEXT("%s：%s"),
			Result.bSucceeded ? TEXT("成功，已接受") : TEXT("失败，未接受"),
			*BuildReviewActionNotificationLabel(Item)),
		Result.bSucceeded
			? EReviewActionNotificationState::Success
			: EReviewActionNotificationState::Fail,
		true,
		false);
	return FReply::Handled();
}

FReply SBlueprintHelperReviewPanel::ExecuteRejectChange(FReviewChangeItem Item)
{
	if (!Item.IsValid())
	{
		return FReply::Handled();
	}

	QueueRejectChange(Item);
	return FReply::Handled();
}

void SBlueprintHelperReviewPanel::QueueRejectChange(FReviewChangeItem Item, bool bShowIndividualNotification)
{
	if (!Item.IsValid())
	{
		return;
	}

	const FString ChangeId = !Item->ChangeId.IsEmpty() ? Item->ChangeId : Item->LatestTransactionId;
	if (ChangeId.IsEmpty())
	{
		AddDebugMessage(TEXT("Reject queue failed reason=missing_change_id"));
		if (bShowIndividualNotification)
		{
			ShowReviewActionNotification(
				TEXT("reject:missing_change_id"),
				TEXT("失败，无法拒绝：缺少 Review id"),
				EReviewActionNotificationState::Fail,
				true,
				false);
		}
		return;
	}
	if (RejectActionInProgressChangeIds.Contains(ChangeId))
	{
		AddDebugMessage(FString::Printf(TEXT("Reject queue skipped id=%s reason=already_pending"), *ChangeId));
		if (bShowIndividualNotification)
		{
			ShowReviewActionNotification(
				TEXT("reject:") + ChangeId,
				FString::Printf(
					TEXT("处理中，已在拒绝队列：%s"),
					*BuildReviewActionNotificationLabel(Item)),
				EReviewActionNotificationState::Pending,
				false,
				true);
		}
		return;
	}

	RejectActionInProgressChangeIds.Add(ChangeId);
	PendingRejectChangeIds.Add(ChangeId);
	SelectedChange = Item;
	Item->Status = EBlueprintHelperReviewChangeStatus::NeedsAction;
	Item->NeedsActionReason = TEXT("reject_queued");
	RebuildChangeTreeItems();
	RefreshChangeTreeWidget();
	RefreshDiffStackWidgets();
	UpdateDetailsSelection();
	AddDebugMessage(FString::Printf(TEXT("Reject queued id=%s"), *ChangeId));
	if (bShowIndividualNotification)
	{
		ShowReviewActionNotification(
			TEXT("reject:") + ChangeId,
			FString::Printf(
				TEXT("处理中，正在拒绝：%s"),
				*BuildReviewActionNotificationLabel(Item)),
			EReviewActionNotificationState::Pending,
			false,
			true);
	}
	RegisterActiveTimer(
		0.03f,
		FWidgetActiveTimerDelegate::CreateSP(this, &SBlueprintHelperReviewPanel::TickAsyncRejectPrepare));
}

EActiveTimerReturnType SBlueprintHelperReviewPanel::TickAsyncRejectPrepare(double InCurrentTime, float InDeltaTime)
{
	if (bAsyncRejectPrepareActive || bAsyncRejectMutationScheduled)
	{
		return EActiveTimerReturnType::Stop;
	}

	while (PendingRejectChangeIds.Num() > 0)
	{
		const FString ChangeId = PendingRejectChangeIds[0];
		PendingRejectChangeIds.RemoveAt(0);
		FReviewChangeItem Item = FindChangeItemById(ChangeId);
		if (!Item.IsValid())
		{
			if (!RejectBatchKeyByChangeId.Contains(ChangeId))
			{
				ShowReviewActionNotification(
					TEXT("reject:") + ChangeId,
					FString::Printf(TEXT("失败，无法拒绝：找不到 Review 项 (%s)"), *ChangeId),
					EReviewActionNotificationState::Fail,
					true,
					false);
			}
			RecordRejectBatchResult(ChangeId, false);
			FinishAsyncReject(ChangeId);
			continue;
		}

		ActiveRejectChangeId = ChangeId;
		bAsyncRejectPrepareActive = true;
		Item->Status = EBlueprintHelperReviewChangeStatus::NeedsAction;
		Item->NeedsActionReason = TEXT("reject_preparing_rollback_journal");
		RebuildChangeTreeItems();
		RefreshChangeTreeWidget();
		RefreshDiffStackWidgets();

		const FBlueprintHelperReviewVisibleChange ChangeSnapshot = *Item;
		TWeakPtr<SBlueprintHelperReviewPanel> WeakPanel =
			StaticCastSharedRef<SBlueprintHelperReviewPanel>(AsShared());
		TFuture<void> PrepareTask = Async(EAsyncExecution::ThreadPool, [WeakPanel, ChangeId, ChangeSnapshot]()
		{
			FBlueprintHelperReviewRejectOptions PreparedOptions =
				FBlueprintHelperReviewPanelLocalUtils::PrepareRejectOptions(ChangeSnapshot);
			if (FBlueprintHelperReviewPanelAsyncUtils::IsShutdownRequested())
			{
				return;
			}
			AsyncTask(ENamedThreads::GameThread, [WeakPanel, ChangeId, PreparedOptions]()
			{
				if (TSharedPtr<SBlueprintHelperReviewPanel> Panel = WeakPanel.Pin())
				{
					Panel->HandlePreparedRejectReady(ChangeId, PreparedOptions);
				}
			});
		});
		FBlueprintHelperReviewPanelAsyncUtils::TrackTask(MoveTemp(PrepareTask));
		return EActiveTimerReturnType::Stop;
	}

	return EActiveTimerReturnType::Stop;
}

void SBlueprintHelperReviewPanel::HandlePreparedRejectReady(
	const FString& ChangeId,
	const FBlueprintHelperReviewRejectOptions& PreparedOptions)
{
	bAsyncRejectPrepareActive = false;
	PreparedRejectOptionsByChangeId.Add(ChangeId, PreparedOptions);
	if (FReviewChangeItem Item = FindChangeItemById(ChangeId))
	{
		Item->Status = EBlueprintHelperReviewChangeStatus::NeedsAction;
		Item->NeedsActionReason = TEXT("reject_mutation_scheduled");
		RebuildChangeTreeItems();
		RefreshChangeTreeWidget();
		RefreshDiffStackWidgets();
		if (!RejectBatchKeyByChangeId.Contains(ChangeId))
		{
			ShowReviewActionNotification(
				TEXT("reject:") + ChangeId,
				FString::Printf(
					TEXT("处理中，正在应用拒绝：%s"),
					*BuildReviewActionNotificationLabel(Item)),
				EReviewActionNotificationState::Pending,
				false,
				true);
		}
	}
	AddDebugMessage(FString::Printf(
		TEXT("Reject rollback journal prepared id=%s journals=%d"),
		*ChangeId,
		PreparedOptions.PreparedRollbackJournalsByTransactionId.Num()));
	bAsyncRejectMutationScheduled = true;
	RegisterActiveTimer(
		0.03f,
		FWidgetActiveTimerDelegate::CreateSP(this, &SBlueprintHelperReviewPanel::TickAsyncRejectMutation));
}

EActiveTimerReturnType SBlueprintHelperReviewPanel::TickAsyncRejectMutation(double InCurrentTime, float InDeltaTime)
{
	bAsyncRejectMutationScheduled = false;
	ExecutePreparedRejectMutation(ActiveRejectChangeId);
	return EActiveTimerReturnType::Stop;
}

void SBlueprintHelperReviewPanel::ExecutePreparedRejectMutation(const FString& ChangeId)
{
	FReviewChangeItem Item = FindChangeItemById(ChangeId);
	if (!Item.IsValid())
	{
		if (!RejectBatchKeyByChangeId.Contains(ChangeId))
		{
			ShowReviewActionNotification(
				TEXT("reject:") + ChangeId,
				FString::Printf(TEXT("失败，无法拒绝：找不到 Review 项 (%s)"), *ChangeId),
				EReviewActionNotificationState::Fail,
				true,
				false);
		}
		RecordRejectBatchResult(ChangeId, false);
		FinishAsyncReject(ChangeId);
		return;
	}

	FBlueprintHelperReviewRejectOptions Options;
	if (const FBlueprintHelperReviewRejectOptions* PreparedOptions =
		PreparedRejectOptionsByChangeId.Find(ChangeId))
	{
		Options = *PreparedOptions;
	}

	SelectedChange = Item;
	Item->Status = EBlueprintHelperReviewChangeStatus::NeedsAction;
	Item->NeedsActionReason = TEXT("reject_mutating_single_frame");
	RebuildChangeTreeItems();
	RefreshChangeTreeWidget();
	RefreshDiffStackWidgets();
	AddDebugMessage(FString::Printf(
		TEXT("Reject mutation started id=%s mode=single_frame_transaction preparedJournals=%d"),
		*ChangeId,
		Options.PreparedRollbackJournalsByTransactionId.Num()));

	if (Item->bIsAssetLifecycleRoot)
	{
		const FBlueprintHelperReviewPanelPresenterEvent PresenterEvent =
			ReviewPanelPresenter.IsValid()
				? ReviewPanelPresenter->HandleVisualEvent(
					FBlueprintHelperReviewPanelVisualEvent::RejectLifecycleRootVisibleChange(
						*Item,
						FBlueprintHelperReviewPanelDataSnapshot::FromSelection(
							BuildPendingChangeSnapshot(),
							*Item),
						Options))
				: FBlueprintHelperReviewPanelPresenterEvent::FromCascadeActionResult(
					FBlueprintHelperReviewCascadeActionResult());
		const FBlueprintHelperReviewCascadeActionResult& CascadeResult =
			PresenterEvent.CascadeActionResult;

		if (CascadeResult.RootResult.bSucceeded)
		{
			const FString PreferredAssetPath = Item->AssetPath;
			const int32 RemovedIndex = ChangeItems.IndexOfByKey(Item);
			TSet<FString> RemovedChangeIds;
			if (!Item->ChangeId.IsEmpty())
			{
				RemovedChangeIds.Add(Item->ChangeId);
			}
			for (const FString& RemovedChildChangeId : CascadeResult.RemovedChildChangeIds)
			{
				RemovedChangeIds.Add(RemovedChildChangeId);
			}

			ChangeItems.RemoveAll([&RemovedChangeIds, &Item](const FReviewChangeItem& Candidate)
			{
				return Candidate == Item
					|| (Candidate.IsValid()
						&& !Candidate->ChangeId.IsEmpty()
						&& RemovedChangeIds.Contains(Candidate->ChangeId));
			});
			SelectNextChangeAfterRemoval(PreferredAssetPath, RemovedIndex);
			FBlueprintHelperReviewRowHighlightModel::InvalidateAssetStates(PreferredAssetPath);
		}
		else
		{
			Item->Status = CascadeResult.RootResult.NewStatus;
			Item->NeedsActionReason = CascadeResult.RootResult.Message;
		}

		RebuildChangeTreeItems();
		RefreshChangeTreeWidget();
		LoadReviewAssetFromSelection();
		RefreshMainWorkspaceAfterReviewStateChanged();
		LastVisibleChangeRefreshSignature.Reset();
		RefreshFromReviewStoreIfChanged();
		AddDebugMessage(FString::Printf(
			TEXT("Reject lifecycle root id=%s success=%d removedChildren=%d status=%s message=\"%s\""),
			*Item->ChangeId,
			CascadeResult.RootResult.bSucceeded ? 1 : 0,
			CascadeResult.RemovedChildChangeIds.Num(),
			BlueprintHelperReviewChangeStatusToString(CascadeResult.RootResult.NewStatus),
			*CascadeResult.RootResult.Message));
		if (!RejectBatchKeyByChangeId.Contains(ChangeId))
		{
			ShowReviewActionNotification(
				TEXT("reject:") + ChangeId,
				FString::Printf(
					TEXT("%s：%s"),
					CascadeResult.RootResult.bSucceeded ? TEXT("成功，已拒绝") : TEXT("失败，未拒绝"),
					*BuildReviewActionNotificationLabel(Item)),
				CascadeResult.RootResult.bSucceeded
					? EReviewActionNotificationState::Success
					: EReviewActionNotificationState::Fail,
				true,
				false);
		}
		RecordRejectBatchResult(ChangeId, CascadeResult.RootResult.bSucceeded);
		FinishAsyncReject(ChangeId);
		return;
	}

	const FBlueprintHelperReviewPanelPresenterEvent PresenterEvent =
		ReviewPanelPresenter.IsValid()
			? ReviewPanelPresenter->HandleVisualEvent(
				FBlueprintHelperReviewPanelVisualEvent::RejectVisibleChange(*Item, Options))
			: FBlueprintHelperReviewPanelPresenterEvent::FromActionResult(
				FBlueprintHelperReviewActionResult());
	const FBlueprintHelperReviewActionResult& Result = PresenterEvent.ActionResult;

	if (Result.bSucceeded)
	{
		const FString PreferredAssetPath = Item->AssetPath;
		const int32 RemovedIndex = ChangeItems.IndexOfByKey(Item);
		ChangeItems.RemoveAll([&Item](const FReviewChangeItem& Candidate)
		{
			return Candidate == Item;
		});
		SelectNextChangeAfterRemoval(PreferredAssetPath, RemovedIndex);
		FBlueprintHelperReviewRowHighlightModel::InvalidateAssetStates(PreferredAssetPath);
	}
	else
	{
		Item->Status = Result.NewStatus;
		Item->NeedsActionReason = Result.Message;
	}
	RebuildChangeTreeItems();
	RefreshChangeTreeWidget();
	LoadReviewAssetFromSelection();
	RefreshMainWorkspaceAfterReviewStateChanged();
	LastVisibleChangeRefreshSignature.Reset();
	RefreshFromReviewStoreIfChanged();

	AddDebugMessage(FString::Printf(
		TEXT("Reject change id=%s success=%d status=%s message=\"%s\""),
		*Item->ChangeId,
		Result.bSucceeded ? 1 : 0,
		BlueprintHelperReviewChangeStatusToString(Result.NewStatus),
		*Result.Message));
	if (!Result.bSucceeded && !Result.HashGuardTargetKey.IsEmpty())
	{
		AddDebugMessage(FString::Printf(
			TEXT("Reject hash guard target=%s expected=%s current=%s"),
			*Result.HashGuardTargetKey,
			*Result.HashGuardExpectedHash,
			*Result.HashGuardCurrentHash));
		AppendDebugBundleEvent(FBlueprintHelperReviewDebugBundleService::BuildActionHashGuardEvent(
			DebugBundleSessionId,
			SelectedChange,
			SelectedChange.IsValid() ? SelectedChange->AssetPath : FString(),
			Result.HashGuardTargetKey,
			Result.HashGuardExpectedHash,
			Result.HashGuardCurrentHash,
			Result.HashGuardCurrentSnapshotJson,
			Result.HashGuardRecordedAfterSnapshotJson));
	}
	if (!RejectBatchKeyByChangeId.Contains(ChangeId))
	{
		ShowReviewActionNotification(
			TEXT("reject:") + ChangeId,
			FString::Printf(
				TEXT("%s：%s"),
				Result.bSucceeded ? TEXT("成功，已拒绝") : TEXT("失败，未拒绝"),
				*BuildReviewActionNotificationLabel(Item)),
			Result.bSucceeded
				? EReviewActionNotificationState::Success
				: EReviewActionNotificationState::Fail,
			true,
			false);
	}
	RecordRejectBatchResult(ChangeId, Result.bSucceeded);
	FinishAsyncReject(ChangeId);
}

void SBlueprintHelperReviewPanel::FinishAsyncReject(const FString& ChangeId)
{
	RejectActionInProgressChangeIds.Remove(ChangeId);
	PreparedRejectOptionsByChangeId.Remove(ChangeId);
	if (ActiveRejectChangeId == ChangeId)
	{
		ActiveRejectChangeId.Reset();
	}
	bAsyncRejectPrepareActive = false;
	bAsyncRejectMutationScheduled = false;
	if (PendingRejectChangeIds.Num() > 0)
	{
		RegisterActiveTimer(
			0.03f,
			FWidgetActiveTimerDelegate::CreateSP(this, &SBlueprintHelperReviewPanel::TickAsyncRejectPrepare));
	}
}


FReply SBlueprintHelperReviewPanel::OnAcceptAll()
{
	FString AssetPath;
	if (SelectedChange.IsValid())
	{
		AssetPath = SelectedChange->AssetPath;
	}

	TArray<FReviewChangeItem> AcceptedItems;
	int32 TargetCount = 0;
	int32 FailedCount = 0;
	for (const FReviewChangeItem& Item : ChangeItems)
	{
		if (!Item.IsValid() || (!AssetPath.IsEmpty() && Item->AssetPath != AssetPath))
		{
			continue;
		}
		++TargetCount;

		const FBlueprintHelperReviewPanelPresenterEvent PresenterEvent =
			ReviewPanelPresenter.IsValid()
				? ReviewPanelPresenter->HandleVisualEvent(
					FBlueprintHelperReviewPanelVisualEvent::AcceptVisibleChange(*Item))
				: FBlueprintHelperReviewPanelPresenterEvent::FromActionResult(
					FBlueprintHelperReviewActionResult());
		const FBlueprintHelperReviewActionResult& Result = PresenterEvent.ActionResult;
		if (Result.bSucceeded)
		{
			AcceptedItems.Add(Item);
		}
		else
		{
			++FailedCount;
			Item->Status = Result.NewStatus;
			Item->NeedsActionReason = Result.Message;
		}
	}

	ChangeItems.RemoveAll([&AcceptedItems](const FReviewChangeItem& Item)
	{
		return Item.IsValid() && AcceptedItems.Contains(Item);
	});
	FBlueprintHelperReviewRowHighlightModel::InvalidateAssetStates(AssetPath);

	SelectedChange = ChangeItems.Num() > 0 ? ChangeItems[0] : FReviewChangeItem();
		RebuildChangeTreeItems();
		RefreshChangeTreeWidget();
		LoadReviewAssetFromSelection();
		RefreshMainWorkspaceAfterReviewStateChanged();
		LastVisibleChangeRefreshSignature.Reset();
		RefreshFromReviewStoreIfChanged();
	AddDebugMessage(FString::Printf(
		TEXT("AcceptAll asset=\"%s\" remainingVisibleChanges=%d"),
		*AssetPath,
		ChangeItems.Num()));
	ShowReviewActionNotification(
		TEXT("accept_all:") + AssetPath,
		TargetCount == 0
			? FString(TEXT("全失败：没有可接受的 Review 项"))
			: FailedCount == 0
				? FString::Printf(TEXT("批量成功：已接受 %d 项"), AcceptedItems.Num())
				: AcceptedItems.Num() > 0
					? FString::Printf(TEXT("有失败：已接受 %d 项，失败 %d 项"), AcceptedItems.Num(), FailedCount)
					: FString::Printf(TEXT("全失败：接受失败 %d 项"), FailedCount),
		TargetCount > 0 && FailedCount == 0
			? EReviewActionNotificationState::Success
			: EReviewActionNotificationState::Fail,
		true,
		false);
	return FReply::Handled();
}


FReply SBlueprintHelperReviewPanel::OnRejectAll()
{
	FString AssetPath;
	if (SelectedChange.IsValid())
	{
		AssetPath = SelectedChange->AssetPath;
	}

	TArray<FReviewChangeItem> ItemsToQueue;
	for (const FReviewChangeItem& Item : ChangeItems)
	{
		if (Item.IsValid() && (AssetPath.IsEmpty() || Item->AssetPath == AssetPath))
		{
			ItemsToQueue.Add(Item);
		}
	}
	const FString BatchKey = FString::Printf(
		TEXT("reject_all:%s:%lld"),
		*AssetPath,
		FDateTime::UtcNow().GetTicks());
	if (ItemsToQueue.Num() == 0)
	{
		ShowReviewActionNotification(
			BatchKey,
			TEXT("全失败：没有可拒绝的 Review 项"),
			EReviewActionNotificationState::Fail,
			true,
			false);
		AddDebugMessage(FString::Printf(
			TEXT("RejectAll queued asset=\"%s\" count=0"),
			*AssetPath));
		return FReply::Handled();
	}

	FReviewActionBatchNotificationState& Batch = RejectBatchNotifications.Add(BatchKey);
	Batch.NotificationKey = BatchKey;
	Batch.TotalCount = ItemsToQueue.Num();
	for (const FReviewChangeItem& Item : ItemsToQueue)
	{
		const FString ChangeId = Item.IsValid()
			? (!Item->ChangeId.IsEmpty() ? Item->ChangeId : Item->LatestTransactionId)
			: FString();
		if (ChangeId.IsEmpty())
		{
			++Batch.FinishedCount;
			++Batch.FailedCount;
			continue;
		}
		RejectBatchKeyByChangeId.Add(ChangeId, BatchKey);
		QueueRejectChange(Item, false);
	}
	AddDebugMessage(FString::Printf(
		TEXT("RejectAll queued asset=\"%s\" count=%d"),
		*AssetPath,
		ItemsToQueue.Num()));
	if (Batch.FinishedCount >= Batch.TotalCount)
	{
		ShowReviewActionNotification(
			BatchKey,
			FString::Printf(TEXT("全失败：拒绝失败 %d 项"), Batch.FailedCount),
			EReviewActionNotificationState::Fail,
			true,
			false);
		RejectBatchNotifications.Remove(BatchKey);
	}
	else
	{
		ShowReviewActionNotification(
			BatchKey,
			FString::Printf(TEXT("处理中：批量拒绝已排队 %d 项"), ItemsToQueue.Num()),
			EReviewActionNotificationState::Pending,
			false,
			true);
	}
	return FReply::Handled();
}

void SBlueprintHelperReviewPanel::ShowReviewActionNotification(
	const FString& NotificationKey,
	const FString& StatusText,
	EReviewActionNotificationState State,
	bool bExpire,
	bool bUseThrobber)
{
	const FString EffectiveKey = NotificationKey.IsEmpty() ? TEXT("review_action") : NotificationKey;
	TSharedPtr<SNotificationItem> Notification;
	if (TWeakPtr<SNotificationItem>* ExistingNotification = ReviewActionNotifications.Find(EffectiveKey))
	{
		Notification = ExistingNotification->Pin();
	}

	if (!Notification.IsValid())
	{
		FNotificationInfo Info(FText::FromString(StatusText));
		Info.bFireAndForget = false;
		Info.bUseThrobber = bUseThrobber;
		Info.bUseSuccessFailIcons = true;
		Info.FadeOutDuration = 0.5f;
		Info.ExpireDuration = 3.0f;
		Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if (Notification.IsValid())
		{
			ReviewActionNotifications.Add(EffectiveKey, Notification);
		}
	}

	if (!Notification.IsValid())
	{
		return;
	}

	Notification->SetText(FText::FromString(StatusText));
	switch (State)
	{
	case EReviewActionNotificationState::Pending:
		Notification->SetCompletionState(SNotificationItem::CS_Pending);
		break;
	case EReviewActionNotificationState::Success:
		Notification->SetCompletionState(SNotificationItem::CS_Success);
		break;
	case EReviewActionNotificationState::Fail:
		Notification->SetCompletionState(SNotificationItem::CS_Fail);
		break;
	default:
		Notification->SetCompletionState(SNotificationItem::CS_None);
		break;
	}

	if (bExpire)
	{
		Notification->ExpireAndFadeout();
		ReviewActionNotifications.Remove(EffectiveKey);
	}
}

FString SBlueprintHelperReviewPanel::BuildReviewActionNotificationLabel(FReviewChangeItem Item)
{
	if (!Item.IsValid())
	{
		return TEXT("unknown change");
	}
	if (!Item->DisplayLabel.IsEmpty())
	{
		return Item->DisplayLabel;
	}
	if (!Item->ChangeId.IsEmpty())
	{
		return Item->ChangeId;
	}
	return Item->LatestTransactionId.IsEmpty() ? TEXT("unknown change") : Item->LatestTransactionId;
}

void SBlueprintHelperReviewPanel::RecordRejectBatchResult(const FString& ChangeId, bool bSucceeded)
{
	FString BatchKey;
	if (!RejectBatchKeyByChangeId.RemoveAndCopyValue(ChangeId, BatchKey))
	{
		return;
	}

	FReviewActionBatchNotificationState* Batch = RejectBatchNotifications.Find(BatchKey);
	if (!Batch)
	{
		return;
	}

	++Batch->FinishedCount;
	if (bSucceeded)
	{
		++Batch->SuccessCount;
	}
	else
	{
		++Batch->FailedCount;
	}

	if (Batch->FinishedCount < Batch->TotalCount)
	{
		ShowReviewActionNotification(
			Batch->NotificationKey,
			FString::Printf(
				TEXT("处理中：批量拒绝 %d/%d 项"),
				Batch->FinishedCount,
				Batch->TotalCount),
			EReviewActionNotificationState::Pending,
			false,
			true);
		return;
	}

	const FString FinalText = Batch->FailedCount == 0
		? FString::Printf(TEXT("批量成功：已拒绝 %d 项"), Batch->SuccessCount)
		: Batch->SuccessCount > 0
			? FString::Printf(TEXT("有失败：已拒绝 %d 项，失败 %d 项"), Batch->SuccessCount, Batch->FailedCount)
			: FString::Printf(TEXT("全失败：拒绝失败 %d 项"), Batch->FailedCount);
	ShowReviewActionNotification(
		Batch->NotificationKey,
		FinalText,
		Batch->FailedCount == 0
			? EReviewActionNotificationState::Success
			: EReviewActionNotificationState::Fail,
		true,
		false);
	RejectBatchNotifications.Remove(BatchKey);
}

FText SBlueprintHelperReviewPanel::GetSelectedTitle() const
{
	if (!SelectedChange.IsValid())
	{
		return FText::FromString(TEXT("No visible change selected."));
	}
	return FText::FromString(SelectedChange->DisplayLabel);
}

FText SBlueprintHelperReviewPanel::GetSelectedBefore() const
{
	return SelectedChange.IsValid()
		? FText::FromString(FString::Printf(TEXT("Before: %s"), *SelectedChange->BeforeSummary))
		: FText::GetEmpty();
}

FText SBlueprintHelperReviewPanel::GetSelectedAfter() const
{
	return SelectedChange.IsValid()
		? FText::FromString(FString::Printf(TEXT("After: %s"), *SelectedChange->AfterSummary))
		: FText::GetEmpty();
}

FText SBlueprintHelperReviewPanel::GetSelectedStatus() const
{
	return SelectedChange.IsValid()
		? FText::Format(FText::FromString(TEXT("Status: {0}")), FBlueprintHelperReviewPanelStyle::StatusToText(SelectedChange->Status))
		: FText::GetEmpty();
}

FText SBlueprintHelperReviewPanel::GetSelectedTransactionChain() const
{
	if (!SelectedChange.IsValid())
	{
		return FText::GetEmpty();
	}

	return FText::FromString(FString::Printf(
		TEXT("Latest: %s\nSources: %s\nNeeds action: %s"),
		*SelectedChange->LatestTransactionId,
		*FString::Join(SelectedChange->SourceTransactionIds, TEXT(", ")),
		*SelectedChange->NeedsActionReason));
}

FSlateColor SBlueprintHelperReviewPanel::GetSelectedDiffColor() const
{
	if (!SelectedChange.IsValid())
	{
		return FSlateColor(FLinearColor::Transparent);
	}

	FLinearColor Color = GetChangeColor(SelectedChange->ChangeKind).GetSpecifiedColor();
	if (FlashAlpha > 0.0f)
	{
		Color.A = FMath::Clamp(0.35f + FlashAlpha * 0.60f, 0.35f, 0.95f);
	}
	return FSlateColor(Color);
}

FSlateColor SBlueprintHelperReviewPanel::GetChangeColor(EBlueprintHelperReviewChangeKind Kind) const
{
	const FString ColorName = BlueprintHelperReviewChangeKindToColorName(Kind);
	if (ColorName == TEXT("green"))
	{
		return FSlateColor(FBlueprintHelperReviewPanelStyle::GetReviewGreen());
	}
	if (ColorName == TEXT("red"))
	{
		return FSlateColor(FBlueprintHelperReviewPanelStyle::GetReviewRed());
	}
	if (ColorName == TEXT("yellow"))
	{
		return FSlateColor(FBlueprintHelperReviewPanelStyle::GetReviewYellow());
	}
	return FSlateColor(FLinearColor(0.18f, 0.18f, 0.18f, 0.85f));
}

EActiveTimerReturnType SBlueprintHelperReviewPanel::TickFlash(double InCurrentTime, float InDeltaTime)
{
	FlashAlpha = FMath::Max(0.0f, FlashAlpha - InDeltaTime * 1.8f);
	Invalidate(EInvalidateWidgetReason::Paint);
	return FlashAlpha > 0.0f ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
}

void SBlueprintHelperReviewPanel::StartFlash()
{
	FlashAlpha = 1.0f;
	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SBlueprintHelperReviewPanel::TickFlash));
}

void SBlueprintHelperReviewPanel::RefreshFromReviewStoreIfChanged()
{
	if (!ReviewPanelPresenter.IsValid())
	{
		return;
	}

	const TArray<FBlueprintHelperReviewVisibleChange> LatestChanges =
		ReviewPanelPresenter->LoadPendingVisibleChanges();
	const FString LatestSignature = BuildVisibleChangeRefreshSignature(LatestChanges);
	if (LatestSignature == LastVisibleChangeRefreshSignature)
	{
		return;
	}

	const FString PreviousSelectedChangeId = SelectedChange.IsValid() ? SelectedChange->ChangeId : FString();
	RefreshVisibleChanges(LatestChanges);
	LastVisibleChangeRefreshSignature = LatestSignature;
	SelectedChange = FindChangeItemById(PreviousSelectedChangeId);
	if (!SelectedChange.IsValid() && ChangeItems.Num() > 0)
	{
		SelectedChange = ChangeItems[0];
	}

	RebuildChangeTreeItems();
	RefreshChangeTreeWidget();
	LoadReviewAssetFromSelection();
	RefreshMainWorkspaceAfterReviewStateChanged();
	AddDebugMessage(TEXT("Review store refreshed dynamically."));
}

void SBlueprintHelperReviewPanel::RefreshDiffStackWidgets()
{
	if (ComponentsDiffStackBox.IsValid())
	{
		ComponentsDiffStackBox->SetContent(BuildStructurePanelDiffFrames());
	}
	if (MainWorkspaceDiffStackBox.IsValid())
	{
		MainWorkspaceDiffStackBox->SetContent(BuildMainWorkspaceDiffFrames());
	}
	if (MyBlueprintDiffStackBox.IsValid())
	{
		MyBlueprintDiffStackBox->SetContent(BuildPanelDiffFrames(
			&FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange,
			EBlueprintHelperReviewSurface::MyBlueprint));
	}
	if (DetailsDiffStackBox.IsValid())
	{
		DetailsDiffStackBox->SetContent(BuildDetailsPanelDiffFrames());
	}
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

void SBlueprintHelperReviewPanel::RefreshMainWorkspaceAfterReviewStateChanged()
{
	RefreshDiffStackWidgets();
	if (GraphEditorBox.IsValid())
	{
		GraphEditorBox->SetContent(BuildMainWorkspaceWidget());
	}
	RefreshDiffStackWidgets();
	UpdateDetailsSelection();
}

void SBlueprintHelperReviewPanel::RefreshSurfaceOverlay(EBlueprintHelperReviewSurface Surface)
{
	using FSurfaceRefreshHandler = TFunction<void()>;
	const TArray<TPair<EBlueprintHelperReviewSurface, FSurfaceRefreshHandler>> RefreshHandlers =
	{
		TPair<EBlueprintHelperReviewSurface, FSurfaceRefreshHandler>(
			EBlueprintHelperReviewSurface::Components,
			[this]()
			{
				if (ComponentsDiffStackBox.IsValid())
				{
					ComponentsDiffStackBox->SetContent(BuildStructurePanelDiffFrames());
				}
			}),
		TPair<EBlueprintHelperReviewSurface, FSurfaceRefreshHandler>(
			EBlueprintHelperReviewSurface::UMGWidgetTree,
			[this]()
			{
				if (ComponentsDiffStackBox.IsValid())
				{
					ComponentsDiffStackBox->SetContent(BuildStructurePanelDiffFrames());
				}
			}),
		TPair<EBlueprintHelperReviewSurface, FSurfaceRefreshHandler>(
			EBlueprintHelperReviewSurface::MyBlueprint,
			[this]()
			{
				if (MyBlueprintDiffStackBox.IsValid())
				{
					MyBlueprintDiffStackBox->SetContent(BuildPanelDiffFrames(
						&FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange,
						EBlueprintHelperReviewSurface::MyBlueprint));
				}
			}),
		TPair<EBlueprintHelperReviewSurface, FSurfaceRefreshHandler>(
			EBlueprintHelperReviewSurface::Details,
			[this]()
			{
				if (DetailsDiffStackBox.IsValid())
				{
					DetailsDiffStackBox->SetContent(BuildDetailsPanelDiffFrames());
				}
			}),
		TPair<EBlueprintHelperReviewSurface, FSurfaceRefreshHandler>(
			EBlueprintHelperReviewSurface::DataTable,
			[this]()
			{
				if (MainWorkspaceDiffStackBox.IsValid())
				{
					MainWorkspaceDiffStackBox->SetContent(BuildMainWorkspaceDiffFrames());
				}
			}),
		TPair<EBlueprintHelperReviewSurface, FSurfaceRefreshHandler>(
			EBlueprintHelperReviewSurface::DataAsset,
			[this]()
			{
				if (MainWorkspaceDiffStackBox.IsValid())
				{
					MainWorkspaceDiffStackBox->SetContent(BuildMainWorkspaceDiffFrames());
				}
			})
	};

	for (const TPair<EBlueprintHelperReviewSurface, FSurfaceRefreshHandler>& Handler : RefreshHandlers)
	{
		if (Handler.Key == Surface)
		{
			Handler.Value();
			Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
			return;
		}
	}
}

void SBlueprintHelperReviewPanel::OnRegisteredRowGeometryChanged(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface)
{
	if (Surface == EBlueprintHelperReviewSurface::Unknown)
	{
		return;
	}
	if (!ReviewAssetContext.AssetPath.IsEmpty()
		&& !AssetPath.IsEmpty()
		&& AssetPath != ReviewAssetContext.AssetPath)
	{
		return;
	}

	AddDebugMessage(FString::Printf(
		TEXT("ReviewRowLifecycle surface=%s event=row_registered asset=\"%s\" result=refresh"),
		BlueprintHelperReviewSurfaceToString(Surface),
		*AssetPath));
	RefreshSurfaceOverlay(Surface);
}

void SBlueprintHelperReviewPanel::OnSurfaceGeometryInvalidated(EBlueprintHelperReviewSurface Surface)
{
	if (Surface == EBlueprintHelperReviewSurface::Unknown)
	{
		return;
	}

	AddDebugMessage(FString::Printf(
		TEXT("ReviewRowLifecycle surface=%s event=geometry_changed result=refresh"),
		BlueprintHelperReviewSurfaceToString(Surface)));
	RefreshSurfaceOverlay(Surface);
}

void SBlueprintHelperReviewPanel::LoadReviewAssetFromSelection()
{
	FString AssetPath;
	if (SelectedChange.IsValid())
	{
		AssetPath = SelectedChange->AssetPath;
	}
	if (AssetPath.IsEmpty())
	{
		for (const FReviewChangeItem& Item : ChangeItems)
		{
			if (Item.IsValid() && !Item->AssetPath.IsEmpty())
			{
				AssetPath = Item->AssetPath;
				break;
			}
		}
	}

	FBlueprintHelperReviewAssetContext LoadedContext =
		FBlueprintHelperReviewAssetContext::LoadForAssetPath(AssetPath);
	const EBlueprintHelperReviewSurface LoadedDetailsSurface = ResolveDetailsSurfaceFromSelectedChange();
	if (LoadedContext.AssetPath == ReviewAssetContext.AssetPath
		&& LoadedContext.AssetKind == ReviewAssetContext.AssetKind
		&& LoadedContext.AssetObject.Get() == ReviewAssetContext.AssetObject.Get()
		&& LoadedContext.Blueprint.Get() == ReviewAssetContext.Blueprint.Get()
		&& LoadedDetailsSurface == DetailsSurface)
	{
		return;
	}

	ReviewAssetContext = LoadedContext;
	DetailsSurface = LoadedDetailsSurface;
	AddDebugMessage(FString::Printf(
		TEXT("ReviewAssetContext asset=\"%s\" package=\"%s\" object=\"%s\" kind=%s valid=%d blueprint=%d detailsSurface=%s"),
		*ReviewAssetContext.AssetPath,
		*ReviewAssetContext.PackageName,
		*ReviewAssetContext.ObjectPath,
		BlueprintHelperReviewAssetKindToString(ReviewAssetContext.AssetKind),
		ReviewAssetContext.IsValid() ? 1 : 0,
		ReviewAssetContext.Blueprint.IsValid() ? 1 : 0,
		BlueprintHelperReviewSurfaceToString(DetailsSurface)));
	if (ComponentsContentBox.IsValid())
	{
		ComponentsContentBox->SetContent(BuildReadonlyComponentsWidget());
	}
	if (MyBlueprintContentBox.IsValid())
	{
		MyBlueprintContentBox->SetContent(BuildReadonlyMyBlueprintWidget());
	}
	if (DetailsContentBox.IsValid())
	{
		DetailsContentBox->SetContent(BuildReadonlyDetailsWidget());
	}
	if (KismetInspector.IsValid())
	{
		UpdateDetailsSelection();
	}
}

void SBlueprintHelperReviewPanel::UpdateDetailsSelection()
{
	if (!KismetInspector.IsValid())
	{
		return;
	}

	UObject* DetailsObject = ResolveDetailsObjectForSelectedChange();
	if (DetailsObject)
	{
		KismetInspector->ShowDetailsForSingleObject(
			DetailsObject,
			SKismetInspector::FShowDetailsOptions(GetSelectedTitle(), true));
		RefreshSurfaceOverlay(EBlueprintHelperReviewSurface::Details);
		return;
	}

	TArray<UObject*> EmptySelection;
	KismetInspector->ShowDetailsForObjects(
		EmptySelection,
		SKismetInspector::FShowDetailsOptions(FText::GetEmpty(), true));
	RefreshSurfaceOverlay(EBlueprintHelperReviewSurface::Details);
}

void SBlueprintHelperReviewPanel::OnDetailsDisplayedPropertiesChanged()
{
	AddDebugMessage(TEXT("ReviewFrameGeometry surface=details event=displayed_properties_changed result=refresh"));
	RefreshSurfaceOverlay(EBlueprintHelperReviewSurface::Details);
	DetailsGeometryRetryCount = 0;
	if (!bDetailsGeometryRetryActive)
	{
		bDetailsGeometryRetryActive = true;
		RegisterActiveTimer(
			0.05f,
			FWidgetActiveTimerDelegate::CreateSP(this, &SBlueprintHelperReviewPanel::TickDetailsGeometryRetry));
	}
}

EActiveTimerReturnType SBlueprintHelperReviewPanel::TickDetailsGeometryRetry(double InCurrentTime, float InDeltaTime)
{
	if (++DetailsGeometryRetryCount > 6)
	{
		bDetailsGeometryRetryActive = false;
		return EActiveTimerReturnType::Stop;
	}

	RefreshSurfaceOverlay(EBlueprintHelperReviewSurface::Details);
	return EActiveTimerReturnType::Continue;
}

EBlueprintHelperReviewSurface SBlueprintHelperReviewPanel::ResolveDetailsSurfaceFromSelectedChange() const
{
	auto ResolveFromChange = [](const FReviewChangeItem& Item)
	{
		if (!Item.IsValid())
		{
			return EBlueprintHelperReviewSurface::Unknown;
		}
		if (FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange(*Item))
		{
			return EBlueprintHelperReviewSurface::Details;
		}
		return EBlueprintHelperReviewSurface::Unknown;
	};

	const EBlueprintHelperReviewSurface SelectedSurface = ResolveFromChange(SelectedChange);
	if (SelectedSurface != EBlueprintHelperReviewSurface::Unknown)
	{
		return SelectedSurface;
	}

	const FString CurrentAssetPath = SelectedChange.IsValid() ? SelectedChange->AssetPath : FString();
	for (const FReviewChangeItem& Item : ChangeItems)
	{
		if (!CurrentAssetPath.IsEmpty() && Item.IsValid() && Item->AssetPath != CurrentAssetPath)
		{
			continue;
		}

		const EBlueprintHelperReviewSurface ItemSurface = ResolveFromChange(Item);
		if (ItemSurface != EBlueprintHelperReviewSurface::Unknown)
		{
			return ItemSurface;
		}
	}

	return EBlueprintHelperReviewSurface::Details;
}

bool SBlueprintHelperReviewPanel::ResolveReviewRowGeometry(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface,
	FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
{
	TSharedPtr<SWidget> OverlayWidget;
	const TArray<TPair<EBlueprintHelperReviewSurface, TSharedPtr<SWidget>>> OverlayTargets =
	{
		TPair<EBlueprintHelperReviewSurface, TSharedPtr<SWidget>>(
			EBlueprintHelperReviewSurface::Components,
			ComponentsDiffStackBox),
		TPair<EBlueprintHelperReviewSurface, TSharedPtr<SWidget>>(
			EBlueprintHelperReviewSurface::UMGWidgetTree,
			ComponentsDiffStackBox),
		TPair<EBlueprintHelperReviewSurface, TSharedPtr<SWidget>>(
			EBlueprintHelperReviewSurface::MyBlueprint,
			MyBlueprintDiffStackBox),
		TPair<EBlueprintHelperReviewSurface, TSharedPtr<SWidget>>(
			EBlueprintHelperReviewSurface::Details,
			DetailsDiffStackBox),
		TPair<EBlueprintHelperReviewSurface, TSharedPtr<SWidget>>(
			EBlueprintHelperReviewSurface::DataTable,
			MainWorkspaceDiffStackBox),
		TPair<EBlueprintHelperReviewSurface, TSharedPtr<SWidget>>(
			EBlueprintHelperReviewSurface::DataAsset,
			MainWorkspaceDiffStackBox)
	};

	for (const TPair<EBlueprintHelperReviewSurface, TSharedPtr<SWidget>>& Target : OverlayTargets)
	{
		if (Target.Key == Surface)
		{
			OverlayWidget = Target.Value;
			break;
		}
	}

	if (!OverlayWidget.IsValid())
	{
		OutAnchor.Reason = TEXT("unsupported_surface_geometry");
		return false;
	}

	if (Surface == EBlueprintHelperReviewSurface::Components)
	{
		if (FBlueprintHelperReviewBlueprintComponentsPresenter::ResolveRowGeometry(
			Change,
			ComponentsPresenterState,
			OverlayWidget,
			OutAnchor))
		{
			return true;
		}
	}
	else if (Surface == EBlueprintHelperReviewSurface::MyBlueprint)
	{
		if (FBlueprintHelperReviewMyBlueprintPresenter::ResolveRowGeometry(
			Change,
			MyBlueprintPresenterState,
			OverlayWidget,
			OutAnchor))
		{
			return true;
		}
	}
	else if (Surface == EBlueprintHelperReviewSurface::Details)
	{
		if (ResolveDetailsRowGeometry(Change, OverlayWidget, OutAnchor))
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

	const FString PrimaryAssetPath = Change.AssetPath.IsEmpty() ? ReviewAssetContext.AssetPath : Change.AssetPath;
	if (FBlueprintHelperReviewSlateRowGeometryRegistry::ResolveRowGeometry(
		PrimaryAssetPath,
		Surface,
		TargetText,
		OverlayWidget,
		OutAnchor))
	{
		return true;
	}

	if (PrimaryAssetPath != ReviewAssetContext.AssetPath)
	{
		return FBlueprintHelperReviewSlateRowGeometryRegistry::ResolveRowGeometry(
			ReviewAssetContext.AssetPath,
			Surface,
			TargetText,
			OverlayWidget,
			OutAnchor);
	}

	return false;
}

bool SBlueprintHelperReviewPanel::ResolveDetailsRowGeometry(
	const FBlueprintHelperReviewVisibleChange& Change,
	const TSharedPtr<SWidget>& OverlayWidget,
	FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
{
	if (!KismetInspector.IsValid())
	{
		OutAnchor.Reason = TEXT("details_inspector_unavailable");
		return false;
	}
	if (!OverlayWidget.IsValid())
	{
		OutAnchor.Reason = TEXT("overlay_geometry_unavailable");
		return false;
	}

	TArray<FString> Candidates;
	FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(
		FBlueprintHelperReviewSurfaceFrameBuilder::GetReviewTargetText(Change, EBlueprintHelperReviewSurface::Details),
		Candidates);
	FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(Change.LocationKey, Candidates);
	FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(Change.DisplayLabel, Candidates);
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if (Target.Surface == EBlueprintHelperReviewSurface::Details
			|| BlueprintHelperReviewTargetKindCanRouteToDetails(Target.TargetKind))
		{
			FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(Target.TargetKey, Candidates);
			FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(Target.PropertyPath, Candidates);
			FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(Target.DisplayLabel, Candidates);
			FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(Target.TargetKind, Candidates);
		}
	}

	bool bRequestedPropertyScroll = false;
	if (UObject* DetailsObject = ResolveDetailsObjectForSelectedChange())
	{
		if (TSharedPtr<IDetailsView> PropertyView = KismetInspector->GetPropertyView())
		{
			const TArray<FString> InitialCandidates = Candidates;
			for (const FString& Candidate : InitialCandidates)
			{
				FString PropertyName = Candidate;
				PropertyName.TrimStartAndEndInline();
				int32 DelimiterIndex = INDEX_NONE;
				if (PropertyName.FindLastChar(TEXT(':'), DelimiterIndex)
					|| PropertyName.FindLastChar(TEXT('/'), DelimiterIndex)
					|| PropertyName.FindLastChar(TEXT('.'), DelimiterIndex))
				{
					PropertyName = PropertyName.Mid(DelimiterIndex + 1);
				}
				PropertyName.TrimStartAndEndInline();
				if (PropertyName.IsEmpty())
				{
					continue;
				}

				if (FProperty* Property = FindFProperty<FProperty>(DetailsObject->GetClass(), FName(*PropertyName)))
				{
					FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(Property->GetName(), Candidates);
					FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(
						Property->GetDisplayNameText().ToString(),
						Candidates);
					const TSharedRef<FPropertyPath> PropertyPath = FPropertyPath::Create(TWeakFieldPtr<FProperty>(Property));
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
					PropertyView->ScrollPropertyIntoView(*PropertyPath, true);
#endif
					PropertyView->HighlightProperty(*PropertyPath);
					bRequestedPropertyScroll = true;
				}
			}
		}
	}

	if (Candidates.Num() == 0)
	{
		OutAnchor.Reason = TEXT("missing_geometry_target");
		return false;
	}

	if (FBlueprintHelperReviewPanelGeometryUtils::ResolveTextGeometryRecursive(
		KismetInspector.ToSharedRef(),
		OverlayWidget,
		Candidates,
		TEXT("details_text"),
		OutAnchor))
	{
		return true;
	}

	OutAnchor.TargetText = Candidates[0];
	OutAnchor.Reason = bRequestedPropertyScroll
		? TEXT("details_row_geometry_not_ready")
		: TEXT("no_matching_details_text");
	return false;
}

UObject* SBlueprintHelperReviewPanel::ResolveDetailsObjectForSelectedChange() const
{
	const EBlueprintHelperReviewSurface SelectedDetailsSurface = ResolveDetailsSurfaceFromSelectedChange();
	if (SelectedDetailsSurface == EBlueprintHelperReviewSurface::UMGWidgetTree
		|| SelectedDetailsSurface == EBlueprintHelperReviewSurface::DataTable
		|| SelectedDetailsSurface == EBlueprintHelperReviewSurface::DataAsset)
	{
		return nullptr;
	}

	if (SelectedChange.IsValid())
	{
		if (UObject* ComponentDetailsObject = ResolveComponentDetailsObjectForChange(*SelectedChange))
		{
			return ComponentDetailsObject;
		}
	}

	if (SelectedChange.IsValid()
		&& FBlueprintHelperReviewGraphPresenter::ShouldShowChange(*SelectedChange))
	{
		if (UEdGraph* Graph = ResolveGraphForSelectedChange())
		{
			return Graph;
		}
	}

	if (ReviewAssetContext.DefaultObject.IsValid())
	{
		return ReviewAssetContext.DefaultObject.Get();
	}
	return ReviewAssetContext.AssetObject.Get();
}

UObject* SBlueprintHelperReviewPanel::ResolveComponentDetailsObjectForChange(const FBlueprintHelperReviewVisibleChange& Change) const
{
	if (!FBlueprintHelperReviewPanelLocalUtils::ChangeLooksLikeComponentDetailsTarget(Change))
	{
		return nullptr;
	}

	UBlueprint* Blueprint = ReviewAssetContext.Blueprint.Get();
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		return nullptr;
	}

	const TArray<FString> Candidates =
		FBlueprintHelperReviewPanelLocalUtils::BuildDetailsObjectCandidates(Change);
	if (Candidates.Num() == 0)
	{
		return nullptr;
	}

	TArray<USCS_Node*> Nodes = Blueprint->SimpleConstructionScript->GetAllNodes();
	for (USCS_Node* Node : Nodes)
	{
		if (!Node)
		{
			continue;
		}

		UActorComponent* ComponentTemplate =
			FBlueprintHelperReviewPanelLocalUtils::GetSCSNodeComponentTemplate(Node);
		if (!ComponentTemplate)
		{
			continue;
		}

		if (FBlueprintHelperReviewPanelLocalUtils::DetailsObjectCandidateMatches(
				Candidates,
				Node->GetVariableName().ToString())
			|| FBlueprintHelperReviewPanelLocalUtils::DetailsObjectCandidateMatches(
				Candidates,
				ComponentTemplate->GetName())
			|| FBlueprintHelperReviewPanelLocalUtils::DetailsObjectCandidateMatches(
				Candidates,
				ComponentTemplate->GetFName().ToString()))
		{
			return ComponentTemplate;
		}
	}

	return nullptr;
}

UEdGraph* SBlueprintHelperReviewPanel::ResolveGraphForSelectedChange() const
{
	return FBlueprintHelperReviewGraphPresenter::ResolveGraphForSelection(
		ReviewAssetContext,
		SelectedChange);
}
