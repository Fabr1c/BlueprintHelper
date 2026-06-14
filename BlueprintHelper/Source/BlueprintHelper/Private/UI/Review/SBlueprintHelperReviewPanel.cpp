// BlueprintHelper fake Review panel implementation.

#include "UI/Review/SBlueprintHelperReviewPanel.h"

#include "Async/Async.h"
#include "HAL/PlatformTime.h"
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
#include "UI/Review/BlueprintHelperReviewPanelStateService.h"
#include "UI/Review/BlueprintHelperReviewAssetPresenters.h"
#include "UI/Review/BlueprintHelperReviewDebugBundleService.h"
#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"
#include "UI/Review/BlueprintHelperReviewSlateRowGeometryRegistry.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenter.h"
#include "UI/Review/Native/Components/SBlueprintHelperReviewComponentsPanel.h"
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
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_CustomEvent.h"

static FString BlueprintHelperReviewFriendlyActionMessage(const FString& Prefix, const FString& Detail);

static void BlueprintHelperReviewInvalidateRowWidget(const TWeakPtr<SWidget>& RowWidget)
{
	if (TSharedPtr<SWidget> Widget = RowWidget.Pin())
	{
		Widget->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	}
}

static void BlueprintHelperReviewInvalidateComponentRows(
	const TArray<TSharedPtr<FBlueprintHelperReviewComponentRowItem>>& Items)
{
	for (const TSharedPtr<FBlueprintHelperReviewComponentRowItem>& Item : Items)
	{
		if (!Item.IsValid())
		{
			continue;
		}
		BlueprintHelperReviewInvalidateRowWidget(Item->RowWidget);
		BlueprintHelperReviewInvalidateComponentRows(Item->Children);
	}
}

static void BlueprintHelperReviewInvalidateMyBlueprintRows(
	const TArray<TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>>& Items)
{
	for (const TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>& Item : Items)
	{
		if (!Item.IsValid())
		{
			continue;
		}
		BlueprintHelperReviewInvalidateRowWidget(Item->RowWidget);
		BlueprintHelperReviewInvalidateMyBlueprintRows(Item->Children);
	}
}

static void BlueprintHelperReviewInvalidateWidgetTreeRows(
	const TArray<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>>& Items)
{
	for (const TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>& Item : Items)
	{
		if (!Item.IsValid())
		{
			continue;
		}
		BlueprintHelperReviewInvalidateRowWidget(Item->RowWidget);
		BlueprintHelperReviewInvalidateWidgetTreeRows(Item->Children);
	}
}

static void BlueprintHelperReviewInvalidateDataAssetRows(
	const TArray<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>& Items)
{
	for (const TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>& Item : Items)
	{
		if (Item.IsValid())
		{
			BlueprintHelperReviewInvalidateRowWidget(Item->RowWidget);
		}
	}
}

static FString BlueprintHelperReviewExtractPrefixedName(const FString& Value, const FString& Prefix)
{
	if (Value.StartsWith(Prefix, ESearchCase::IgnoreCase))
	{
		return Value.Mid(Prefix.Len());
	}
	return FString();
}

static bool BlueprintHelperReviewGraphExists(const TArray<TObjectPtr<UEdGraph>>& Graphs, const FString& GraphName)
{
	if (GraphName.IsEmpty())
	{
		return false;
	}
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

static bool BlueprintHelperReviewTryResolveSignatureGraph(
	UBlueprint* Blueprint,
	const FString& SignatureName,
	FString& OutGraphName)
{
	if (!Blueprint || SignatureName.IsEmpty())
	{
		return false;
	}

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName().Equals(SignatureName, ESearchCase::IgnoreCase))
		{
			OutGraphName = Graph->GetName();
			return true;
		}
	}
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && Graph->GetName().Equals(SignatureName, ESearchCase::IgnoreCase))
		{
			OutGraphName = Graph->GetName();
			return true;
		}
	}
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph)
		{
			continue;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node);
			if (CustomEvent && CustomEvent->CustomFunctionName.ToString().Equals(SignatureName, ESearchCase::IgnoreCase))
			{
				OutGraphName = Graph->GetName();
				return true;
			}
		}
	}
	return false;
}

SBlueprintHelperReviewPanel::~SBlueprintHelperReviewPanel()
{
	if (PendingLoadCoordinator.IsValid())
	{
		PendingLoadCoordinator->CancelPendingLoads();
		PendingLoadCoordinator.Reset();
	}
	if (ReviewPanelPresenter.IsValid() && PendingReviewChangedHandle.IsValid())
	{
		ReviewPanelPresenter->RemovePendingReviewChangedEventHandler(PendingReviewChangedHandle);
	}
	FBlueprintHelperReviewSlateRowGeometryRegistry::RemoveRowsChangedHandler(RowGeometryChangedHandle);
	FBlueprintHelperReviewRowHighlightModel::RemoveStateChangedHandler(RowHighlightStateChangedHandle);
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
	TSet<FString> SeenChangeIds;
	for (const FBlueprintHelperReviewVisibleChange& Change : SourceChanges)
	{
		if (!Change.ChangeId.IsEmpty())
		{
			if (SeenChangeIds.Contains(Change.ChangeId))
			{
				AddDebugMessage(FString::Printf(
					TEXT("VisibleChange folded reason=duplicate_change_id change=%s asset=\"%s\""),
					*Change.ChangeId,
					*Change.AssetPath));
				continue;
			}
			SeenChangeIds.Add(Change.ChangeId);
		}
		ChangeItems.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(Change));
	}
	RebuildReviewPanelStatePreservingTransient();
	RebuildChangeTreeItems();
	SyncReviewRowHighlightStates();
}

bool SBlueprintHelperReviewPanel::TryResolveGraphNavigationForChange(
	FReviewChangeItem Item,
	FString& OutGraphName) const
{
	OutGraphName.Reset();
	if (!Item.IsValid() || !ReviewAssetContext.Blueprint.IsValid())
	{
		return false;
	}

	UBlueprint* Blueprint = ReviewAssetContext.Blueprint.Get();
	TArray<FString> SignatureCandidates;
	TArray<FString> GraphCandidates;
	for (const FBlueprintHelperReviewAtomicTarget& Target : Item->AtomicTargets)
	{
		if (!Target.TargetKind.Equals(TEXT("signature"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		const FString NameFromTargetKey = BlueprintHelperReviewExtractPrefixedName(Target.TargetKey, TEXT("signature:"));
		if (!NameFromTargetKey.IsEmpty())
		{
			SignatureCandidates.AddUnique(NameFromTargetKey);
		}
		if (!Target.DisplayLabel.IsEmpty())
		{
			SignatureCandidates.AddUnique(Target.DisplayLabel);
		}
		if (!Target.GraphName.IsEmpty())
		{
			GraphCandidates.AddUnique(Target.GraphName);
		}
	}

	if (SignatureCandidates.Num() == 0)
	{
		const FString NameFromChangeKey = BlueprintHelperReviewExtractPrefixedName(Item->LocationKey, TEXT("signature:"));
		if (!NameFromChangeKey.IsEmpty())
		{
			SignatureCandidates.AddUnique(NameFromChangeKey);
		}
	}
	if (SignatureCandidates.Num() == 0 && Item->ChangeKind == EBlueprintHelperReviewChangeKind::Added)
	{
		SignatureCandidates.AddUnique(Item->DisplayLabel);
	}

	for (const FString& SignatureName : SignatureCandidates)
	{
		if (BlueprintHelperReviewTryResolveSignatureGraph(Blueprint, SignatureName, OutGraphName))
		{
			return true;
		}
	}

	for (const FString& GraphName : GraphCandidates)
	{
		if (BlueprintHelperReviewGraphExists(Blueprint->FunctionGraphs, GraphName)
			|| BlueprintHelperReviewGraphExists(Blueprint->MacroGraphs, GraphName)
			|| BlueprintHelperReviewGraphExists(Blueprint->UbergraphPages, GraphName))
		{
			OutGraphName = GraphName;
			return true;
		}
	}
	return false;
}

void SBlueprintHelperReviewPanel::OnChangeSelectionChanged(FReviewChangeItem Item, ESelectInfo::Type SelectInfo)
{
	const bool bKeepGraphNavigationRequest =
		bAllowGraphNavigationWithoutGraphReview
		&& Item.IsValid()
		&& !RequestedGraphNavigationChangeId.IsEmpty()
		&& Item->ChangeId == RequestedGraphNavigationChangeId;

	SelectedChange = Item;
	LoadReviewAssetFromSelection();
	if (!bKeepGraphNavigationRequest)
	{
		RequestedGraphNavigationChangeId.Reset();
		RequestedGraphNavigationGraphName.Reset();
		bAllowGraphNavigationWithoutGraphReview = false;

		FString ResolvedGraphName;
		if (TryResolveGraphNavigationForChange(Item, ResolvedGraphName))
		{
			Item->GraphName = ResolvedGraphName;
			RequestedGraphNavigationChangeId = Item->ChangeId;
			RequestedGraphNavigationGraphName = ResolvedGraphName;
			bAllowGraphNavigationWithoutGraphReview = true;
			AddDebugMessage(FString::Printf(
				TEXT("GraphEditor navigation request change=%s graph=\"%s\" reason=selected_signature_navigation"),
				*Item->ChangeId,
				*ResolvedGraphName));
		}
	}
	RefreshMainWorkspaceAfterReviewStateChanged();
	RebuildReviewPanelStatePreservingTransient();
	SyncReviewRowHighlightStates(Item.IsValid() ? Item->AssetPath : FString());
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	StartFlash();
	if (Item.IsValid())
	{
		AddDebugMessage(FString::Printf(
			TEXT("Selected change id=%s label=\"%s\" asset=\"%s\" graph=\"%s\" latest=%s"),
			*Item->ChangeId,
			*Item->DisplayLabel,
			*Item->AssetPath,
			*Item->GraphName,
			*Item->LatestEvidenceId));
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
			.Padding(ReviewPanelSettings.RootRowPadding)
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
		if (!Change.ParentChangeId.IsEmpty())
		{
			FReviewTreeItemPtr* ParentRoot = nullptr;
			ParentRoot = LifecycleRootItemsByAssetAndChangeId.Find(
				FString::Printf(TEXT("%s|%s"), *AssetKey, *Change.ParentChangeId));
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
			TEXT("%s|%s|%s|%s|%s|%s|%s|%s|root=%d"),
			*Change.ChangeId,
			BlueprintHelperReviewChangeStatusToString(Change.Status),
			BlueprintHelperReviewChangeKindToString(Change.ChangeKind),
			*Change.AssetPath,
			*Change.ParentChangeId,
			*Change.LocationKey,
			*Change.LatestEvidenceId,
			*Change.DisplayLabel,
			Change.bIsAssetLifecycleRoot ? 1 : 0));
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			Parts.Add(FString::Printf(
				TEXT("target|%s|%s|%s|%s|%s|%s"),
				BlueprintHelperReviewSurfaceToString(Target.Surface),
				*Target.TargetKind,
				*Target.TargetKey,
				*Target.PropertyPath,
				*Target.ComponentPath,
				*Target.DisplayLabel));
		}
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

FReply SBlueprintHelperReviewPanel::OnReviewActionIntent(const FBlueprintHelperReviewActionIntent& Intent)
{
	const FReviewChangeItem Item = FindChangeItemById(Intent.Binding.ChangeId);
	if (!Item.IsValid())
	{
		return FReply::Handled();
	}
	if (Intent.Action == EBlueprintHelperReviewActionIntentKind::Reject)
	{
		const FString ChangeId = !Item->ChangeId.IsEmpty() ? Item->ChangeId : Item->LatestEvidenceId;
		if (!ChangeId.IsEmpty())
		{
			FBlueprintHelperReviewVisibleChange TimingChange = *Item;
			if (TimingChange.ChangeId.IsEmpty())
			{
				TimingChange.ChangeId = ChangeId;
			}
			RejectTimingModel.Begin(TimingChange, FPlatformTime::Seconds());
			RecordRejectStageElapsed(
				ChangeId,
				TEXT("ui_intent_received"),
				Intent.SourceWidget);
		}
	}
	return Intent.Action == EBlueprintHelperReviewActionIntentKind::Accept
		? ExecuteAcceptChange(Item)
		: ExecuteRejectChange(Item);
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
	if (MainSurface == EBlueprintHelperReviewSurface::Material)
	{
		return BuildPanelDiffFrames(
			&FBlueprintHelperReviewMaterialPresenter::ShouldShowChange,
			EBlueprintHelperReviewSurface::Material);
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
		if (ReviewPanelSettings.bOverlayFilterCurrentAssetOnly
			&& !CurrentAssetKey.IsEmpty()
			&& ItemAssetKey != CurrentAssetKey)
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
	Args.OnReviewActionIntent = [this](const FBlueprintHelperReviewActionIntent& Intent)
	{
		return OnReviewActionIntent(Intent);
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
	Args.ReviewPanelSettings = ReviewPanelSettings;

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
	if (Surface == EBlueprintHelperReviewSurface::Material
		&& Predicate == &FBlueprintHelperReviewMaterialPresenter::ShouldShowChange)
	{
		return FBlueprintHelperReviewMaterialPresenter::BuildOverlay(Args);
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
			*Item->LatestEvidenceId)
		: TEXT("");

	TSharedRef<SWidget> Content = SNew(SButton)
		.ContentPadding(ReviewPanelSettings.RowContentPadding)
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
		[this](const FBlueprintHelperReviewActionIntent& Intent)
		{
			return OnReviewActionIntent(Intent);
		},
		ReviewPanelSettings,
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
	FBlueprintHelperReviewPanelStateService::ClearPresenterErrorState(ReviewPanelState, Item->ChangeId);

	const FBlueprintHelperReviewActionIntent Intent = FBlueprintHelperReviewActionIntent::Accept(
		FBlueprintHelperReviewPanelStateService::MakeChangeBinding(
			*Item,
			EBlueprintHelperReviewSurface::Unknown,
			Item->LocationKey),
		TEXT("review_panel"));
	const FBlueprintHelperReviewPanelPresenterEvent PresenterEvent =
		ReviewPanelPresenter.IsValid()
			? ReviewPanelPresenter->HandleActionIntent(
				Intent,
				{ *Item })
			: FBlueprintHelperReviewPanelPresenterEvent::FromActionResult(
				FBlueprintHelperReviewActionResult());
	const FBlueprintHelperReviewActionResult& Result = PresenterEvent.ActionResult;

	if (Result.bSucceeded)
	{
		AddDebugMessage(FString::Printf(
			TEXT("Accept change id=%s result=waiting_for_store_refresh"),
			*Item->ChangeId));
	}
	else
	{
		FBlueprintHelperReviewPanelStateService::SetPresenterErrorState(
			ReviewPanelState,
			Item->ChangeId,
			Result.NewStatus,
			Result.Message);
	}

	AddDebugMessage(FString::Printf(
		TEXT("Accept change id=%s success=%d message=\"%s\""),
		*Item->ChangeId,
		Result.bSucceeded ? 1 : 0,
		*Result.Message));
	ShowReviewActionNotification(
		TEXT("accept:") + Item->ChangeId,
		FString::Printf(
			TEXT("%s: %s"),
			Result.bSucceeded ? TEXT("Accepted") : TEXT("Accept failed"),
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

	const FString ChangeId = !Item->ChangeId.IsEmpty() ? Item->ChangeId : Item->LatestEvidenceId;
	if (!ChangeId.IsEmpty() && !RejectTimingModel.Contains(ChangeId))
	{
		FBlueprintHelperReviewVisibleChange TimingChange = *Item;
		if (TimingChange.ChangeId.IsEmpty())
		{
			TimingChange.ChangeId = ChangeId;
		}
		RejectTimingModel.Begin(TimingChange, FPlatformTime::Seconds());
		RecordRejectStageElapsed(
			ChangeId,
			TEXT("execute_reject_change"),
			TEXT("review_panel_button"));
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

	const FString ChangeId = !Item->ChangeId.IsEmpty() ? Item->ChangeId : Item->LatestEvidenceId;
	if (ChangeId.IsEmpty())
	{
		AddDebugMessage(TEXT("Reject queue failed reason=missing_change_id"));
		if (bShowIndividualNotification)
		{
			ShowReviewActionNotification(
				TEXT("reject:missing_change_id"),
				TEXT("Reject failed: missing Review id."),
				EReviewActionNotificationState::Fail,
				true,
				false);
		}
		return;
	}
	if (FBlueprintHelperReviewPanelStateService::IsTransientActionInProgress(ReviewPanelState, ChangeId))
	{
		AddDebugMessage(FString::Printf(TEXT("Reject queue skipped id=%s reason=already_pending"), *ChangeId));
		if (bShowIndividualNotification)
		{
			ShowReviewActionNotification(
				TEXT("reject:") + ChangeId,
				FString::Printf(
					TEXT("Reject already running: %s"),
					*BuildReviewActionNotificationLabel(Item)),
				EReviewActionNotificationState::Pending,
				false,
				true);
		}
		return;
	}

	FBlueprintHelperReviewPanelStateService::SetTransientActionState(
		ReviewPanelState,
		ChangeId,
		EBlueprintHelperReviewActionIntentKind::Reject,
		EBlueprintHelperReviewChangeStatus::NeedsAction,
		TEXT("reject_queued"));
	FBlueprintHelperReviewPanelStateService::ClearPresenterErrorState(ReviewPanelState, ChangeId);
	PendingRejectChangeIds.Add(ChangeId);
	if (!RejectTimingModel.Contains(ChangeId))
	{
		FBlueprintHelperReviewVisibleChange TimingChange = *Item;
		if (TimingChange.ChangeId.IsEmpty())
		{
			TimingChange.ChangeId = ChangeId;
		}
		RejectTimingModel.Begin(TimingChange, FPlatformTime::Seconds());
	}
	SelectedChange = Item;
	RefreshReviewActionQueueState(TEXT("reject_queued"), Item->AssetPath);
	AddDebugMessage(FString::Printf(TEXT("Reject queued id=%s"), *ChangeId));
	RecordRejectStageElapsed(ChangeId, TEXT("queued"), TEXT("review_panel_action"));
	if (bShowIndividualNotification)
	{
		ShowReviewActionNotification(
			TEXT("reject:") + ChangeId,
			FString::Printf(
				TEXT("Reject queued: %s"),
				*BuildReviewActionNotificationLabel(Item)),
			EReviewActionNotificationState::Pending,
			false,
			true);
	}
	StartNextRejectPrepare();
}

void SBlueprintHelperReviewPanel::EmitRejectTimingSample(
	const FBlueprintHelperReviewRejectTimingSample& Sample)
{
	if (!Sample.bValid)
	{
		return;
	}
	AddDebugMessage(FString::Printf(
		TEXT("RejectPerf id=%s stage=%s stage_ms=%.2f total_ms=%.2f detail=%s"),
		*Sample.ChangeId,
		*Sample.Stage,
		Sample.StageMs,
		Sample.TotalMs,
		*Sample.Detail));
	TSharedPtr<FBlueprintHelperReviewVisibleChange> ChangePtr;
	if (Sample.bHasChangeSnapshot)
	{
		ChangePtr = MakeShared<FBlueprintHelperReviewVisibleChange>(Sample.ChangeSnapshot);
	}
	AppendDebugBundleEvent(FBlueprintHelperReviewDebugBundleService::BuildRejectTimingEvent(
		DebugBundleSessionId,
		Sample.Stage,
		Sample.ChangeId,
		ChangePtr,
		Sample.AssetPath,
		Sample.StageMs,
		Sample.TotalMs,
		Sample.Detail));
}

void SBlueprintHelperReviewPanel::RecordRejectStageElapsed(
	const FString& ChangeId,
	const FString& Stage,
	const FString& Detail)
{
	EmitRejectTimingSample(RejectTimingModel.RecordStage(
		ChangeId,
		Stage,
		FPlatformTime::Seconds(),
		Detail));
}

void SBlueprintHelperReviewPanel::RecordRejectStoreEventTiming(
	const FBlueprintHelperReviewStoreChangedEvent& Event,
	const FString& Stage,
	const FString& Detail,
	bool bCompleteMatches)
{
	for (const FBlueprintHelperReviewRejectTimingSample& Sample :
		RejectTimingModel.RecordMatchingStoreEvent(
			Event,
			Stage,
			FPlatformTime::Seconds(),
			Detail,
			bCompleteMatches))
	{
		EmitRejectTimingSample(Sample);
	}
}

void SBlueprintHelperReviewPanel::StartNextRejectPrepare()
{
	if (bAsyncRejectPrepareActive)
	{
		return;
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
					FString::Printf(TEXT("Reject failed: Review change no longer exists (%s)."), *ChangeId),
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
		FBlueprintHelperReviewPanelStateService::SetTransientActionState(
			ReviewPanelState,
			ChangeId,
			EBlueprintHelperReviewActionIntentKind::Reject,
			EBlueprintHelperReviewChangeStatus::NeedsAction,
			TEXT("reject_preparing_rollback_journal"));
		RefreshReviewActionQueueState(TEXT("reject_preparing"), Item->AssetPath);
		RecordRejectStageElapsed(ChangeId, TEXT("prepare_started"));

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
		return;
	}
}

void SBlueprintHelperReviewPanel::HandlePreparedRejectReady(
	const FString& ChangeId,
	const FBlueprintHelperReviewRejectOptions& PreparedOptions)
{
	bAsyncRejectPrepareActive = false;
	PreparedRejectOptionsByChangeId.Add(ChangeId, PreparedOptions);
	RecordRejectStageElapsed(ChangeId, TEXT("prepare_finished"));
	if (FReviewChangeItem Item = FindChangeItemById(ChangeId))
	{
		FBlueprintHelperReviewPanelStateService::SetTransientActionState(
			ReviewPanelState,
			ChangeId,
			EBlueprintHelperReviewActionIntentKind::Reject,
			EBlueprintHelperReviewChangeStatus::NeedsAction,
			TEXT("reject_mutation_scheduled"));
		RefreshReviewActionQueueState(TEXT("reject_mutation_scheduled"), Item->AssetPath);
		if (!RejectBatchKeyByChangeId.Contains(ChangeId))
		{
			ShowReviewActionNotification(
				TEXT("reject:") + ChangeId,
				FString::Printf(
					TEXT("Applying reject: %s"),
					*BuildReviewActionNotificationLabel(Item)),
				EReviewActionNotificationState::Pending,
				false,
				true);
		}
	}
	ExecutePreparedRejectMutation(ChangeId);
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
				FString::Printf(TEXT("Reject failed: Review change no longer exists (%s)."), *ChangeId),
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
	FBlueprintHelperReviewPanelStateService::SetTransientActionState(
		ReviewPanelState,
		ChangeId,
		EBlueprintHelperReviewActionIntentKind::Reject,
		EBlueprintHelperReviewChangeStatus::NeedsAction,
		TEXT("reject_mutating_single_frame"));
	RefreshReviewActionQueueState(TEXT("reject_mutating"), Item->AssetPath);
	RecordRejectStageElapsed(ChangeId, TEXT("mutation_started"));
	AddDebugMessage(FString::Printf(
		TEXT("Reject mutation started id=%s mode=archive_baseline_snapshot"),
		*ChangeId));

	const FString ActionNotificationLabel = BuildReviewActionNotificationLabel(Item);

	if (Item->bIsAssetLifecycleRoot)
	{
		const FBlueprintHelperReviewActionIntent Intent = FBlueprintHelperReviewActionIntent::Reject(
			FBlueprintHelperReviewPanelStateService::MakeChangeBinding(
				*Item,
				EBlueprintHelperReviewSurface::Unknown,
				Item->LocationKey),
			TEXT("review_panel"));
		RejectTimingModel.MarkWaitingForStoreRefresh(ChangeId);
		const FBlueprintHelperReviewPanelPresenterEvent PresenterEvent =
			ReviewPanelPresenter.IsValid()
				? ReviewPanelPresenter->HandleActionIntent(
					Intent,
					BuildPendingChangeSnapshot(),
					Options)
				: FBlueprintHelperReviewPanelPresenterEvent::FromCascadeActionResult(
					FBlueprintHelperReviewCascadeActionResult());
		const FBlueprintHelperReviewCascadeActionResult& CascadeResult =
			PresenterEvent.CascadeActionResult;

		if (CascadeResult.RootResult.bSucceeded)
		{
			AddDebugMessage(FString::Printf(
				TEXT("Reject lifecycle root id=%s result=waiting_for_store_refresh"),
				*Item->ChangeId));
		}
		else
		{
			RejectTimingModel.CancelWaitingForStoreRefresh(ChangeId);
			FBlueprintHelperReviewPanelStateService::SetPresenterErrorState(
				ReviewPanelState,
				Item->ChangeId,
				CascadeResult.RootResult.NewStatus,
				CascadeResult.RootResult.Message);
		}

		if (!CascadeResult.RootResult.bSucceeded)
		{
			RefreshReviewUiAfterStateChanged(TEXT("reject_lifecycle_root_failed"), Item->AssetPath);
		}
		RecordRejectStageElapsed(ChangeId, TEXT("mutation_finished"));
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
				(CascadeResult.RootResult.bSucceeded
					? FString::Printf(TEXT("Rejected: %s"), *ActionNotificationLabel)
					: BlueprintHelperReviewFriendlyActionMessage(TEXT("Reject failed"), CascadeResult.RootResult.Message)),
				CascadeResult.RootResult.bSucceeded
					? EReviewActionNotificationState::Success
					: EReviewActionNotificationState::Fail,
				true,
				false);
		}
		RecordRejectStageElapsed(
			ChangeId,
			CascadeResult.RootResult.bSucceeded ? TEXT("success_feedback_shown") : TEXT("failure_feedback_shown"),
			CascadeResult.RootResult.Message);
		RecordRejectBatchResult(ChangeId, CascadeResult.RootResult.bSucceeded);
		FinishAsyncReject(ChangeId);
		return;
	}

	const FBlueprintHelperReviewActionIntent Intent = FBlueprintHelperReviewActionIntent::Reject(
		FBlueprintHelperReviewPanelStateService::MakeChangeBinding(
			*Item,
			EBlueprintHelperReviewSurface::Unknown,
			Item->LocationKey),
		TEXT("review_panel"));
	RejectTimingModel.MarkWaitingForStoreRefresh(ChangeId);
	const FBlueprintHelperReviewPanelPresenterEvent PresenterEvent =
		ReviewPanelPresenter.IsValid()
			? ReviewPanelPresenter->HandleActionIntent(
				Intent,
				{ *Item },
				Options)
			: FBlueprintHelperReviewPanelPresenterEvent::FromActionResult(
				FBlueprintHelperReviewActionResult());
	const FBlueprintHelperReviewActionResult& Result = PresenterEvent.ActionResult;

	if (Result.bSucceeded)
	{
		AddDebugMessage(FString::Printf(
			TEXT("Reject change id=%s result=waiting_for_store_refresh"),
			*Item->ChangeId));
	}
	else
	{
		RejectTimingModel.CancelWaitingForStoreRefresh(ChangeId);
		FBlueprintHelperReviewPanelStateService::SetPresenterErrorState(
			ReviewPanelState,
			Item->ChangeId,
			Result.NewStatus,
			Result.Message);
	}
	if (!Result.bSucceeded)
	{
		RefreshReviewUiAfterStateChanged(TEXT("reject_change_failed"), Item->AssetPath);
	}

	RecordRejectStageElapsed(ChangeId, TEXT("mutation_finished"));
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
			(Result.bSucceeded
				? FString::Printf(TEXT("Rejected: %s"), *ActionNotificationLabel)
				: BlueprintHelperReviewFriendlyActionMessage(TEXT("Reject failed"), Result.Message)),
			Result.bSucceeded
				? EReviewActionNotificationState::Success
				: EReviewActionNotificationState::Fail,
			true,
			false);
	}
	RecordRejectStageElapsed(
		ChangeId,
		Result.bSucceeded ? TEXT("success_feedback_shown") : TEXT("failure_feedback_shown"),
		Result.Message);
	RecordRejectBatchResult(ChangeId, Result.bSucceeded);
	FinishAsyncReject(ChangeId);
}

void SBlueprintHelperReviewPanel::FinishAsyncReject(const FString& ChangeId)
{
	RecordRejectStageElapsed(ChangeId, TEXT("finished"));
	FBlueprintHelperReviewPanelStateService::ClearTransientActionState(ReviewPanelState, ChangeId);
	PreparedRejectOptionsByChangeId.Remove(ChangeId);
	if (!RejectTimingModel.IsWaitingForStoreRefresh(ChangeId))
	{
		RejectTimingModel.Complete(ChangeId);
	}
	if (ActiveRejectChangeId == ChangeId)
	{
		ActiveRejectChangeId.Reset();
	}
	bAsyncRejectPrepareActive = false;
	if (PendingRejectChangeIds.Num() > 0)
	{
		StartNextRejectPrepare();
	}
}


FReply SBlueprintHelperReviewPanel::OnAcceptAll()
{
	FString AssetPath;
	if (SelectedChange.IsValid())
	{
		AssetPath = SelectedChange->AssetPath;
	}

	const FBlueprintHelperReviewCommandBatchResult BatchResult =
		ReviewPanelPresenter.IsValid()
			? ReviewPanelPresenter->AcceptPendingVisibleChangesForAsset(AssetPath)
			: FBlueprintHelperReviewCommandBatchResult();
	const int32 TargetCount = BatchResult.BatchActionResult.RequestedCount;
	const int32 FailedCount = BatchResult.BatchActionResult.FailedCount;
	const int32 AcceptedCount = BatchResult.BatchActionResult.SucceededCount;

	AddDebugMessage(FString::Printf(
		TEXT("AcceptAll asset=\"%s\" accepted=%d failed=%d result=waiting_for_store_refresh"),
		*AssetPath,
		AcceptedCount,
		FailedCount));

	const FString NotificationText = TargetCount == 0
		? FString(TEXT("Accept all failed: no acceptable review item."))
		: FailedCount == 0 && AcceptedCount > 0
			? FString::Printf(TEXT("Accept all succeeded: %d target(s)."), AcceptedCount)
			: AcceptedCount > 0
				? FString::Printf(TEXT("Accept all partially succeeded: %d accepted, %d failed."), AcceptedCount, FailedCount)
				: FString::Printf(TEXT("Accept all failed: %d item(s) failed."), FailedCount);
	ShowReviewActionNotification(
		TEXT("accept_all:") + AssetPath,
		NotificationText,
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

	const FString BatchKey = FString::Printf(
		TEXT("reject_all:%s:%lld"),
		*AssetPath,
		FDateTime::UtcNow().GetTicks());

	const FBlueprintHelperReviewCommandBatchResult BatchResult =
		ReviewPanelPresenter.IsValid()
			? ReviewPanelPresenter->RejectPendingVisibleChangesForAsset(AssetPath)
			: FBlueprintHelperReviewCommandBatchResult();
	AddDebugMessage(FString::Printf(
		TEXT("RejectAll batch asset=\"%s\" requested=%d rejected=%d failed=%d"),
		*AssetPath,
		BatchResult.BatchActionResult.RequestedCount,
		BatchResult.BatchActionResult.SucceededCount,
		BatchResult.BatchActionResult.FailedCount));
	ShowReviewActionNotification(
		BatchKey,
		BatchResult.BatchActionResult.RequestedCount == 0
			? FString(TEXT("Reject all failed: no rejectable review item."))
			: BatchResult.BatchActionResult.bSucceeded
			? FString::Printf(TEXT("Reject all succeeded: %d target(s)."), BatchResult.BatchActionResult.SucceededCount)
			: FString::Printf(TEXT("Reject all failed: %s"), *BatchResult.BatchActionResult.Message),
		BatchResult.BatchActionResult.bSucceeded
			? EReviewActionNotificationState::Success
			: EReviewActionNotificationState::Fail,
		true,
		false);
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

static FString BlueprintHelperReviewFriendlyActionMessage(const FString& Prefix, const FString& Detail)
{
	if (Detail.StartsWith(TEXT("struct_field_last_row_cannot_remove:")))
	{
		const FString FieldName = Detail.Mid(FString(TEXT("struct_field_last_row_cannot_remove:")).Len());
		return FString::Printf(TEXT("%s: cannot remove the last struct field (%s). Reject the whole struct asset instead."), *Prefix, *FieldName);
	}
	if (Detail.IsEmpty())
	{
		return Prefix;
	}
	return FString::Printf(TEXT("%s: %s"), *Prefix, *Detail);
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
	return Item->LatestEvidenceId.IsEmpty() ? TEXT("unknown change") : Item->LatestEvidenceId;
}

EBlueprintHelperReviewChangeStatus SBlueprintHelperReviewPanel::GetEffectiveChangeStatus(FReviewChangeItem Item) const
{
	if (!Item.IsValid())
	{
		return EBlueprintHelperReviewChangeStatus::Pending;
	}

	const FString ChangeId = !Item->ChangeId.IsEmpty() ? Item->ChangeId : Item->LatestEvidenceId;
	if (const FBlueprintHelperReviewTransientActionState* TransientState =
		ReviewPanelState.TransientActionStatesByChangeId.Find(ChangeId))
	{
		return TransientState->Status;
	}
	if (const FBlueprintHelperReviewPresenterErrorState* ErrorState =
		ReviewPanelState.PresenterErrorStatesByChangeId.Find(ChangeId))
	{
		return ErrorState->Status;
	}
	return Item->Status;
}

FString SBlueprintHelperReviewPanel::GetEffectiveNeedsActionReason(FReviewChangeItem Item) const
{
	if (!Item.IsValid())
	{
		return FString();
	}

	const FString ChangeId = !Item->ChangeId.IsEmpty() ? Item->ChangeId : Item->LatestEvidenceId;
	if (const FBlueprintHelperReviewTransientActionState* TransientState =
		ReviewPanelState.TransientActionStatesByChangeId.Find(ChangeId))
	{
		return TransientState->Message;
	}
	if (const FBlueprintHelperReviewPresenterErrorState* ErrorState =
		ReviewPanelState.PresenterErrorStatesByChangeId.Find(ChangeId))
	{
		return ErrorState->Message;
	}
	return Item->NeedsActionReason;
}

void SBlueprintHelperReviewPanel::RecordRejectBatchResult(const FString& ChangeId, bool bSucceeded)
{
	const FString* BatchKeyPtr = RejectBatchKeyByChangeId.Find(ChangeId);
	if (!BatchKeyPtr)
	{
		return;
	}

	const FString BatchKey = *BatchKeyPtr;
	RejectBatchKeyByChangeId.Remove(ChangeId);
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
				TEXT("Reject all running: %d/%d item(s)."),
				Batch->FinishedCount,
				Batch->TotalCount),
			EReviewActionNotificationState::Pending,
			false,
			true);
		return;
	}

	const FString FinalText = Batch->FailedCount == 0
		? FString::Printf(TEXT("Reject all succeeded: %d item(s)."), Batch->SuccessCount)
		: Batch->SuccessCount > 0
			? FString::Printf(TEXT("Reject all partially succeeded: %d rejected, %d failed."), Batch->SuccessCount, Batch->FailedCount)
			: FString::Printf(TEXT("Reject all failed: %d item(s) failed."), Batch->FailedCount);
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
		? FText::Format(
			FText::FromString(TEXT("Status: {0}")),
			FBlueprintHelperReviewPanelStyle::StatusToText(GetEffectiveChangeStatus(SelectedChange)))
		: FText::GetEmpty();
}

FText SBlueprintHelperReviewPanel::GetSelectedEvidenceChain() const
{
	if (!SelectedChange.IsValid())
	{
		return FText::GetEmpty();
	}

	return FText::FromString(FString::Printf(
		TEXT("Latest: %s\nSources: %s\nNeeds action: %s"),
		*SelectedChange->LatestEvidenceId,
		*FString::Join(SelectedChange->SourceEvidenceIds, TEXT(", ")),
		*GetEffectiveNeedsActionReason(SelectedChange)));
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
	FlashAlpha = FMath::Max(0.0f, FlashAlpha - InDeltaTime * ReviewPanelSettings.FlashTickDecay);
	Invalidate(EInvalidateWidgetReason::Paint);
	return FlashAlpha > 0.0f ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
}

void SBlueprintHelperReviewPanel::StartFlash()
{
	FlashAlpha = 1.0f;
	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SBlueprintHelperReviewPanel::TickFlash));
}

void SBlueprintHelperReviewPanel::RefreshFromReviewStoreIfChanged(
	const FBlueprintHelperReviewStoreChangedEvent& Event)
{
	RecordRejectStoreEventTiming(
		Event,
		TEXT("store_changed_event"),
		TEXT("review_store_notify"),
		false);
	RequestPendingReviewLoad(TEXT("store_changed"), Event);
}

void SBlueprintHelperReviewPanel::RequestPendingReviewLoad(
	const FString& Reason,
	const FBlueprintHelperReviewStoreChangedEvent& SourceEvent)
{
	const FBlueprintHelperReviewStoreChangedEvent NormalizedEvent = !SourceEvent.bRequiresFullReload
		&& SourceEvent.ChangeIds.Num() == 0
		&& SourceEvent.AssetPaths.Num() == 0
			? FBlueprintHelperReviewStoreChangedEvent::FullReload()
			: SourceEvent;
	const bool bFullReload = NormalizedEvent.bRequiresFullReload
		|| (NormalizedEvent.ChangeIds.Num() == 0 && NormalizedEvent.AssetPaths.Num() == 0);
	RequestPendingReviewPage(
		Reason,
		bFullReload
			? EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage
			: EBlueprintHelperReviewPendingLoadMode::RefreshChanged,
		NormalizedEvent);
}

void SBlueprintHelperReviewPanel::RequestPendingReviewPage(
	const FString& Reason,
	EBlueprintHelperReviewPendingLoadMode Mode,
	const FBlueprintHelperReviewStoreChangedEvent& SourceEvent)
{
	if (!PendingLoadCoordinator.IsValid())
	{
		return;
	}
	if (PagedChangeModel.IsPageRequestInFlight())
	{
		if (Mode == EBlueprintHelperReviewPendingLoadMode::AppendNextPage)
		{
			return;
		}
		PendingLoadCoordinator->CancelPendingLoads();
		PagedChangeModel.MarkPageRequestFinished();
	}

	FBlueprintHelperReviewPendingLoadRequest Request;
	Request.Source = Reason;
	Request.SourceEvent = SourceEvent;
	Request.Mode = Mode;
	Request.PageSize = PendingPageSize;
	Request.Cursor = Mode == EBlueprintHelperReviewPendingLoadMode::AppendNextPage
		? PagedChangeModel.GetNextCursor()
		: FBlueprintHelperReviewPendingIndexPageCursor();

	PagedChangeModel.MarkPageRequestStarted();
	PendingPageRequestId = PendingLoadCoordinator->RequestLoad(
		Request,
		FBlueprintHelperReviewPendingLoadCompleted::CreateSP(
			this,
			&SBlueprintHelperReviewPanel::HandlePendingReviewLoadCompleted));
	if (Reason == TEXT("store_changed"))
	{
		RecordRejectStoreEventTiming(
			SourceEvent,
			TEXT("pending_load_requested"),
			Reason,
			false);
	}
	AddDebugMessage(FString::Printf(
		TEXT("Review pending load requested reason=%s mode=%d page_size=%d"),
		*Reason,
		static_cast<int32>(Mode),
		PendingPageSize));
}

void SBlueprintHelperReviewPanel::OnChangeTreeScrolled(double ScrollOffset)
{
	if (!ChangeTreeView.IsValid())
	{
		return;
	}

	const int32 GeneratedRows = ChangeTreeView->GetNumGeneratedChildren();
	const int32 LoadedRows = CountLoadedChangeTreeRows();
	if (PagedChangeModel.ShouldRequestNextPage(
		ScrollOffset,
		GeneratedRows,
		LoadedRows,
		PendingScrollPrefetchRows))
	{
		RequestPendingReviewPage(
			TEXT("scroll_append"),
			EBlueprintHelperReviewPendingLoadMode::AppendNextPage);
	}
}

int32 SBlueprintHelperReviewPanel::CountLoadedChangeTreeRows() const
{
	int32 Count = 0;
	TArray<FReviewTreeItemPtr> Stack = ChangeTreeRootItems;
	while (Stack.Num() > 0)
	{
		FReviewTreeItemPtr Item = FBlueprintHelperVersionCompat::PopNoShrink(Stack);
		if (!Item.IsValid())
		{
			continue;
		}
		++Count;
		for (const FReviewTreeItemPtr& Child : Item->Children)
		{
			Stack.Add(Child);
		}
	}
	return Count;
}

FText SBlueprintHelperReviewPanel::GetPendingPageStatusText() const
{
	const int32 Loaded = PagedChangeModel.GetLoadedChanges().Num();
	const int32 Total = PagedChangeModel.GetTotalMatchingCount();
	if (PagedChangeModel.IsPageRequestInFlight())
	{
		return FText::FromString(FString::Printf(TEXT("正在加载 %d / %d"), Loaded, Total));
	}
	if (PagedChangeModel.HasMorePages())
	{
		return FText::FromString(FString::Printf(TEXT("已加载 %d / %d，滚动到底部继续加载"), Loaded, Total));
	}
	return FText::FromString(FString::Printf(TEXT("已加载 %d / %d"), Loaded, Total));
}

FReply SBlueprintHelperReviewPanel::OnLoadMorePendingChanges()
{
	RequestPendingReviewPage(
		TEXT("manual_load_more"),
		EBlueprintHelperReviewPendingLoadMode::AppendNextPage);
	return FReply::Handled();
}

void SBlueprintHelperReviewPanel::HandlePendingReviewLoadCompleted(
	const FBlueprintHelperReviewPendingLoadResult& Result)
{
	if (PendingPageRequestId != 0 && Result.RequestId != PendingPageRequestId)
	{
		return;
	}
	PagedChangeModel.MarkPageRequestFinished();
	if (Result.bDiscarded)
	{
		RecordRejectStoreEventTiming(
			Result.SourceEvent,
			TEXT("pending_load_discarded"),
			Result.Source,
			false);
		return;
	}
	if (!Result.bSucceeded)
	{
		RecordRejectStoreEventTiming(
			Result.SourceEvent,
			TEXT("pending_load_failed"),
			Result.Source,
			true);
		AddDebugMessage(FString::Printf(
			TEXT("Review pending load failed reason=%s error=%s"),
			*Result.Source,
			*Result.Error));
		return;
	}
	if (OnValidityCandidatesReady.IsBound() && Result.ValidityCandidates.Num() > 0)
	{
		OnValidityCandidatesReady.Execute(Result.Source, Result.ValidityCandidates);
	}
	ensureMsgf(
		Result.Mode != EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage
			|| Result.Changes.Num() <= PendingPageSize,
		TEXT("ReviewPanel reset pending load returned more rows than page size."));

	PagedChangeModel.ApplyPendingLoadResult(Result);
	const TArray<FBlueprintHelperReviewVisibleChange>& NextChanges =
		PagedChangeModel.GetLoadedChanges();
	const FString LatestSignature = BuildVisibleChangeRefreshSignature(NextChanges);
	if (LatestSignature == LastVisibleChangeRefreshSignature)
	{
		Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
		RecordRejectStoreEventTiming(
			Result.SourceEvent,
			TEXT("panel_refresh_no_change"),
			Result.Source,
			true);
		return;
	}

	const FString PreviousSelectedChangeId = SelectedChange.IsValid() ? SelectedChange->ChangeId : FString();
	const FString PreviousSelectedAssetPath = SelectedChange.IsValid() ? SelectedChange->AssetPath : FString();
	int32 PreviousSelectedIndex = INDEX_NONE;
	for (int32 Index = 0; Index < ChangeItems.Num(); ++Index)
	{
		if (ChangeItems[Index].IsValid() && ChangeItems[Index]->ChangeId == PreviousSelectedChangeId)
		{
			PreviousSelectedIndex = Index;
			break;
		}
	}

	ApplyVisibleChangesFromPendingLoad(Result, NextChanges);
	LastVisibleChangeRefreshSignature = LatestSignature;
	SelectedChange = FindChangeItemById(PreviousSelectedChangeId);
	if (!SelectedChange.IsValid())
	{
		SelectNextChangeAfterRemoval(
			PreviousSelectedAssetPath,
			PreviousSelectedIndex == INDEX_NONE ? 0 : PreviousSelectedIndex);
	}

	const bool bSelectionChanged = SelectedChange.IsValid()
		? SelectedChange->ChangeId != PreviousSelectedChangeId
		: !PreviousSelectedChangeId.IsEmpty();
	const bool bSelectedChangeRefreshed =
		Result.Mode == EBlueprintHelperReviewPendingLoadMode::RefreshChanged
		&& SelectedChange.IsValid()
		&& SelectedChange->ChangeId == PreviousSelectedChangeId
		&& FBlueprintHelperReviewPagedChangeModel::PendingLoadResultContainsChange(
			Result,
			PreviousSelectedChangeId);

	RefreshChangeTreeWidget();
	if (bSelectionChanged || bSelectedChangeRefreshed)
	{
		LoadReviewAssetFromSelection();
		RefreshMainWorkspaceAfterReviewStateChanged();
	}
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	RecordRejectStoreEventTiming(
		Result.SourceEvent,
		TEXT("panel_refresh_applied"),
		Result.Source,
		true);
	AddDebugMessage(FString::Printf(
		TEXT("Review store refreshed dynamically source=%s loaded=%d total=%d has_more=%d"),
		*Result.Source,
		NextChanges.Num(),
		PagedChangeModel.GetTotalMatchingCount(),
		PagedChangeModel.HasMorePages() ? 1 : 0));
}

void SBlueprintHelperReviewPanel::ApplyVisibleChangesFromPendingLoad(
	const FBlueprintHelperReviewPendingLoadResult& /*Result*/,
	const TArray<FBlueprintHelperReviewVisibleChange>& NextChanges)
{
	RefreshVisibleChanges(NextChanges);
}

void SBlueprintHelperReviewPanel::RefreshDiffStackWidgets()
{
	if (SurfaceViewCoordinator.RefreshAllOverlays())
	{
		Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	}
}

void SBlueprintHelperReviewPanel::RefreshMainWorkspaceAfterReviewStateChanged()
{
	if (ComponentsContentBox.IsValid())
	{
		ComponentsContentBox->SetContent(BuildReadonlyComponentsWidget());
	}
	if (MyBlueprintContentBox.IsValid())
	{
		MyBlueprintContentBox->SetContent(BuildReadonlyMyBlueprintWidget());
	}
	if (GraphEditorBox.IsValid())
	{
		GraphEditorBox->SetContent(BuildMainWorkspaceWidget());
	}
	RefreshDiffStackWidgets();
	UpdateDetailsSelection();
}

void SBlueprintHelperReviewPanel::RefreshReviewUiAfterStateChanged(
	const FString& Reason,
	const FString& PreferredAssetPath)
{
	RebuildReviewPanelStatePreservingTransient();
	RebuildChangeTreeItems();
	RefreshChangeTreeWidget();
	LoadReviewAssetFromSelection();
	RefreshMainWorkspaceAfterReviewStateChanged();
	SyncReviewRowHighlightStates(PreferredAssetPath);
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	AddDebugMessage(FString::Printf(
		TEXT("ReviewUiRefresh reason=%s preferredAsset=\"%s\" selected=\"%s\" pending=%d"),
		*Reason,
		*PreferredAssetPath,
		SelectedChange.IsValid() ? *SelectedChange->ChangeId : TEXT(""),
		ChangeItems.Num()));
}

void SBlueprintHelperReviewPanel::RefreshReviewActionQueueState(
	const FString& Reason,
	const FString& PreferredAssetPath)
{
	(void)Reason;
	(void)PreferredAssetPath;
	RebuildReviewPanelStatePreservingTransient();
	RebuildChangeTreeItems();
	RefreshChangeTreeWidget();
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

void SBlueprintHelperReviewPanel::RebuildReviewPanelStatePreservingTransient()
{
	TMap<FString, FBlueprintHelperReviewTransientActionState> TransientStates =
		MoveTemp(ReviewPanelState.TransientActionStatesByChangeId);
	TMap<FString, FBlueprintHelperReviewPresenterErrorState> ErrorStates =
		MoveTemp(ReviewPanelState.PresenterErrorStatesByChangeId);
	ReviewPanelState = FBlueprintHelperReviewPanelStateService::BuildPanelState(ChangeItems, SelectedChange);
	ReviewPanelState.TransientActionStatesByChangeId = MoveTemp(TransientStates);
	ReviewPanelState.PresenterErrorStatesByChangeId = MoveTemp(ErrorStates);
}

void SBlueprintHelperReviewPanel::SyncReviewRowHighlightStates(const FString& PreferredAssetPath)
{
	FBlueprintHelperReviewPanelSurfacePresenterArgs Args;
	Args.AssetContext = &ReviewAssetContext;
	Args.ChangeItems = &ChangeItems;
	Args.SelectedChange = SelectedChange;
	Args.AddDebugMessage = [this](const FString& Message)
	{
		AddDebugMessage(Message);
	};
	Args.OnReviewActionIntent = [this](const FBlueprintHelperReviewActionIntent& Intent)
	{
		return OnReviewActionIntent(Intent);
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
	Args.ReviewPanelSettings = ReviewPanelSettings;

	FBlueprintHelperReviewRowHighlightModel::RebuildSurfaceState(
		Args,
		EBlueprintHelperReviewSurface::Components,
		&FBlueprintHelperReviewBlueprintComponentsPresenter::ShouldShowChange,
		PreferredAssetPath);
	FBlueprintHelperReviewRowHighlightModel::RebuildSurfaceState(
		Args,
		EBlueprintHelperReviewSurface::UMGWidgetTree,
		&FBlueprintHelperReviewUMGWidgetTreePresenter::ShouldShowChange,
		PreferredAssetPath);
	FBlueprintHelperReviewRowHighlightModel::RebuildSurfaceState(
		Args,
		EBlueprintHelperReviewSurface::MyBlueprint,
		&FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange,
		PreferredAssetPath);
	FBlueprintHelperReviewRowHighlightModel::RebuildSurfaceState(
		Args,
		EBlueprintHelperReviewSurface::Details,
		&FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange,
		PreferredAssetPath);
	FBlueprintHelperReviewRowHighlightModel::RebuildSurfaceState(
		Args,
		EBlueprintHelperReviewSurface::DataTable,
		&FBlueprintHelperReviewDataTablePresenter::ShouldShowChange,
		PreferredAssetPath);
	FBlueprintHelperReviewRowHighlightModel::RebuildSurfaceState(
		Args,
		EBlueprintHelperReviewSurface::DataAsset,
		&FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange,
		PreferredAssetPath);
	FBlueprintHelperReviewRowHighlightModel::RebuildSurfaceState(
		Args,
		EBlueprintHelperReviewSurface::Material,
		&FBlueprintHelperReviewMaterialPresenter::ShouldShowChange,
		PreferredAssetPath);
}

void SBlueprintHelperReviewPanel::ConfigureSurfaceViewCoordinator()
{
	SurfaceViewCoordinator.Reset();

	const TFunction<bool()> StructureOverlayRefresh = [this]()
	{
		if (!ComponentsDiffStackBox.IsValid())
		{
			return false;
		}
		ComponentsDiffStackBox->SetContent(BuildStructurePanelDiffFrames());
		return true;
	};

	TSharedRef<FBlueprintHelperReviewSurfaceView> ComponentsView =
		MakeShared<FBlueprintHelperReviewSurfaceView>(EBlueprintHelperReviewSurface::Components);
	ComponentsView->SetOverlayRefresh(StructureOverlayRefresh);
	ComponentsView->SetRowsRefresh([this]()
	{
		bool bHandled = false;
		if (ComponentsPresenterState.ComponentsPanel.IsValid())
		{
			BlueprintHelperReviewInvalidateComponentRows(ComponentsPresenterState.ComponentsPanel->GetRootItems());
			ComponentsPresenterState.ComponentsPanel->RequestRowsRefresh();
			bHandled = true;
		}
		if (ComponentsContentBox.IsValid())
		{
			ComponentsContentBox->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
			bHandled = true;
		}
		return bHandled;
	});
	SurfaceViewCoordinator.RegisterSurface(ComponentsView);

	TSharedRef<FBlueprintHelperReviewSurfaceView> WidgetTreeView =
		MakeShared<FBlueprintHelperReviewSurfaceView>(EBlueprintHelperReviewSurface::UMGWidgetTree);
	WidgetTreeView->SetOverlayRefresh(StructureOverlayRefresh);
	WidgetTreeView->SetRowsRefresh([this]()
	{
		bool bHandled = false;
		BlueprintHelperReviewInvalidateWidgetTreeRows(WidgetTreePresenterState.RootItems);
		if (WidgetTreePresenterState.TreeView.IsValid())
		{
			WidgetTreePresenterState.TreeView->RequestTreeRefresh();
			bHandled = true;
		}
		if (ComponentsContentBox.IsValid())
		{
			ComponentsContentBox->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
			bHandled = true;
		}
		return bHandled;
	});
	SurfaceViewCoordinator.RegisterSurface(WidgetTreeView);

	TSharedRef<FBlueprintHelperReviewSurfaceView> MyBlueprintView =
		MakeShared<FBlueprintHelperReviewSurfaceView>(EBlueprintHelperReviewSurface::MyBlueprint);
	MyBlueprintView->SetOverlayRefresh([this]()
	{
		if (!MyBlueprintDiffStackBox.IsValid())
		{
			return false;
		}
		MyBlueprintDiffStackBox->SetContent(BuildPanelDiffFrames(
			&FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange,
			EBlueprintHelperReviewSurface::MyBlueprint));
		return true;
	});
	MyBlueprintView->SetRowsRefresh([this]()
	{
		bool bHandled = false;
		BlueprintHelperReviewInvalidateMyBlueprintRows(MyBlueprintPresenterState.RootItems);
		if (MyBlueprintPresenterState.TreeView.IsValid())
		{
			MyBlueprintPresenterState.TreeView->RequestTreeRefresh();
			bHandled = true;
		}
		if (MyBlueprintContentBox.IsValid())
		{
			MyBlueprintContentBox->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
			bHandled = true;
		}
		return bHandled;
	});
	SurfaceViewCoordinator.RegisterSurface(MyBlueprintView);

	TSharedRef<FBlueprintHelperReviewSurfaceView> DetailsView =
		MakeShared<FBlueprintHelperReviewSurfaceView>(EBlueprintHelperReviewSurface::Details);
	DetailsView->SetOverlayRefresh([this]()
	{
		if (!DetailsDiffStackBox.IsValid())
		{
			return false;
		}
		DetailsDiffStackBox->SetContent(BuildDetailsPanelDiffFrames());
		return true;
	});
	DetailsView->SetRowsRefresh([this]()
	{
		bool bHandled = false;
		if (KismetInspector.IsValid())
		{
			if (TSharedPtr<IDetailsView> PropertyView = KismetInspector->GetPropertyView())
			{
				PropertyView->ForceRefresh();
				bHandled = true;
			}
		}
		if (DetailsContentBox.IsValid())
		{
			DetailsContentBox->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
			bHandled = true;
		}
		return bHandled;
	});
	SurfaceViewCoordinator.RegisterSurface(DetailsView);

	const TFunction<bool()> MainWorkspaceOverlayRefresh = [this]()
	{
		if (!MainWorkspaceDiffStackBox.IsValid())
		{
			return false;
		}
		MainWorkspaceDiffStackBox->SetContent(BuildMainWorkspaceDiffFrames());
		return true;
	};

	TSharedRef<FBlueprintHelperReviewSurfaceView> DataTableView =
		MakeShared<FBlueprintHelperReviewSurfaceView>(EBlueprintHelperReviewSurface::DataTable);
	DataTableView->SetOverlayRefresh(MainWorkspaceOverlayRefresh);
	DataTableView->SetRowsRefresh([this]()
	{
		bool bHandled = false;
		BlueprintHelperReviewInvalidateDataAssetRows(DataTablePresenterState.SelectedRowFields);
		if (DataTablePresenterState.ListView.IsValid())
		{
			DataTablePresenterState.ListView->RequestListRefresh();
			bHandled = true;
		}
		if (DataTablePresenterState.SelectedRowFieldListView.IsValid())
		{
			DataTablePresenterState.SelectedRowFieldListView->RequestListRefresh();
			bHandled = true;
		}
		if (GraphEditorBox.IsValid())
		{
			GraphEditorBox->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
			bHandled = true;
		}
		return bHandled;
	});
	SurfaceViewCoordinator.RegisterSurface(DataTableView);

	TSharedRef<FBlueprintHelperReviewSurfaceView> DataAssetView =
		MakeShared<FBlueprintHelperReviewSurfaceView>(EBlueprintHelperReviewSurface::DataAsset);
	DataAssetView->SetOverlayRefresh(MainWorkspaceOverlayRefresh);
	DataAssetView->SetRowsRefresh([this]()
	{
		bool bHandled = false;
		BlueprintHelperReviewInvalidateDataAssetRows(DataAssetPresenterState.Rows);
		if (DataAssetPresenterState.ListView.IsValid())
		{
			DataAssetPresenterState.ListView->RequestListRefresh();
			bHandled = true;
		}
		if (GraphEditorBox.IsValid())
		{
			GraphEditorBox->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
			bHandled = true;
		}
		return bHandled;
	});
	SurfaceViewCoordinator.RegisterSurface(DataAssetView);

	TSharedRef<FBlueprintHelperReviewSurfaceView> MaterialView =
		MakeShared<FBlueprintHelperReviewSurfaceView>(EBlueprintHelperReviewSurface::Material);
	MaterialView->SetOverlayRefresh(MainWorkspaceOverlayRefresh);
	MaterialView->SetRowsRefresh([this]()
	{
		bool bHandled = false;
		BlueprintHelperReviewInvalidateDataAssetRows(MaterialPresenterState.Rows);
		if (MaterialPresenterState.ListView.IsValid())
		{
			MaterialPresenterState.ListView->RequestListRefresh();
			bHandled = true;
		}
		if (GraphEditorBox.IsValid())
		{
			GraphEditorBox->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
			bHandled = true;
		}
		return bHandled;
	});
	SurfaceViewCoordinator.RegisterSurface(MaterialView);
}

void SBlueprintHelperReviewPanel::RefreshSurfaceOverlay(EBlueprintHelperReviewSurface Surface)
{
	if (SurfaceViewCoordinator.RefreshOverlay(Surface))
	{
		Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
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
	ProcessDebugFocusTraversalGeometryEvent();
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
	ProcessDebugFocusTraversalGeometryEvent();
}

void SBlueprintHelperReviewPanel::OnRowHighlightStateChanged(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	uint64 Revision)
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
	(void)Revision;

	const bool bHandled = SurfaceViewCoordinator.RefreshRows(Surface);
	if (bHandled)
	{
		Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
		AddDebugMessage(FString::Printf(
			TEXT("ReviewRowHighlightState surface=%s asset=\"%s\" revision=%llu result=refresh_rows"),
			BlueprintHelperReviewSurfaceToString(Surface),
			*AssetPath,
			Revision));
	}
	ProcessDebugFocusTraversalGeometryEvent();
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
		if (ReviewPanelSettings.bOverlayFilterCurrentAssetOnly
			&& !CurrentAssetPath.IsEmpty()
			&& Item.IsValid()
			&& Item->AssetPath != CurrentAssetPath)
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
			MainWorkspaceDiffStackBox),
		TPair<EBlueprintHelperReviewSurface, TSharedPtr<SWidget>>(
			EBlueprintHelperReviewSurface::Material,
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
#if BLUEPRINTHELPER_UE_HAS_DETAILS_VIEW_SCROLL_PROPERTY_BOOL
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




