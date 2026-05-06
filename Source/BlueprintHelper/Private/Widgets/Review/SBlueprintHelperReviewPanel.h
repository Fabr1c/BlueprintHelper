// BlueprintHelper fake Review panel.

#pragma once

#include "CoreMinimal.h"
#include "Structure/Review/BlueprintHelperReviewTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class FBlueprintHelperReviewActionService;
class FBlueprintHelperReviewStoreService;
class SKismetInspector;
class SBox;
class SGraphEditor;
class SMyBlueprint;
class STextBlock;
class UBlueprint;
class UEdGraph;
class UEdGraphNode;
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

	TSharedRef<SWidget> BuildFinalChangeSidebar();
	TSharedRef<SWidget> BuildComponentsPanel();
	TSharedRef<SWidget> BuildMyBlueprintPanel();
	TSharedRef<SWidget> BuildGraphPanel();
	TSharedRef<SWidget> BuildDetailsPanel();
	TSharedRef<SWidget> BuildDebugPanel();
	TSharedRef<SWidget> BuildGraphEditorWidget();
	TSharedRef<SWidget> BuildActionButtonBar();
	TSharedRef<SWidget> BuildReadonlyComponentsWidget();
	TSharedRef<SWidget> BuildReadonlyMyBlueprintWidget();
	TSharedRef<SWidget> BuildReadonlyDetailsWidget();
	TSharedRef<SWidget> BuildScopedDiffStack(bool (*Predicate)(const FBlueprintHelperReviewVisibleChange&));
	TSharedRef<SWidget> BuildDiffRow(FReviewChangeItem Item, bool bShowActions);
	TSharedRef<SWidget> BuildDiffFrame(FReviewChangeItem Item, const TSharedRef<SWidget>& Content, bool bShowActions);
	void RefreshDiffStackWidgets();
	void RebuildChangeTreeItems();
	void RefreshChangeTreeWidget();
	FReviewTreeItemPtr FindTreeItemForChange(FReviewChangeItem Item) const;
	FReviewChangeItem FindChangeItemById(const FString& ChangeId) const;
	FReply OnAcceptChangeId(const FString& ChangeId);
	FReply OnRejectChangeId(const FString& ChangeId);
	void AddGraphDiffBlocks(UEdGraph* PreviewGraphToEdit, const UEdGraph* SourceGraph);
	bool BuildGraphBoundsForChange(
		const FReviewChangeItem& Item,
		const UEdGraph* SourceGraph,
		const FString& GraphName,
		FVector2D& OutPosition,
		FVector2D& OutSize) const;
	void IncludeGraphTargetBounds(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const UEdGraph* SourceGraph,
		FBox2D& InOutBounds,
		bool& bInOutHasBounds) const;
	bool DoesGraphNodeMatchTarget(const UEdGraphNode* Node, const FString& TargetKey) const;
	UEdGraphNode* FindGraphNodeByGuid(const UEdGraph* Graph, const FString& NodeGuid) const;

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
	void LoadReviewBlueprintFromSelection();
	void UpdateDetailsSelection();
	UObject* ResolveDetailsObjectForSelectedChange() const;
	UEdGraph* ResolveGraphForSelectedChange() const;

	const FBlueprintHelperReviewStoreService* ReviewStoreService = nullptr;
	const FBlueprintHelperReviewActionService* ReviewActionService = nullptr;

	TArray<FReviewChangeItem> ChangeItems;
	TArray<FReviewTreeItemPtr> ChangeTreeRootItems;
	TSharedPtr<STreeView<FReviewTreeItemPtr>> ChangeTreeView;
	TSharedPtr<SBox> GraphEditorBox;
	TSharedPtr<SGraphEditor> GraphEditorWidget;
	TSharedPtr<SBox> ComponentsContentBox;
	TSharedPtr<SBox> MyBlueprintContentBox;
	TSharedPtr<SBox> ComponentsDiffStackBox;
	TSharedPtr<SBox> MyBlueprintDiffStackBox;
	TSharedPtr<SBox> DetailsDiffStackBox;
	TSharedPtr<SKismetInspector> KismetInspector;
	TSharedPtr<SMyBlueprint> MyBlueprintWidget;
	FReviewChangeItem SelectedChange;
	TSharedPtr<STextBlock> StatusTextBlock;
	TWeakObjectPtr<UBlueprint> ReviewBlueprint;
	TStrongObjectPtr<UBlueprint> PreviewBlueprint;
	TStrongObjectPtr<UEdGraph> PreviewGraph;
	float FlashAlpha = 0.0f;
};
