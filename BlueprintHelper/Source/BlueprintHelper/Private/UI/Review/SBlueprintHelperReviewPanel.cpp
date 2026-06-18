// BlueprintHelper fake Review panel implementation.

#include "UI/Review/SBlueprintHelperReviewPanel.h"

#include "Async/Async.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/DateTime.h"
#include "IDetailsView.h"
#include "PropertyEditorDelegates.h"
#include "PropertyPath.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Shared/Review/BlueprintHelperReviewStatusUtils.h"
#include "SKismetInspector.h"
#include "UI/Review/BlueprintHelperReviewAcceptMutationApplicationService.h"
#include "UI/Review/BlueprintHelperReviewAcceptMutationPresenter.h"
#include "UI/Review/BlueprintHelperReviewDebugText.h"
#include "UI/Review/BlueprintHelperReviewPanelGeometryUtils.h"
#include "UI/Review/BlueprintHelperReviewPanelStyle.h"
#include "UI/Review/BlueprintHelperReviewPanelPresenter.h"
#include "UI/Review/BlueprintHelperReviewPanelStateService.h"
#include "UI/Review/BlueprintHelperReviewPendingLoadApplicationService.h"
#include "UI/Review/BlueprintHelperReviewRejectMutationApplicationService.h"
#include "UI/Review/BlueprintHelperReviewRejectMutationPresenter.h"
#include "UI/Review/BlueprintHelperReviewAssetPresenters.h"
#include "UI/Review/BlueprintHelperReviewDebugBundleService.h"
#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"
#include "UI/Review/BlueprintHelperReviewSlateRowGeometryRegistry.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenter.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenterRegistry.h"
#include "UI/Review/BlueprintHelperReviewSurfaceProjectionRegistry.h"
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
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_CustomEvent.h"

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

int32 SBlueprintHelperReviewPanel::GetVisibleChangeCountForTesting() const
{
	return ChangeItems.Num();
}

bool SBlueprintHelperReviewPanel::SelectChangeForTesting(const FString& ChangeId)
{
	const FReviewChangeItem Item = FindChangeItemById(ChangeId);
	if (!Item.IsValid())
	{
		return false;
	}

	OnChangeSelectionChanged(Item, ESelectInfo::Direct);
	return true;
}

bool SBlueprintHelperReviewPanel::CaptureFocusDebugBundleForTesting(
	FString& OutBundlePath,
	FString& OutDebugMessages)
{
	OutBundlePath.Reset();
	OutDebugMessages.Reset();

	OnCaptureFocusDebugBundle();
	OutBundlePath = DebugBundlePath;
	OutDebugMessages = BuildDebugMessagesString();
	return !OutBundlePath.IsEmpty();
}

bool SBlueprintHelperReviewPanel::LoadDebugBundleForTesting(
	const FString& InBundlePath,
	FString& OutDebugMessages)
{
	OutDebugMessages.Reset();
	DebugBundlePath = FBlueprintHelperReviewDebugBundleService::NormalizeBundlePath(InBundlePath);
	OnLoadDebugBundle();
	OutDebugMessages = BuildDebugMessagesString();
	return OutDebugMessages.Contains(TEXT("DebugBundle loaded from"));
}

void SBlueprintHelperReviewPanel::RefreshVisibleChangesForTesting(
	const TArray<FBlueprintHelperReviewVisibleChange>& SourceChanges)
{
	RefreshVisibleChanges(SourceChanges);
	RefreshDiffStackWidgets();
}

TArray<FString> SBlueprintHelperReviewPanel::GetSurfaceDiffModelIdsForTesting(
	EBlueprintHelperReviewSurface Surface) const
{
	TArray<FString> ReviewEventIds;
	const FBlueprintHelperReviewSurfaceDiffFrameRoute Route =
		FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveSurfaceRoute(Surface);
	if (!Route.ShouldShowChange)
	{
		return ReviewEventIds;
	}

	const TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel> DiffModels =
		BuildSurfaceDiffModelsForSurface(Route);
	for (const FBlueprintHelperReviewSurfaceDiffProjectionModel& DiffModel : DiffModels)
	{
		ReviewEventIds.AddUnique(DiffModel.ReviewEventId);
	}
	return ReviewEventIds;
}

int32 SBlueprintHelperReviewPanel::GetSurfaceDiffModelCountForTesting(
	EBlueprintHelperReviewSurface Surface) const
{
	return GetSurfaceDiffModelIdsForTesting(Surface).Num();
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
			EnsureRejectWorkflowCoordinator()->BeginTiming(TimingChange, FPlatformTime::Seconds());
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
	const FBlueprintHelperReviewSurfaceDiffFrameRoute StructureRoute =
		FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveStructurePanelRoute(
			ReviewAssetContext.AssetKind);
	if (StructureRoute.Surface == EBlueprintHelperReviewSurface::UMGWidgetTree)
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
	return BuildPanelDiffFrames(
		FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveStructurePanelRoute(
			ReviewAssetContext.AssetKind));
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildMainWorkspaceDiffFrames()
{
	return BuildPanelDiffFrames(
		FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveMainWorkspaceRoute(
			ReviewAssetContext.AssetKind));
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
	const FBlueprintHelperReviewSurfaceDiffFrameRoute& Route)
{
	if (!Route.bShouldBuildOverlay || Route.ShouldShowChange == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel> SurfaceDiffModels =
		BuildSurfaceDiffModelsForSurface(Route);
	FBlueprintHelperReviewPanelSurfacePresenterArgs Args;
	ConfigurePanelSurfacePresenterArgs(Args, SurfaceDiffModels);

	const TSharedRef<FBlueprintHelperReviewSurfacePresenterRegistry> Registry =
		FBlueprintHelperReviewSurfacePresenterRegistry::CreateDefault();
	return Registry->BuildOverlayOrNull(Route.Surface, Args);
}

TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel> SBlueprintHelperReviewPanel::BuildSurfaceDiffModelsForSurface(
	const FBlueprintHelperReviewSurfaceDiffFrameRoute& Route) const
{
	TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel> SurfaceDiffModels;
	const TSharedRef<FBlueprintHelperReviewSurfaceProjectionRegistry> ProjectionRegistry =
		FBlueprintHelperReviewSurfaceProjectionRegistry::CreateDefault();
	const FString AssetKindName = BlueprintHelperReviewAssetKindToString(ReviewAssetContext.AssetKind);
	const FString SurfaceName = BlueprintHelperReviewSurfaceToString(Route.Surface);
	const FString CurrentAssetPath = SelectedChange.IsValid() ? SelectedChange->AssetPath : FString();

	for (const FReviewChangeItem& Item : ChangeItems)
	{
		if (!Item.IsValid() || (Route.ShouldShowChange && !Route.ShouldShowChange(*Item)))
		{
			continue;
		}
		if (!FBlueprintHelperReviewStatusUtils::IsOpenReviewStatus(Item->Status))
		{
			continue;
		}
		if (ReviewPanelSettings.bOverlayFilterCurrentAssetOnly
			&& !CurrentAssetPath.IsEmpty()
			&& Item->AssetPath != CurrentAssetPath)
		{
			continue;
		}

		SurfaceDiffModels.Append(ProjectionRegistry->ProjectVisibleChange(
			*Item,
			AssetKindName,
			SurfaceName));
	}

	return SurfaceDiffModels;
}

void SBlueprintHelperReviewPanel::ConfigurePanelSurfacePresenterArgs(
	FBlueprintHelperReviewPanelSurfacePresenterArgs& Args,
	const TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel>& SurfaceDiffModels)
{
	Args.AssetContext = &ReviewAssetContext;
	Args.ChangeItems = &ChangeItems;
	Args.SurfaceDiffModels = &SurfaceDiffModels;
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
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildDetailsPanelDiffFrames()
{
	return BuildPanelDiffFrames(
		FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveDetailsRoute());
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

	FBlueprintHelperReviewAcceptMutationApplicationCallbacks ApplicationCallbacks;
	ApplicationCallbacks.SetPresenterError = [this, Item](
		EBlueprintHelperReviewChangeStatus Status,
		const FString& Message)
	{
		FBlueprintHelperReviewPanelStateService::SetPresenterErrorState(
			ReviewPanelState,
			Item->ChangeId,
			Status,
			Message);
	};
	ApplicationCallbacks.AddDebugMessage = [this](const FString& Message)
	{
		AddDebugMessage(Message);
	};
	ApplicationCallbacks.ShowNotification = [this](
		const FString& NotificationKey,
		const FString& StatusText,
		EBlueprintHelperReviewActionNotificationState State,
		bool bExpire,
		bool bUseThrobber)
	{
		ShowReviewActionNotification(
			NotificationKey,
			StatusText,
			State,
			bExpire,
			bUseThrobber);
	};
	FBlueprintHelperReviewAcceptMutationApplicationService::ApplyPresentation(
		FBlueprintHelperReviewAcceptMutationPresenter::BuildResult(
			Item->ChangeId,
			Result,
			BuildReviewActionNotificationLabel(Item)),
		ApplicationCallbacks);
	return FReply::Handled();
}

FReply SBlueprintHelperReviewPanel::ExecuteRejectChange(FReviewChangeItem Item)
{
	if (!Item.IsValid())
	{
		return FReply::Handled();
	}

	const FString ChangeId = !Item->ChangeId.IsEmpty() ? Item->ChangeId : Item->LatestEvidenceId;
	if (!ChangeId.IsEmpty() && !EnsureRejectWorkflowCoordinator()->ContainsTiming(ChangeId))
	{
		FBlueprintHelperReviewVisibleChange TimingChange = *Item;
		if (TimingChange.ChangeId.IsEmpty())
		{
			TimingChange.ChangeId = ChangeId;
		}
		EnsureRejectWorkflowCoordinator()->BeginTiming(TimingChange, FPlatformTime::Seconds());
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
				EBlueprintHelperReviewActionNotificationState::Fail,
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
				EBlueprintHelperReviewActionNotificationState::Pending,
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
	EnsureRejectWorkflowCoordinator()->EnqueueReject(ChangeId);
	if (!EnsureRejectWorkflowCoordinator()->ContainsTiming(ChangeId))
	{
		FBlueprintHelperReviewVisibleChange TimingChange = *Item;
		if (TimingChange.ChangeId.IsEmpty())
		{
			TimingChange.ChangeId = ChangeId;
		}
		EnsureRejectWorkflowCoordinator()->BeginTiming(TimingChange, FPlatformTime::Seconds());
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
			EBlueprintHelperReviewActionNotificationState::Pending,
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
	EmitRejectTimingSample(EnsureRejectWorkflowCoordinator()->RecordStage(
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
		EnsureRejectWorkflowCoordinator()->RecordMatchingStoreEvent(
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
	EnsureRejectWorkflowCoordinator()->StartNextPrepare();
}

TSharedRef<FBlueprintHelperReviewRejectWorkflowCoordinator>
SBlueprintHelperReviewPanel::EnsureRejectWorkflowCoordinator()
{
	if (!RejectWorkflowCoordinator.IsValid())
	{
		RejectWorkflowCoordinator = MakeShared<FBlueprintHelperReviewRejectWorkflowCoordinator>();
		TWeakPtr<SBlueprintHelperReviewPanel> WeakPanel =
			StaticCastSharedRef<SBlueprintHelperReviewPanel>(AsShared());

		FBlueprintHelperReviewRejectWorkflowCoordinator::FCallbacks Callbacks;
		Callbacks.ResolveChangeSnapshot = [WeakPanel](const FString& ChangeId)
			-> TOptional<FBlueprintHelperReviewVisibleChange>
		{
			if (TSharedPtr<SBlueprintHelperReviewPanel> Panel = WeakPanel.Pin())
			{
				return Panel->ResolveRejectWorkflowChangeSnapshot(ChangeId);
			}
			return TOptional<FBlueprintHelperReviewVisibleChange>();
		};
		Callbacks.OnChangeMissing = [WeakPanel](const FString& ChangeId)
		{
			if (TSharedPtr<SBlueprintHelperReviewPanel> Panel = WeakPanel.Pin())
			{
				Panel->HandleRejectWorkflowMissingChange(ChangeId);
			}
		};
		Callbacks.OnPrepareStarted = [WeakPanel](
			const FString& ChangeId,
			const FBlueprintHelperReviewVisibleChange& ChangeSnapshot)
		{
			if (TSharedPtr<SBlueprintHelperReviewPanel> Panel = WeakPanel.Pin())
			{
				Panel->HandleRejectWorkflowPrepareStarted(ChangeId, ChangeSnapshot);
			}
		};
		Callbacks.OnPrepareFinished = [WeakPanel](
			const FString& ChangeId,
			const FBlueprintHelperReviewRejectOptions& PreparedOptions)
		{
			if (TSharedPtr<SBlueprintHelperReviewPanel> Panel = WeakPanel.Pin())
			{
				Panel->HandleRejectWorkflowPrepareFinished(ChangeId, PreparedOptions);
			}
		};
		Callbacks.ExecutePreparedMutation = [WeakPanel](const FString& ChangeId)
		{
			if (TSharedPtr<SBlueprintHelperReviewPanel> Panel = WeakPanel.Pin())
			{
				Panel->ExecutePreparedRejectMutation(ChangeId);
			}
		};
		RejectWorkflowCoordinator->SetCallbacks(MoveTemp(Callbacks));
	}

	return RejectWorkflowCoordinator.ToSharedRef();
}

TOptional<FBlueprintHelperReviewVisibleChange>
SBlueprintHelperReviewPanel::ResolveRejectWorkflowChangeSnapshot(const FString& ChangeId) const
{
	if (FReviewChangeItem Item = FindChangeItemById(ChangeId))
	{
		return *Item;
	}
	return TOptional<FBlueprintHelperReviewVisibleChange>();
}

void SBlueprintHelperReviewPanel::HandleRejectWorkflowMissingChange(const FString& ChangeId)
{
	if (!ReviewActionNotificationPresenter.IsChangeInBatch(ChangeId))
	{
		ShowReviewActionNotification(
			TEXT("reject:") + ChangeId,
			FString::Printf(TEXT("Reject failed: Review change no longer exists (%s)."), *ChangeId),
			EBlueprintHelperReviewActionNotificationState::Fail,
			true,
			false);
	}
	RecordRejectBatchResult(ChangeId, false);
	if (EnsureRejectWorkflowCoordinator()->ContainsTiming(ChangeId) && !EnsureRejectWorkflowCoordinator()->IsWaitingForStoreRefresh(ChangeId))
	{
		EnsureRejectWorkflowCoordinator()->CompleteTiming(ChangeId);
	}
}

void SBlueprintHelperReviewPanel::HandleRejectWorkflowPrepareStarted(
	const FString& ChangeId,
	const FBlueprintHelperReviewVisibleChange& ChangeSnapshot)
{
	FBlueprintHelperReviewPanelStateService::SetTransientActionState(
		ReviewPanelState,
		ChangeId,
		EBlueprintHelperReviewActionIntentKind::Reject,
		EBlueprintHelperReviewChangeStatus::NeedsAction,
		TEXT("reject_preparing_rollback_journal"));
	RefreshReviewActionQueueState(TEXT("reject_preparing"), ChangeSnapshot.AssetPath);
	RecordRejectStageElapsed(ChangeId, TEXT("prepare_started"));
}

void SBlueprintHelperReviewPanel::HandleRejectWorkflowPrepareFinished(
	const FString& ChangeId,
	const FBlueprintHelperReviewRejectOptions& PreparedOptions)
{
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
		if (!ReviewActionNotificationPresenter.IsChangeInBatch(ChangeId))
		{
			ShowReviewActionNotification(
				TEXT("reject:") + ChangeId,
				FString::Printf(
					TEXT("Applying reject: %s"),
					*BuildReviewActionNotificationLabel(Item)),
				EBlueprintHelperReviewActionNotificationState::Pending,
				false,
				true);
		}
	}
}

void SBlueprintHelperReviewPanel::HandlePreparedRejectReady(
	const FString& ChangeId,
	const FBlueprintHelperReviewRejectOptions& PreparedOptions)
{
	EnsureRejectWorkflowCoordinator()->HandlePreparedRejectReady(ChangeId, PreparedOptions);
}

void SBlueprintHelperReviewPanel::ExecutePreparedRejectMutation(const FString& ChangeId)
{
	FReviewChangeItem Item = FindChangeItemById(ChangeId);
	const FBlueprintHelperReviewRejectMutationApplicationCallbacks ApplicationCallbacks =
		BuildRejectMutationApplicationCallbacks(ChangeId, Item);
	if (!Item.IsValid())
	{
		FBlueprintHelperReviewRejectMutationApplicationService::ApplyPresentation(
			FBlueprintHelperReviewRejectMutationPresenter::BuildMissingChange(
				ChangeId,
				ReviewActionNotificationPresenter.IsChangeInBatch(ChangeId)),
			false,
			ApplicationCallbacks);
		FBlueprintHelperReviewRejectMutationApplicationService::FinishMutation(ApplicationCallbacks);
		return;
	}

	FBlueprintHelperReviewRejectOptions Options;
	if (const FBlueprintHelperReviewRejectOptions* PreparedOptions =
		EnsureRejectWorkflowCoordinator()->FindPreparedOptions(ChangeId))
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
		EnsureRejectWorkflowCoordinator()->MarkWaitingForStoreRefresh(ChangeId);
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
		RecordRejectStageElapsed(ChangeId, TEXT("mutation_finished"));
		FBlueprintHelperReviewRejectMutationApplicationService::ApplyPresentation(
			FBlueprintHelperReviewRejectMutationPresenter::BuildLifecycleRootResult(
				ChangeId,
				*Item,
				CascadeResult,
				ActionNotificationLabel,
				ReviewActionNotificationPresenter.IsChangeInBatch(ChangeId)),
			true,
			ApplicationCallbacks);
		FBlueprintHelperReviewRejectMutationApplicationService::FinishMutation(ApplicationCallbacks);
		return;
	}

	const FBlueprintHelperReviewActionIntent Intent = FBlueprintHelperReviewActionIntent::Reject(
		FBlueprintHelperReviewPanelStateService::MakeChangeBinding(
			*Item,
			EBlueprintHelperReviewSurface::Unknown,
			Item->LocationKey),
		TEXT("review_panel"));
	EnsureRejectWorkflowCoordinator()->MarkWaitingForStoreRefresh(ChangeId);
	const FBlueprintHelperReviewPanelPresenterEvent PresenterEvent =
		ReviewPanelPresenter.IsValid()
			? ReviewPanelPresenter->HandleActionIntent(
				Intent,
				{ *Item },
				Options)
			: FBlueprintHelperReviewPanelPresenterEvent::FromActionResult(
				FBlueprintHelperReviewActionResult());
	const FBlueprintHelperReviewActionResult& Result = PresenterEvent.ActionResult;

	RecordRejectStageElapsed(ChangeId, TEXT("mutation_finished"));
	FBlueprintHelperReviewRejectMutationApplicationService::ApplyPresentation(
		FBlueprintHelperReviewRejectMutationPresenter::BuildSingleResult(
			ChangeId,
			*Item,
			Result,
			ActionNotificationLabel,
			ReviewActionNotificationPresenter.IsChangeInBatch(ChangeId),
			DebugBundleSessionId,
			SelectedChange),
		true,
		ApplicationCallbacks);
	FBlueprintHelperReviewRejectMutationApplicationService::FinishMutation(ApplicationCallbacks);
}

FBlueprintHelperReviewRejectMutationApplicationCallbacks
SBlueprintHelperReviewPanel::BuildRejectMutationApplicationCallbacks(
	const FString& ChangeId,
	FReviewChangeItem Item)
{
	FBlueprintHelperReviewRejectMutationApplicationCallbacks Callbacks;
	Callbacks.CancelWaitingForStoreRefresh = [this, ChangeId]()
	{
		EnsureRejectWorkflowCoordinator()->CancelWaitingForStoreRefresh(ChangeId);
	};
	Callbacks.SetPresenterError = [this, Item](
		EBlueprintHelperReviewChangeStatus Status,
		const FString& Message)
	{
		if (Item.IsValid())
		{
			FBlueprintHelperReviewPanelStateService::SetPresenterErrorState(
				ReviewPanelState,
				Item->ChangeId,
				Status,
				Message);
		}
	};
	Callbacks.RefreshAfterFailure = [this, Item](const FString& Reason)
	{
		if (Item.IsValid())
		{
			RefreshReviewUiAfterStateChanged(Reason, Item->AssetPath);
		}
	};
	Callbacks.AddDebugMessage = [this](const FString& Message)
	{
		AddDebugMessage(Message);
	};
	Callbacks.AppendDebugBundleEvent = [this](const TSharedRef<FJsonObject>& Event)
	{
		AppendDebugBundleEvent(Event);
	};
	Callbacks.ShowNotification = [this](
		const FString& NotificationKey,
		const FString& StatusText,
		EBlueprintHelperReviewActionNotificationState State,
		bool bExpire,
		bool bUseThrobber)
	{
		ShowReviewActionNotification(
			NotificationKey,
			StatusText,
			State,
			bExpire,
			bUseThrobber);
	};
	Callbacks.RecordFeedbackStage = [this, ChangeId](
		const FString& Stage,
		const FString& Detail)
	{
		RecordRejectStageElapsed(
			ChangeId,
			Stage,
			Detail);
	};
	Callbacks.RecordBatchResult = [this, ChangeId](bool bSucceeded)
	{
		RecordRejectBatchResult(ChangeId, bSucceeded);
	};
	Callbacks.RecordFinishedStage = [this, ChangeId]()
	{
		RecordRejectStageElapsed(ChangeId, TEXT("finished"));
	};
	Callbacks.ClearTransientActionState = [this, ChangeId]()
	{
		FBlueprintHelperReviewPanelStateService::ClearTransientActionState(ReviewPanelState, ChangeId);
	};
	Callbacks.FinishWorkflow = [this, ChangeId]()
	{
		EnsureRejectWorkflowCoordinator()->FinishReject(ChangeId);
	};
	Callbacks.IsWaitingForStoreRefresh = [this, ChangeId]()
	{
		return EnsureRejectWorkflowCoordinator()->IsWaitingForStoreRefresh(ChangeId);
	};
	Callbacks.CompleteTiming = [this, ChangeId]()
	{
		EnsureRejectWorkflowCoordinator()->CompleteTiming(ChangeId);
	};
	return Callbacks;
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
			? EBlueprintHelperReviewActionNotificationState::Success
			: EBlueprintHelperReviewActionNotificationState::Fail,
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
			? EBlueprintHelperReviewActionNotificationState::Success
			: EBlueprintHelperReviewActionNotificationState::Fail,
		true,
		false);
	return FReply::Handled();
}
void SBlueprintHelperReviewPanel::ShowReviewActionNotification(
	const FString& NotificationKey,
	const FString& StatusText,
	EBlueprintHelperReviewActionNotificationState State,
	bool bExpire,
	bool bUseThrobber)
{
	ReviewActionNotificationPresenter.Show(
		NotificationKey,
		StatusText,
		State,
		bExpire,
		bUseThrobber);
}

FString SBlueprintHelperReviewPanel::BuildReviewActionNotificationLabel(FReviewChangeItem Item)
{
	if (!Item.IsValid())
	{
		return FBlueprintHelperReviewActionNotificationPresenter::BuildChangeLabel(nullptr);
	}
	return FBlueprintHelperReviewActionNotificationPresenter::BuildChangeLabel(Item.Get());
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
	ReviewActionNotificationPresenter.RecordBatchResult(ChangeId, bSucceeded);
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

void SBlueprintHelperReviewPanel::StartFlash()
{
	FlashAlpha = 0.0f;
	Invalidate(EInvalidateWidgetReason::Paint);
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
	const FBlueprintHelperReviewStoreChangedEvent NormalizedEvent =
		FBlueprintHelperReviewPendingLoadApplicationService::NormalizeStoreChangedEvent(SourceEvent);
	RequestPendingReviewPage(
		Reason,
		FBlueprintHelperReviewPendingLoadApplicationService::ResolveLoadMode(NormalizedEvent),
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
	const FBlueprintHelperReviewPendingLoadRequestApplication Application =
		FBlueprintHelperReviewPendingLoadApplicationService::BuildRequestApplication(
			Reason,
			Mode,
			SourceEvent,
			PagedChangeModel,
			PendingPageSize);
	if (!Application.DebugMessage.IsEmpty())
	{
		AddDebugMessage(Application.DebugMessage);
	}
	if (!Application.bShouldRequestLoad)
	{
		return;
	}
	if (Application.bShouldCancelPendingLoads)
	{
		PendingLoadCoordinator->CancelPendingLoads();
	}
	if (Application.bShouldFinishInFlightRequest)
	{
		PagedChangeModel.MarkPageRequestFinished();
	}

	PagedChangeModel.MarkPageRequestStarted();
	PendingPageRequestId = PendingLoadCoordinator->RequestLoad(
		Application.Request,
		FBlueprintHelperReviewPendingLoadCompleted::CreateSP(
			this,
			&SBlueprintHelperReviewPanel::HandlePendingReviewLoadCompleted));
	if (Application.bShouldRecordStoreTiming)
	{
		RecordRejectStoreEventTiming(
			Application.Request.SourceEvent,
			TEXT("pending_load_requested"),
			Reason,
			false);
	}
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
		return FText::FromString(FString::Printf(TEXT("濮濓絽婀崝鐘烘祰 %d / %d"), Loaded, Total));
	}
	if (PagedChangeModel.HasMorePages())
	{
		return FText::FromString(FString::Printf(TEXT("瀹告彃濮炴潪?%d / %d閿涘本绮撮崝銊ュ煂鎼存洟鍎寸紒褏鐢婚崝鐘烘祰"), Loaded, Total));
	}
	return FText::FromString(FString::Printf(TEXT("瀹告彃濮炴潪?%d / %d"), Loaded, Total));
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
	FBlueprintHelperReviewPendingLoadResultApplication Application =
		FBlueprintHelperReviewPendingLoadApplicationService::ApplyResult(
			Result,
			PagedChangeModel,
			ChangeItems,
			SelectedChange,
			LastVisibleChangeRefreshSignature,
			PendingPageSize);

	if (Application.bShouldRecordTiming)
	{
		RecordRejectStoreEventTiming(
			Result.SourceEvent,
			Application.TimingStage,
			Application.TimingSource,
			Application.bTimingSucceeded);
	}
	if (!Application.DebugMessage.IsEmpty())
	{
		AddDebugMessage(Application.DebugMessage);
	}
	if (Application.bShouldIgnore)
	{
		return;
	}
	if (Application.bShouldDispatchValidityCandidates && OnValidityCandidatesReady.IsBound())
	{
		OnValidityCandidatesReady.Execute(Result.Source, Result.ValidityCandidates);
	}
	ensureMsgf(
		!Application.bReturnedMoreThanPageSize,
		TEXT("ReviewPanel reset pending load returned more rows than page size."));
	if (Application.bSignatureUnchanged)
	{
		if (Application.bShouldInvalidate)
		{
			Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
		}
		return;
	}
	if (Application.bShouldApplyVisibleChanges)
	{
		ApplyVisibleChangesFromPendingLoad(Result, Application.NextChanges);
		LastVisibleChangeRefreshSignature = Application.NewRefreshSignature;
		SelectedChange = FindChangeItemById(Application.RecommendedSelectedChangeId);
	}
	RefreshChangeTreeWidget();
	if (Application.bShouldRefreshMainWorkspace)
	{
		LoadReviewAssetFromSelection();
		RefreshMainWorkspaceAfterReviewStateChanged();
	}
	if (Application.bShouldInvalidate)
	{
		Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	}
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
	FBlueprintHelperReviewRowHighlightSyncCallbacks Callbacks;
	Callbacks.BuildSurfaceDiffModels =
		[this](const FBlueprintHelperReviewSurfaceDiffFrameRoute& Route)
		{
			return BuildSurfaceDiffModelsForSurface(Route);
		};
	Callbacks.ConfigurePresenterArgs =
		[this](
			FBlueprintHelperReviewPanelSurfacePresenterArgs& Args,
			const TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel>& SurfaceDiffModels)
	{
		ConfigurePanelSurfacePresenterArgs(Args, SurfaceDiffModels);
	};
	RowHighlightSyncService.Sync(PreferredAssetPath, Callbacks);
}

void SBlueprintHelperReviewPanel::ConfigureSurfaceViewCoordinator()
{
	FBlueprintHelperReviewSurfaceHostCoordinatorDelegates Delegates;
	Delegates.StructureOverlayRefresh = [this]()
	{
		if (!ComponentsDiffStackBox.IsValid())
		{
			return false;
		}
		ComponentsDiffStackBox->SetContent(BuildStructurePanelDiffFrames());
		return true;
	};
	Delegates.MyBlueprintOverlayRefresh = [this]()
	{
		if (!MyBlueprintDiffStackBox.IsValid())
		{
			return false;
		}
		MyBlueprintDiffStackBox->SetContent(BuildPanelDiffFrames(
			FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveMyBlueprintRoute()));
		return true;
	};
	Delegates.DetailsOverlayRefresh = [this]()
	{
		if (!DetailsDiffStackBox.IsValid())
		{
			return false;
		}
		DetailsDiffStackBox->SetContent(BuildDetailsPanelDiffFrames());
		return true;
	};
	Delegates.MainWorkspaceOverlayRefresh = [this]()
	{
		if (!MainWorkspaceDiffStackBox.IsValid())
		{
			return false;
		}
		MainWorkspaceDiffStackBox->SetContent(BuildMainWorkspaceDiffFrames());
		return true;
	};
	Delegates.ComponentsRowsRefresh = [this]()
	{
		bool bHandled = false;
		if (ComponentsPresenterState.ComponentsPanel.IsValid())
		{
			ComponentsPresenterState.ComponentsPanel->RequestRowsRefresh();
			bHandled = true;
		}
		if (ComponentsContentBox.IsValid())
		{
			ComponentsContentBox->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
			bHandled = true;
		}
		return bHandled;
	};
	Delegates.WidgetTreeRowsRefresh = [this]()
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
	};
	Delegates.MyBlueprintRowsRefresh = [this]()
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
	};
	Delegates.DetailsRowsRefresh = [this]()
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
	};
	Delegates.DataTableRowsRefresh = [this]()
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
	};
	Delegates.DataAssetRowsRefresh = [this]()
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
	};
	Delegates.MaterialRowsRefresh = [this]()
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
	};

	SurfaceHostCoordinator.Configure(SurfaceViewCoordinator, MoveTemp(Delegates));
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
	FBlueprintHelperReviewSurfaceGeometryEventCallbacks Callbacks;
	Callbacks.GetReviewAssetPath = [this]()
	{
		return ReviewAssetContext.AssetPath;
	};
	Callbacks.AddDebugMessage = [this](const FString& Message)
	{
		AddDebugMessage(Message);
	};
	Callbacks.RefreshOverlay = [this](EBlueprintHelperReviewSurface RoutedSurface)
	{
		RefreshSurfaceOverlay(RoutedSurface);
		return true;
	};
	Callbacks.ProcessDebugFocusTraversalGeometryEvent = [this]()
	{
		ProcessDebugFocusTraversalGeometryEvent();
	};
	SurfaceGeometryCoordinator.HandleRegisteredRowGeometryChanged(AssetPath, Surface, Callbacks);
}

void SBlueprintHelperReviewPanel::OnSurfaceGeometryInvalidated(EBlueprintHelperReviewSurface Surface)
{
	FBlueprintHelperReviewSurfaceGeometryEventCallbacks Callbacks;
	Callbacks.AddDebugMessage = [this](const FString& Message)
	{
		AddDebugMessage(Message);
	};
	Callbacks.RefreshOverlay = [this](EBlueprintHelperReviewSurface RoutedSurface)
	{
		RefreshSurfaceOverlay(RoutedSurface);
		return true;
	};
	Callbacks.ProcessDebugFocusTraversalGeometryEvent = [this]()
	{
		ProcessDebugFocusTraversalGeometryEvent();
	};
	SurfaceGeometryCoordinator.HandleSurfaceGeometryInvalidated(Surface, Callbacks);
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
	FBlueprintHelperReviewSurfaceGeometryResolutionContext Context;
	Context.ReviewAssetPath = ReviewAssetContext.AssetPath;
	Context.ResolveOverlayWidget = [this](EBlueprintHelperReviewSurface RoutedSurface) -> TSharedPtr<SWidget>
	{
		if (RoutedSurface == EBlueprintHelperReviewSurface::Components
			|| RoutedSurface == EBlueprintHelperReviewSurface::UMGWidgetTree)
		{
			return ComponentsDiffStackBox;
		}
		if (RoutedSurface == EBlueprintHelperReviewSurface::MyBlueprint)
		{
			return MyBlueprintDiffStackBox;
		}
		if (RoutedSurface == EBlueprintHelperReviewSurface::Details)
		{
			return DetailsDiffStackBox;
		}
		if (RoutedSurface == EBlueprintHelperReviewSurface::DataTable
			|| RoutedSurface == EBlueprintHelperReviewSurface::DataAsset
			|| RoutedSurface == EBlueprintHelperReviewSurface::Material)
		{
			return MainWorkspaceDiffStackBox;
		}
		return TSharedPtr<SWidget>();
	};
	Context.ResolveComponentsRowGeometry = [this](
		const FBlueprintHelperReviewVisibleChange& RoutedChange,
		const TSharedPtr<SWidget>& OverlayWidget,
		FBlueprintHelperReviewSurfaceGeometryAnchor& RoutedAnchor)
	{
		return FBlueprintHelperReviewBlueprintComponentsPresenter::ResolveRowGeometry(
			RoutedChange,
			ComponentsPresenterState,
			OverlayWidget,
			RoutedAnchor);
	};
	Context.ResolveMyBlueprintRowGeometry = [this](
		const FBlueprintHelperReviewVisibleChange& RoutedChange,
		const TSharedPtr<SWidget>& OverlayWidget,
		FBlueprintHelperReviewSurfaceGeometryAnchor& RoutedAnchor)
	{
		return FBlueprintHelperReviewMyBlueprintPresenter::ResolveRowGeometry(
			RoutedChange,
			MyBlueprintPresenterState,
			OverlayWidget,
			RoutedAnchor);
	};
	Context.ResolveDetailsRowGeometry = [this](
		const FBlueprintHelperReviewVisibleChange& RoutedChange,
		const TSharedPtr<SWidget>& OverlayWidget,
		FBlueprintHelperReviewSurfaceGeometryAnchor& RoutedAnchor)
	{
		return ResolveDetailsRowGeometry(RoutedChange, OverlayWidget, RoutedAnchor);
	};

	return SurfaceGeometryCoordinator.ResolveRowGeometry(Change, Surface, Context, OutAnchor);
}

bool SBlueprintHelperReviewPanel::ResolveDetailsRowGeometry(
	const FBlueprintHelperReviewVisibleChange& Change,
	const TSharedPtr<SWidget>& OverlayWidget,
	FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
{
	FBlueprintHelperReviewDetailsGeometryResolutionContext Context;
	Context.KismetInspector = KismetInspector;
	Context.ResolveDetailsObject = [this]()
	{
		return ResolveDetailsObjectForSelectedChange();
	};
	return DetailsGeometryResolver.ResolveRowGeometry(Change, OverlayWidget, Context, OutAnchor);
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





