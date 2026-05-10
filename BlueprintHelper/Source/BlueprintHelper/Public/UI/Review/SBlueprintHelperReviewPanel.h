// BlueprintHelper fake Review panel.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewAssetPresenters.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenter.h"
#include "Widgets/SCompoundWidget.h"

class FBlueprintHelperReviewActionService;
class FBlueprintHelperReviewStoreService;
class SKismetInspector;
class SBox;
class SMultiLineEditableTextBox;
class UEdGraph;
template <typename ItemType> class STreeView;

class SBlueprintHelperReviewPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperReviewPanel)
		: _ReviewStoreService(nullptr)
		, _ReviewActionService(nullptr)
	{
	}

	SLATE_ARGUMENT(const FBlueprintHelperReviewStoreService*, ReviewStoreService)
	SLATE_ARGUMENT(const FBlueprintHelperReviewActionService*, ReviewActionService)
	SLATE_ARGUMENT(TArray<FBlueprintHelperReviewVisibleChange>, InitialChanges)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

#if WITH_DEV_AUTOMATION_TESTS
	struct FReviewTreeSnapshotEntry
	{
		bool bIsAssetHeader = false;
		FString AssetPath;
		FString ChangeId;
		int32 Depth = 0;
	};

	static TArray<FReviewTreeSnapshotEntry> BuildReviewTreeSnapshotForTesting(
		const TArray<FBlueprintHelperReviewVisibleChange>& SourceChanges);
#endif

private:
	using FReviewChangeItem = TSharedPtr<FBlueprintHelperReviewVisibleChange>;
	struct FReviewTreeItem
	{
		bool bIsAssetRoot = false;
		FString AssetPath;
		FReviewChangeItem Change;
		TArray<TSharedPtr<FReviewTreeItem>> Children;
	};
	using FReviewTreeItemPtr = TSharedPtr<FReviewTreeItem>;

	void RefreshVisibleChanges(const TArray<FBlueprintHelperReviewVisibleChange>& SourceChanges);
	TSharedRef<ITableRow> GenerateChangeTreeRow(FReviewTreeItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable);
	void GetChangeTreeChildren(FReviewTreeItemPtr Item, TArray<FReviewTreeItemPtr>& OutChildren) const;
	void OnChangeSelectionChanged(FReviewChangeItem Item, ESelectInfo::Type SelectInfo);
	void OnChangeTreeSelectionChanged(FReviewTreeItemPtr Item, ESelectInfo::Type SelectInfo);
	void AddDebugMessage(const FString& Message);
	FString BuildDebugMessagesString() const;
	FText GetDebugMessagesText() const;
	FReply OnCopyDebugMessages() const;

	TSharedRef<SWidget> BuildFinalChangeSidebar();
	TSharedRef<SWidget> BuildComponentsPanel();
	TSharedRef<SWidget> BuildMyBlueprintPanel();
	TSharedRef<SWidget> BuildGraphPanel();
	TSharedRef<SWidget> BuildDetailsPanel();
	TSharedRef<SWidget> BuildDebugPanel();
	TSharedRef<SWidget> BuildMainWorkspaceWidget();
	TSharedRef<SWidget> BuildGraphEditorWidget();
	TSharedRef<SWidget> BuildActionButtonBar();
	TSharedRef<SWidget> BuildAssetChangeButtonBar();
	TSharedRef<SWidget> BuildReadonlyComponentsWidget();
	TSharedRef<SWidget> BuildReadonlyMyBlueprintWidget();
	TSharedRef<SWidget> BuildReadonlyDetailsWidget();
	TSharedRef<SWidget> BuildStructurePanelDiffFrames();
	TSharedRef<SWidget> BuildMainWorkspaceDiffFrames();
	TSharedRef<SWidget> BuildScopedDiffStack(bool (*Predicate)(const FBlueprintHelperReviewVisibleChange&));
	TSharedRef<SWidget> BuildPanelDiffFrames(
		bool (*Predicate)(const FBlueprintHelperReviewVisibleChange&),
		EBlueprintHelperReviewSurface Surface);
	TSharedRef<SWidget> BuildDetailsPanelDiffFrames();
	TSharedRef<SWidget> BuildDiffRow(FReviewChangeItem Item, bool bShowActions);
	TSharedRef<SWidget> BuildDiffFrame(FReviewChangeItem Item, const TSharedRef<SWidget>& Content, bool bShowActions);
	void RefreshDiffStackWidgets();
	void RefreshSurfaceOverlay(EBlueprintHelperReviewSurface Surface);
	void RebuildChangeTreeItems();
	void RefreshChangeTreeWidget();
	static void BuildChangeTreeItemsFromChangeItems(
		const TArray<FReviewChangeItem>& SourceItems,
		TArray<FReviewTreeItemPtr>& OutRootItems);
	static FReviewTreeItemPtr FindTreeItemForChangeRecursive(
		const TArray<FReviewTreeItemPtr>& Items,
		FReviewChangeItem ChangeItem);
	void ExpandChangeTreeItemRecursive(FReviewTreeItemPtr Item);
	FReviewTreeItemPtr FindTreeItemForChange(FReviewChangeItem Item) const;
	FReviewChangeItem FindChangeItemById(const FString& ChangeId) const;
	TArray<FBlueprintHelperReviewVisibleChange> BuildPendingChangeSnapshot() const;
	void SelectNextChangeAfterRemoval(const FString& PreferredAssetPath, int32 RemovedIndex);
	FReply OnAcceptChangeId(const FString& ChangeId);
	FReply OnRejectChangeId(const FString& ChangeId);

	FReply OnAcceptSelected();
	FReply OnRejectSelected();
	FReply OnAcceptChange(FReviewChangeItem Item);
	FReply OnRejectChange(FReviewChangeItem Item);
	FReply OnAcceptAll();
	FReply OnRejectAll();

	FText GetSelectedTitle() const;
	FText GetSelectedBefore() const;
	FText GetSelectedAfter() const;
	FText GetSelectedStatus() const;
	FText GetSelectedTransactionChain() const;
	FSlateColor GetSelectedDiffColor() const;
	FSlateColor GetChangeColor(EBlueprintHelperReviewChangeKind Kind) const;

	EActiveTimerReturnType TickFlash(double InCurrentTime, float InDeltaTime);
	void StartFlash();
	void LoadReviewAssetFromSelection();
	void UpdateDetailsSelection();
	void OnDetailsDisplayedPropertiesChanged();
	EBlueprintHelperReviewSurface ResolveDetailsSurfaceFromSelectedChange() const;
	bool ResolveReviewRowGeometry(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor);
	bool ResolveDetailsRowGeometry(
		const FBlueprintHelperReviewVisibleChange& Change,
		const TSharedPtr<SWidget>& OverlayWidget,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor);
	UObject* ResolveDetailsObjectForSelectedChange() const;
	UEdGraph* ResolveGraphForSelectedChange() const;

	const FBlueprintHelperReviewStoreService* ReviewStoreService = nullptr;
	const FBlueprintHelperReviewActionService* ReviewActionService = nullptr;

	TArray<FReviewChangeItem> ChangeItems;
	TArray<FReviewTreeItemPtr> ChangeTreeRootItems;
	TSharedPtr<STreeView<FReviewTreeItemPtr>> ChangeTreeView;
	TSharedPtr<SBox> GraphEditorBox;
	TSharedPtr<SBox> ComponentsContentBox;
	TSharedPtr<SBox> MyBlueprintContentBox;
	TSharedPtr<SBox> DetailsContentBox;
	TSharedPtr<SBox> ComponentsDiffStackBox;
	TSharedPtr<SBox> MainWorkspaceDiffStackBox;
	TSharedPtr<SBox> MyBlueprintDiffStackBox;
	TSharedPtr<SBox> DetailsDiffStackBox;
	TSharedPtr<SKismetInspector> KismetInspector;
	FReviewChangeItem SelectedChange;
	TArray<FString> DebugMessages;
	TSharedPtr<SMultiLineEditableTextBox> DebugMessageTextBox;
	FBlueprintHelperReviewAssetContext ReviewAssetContext;
	EBlueprintHelperReviewSurface DetailsSurface = EBlueprintHelperReviewSurface::Unknown;
	FBlueprintHelperReviewGraphPresenterState GraphPresenterState;
	FBlueprintHelperReviewBlueprintComponentsPresenter::FState ComponentsPresenterState;
	FBlueprintHelperReviewWidgetTreePresenterState WidgetTreePresenterState;
	FBlueprintHelperReviewMyBlueprintPresenter::FState MyBlueprintPresenterState;
	FBlueprintHelperReviewDataTablePresenterState DataTablePresenterState;
	FBlueprintHelperReviewDataAssetPresenterState DataAssetPresenterState;
	float FlashAlpha = 0.0f;
};
