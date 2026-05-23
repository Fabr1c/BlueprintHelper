// BlueprintHelper fake Review panel.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewAssetPresenters.h"
#include "UI/Review/BlueprintHelperReviewPanelData.h"
#include "UI/Review/BlueprintHelperReviewPanelSettings.h"
#include "UI/Review/BlueprintHelperReviewSurfaceViewCoordinator.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenter.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

class FBlueprintHelperReviewStoreService;
class FBlueprintHelperReviewPanelPresenter;
class FJsonObject;
class SEditableTextBox;
class SKismetInspector;
class SNotificationItem;
class SBox;
class SMultiLineEditableTextBox;
class UEdGraph;

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

	~SBlueprintHelperReviewPanel();
	void Construct(const FArguments& InArgs);
	static void FlushAsyncTasks();
	static void ShutdownAsyncTasks();

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
	enum class EReviewActionNotificationState : uint8
	{
		Pending,
		Success,
		Fail
	};

	struct FReviewActionBatchNotificationState
	{
		FString NotificationKey;
		int32 TotalCount = 0;
		int32 FinishedCount = 0;
		int32 SuccessCount = 0;
		int32 FailedCount = 0;
	};

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
	bool TryResolveGraphNavigationForChange(FReviewChangeItem Item, FString& OutGraphName) const;
	void OnChangeSelectionChanged(FReviewChangeItem Item, ESelectInfo::Type SelectInfo);
	void OnChangeTreeSelectionChanged(FReviewTreeItemPtr Item, ESelectInfo::Type SelectInfo);
	void OnMyBlueprintGraphNavigationRequested(const FString& ChangeId, const FString& GraphName);
	void AddDebugMessage(const FString& Message);
	FString BuildDebugMessagesString() const;
	FText GetDebugMessagesText() const;
	FText GetDebugBundlePathText() const;
	void OnDebugBundlePathCommitted(const FText& Text, ETextCommit::Type CommitType);
	FReply OnCopyDebugMessages() const;
	FReply OnCopyDebugBundlePath() const;
	FReply OnLoadDebugBundle();
	FReply OnCaptureFocusDebugBundle();
	void AdvanceDebugFocusTraversal();
	void ProcessDebugFocusTraversalGeometryEvent();
	bool IsDebugFocusTraversalChangeReady(FReviewChangeItem Item, FString& OutReason);
	void EnsureDebugBundleSession();
	void AppendDebugBundleEvent(const TSharedRef<FJsonObject>& Event);

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
	void ConfigureSurfaceViewCoordinator();
	void RefreshDiffStackWidgets();
	void RefreshMainWorkspaceAfterReviewStateChanged();
	void RefreshReviewUiAfterStateChanged(const FString& Reason, const FString& PreferredAssetPath = FString());
	void RebuildReviewPanelStatePreservingTransient();
	void SyncReviewRowHighlightStates(const FString& PreferredAssetPath = FString());
	void OnRowHighlightStateChanged(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		uint64 Revision);
	void RefreshSurfaceOverlay(EBlueprintHelperReviewSurface Surface);
	void OnRegisteredRowGeometryChanged(const FString& AssetPath, EBlueprintHelperReviewSurface Surface);
	void OnSurfaceGeometryInvalidated(EBlueprintHelperReviewSurface Surface);
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
	static FString BuildVisibleChangeRefreshSignature(const TArray<FBlueprintHelperReviewVisibleChange>& Changes);
	void SelectNextChangeAfterRemoval(const FString& PreferredAssetPath, int32 RemovedIndex);
	FReply OnReviewActionIntent(const FBlueprintHelperReviewActionIntent& Intent);
	EBlueprintHelperReviewChangeStatus GetEffectiveChangeStatus(FReviewChangeItem Item) const;
	FString GetEffectiveNeedsActionReason(FReviewChangeItem Item) const;

	FReply OnAcceptSelected();
	FReply OnRejectSelected();
	FReply ExecuteAcceptChange(FReviewChangeItem Item);
	FReply ExecuteRejectChange(FReviewChangeItem Item);
	FReply OnAcceptAll();
	FReply OnRejectAll();
	void ShowReviewActionNotification(
		const FString& NotificationKey,
		const FString& StatusText,
		EReviewActionNotificationState State,
		bool bExpire,
		bool bUseThrobber);
	static FString BuildReviewActionNotificationLabel(FReviewChangeItem Item);
	void QueueRejectChange(FReviewChangeItem Item, bool bShowIndividualNotification = true);
	void RecordRejectBatchResult(const FString& ChangeId, bool bSucceeded);
	void StartNextRejectPrepare();
	void HandlePreparedRejectReady(const FString& ChangeId, const FBlueprintHelperReviewRejectOptions& PreparedOptions);
	void ExecutePreparedRejectMutation(const FString& ChangeId);
	void FinishAsyncReject(const FString& ChangeId);

	FText GetSelectedTitle() const;
	FText GetSelectedBefore() const;
	FText GetSelectedAfter() const;
	FText GetSelectedStatus() const;
	FText GetSelectedEvidenceChain() const;
	FSlateColor GetSelectedDiffColor() const;
	FSlateColor GetChangeColor(EBlueprintHelperReviewChangeKind Kind) const;

	EActiveTimerReturnType TickFlash(double InCurrentTime, float InDeltaTime);
	void StartFlash();
	void RefreshFromReviewStoreIfChanged();
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
	UObject* ResolveComponentDetailsObjectForChange(const FBlueprintHelperReviewVisibleChange& Change) const;
	UEdGraph* ResolveGraphForSelectedChange() const;

	TSharedPtr<FBlueprintHelperReviewPanelPresenter> ReviewPanelPresenter;

	TArray<FReviewChangeItem> ChangeItems;
	FBlueprintHelperReviewPanelState ReviewPanelState;
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
	FString LastVisibleChangeRefreshSignature;
	FDelegateHandle PendingReviewChangedHandle;
	FDelegateHandle RowGeometryChangedHandle;
	FDelegateHandle RowHighlightStateChangedHandle;
	TArray<FString> DebugMessages;
	TSharedPtr<SMultiLineEditableTextBox> DebugMessageTextBox;
	TSharedPtr<SEditableTextBox> DebugBundlePathTextBox;
	FString DebugBundleSessionId;
	FString DebugBundlePath;
	TArray<FString> PendingRejectChangeIds;
	TMap<FString, FBlueprintHelperReviewRejectOptions> PreparedRejectOptionsByChangeId;
	TMap<FString, TWeakPtr<SNotificationItem>> ReviewActionNotifications;
	TMap<FString, FString> RejectBatchKeyByChangeId;
	TMap<FString, FReviewActionBatchNotificationState> RejectBatchNotifications;
	FString ActiveRejectChangeId;
	bool bAsyncRejectPrepareActive = false;
	TArray<FReviewChangeItem> DebugFocusTraversalItems;
	int32 DebugFocusTraversalIndex = 0;
	bool bDebugFocusTraversalAwaitingGeometry = false;
	bool bDebugFocusTraversalActive = false;
	FString RequestedGraphNavigationChangeId;
	FString RequestedGraphNavigationGraphName;
	bool bAllowGraphNavigationWithoutGraphReview = false;
	FBlueprintHelperReviewAssetContext ReviewAssetContext;
	FBlueprintHelperReviewSurfaceViewCoordinator SurfaceViewCoordinator;
	FBlueprintHelperReviewPanelSettings ReviewPanelSettings;
	EBlueprintHelperReviewSurface DetailsSurface = EBlueprintHelperReviewSurface::Unknown;
	FBlueprintHelperReviewGraphPresenterState GraphPresenterState;
	FBlueprintHelperReviewBlueprintComponentsPresenter::FState ComponentsPresenterState;
	FBlueprintHelperReviewWidgetTreePresenterState WidgetTreePresenterState;
	FBlueprintHelperReviewMyBlueprintPresenter::FState MyBlueprintPresenterState;
	FBlueprintHelperReviewDataTablePresenterState DataTablePresenterState;
	FBlueprintHelperReviewDataAssetPresenterState DataAssetPresenterState;
	float FlashAlpha = 0.0f;
};
