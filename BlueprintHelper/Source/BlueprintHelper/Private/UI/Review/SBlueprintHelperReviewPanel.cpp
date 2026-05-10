// BlueprintHelper fake Review panel implementation.

#include "UI/Review/SBlueprintHelperReviewPanel.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Misc/DateTime.h"
#include "IDetailsView.h"
#include "PropertyEditorDelegates.h"
#include "PropertyPath.h"
#include "SKismetInspector.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "UI/Review/BlueprintHelperReviewDebugText.h"
#include "UI/Review/BlueprintHelperReviewPanelGeometryUtils.h"
#include "UI/Review/BlueprintHelperReviewPanelStyle.h"
#include "UI/Review/BlueprintHelperReviewAssetPresenters.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenter.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

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
	SelectedChange = Item;
	LoadReviewAssetFromSelection();
	if (GraphEditorBox.IsValid())
	{
		GraphEditorBox->SetContent(BuildMainWorkspaceWidget());
	}
	RefreshDiffStackWidgets();
	UpdateDetailsSelection();
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
		FReviewTreeItemPtr* ExistingRoot = AssetRootsByPath.Find(AssetPath);
		if (!ExistingRoot)
		{
			FReviewTreeItemPtr Root = MakeShared<FReviewTreeItem>();
			Root->bIsAssetRoot = true;
			Root->AssetPath = AssetPath;
			AssetRootsByPath.Add(AssetPath, Root);
			OutRootItems.Add(Root);
			ExistingRoot = AssetRootsByPath.Find(AssetPath);
		}

		FReviewTreeItemPtr Leaf = MakeShared<FReviewTreeItem>();
		Leaf->AssetPath = AssetPath;
		Leaf->Change = Item;
		LeafItems.Add(Leaf);
		if (Item->bIsAssetLifecycleRoot && !Item->ChangeId.IsEmpty())
		{
			LifecycleRootItemsByAssetAndChangeId.Add(
				FString::Printf(TEXT("%s|%s"), *AssetPath, *Item->ChangeId),
				Leaf);
		}
	}

	for (const FReviewTreeItemPtr& Leaf : LeafItems)
	{
		if (!Leaf.IsValid() || !Leaf->Change.IsValid())
		{
			continue;
		}

		const FString AssetPath = Leaf->AssetPath.IsEmpty() ? TEXT("(unknown asset)") : Leaf->AssetPath;
		FReviewTreeItemPtr* ExistingRoot = AssetRootsByPath.Find(AssetPath);
		if (!ExistingRoot || !ExistingRoot->IsValid())
		{
			continue;
		}

		const FBlueprintHelperReviewVisibleChange& Change = *Leaf->Change;
		if (!Change.bIsAssetLifecycleRoot && !Change.ParentChangeId.IsEmpty())
		{
			if (FReviewTreeItemPtr* ParentRoot = LifecycleRootItemsByAssetAndChangeId.Find(
				FString::Printf(TEXT("%s|%s"), *AssetPath, *Change.ParentChangeId)))
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
		const TPair<FReviewTreeItemPtr, int32> PendingItem = PendingItems.Pop(EAllowShrinking::No);
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
	return OnAcceptChange(FindChangeItemById(ChangeId));
}

FReply SBlueprintHelperReviewPanel::OnRejectChangeId(const FString& ChangeId)
{
	return OnRejectChange(FindChangeItemById(ChangeId));
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
				&SBlueprintHelperReviewPanel::RefreshSurfaceOverlay));
	}

	return FBlueprintHelperReviewBlueprintComponentsPresenter::BuildContent(
		ReviewAssetContext,
		ComponentsPresenterState,
		FBlueprintHelperReviewGeometryInvalidated::CreateSP(
			this,
			&SBlueprintHelperReviewPanel::RefreshSurfaceOverlay));
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildReadonlyMyBlueprintWidget()
{
	return FBlueprintHelperReviewMyBlueprintPresenter::BuildContent(
		ReviewAssetContext,
		MyBlueprintPresenterState,
		ChangeItems,
		FBlueprintHelperReviewGeometryInvalidated::CreateSP(
			this,
			&SBlueprintHelperReviewPanel::RefreshSurfaceOverlay));
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
	int32 VisibleRowCount = 0;
	for (const FReviewChangeItem& Item : ChangeItems)
	{
		if (!Item.IsValid() || !Predicate(*Item))
		{
			continue;
		}
		if (!CurrentAssetPath.IsEmpty() && Item->AssetPath != CurrentAssetPath)
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
	Args.OnAcceptChange = [this](FReviewChangeItem Item)
	{
		return OnAcceptChange(Item);
	};
	Args.OnRejectChange = [this](FReviewChangeItem Item)
	{
		return OnRejectChange(Item);
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
		&SBlueprintHelperReviewPanel::RefreshSurfaceOverlay);

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
		[this](FReviewChangeItem ChangeItem)
		{
			return OnAcceptChange(ChangeItem);
		},
		[this](FReviewChangeItem ChangeItem)
		{
			return OnRejectChange(ChangeItem);
		},
		Item == SelectedChange);
}

FReply SBlueprintHelperReviewPanel::OnAcceptSelected()
{
	return OnAcceptChange(SelectedChange);
}

FReply SBlueprintHelperReviewPanel::OnRejectSelected()
{
	return OnRejectChange(SelectedChange);
}

FReply SBlueprintHelperReviewPanel::OnAcceptChange(FReviewChangeItem Item)
{
	if (!Item.IsValid())
	{
		return FReply::Handled();
	}

	SelectedChange = Item;

	FBlueprintHelperReviewActionResult Result;
	if (ReviewActionService)
	{
		Result = ReviewActionService->AcceptVisibleChange(*Item);
	}
	else
	{
		Result.bSucceeded = true;
		Result.TargetTransactionId = Item->LatestTransactionId;
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::Accepted;
		Result.Message = TEXT("Accepted visible change.");
	}

	if (Result.bSucceeded)
	{
		const int32 RemovedIndex = ChangeItems.IndexOfByKey(Item);
		ChangeItems.Remove(Item);
		if (ChangeItems.Num() > 0)
		{
			const int32 NextIndex = FMath::Clamp(RemovedIndex, 0, ChangeItems.Num() - 1);
			SelectedChange = ChangeItems[NextIndex];
		}
		else
		{
			SelectedChange.Reset();
		}
		RebuildChangeTreeItems();
		RefreshChangeTreeWidget();
		LoadReviewAssetFromSelection();
		if (GraphEditorBox.IsValid())
		{
			GraphEditorBox->SetContent(BuildMainWorkspaceWidget());
		}
		RefreshDiffStackWidgets();
		UpdateDetailsSelection();
	}

	AddDebugMessage(FString::Printf(
		TEXT("Accept change id=%s success=%d message=\"%s\""),
		*Item->ChangeId,
		Result.bSucceeded ? 1 : 0,
		*Result.Message));
	return FReply::Handled();
}

FReply SBlueprintHelperReviewPanel::OnRejectChange(FReviewChangeItem Item)
{
	if (!Item.IsValid())
	{
		return FReply::Handled();
	}

	SelectedChange = Item;

	if (Item->bIsAssetLifecycleRoot)
	{
		FBlueprintHelperReviewCascadeActionResult CascadeResult;
		if (ReviewActionService)
		{
			CascadeResult = ReviewActionService->RejectLifecycleRootVisibleChange(
				*Item,
				BuildPendingChangeSnapshot());
		}
		else
		{
			CascadeResult.RootResult.bSucceeded = false;
			CascadeResult.RootResult.TargetTransactionId = Item->LatestTransactionId;
			CascadeResult.RootResult.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			CascadeResult.RootResult.RollbackMode = TEXT("archive_baseline");
			CascadeResult.RootResult.Message = TEXT("Reject requires archive-baseline rollback service.");
		}

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
		}
		else
		{
			Item->Status = CascadeResult.RootResult.NewStatus;
			Item->NeedsActionReason = CascadeResult.RootResult.Message;
		}

		RebuildChangeTreeItems();
		RefreshChangeTreeWidget();
		LoadReviewAssetFromSelection();
		RefreshDiffStackWidgets();
		if (GraphEditorBox.IsValid())
		{
			GraphEditorBox->SetContent(BuildMainWorkspaceWidget());
		}
		UpdateDetailsSelection();

		AddDebugMessage(FString::Printf(
			TEXT("Reject lifecycle root id=%s success=%d removedChildren=%d status=%s message=\"%s\""),
			*Item->ChangeId,
			CascadeResult.RootResult.bSucceeded ? 1 : 0,
			CascadeResult.RemovedChildChangeIds.Num(),
			BlueprintHelperReviewChangeStatusToString(CascadeResult.RootResult.NewStatus),
			*CascadeResult.RootResult.Message));
		return FReply::Handled();
	}

	FBlueprintHelperReviewActionResult Result;
	if (ReviewActionService)
	{
		Result = ReviewActionService->RejectVisibleChange(*Item);
	}
	else
	{
		Result.bSucceeded = false;
		Result.TargetTransactionId = Item->LatestTransactionId;
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
		Result.RollbackMode = TEXT("archive_baseline");
		Result.Message = TEXT("Reject requires archive-baseline rollback service.");
	}

	Item->Status = Result.NewStatus;
	Item->NeedsActionReason = Result.Message;
	RebuildChangeTreeItems();
	RefreshChangeTreeWidget();
	LoadReviewAssetFromSelection();
	RefreshDiffStackWidgets();
	if (GraphEditorBox.IsValid())
	{
		GraphEditorBox->SetContent(BuildMainWorkspaceWidget());
	}
	UpdateDetailsSelection();

	AddDebugMessage(FString::Printf(
		TEXT("Reject change id=%s success=%d status=%s message=\"%s\""),
		*Item->ChangeId,
		Result.bSucceeded ? 1 : 0,
		BlueprintHelperReviewChangeStatusToString(Result.NewStatus),
		*Result.Message));
	return FReply::Handled();
}


FReply SBlueprintHelperReviewPanel::OnAcceptAll()
{
	FString AssetPath;
	if (SelectedChange.IsValid())
	{
		AssetPath = SelectedChange->AssetPath;
	}

	TArray<FReviewChangeItem> AcceptedItems;
	for (const FReviewChangeItem& Item : ChangeItems)
	{
		if (!Item.IsValid() || (!AssetPath.IsEmpty() && Item->AssetPath != AssetPath))
		{
			continue;
		}

		FBlueprintHelperReviewActionResult Result;
		if (ReviewActionService)
		{
			Result = ReviewActionService->AcceptVisibleChange(*Item);
		}
		else
		{
			Result.bSucceeded = true;
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::Accepted;
		}
		if (Result.bSucceeded)
		{
			AcceptedItems.Add(Item);
		}
		else
		{
			Item->Status = Result.NewStatus;
			Item->NeedsActionReason = Result.Message;
		}
	}

	ChangeItems.RemoveAll([&AcceptedItems](const FReviewChangeItem& Item)
	{
		return Item.IsValid() && AcceptedItems.Contains(Item);
	});

	SelectedChange = ChangeItems.Num() > 0 ? ChangeItems[0] : FReviewChangeItem();
	RebuildChangeTreeItems();
	RefreshChangeTreeWidget();
	LoadReviewAssetFromSelection();
	RefreshDiffStackWidgets();
	if (GraphEditorBox.IsValid())
	{
		GraphEditorBox->SetContent(BuildMainWorkspaceWidget());
	}
	UpdateDetailsSelection();
	AddDebugMessage(FString::Printf(
		TEXT("AcceptAll asset=\"%s\" remainingVisibleChanges=%d"),
		*AssetPath,
		ChangeItems.Num()));
	return FReply::Handled();
}


FReply SBlueprintHelperReviewPanel::OnRejectAll()
{
	FString AssetPath;
	if (SelectedChange.IsValid())
	{
		AssetPath = SelectedChange->AssetPath;
	}

	FReviewChangeItem LifecycleRoot;
	for (const FReviewChangeItem& Item : ChangeItems)
	{
		if (Item.IsValid()
			&& Item->bIsAssetLifecycleRoot
			&& (AssetPath.IsEmpty() || Item->AssetPath == AssetPath))
		{
			LifecycleRoot = Item;
			break;
		}
	}

	TSet<FString> RemovedChangeIds;
	bool bLifecycleRootSucceeded = false;
	if (LifecycleRoot.IsValid())
	{
		FBlueprintHelperReviewCascadeActionResult CascadeResult;
		if (ReviewActionService)
		{
			CascadeResult = ReviewActionService->RejectLifecycleRootVisibleChange(
				*LifecycleRoot,
				BuildPendingChangeSnapshot());
		}
		else
		{
			CascadeResult.RootResult.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			CascadeResult.RootResult.Message = TEXT("Reject requires archive-baseline rollback service.");
		}

		if (CascadeResult.RootResult.bSucceeded)
		{
			bLifecycleRootSucceeded = true;
			if (!LifecycleRoot->ChangeId.IsEmpty())
			{
				RemovedChangeIds.Add(LifecycleRoot->ChangeId);
			}
			for (const FString& RemovedChildChangeId : CascadeResult.RemovedChildChangeIds)
			{
				RemovedChangeIds.Add(RemovedChildChangeId);
			}
		}
		else
		{
			LifecycleRoot->Status = CascadeResult.RootResult.NewStatus;
			LifecycleRoot->NeedsActionReason = CascadeResult.RootResult.Message;
		}
	}

	if (!LifecycleRoot.IsValid())
	{
		for (const FReviewChangeItem& Item : ChangeItems)
		{
			if (!Item.IsValid()
				|| (!AssetPath.IsEmpty() && Item->AssetPath != AssetPath)
				|| (!Item->ChangeId.IsEmpty() && RemovedChangeIds.Contains(Item->ChangeId)))
			{
				continue;
			}

			FBlueprintHelperReviewActionResult Result;
			if (ReviewActionService)
			{
				Result = ReviewActionService->RejectVisibleChange(*Item);
			}
			else
			{
				Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
				Result.Message = TEXT("Reject requires archive-baseline rollback service.");
			}
			Item->Status = Result.NewStatus;
			Item->NeedsActionReason = Result.Message;
		}
	}

	if (bLifecycleRootSucceeded && RemovedChangeIds.Num() > 0)
	{
		const int32 RemovedIndex = LifecycleRoot.IsValid() ? ChangeItems.IndexOfByKey(LifecycleRoot) : 0;
		ChangeItems.RemoveAll([&RemovedChangeIds, &LifecycleRoot](const FReviewChangeItem& Item)
		{
			return Item == LifecycleRoot
				|| (Item.IsValid()
					&& !Item->ChangeId.IsEmpty()
					&& RemovedChangeIds.Contains(Item->ChangeId));
		});
		SelectNextChangeAfterRemoval(AssetPath, RemovedIndex);
	}
	else if (!SelectedChange.IsValid() && ChangeItems.Num() > 0)
	{
		SelectedChange = ChangeItems[0];
	}

	RebuildChangeTreeItems();
	RefreshChangeTreeWidget();
	LoadReviewAssetFromSelection();
	RefreshDiffStackWidgets();
	if (GraphEditorBox.IsValid())
	{
		GraphEditorBox->SetContent(BuildMainWorkspaceWidget());
	}
	UpdateDetailsSelection();
	AddDebugMessage(FString::Printf(
		TEXT("RejectAll asset=\"%s\" cascadeRemoved=%d"),
		*AssetPath,
		RemovedChangeIds.Num()));
	return FReply::Handled();
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

void SBlueprintHelperReviewPanel::RefreshSurfaceOverlay(EBlueprintHelperReviewSurface Surface)
{
	switch (Surface)
	{
	case EBlueprintHelperReviewSurface::Components:
	case EBlueprintHelperReviewSurface::UMGWidgetTree:
		if (ComponentsDiffStackBox.IsValid())
		{
			ComponentsDiffStackBox->SetContent(BuildStructurePanelDiffFrames());
		}
		break;
	case EBlueprintHelperReviewSurface::MyBlueprint:
		if (MyBlueprintDiffStackBox.IsValid())
		{
			MyBlueprintDiffStackBox->SetContent(BuildPanelDiffFrames(
				&FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange,
				EBlueprintHelperReviewSurface::MyBlueprint));
		}
		break;
	case EBlueprintHelperReviewSurface::Details:
		if (DetailsDiffStackBox.IsValid())
		{
			DetailsDiffStackBox->SetContent(BuildDetailsPanelDiffFrames());
		}
		break;
	case EBlueprintHelperReviewSurface::DataTable:
	case EBlueprintHelperReviewSurface::DataAsset:
		if (MainWorkspaceDiffStackBox.IsValid())
		{
			MainWorkspaceDiffStackBox->SetContent(BuildMainWorkspaceDiffFrames());
		}
		break;
	default:
		return;
	}
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
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
		return;
	}

	TArray<UObject*> EmptySelection;
	KismetInspector->ShowDetailsForObjects(
		EmptySelection,
		SKismetInspector::FShowDetailsOptions(FText::GetEmpty(), true));
}

void SBlueprintHelperReviewPanel::OnDetailsDisplayedPropertiesChanged()
{
	AddDebugMessage(TEXT("ReviewFrameGeometry surface=details event=displayed_properties_changed result=refresh"));
	RefreshSurfaceOverlay(EBlueprintHelperReviewSurface::Details);
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
	switch (Surface)
	{
	case EBlueprintHelperReviewSurface::Components:
	case EBlueprintHelperReviewSurface::UMGWidgetTree:
		OverlayWidget = ComponentsDiffStackBox;
		break;
	case EBlueprintHelperReviewSurface::MyBlueprint:
		OverlayWidget = MyBlueprintDiffStackBox;
		break;
	case EBlueprintHelperReviewSurface::Details:
		OverlayWidget = DetailsDiffStackBox;
		break;
	case EBlueprintHelperReviewSurface::DataTable:
	case EBlueprintHelperReviewSurface::DataAsset:
		OverlayWidget = MainWorkspaceDiffStackBox;
		break;
	default:
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
					PropertyView->ScrollPropertyIntoView(*PropertyPath, true);
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

UEdGraph* SBlueprintHelperReviewPanel::ResolveGraphForSelectedChange() const
{
	return FBlueprintHelperReviewGraphPresenter::ResolveGraphForSelection(
		ReviewAssetContext,
		SelectedChange);
}
