// BlueprintHelper fake Review panel.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewAssetPresenters.h"
#include "UI/Review/BlueprintHelperReviewPagedChangeModel.h"
#include "UI/Review/BlueprintHelperReviewPendingLoadCoordinator.h"
#include "UI/Review/BlueprintHelperReviewPendingLoadApplicationService.h"
#include "UI/Review/BlueprintHelperReviewActionNotificationPresenter.h"
#include "UI/Review/BlueprintHelperReviewDebugFocusTraversalCoordinator.h"
#include "UI/Review/BlueprintHelperReviewDetailsGeometryResolver.h"
#include "UI/Review/BlueprintHelperReviewPanelData.h"
#include "UI/Review/BlueprintHelperReviewPanelSettings.h"
#include "UI/Review/BlueprintHelperReviewRejectMutationApplicationService.h"
#include "UI/Review/BlueprintHelperReviewRejectWorkflowCoordinator.h"
#include "UI/Review/BlueprintHelperReviewRowHighlightSyncService.h"
#include "UI/Review/BlueprintHelperReviewSurfaceDiffFramePresenter.h"
#include "UI/Review/BlueprintHelperReviewSurfaceContentPresenterTypes.h"
#include "UI/Review/BlueprintHelperReviewSurfaceGeometryCoordinator.h"
#include "UI/Review/BlueprintHelperReviewSurfaceHostCoordinator.h"
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
class SBox;
class SMultiLineEditableTextBox;
class UEdGraph;
struct FBlueprintHelperReviewRejectTimingSample;
struct FBlueprintHelperReviewRejectMutationPresentation;

class SBlueprintHelperReviewPanel : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_TwoParams(
		FOnValidityCandidatesReady,
		const FString&,
		const TArray<FBlueprintHelperReviewValidityCandidate>&);

	SLATE_BEGIN_ARGS(SBlueprintHelperReviewPanel)
		: _ReviewStoreService(nullptr)
		, _ReviewActionService(nullptr)
	{
	}

	SLATE_ARGUMENT(const FBlueprintHelperReviewStoreService*, ReviewStoreService)
	SLATE_ARGUMENT(const FBlueprintHelperReviewActionService*, ReviewActionService)
	SLATE_ARGUMENT(TArray<FBlueprintHelperReviewVisibleChange>, InitialChanges)
	SLATE_EVENT(FOnValidityCandidatesReady, OnValidityCandidatesReady)

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
		FString ParentChangeId;
		int32 Depth = 0;
	};

	static TArray<FReviewTreeSnapshotEntry> BuildReviewTreeSnapshotForTesting(
		const TArray<FBlueprintHelperReviewVisibleChange>& SourceChanges);
	int32 GetVisibleChangeCountForTesting() const;
	bool SelectChangeForTesting(const FString& ChangeId);
	bool CaptureFocusDebugBundleForTesting(FString& OutBundlePath, FString& OutDebugMessages);
	bool LoadDebugBundleForTesting(const FString& InBundlePath, FString& OutDebugMessages);
	void RefreshVisibleChangesForTesting(const TArray<FBlueprintHelperReviewVisibleChange>& SourceChanges);
	FText GetPendingPageStatusTextForTesting() const;
	TArray<FString> GetSurfaceDiffModelIdsForTesting(EBlueprintHelperReviewSurface Surface) const;
	int32 GetSurfaceDiffModelCountForTesting(EBlueprintHelperReviewSurface Surface) const;
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
	FReply OnClearDebugMessages();
	FReply OnLoadDebugBundle();
	FReply OnCaptureFocusDebugBundle();
	void AdvanceDebugFocusTraversal();
	void ProcessDebugFocusTraversalGeometryEvent();
	void ApplyDebugFocusTraversalStep(const FBlueprintHelperReviewDebugFocusTraversalStep& Step);
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
	TSharedRef<SWidget> BuildSurfaceContentWidget(
		const FBlueprintHelperReviewSurfaceDiffFrameRoute& Route,
		EBlueprintHelperReviewSurfaceContentHost Host);
	TSharedRef<SWidget> BuildActionButtonBar();
	TSharedRef<SWidget> BuildAssetChangeButtonBar();
	TSharedRef<SWidget> BuildReadonlyComponentsWidget();
	TSharedRef<SWidget> BuildReadonlyMyBlueprintWidget();
	TSharedRef<SWidget> BuildReadonlyDetailsWidget();
	TSharedRef<SWidget> BuildStructurePanelDiffFrames();
	TSharedRef<SWidget> BuildMainWorkspaceDiffFrames();
	TSharedRef<SWidget> BuildScopedDiffStack(bool (*Predicate)(const FBlueprintHelperReviewVisibleChange&));
	TSharedRef<SWidget> BuildPanelDiffFrames(
		const FBlueprintHelperReviewSurfaceDiffFrameRoute& Route);
	TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel> BuildSurfaceDiffModelsForSurface(
		const FBlueprintHelperReviewSurfaceDiffFrameRoute& Route) const;
	void ConfigurePanelSurfacePresenterArgs(
		FBlueprintHelperReviewPanelSurfacePresenterArgs& Args,
		const TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel>& SurfaceDiffModels);
	void ConfigurePanelSurfaceContentArgs(
		FBlueprintHelperReviewPanelSurfaceContentArgs& Args,
		EBlueprintHelperReviewSurfaceContentHost Host,
		const TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel>& SurfaceDiffModels);
	TMap<EBlueprintHelperReviewSurfaceHostSlot, TWeakPtr<SWidget>> BuildSurfaceHostWidgetMap(
		bool bUseOverlayHosts) const;
	TSharedRef<SWidget> BuildDetailsPanelDiffFrames();
	TSharedRef<SWidget> BuildDiffRow(FReviewChangeItem Item, bool bShowActions);
	TSharedRef<SWidget> BuildDiffFrame(FReviewChangeItem Item, const TSharedRef<SWidget>& Content, bool bShowActions);
	void ConfigureSurfaceViewCoordinator();
	void RefreshDiffStackWidgets();
	void RefreshMainWorkspaceAfterReviewStateChanged();
	void RefreshReviewUiAfterStateChanged(const FString& Reason, const FString& PreferredAssetPath = FString());
	void RefreshReviewActionQueueState(const FString& Reason, const FString& PreferredAssetPath = FString());
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
		EBlueprintHelperReviewActionNotificationState State,
		bool bExpire,
		bool bUseThrobber);
	static FString BuildReviewActionNotificationLabel(FReviewChangeItem Item);
	void QueueRejectChange(FReviewChangeItem Item, bool bShowIndividualNotification = true);
	void RecordRejectBatchResult(const FString& ChangeId, bool bSucceeded);
	void EmitRejectTimingSample(const FBlueprintHelperReviewRejectTimingSample& Sample);
	void RecordRejectStageElapsed(
		const FString& ChangeId,
		const FString& Stage,
		const FString& Detail = FString());
	void RecordRejectStoreEventTiming(
		const FBlueprintHelperReviewStoreChangedEvent& Event,
		const FString& Stage,
		const FString& Detail,
		bool bCompleteMatches);
	void StartNextRejectPrepare();
	TSharedRef<FBlueprintHelperReviewRejectWorkflowCoordinator> EnsureRejectWorkflowCoordinator();
	TOptional<FBlueprintHelperReviewVisibleChange> ResolveRejectWorkflowChangeSnapshot(const FString& ChangeId) const;
	void HandleRejectWorkflowMissingChange(const FString& ChangeId);
	void HandleRejectWorkflowPrepareStarted(
		const FString& ChangeId,
		const FBlueprintHelperReviewVisibleChange& ChangeSnapshot);
	void HandleRejectWorkflowPrepareFinished(
		const FString& ChangeId,
		const FBlueprintHelperReviewRejectOptions& PreparedOptions);
	void HandlePreparedRejectReady(const FString& ChangeId, const FBlueprintHelperReviewRejectOptions& PreparedOptions);
	void ExecutePreparedRejectMutation(const FString& ChangeId);
	FBlueprintHelperReviewRejectMutationApplicationCallbacks BuildRejectMutationApplicationCallbacks(
		const FString& ChangeId,
		FReviewChangeItem Item);

	FText GetSelectedTitle() const;
	FText GetSelectedBefore() const;
	FText GetSelectedAfter() const;
	FText GetSelectedStatus() const;
	FText GetSelectedEvidenceChain() const;
	FSlateColor GetSelectedDiffColor() const;
	FSlateColor GetChangeColor(EBlueprintHelperReviewChangeKind Kind) const;

	void StartFlash();
	void RequestPendingReviewLoad(
		const FString& Reason,
		const FBlueprintHelperReviewStoreChangedEvent& SourceEvent =
			FBlueprintHelperReviewStoreChangedEvent::FullReload());
	void RequestPendingReviewPage(
		const FString& Reason,
		EBlueprintHelperReviewPendingLoadMode Mode,
		const FBlueprintHelperReviewStoreChangedEvent& SourceEvent =
			FBlueprintHelperReviewStoreChangedEvent::FullReload());
	void OnChangeTreeScrolled(double ScrollOffset);
	int32 CountLoadedChangeTreeRows() const;
	FText GetPendingPageStatusText() const;
	FReply OnLoadMorePendingChanges();
	void HandlePendingReviewLoadCompleted(const FBlueprintHelperReviewPendingLoadResult& Result);
	void RefreshFromReviewStoreIfChanged(const FBlueprintHelperReviewStoreChangedEvent& Event);
	void ApplyVisibleChangesFromPendingLoad(
		const FBlueprintHelperReviewPendingLoadResult& Result,
		const TArray<FBlueprintHelperReviewVisibleChange>& NextChanges);
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
	TSharedPtr<FBlueprintHelperReviewPendingLoadCoordinator> PendingLoadCoordinator;
	FOnValidityCandidatesReady OnValidityCandidatesReady;
	FBlueprintHelperReviewPagedChangeModel PagedChangeModel;

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
	TSharedPtr<FBlueprintHelperReviewRejectWorkflowCoordinator> RejectWorkflowCoordinator;
	FBlueprintHelperReviewActionNotificationPresenter ReviewActionNotificationPresenter;
	FBlueprintHelperReviewDebugFocusTraversalCoordinator DebugFocusTraversalCoordinator;
	FBlueprintHelperReviewDetailsGeometryResolver DetailsGeometryResolver;
	FString RequestedGraphNavigationChangeId;
	FString RequestedGraphNavigationGraphName;
	bool bAllowGraphNavigationWithoutGraphReview = false;
	FBlueprintHelperReviewAssetContext ReviewAssetContext;
	FBlueprintHelperReviewRowHighlightSyncService RowHighlightSyncService;
	FBlueprintHelperReviewSurfaceGeometryCoordinator SurfaceGeometryCoordinator;
	FBlueprintHelperReviewSurfaceHostCoordinator SurfaceHostCoordinator;
	FBlueprintHelperReviewSurfaceViewCoordinator SurfaceViewCoordinator;
	FBlueprintHelperReviewPanelSettings ReviewPanelSettings;
	int32 PendingPageSize = 100;
	int32 PendingScrollPrefetchRows = 24;
	int64 PendingPageRequestId = 0;
	EBlueprintHelperReviewSurface DetailsSurface = EBlueprintHelperReviewSurface::Unknown;
	FBlueprintHelperReviewGraphPresenterState GraphPresenterState;
	FBlueprintHelperReviewBlueprintComponentsPresenter::FState ComponentsPresenterState;
	FBlueprintHelperReviewWidgetTreePresenterState WidgetTreePresenterState;
	FBlueprintHelperReviewMyBlueprintPresenter::FState MyBlueprintPresenterState;
	FBlueprintHelperReviewDataTablePresenterState DataTablePresenterState;
	FBlueprintHelperReviewDataAssetPresenterState DataAssetPresenterState;
	FBlueprintHelperReviewMaterialPresenterState MaterialPresenterState;
	float FlashAlpha = 0.0f;
};
