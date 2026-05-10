# ReviewPanel Row Highlight, Graph Bounds, Lifecycle Root Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 ReviewPanel 的非 Graph 面板改为直接 Row 背景高亮，将中间 Graph 工作区补齐稳定 bounds 证据，并把资产创建 Review 设为同资产后续 Review 的生命周期根。

**Architecture:** Components、MyBlueprint、Details、WidgetTree 不再使用旧的 Anchor overlay 绘制 Diff 框；这些面板在拿到 Row 后直接改变 Row 背景色，并只在选中 Row 右侧显示 Accept / Reject。Graph 工作区保留现有 Graph anchor 和跳转模型，但 Graph 写操作必须持久化 node guid 和 recorded bounds，避免 `ReviewRoute shown` 后 `GraphDiff bounds failed`。资产创建 Review 进入 store/action 层，作为同一 AssetPath 下子 Review 的 root，Reject root 成功后级联销毁子 pending Review。

**Tech Stack:** Unreal Engine 5.6 Slate, BlueprintHelper Review store/action services, ReviewPanel presenters, GraphWrite transaction journal, UE automation tests.

---

## Decision Contracts

### 1. Non-Graph Panels Use Row Highlight, Not Anchor Overlay

- Components、MyBlueprint、Details、WidgetTree 已经具备直接获取 Row 的能力，后续不再通过全局 overlay anchor 绘制 Diff 框。
- Row 命中后直接给 Row 设置半透明背景色：added 为绿色，modified 为黄色，removed 为红色，默认 alpha 为 `0.6`。
- Row 高亮不能包含 review text、target key、transaction id。文字仍由 Row 本身显示。
- 只有当前选中 Row 右侧显示 Accept / Reject。非选中 Row 只显示背景高亮。
- 没有 Row 时不画假框，不生成 review-list text fallback，只输出 debug：`ReviewRowHighlight ... result=pending|hidden reason=...`。
- Details 仍负责显示变量默认值、事件签名、函数签名、事件分发器签名、蓝图设置、蓝图默认值等属性详情；但高亮方式同样是 Row 背景，不走 overlay anchor。

### 2. Graph Workspace Keeps Anchor Diff Blocks, But Evidence Must Be Complete

- 中间 Graph 工作区继续使用 Graph anchor、Graph node bounds、recorded bounds 和 `SGraphEditor::JumpToNode`。
- Graph route 成功不代表可以绘制。绘制必须满足至少一个条件：
  - atomic target 能通过 `NodeGuid`、target key、display label 匹配到实际 `UEdGraphNode`。
  - atomic target 带有 `bHasGraphBounds=true`、`GraphPosition`、`GraphSize`。
- `ReplaceBlueprintGraph`、append graph、graph block write 必须在写入 review evidence 时记录结构化 node anchors，不再只记录 loose string path。
- Graph diff block 失败时 debug 必须能说明缺哪层证据：`matchedNodes=0`、`recordBounds=0`、`candidates=N`、`graphNodeCount=N`。

### 3. Asset Creation Review Is Lifecycle Root

- `asset_factory` 且 `ChangeKind=Added` 的 visible change 标记为 asset lifecycle root。
- 当 lifecycle root 仍 pending 时，同一 `AssetPath` 下所有后续 pending Review 都挂到它下面。
- Reject lifecycle root 成功等价于删除新增资产，子 Review 必须从 pending Review 流中销毁。
- Reject lifecycle root 失败或 needs_action 时，不销毁子 Review。
- Accept lifecycle root 不级联接受子 Review，只确认资产创建本身。
- Reject child 不影响 root。
- RejectAllAssetChange 先处理 root；root reject 成功后不再逐条 reject children。

## Files

### Modify

- `Source/BlueprintHelper/Public/Shared/Review/BlueprintHelperReviewTypes.h`
  - Add lifecycle root fields to `FBlueprintHelperReviewVisibleChange`.

- `Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewStoreService.cpp`
  - Serialize lifecycle fields.
  - Load legacy records safely.
  - Link pending children to root during pending visible change build.

- `Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewActionService.h`
- `Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewActionService.cpp`
  - Add lifecycle root reject cascade result and status update.

- `Source/BlueprintHelper/Public/Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h`
  - Add structured graph review node anchor payload while preserving `CreatedNodePaths`.

- `Source/BlueprintHelper/Private/Systems/Transactions/BlueprintHelperTransactionJournalService.cpp`
  - Prefer structured graph review anchors.
  - Parse legacy GUID/path records as fallback.

- `Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.cpp`
  - Record imported replacement nodes with guid and bounds.

- `Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp`
- `Source/BlueprintHelper/Public/UI/Review/SBlueprintHelperReviewPanel.h`
  - Render tree as asset header -> lifecycle root -> children.
  - Call cascade reject path for lifecycle root.
  - Keep selection stable after cascade removal.

- `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewSurfacePresenter.cpp`
- `Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewSurfacePresenter.h`
- `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewAssetPresenters.cpp`
- `Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewAssetPresenters.h`
  - Replace non-Graph overlay frames with Row background highlight state.
  - Keep Graph presenter using diff block nodes.

- `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`
- `Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`
  - Add lifecycle root, row highlight, graph bounds regressions.

### Do Not Modify

- Do not reintroduce fake precise overlay frames for Components、MyBlueprint、Details、WidgetTree.
- Do not move Graph workspace to Row highlight. Graph remains node/bounds based.
- Do not change unrelated DebugCase / DebugBundle contracts.
- Do not handle unrelated UE build or request-validator issues in this plan.

## Implementation Plan

### Task 1: Lifecycle Root Metadata

**Files:**
- Modify: `Source/BlueprintHelper/Public/Shared/Review/BlueprintHelperReviewTypes.h`
- Modify: `Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewStoreService.cpp`
- Test: `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [ ] **Step 1: Add visible change lifecycle fields**

Add fields to `FBlueprintHelperReviewVisibleChange`:

```cpp
FString ParentChangeId;
bool bIsAssetLifecycleRoot = false;
bool bRejectRemovesChildren = false;
```

- [ ] **Step 2: Serialize fields**

Write JSON fields only when needed:

```cpp
if (!Change.ParentChangeId.IsEmpty()) Json->SetStringField(TEXT("parent_change_id"), Change.ParentChangeId);
if (Change.bIsAssetLifecycleRoot) Json->SetBoolField(TEXT("is_asset_lifecycle_root"), true);
if (Change.bRejectRemovesChildren) Json->SetBoolField(TEXT("reject_removes_children"), true);
```

Read them with default false or empty for legacy records.

- [ ] **Step 3: Mark asset factory creation as root**

When a visible change has an atomic target where:

```text
target_kind == asset_factory
change_kind == added
```

set:

```cpp
Change.bIsAssetLifecycleRoot = true;
Change.bRejectRemovesChildren = true;
Change.ParentChangeId.Reset();
```

- [ ] **Step 4: Link children on pending load**

After pending visible changes are collected, group by `AssetPath`. If a group has exactly one pending lifecycle root, set every non-root pending child:

```cpp
Child.ParentChangeId = Root.ChangeId;
```

If multiple roots exist for one asset, pick the newest pending root by `LatestTransactionId` order and log debug in test-only helper if needed; do not attach roots to each other.

- [ ] **Step 5: Add store tests**

Add tests:

```text
BlueprintHelper.Review.Record.AssetFactoryChangeIsLifecycleRoot
BlueprintHelper.Review.Record.PendingChangesLinkUnderLifecycleRoot
BlueprintHelper.Review.Record.LegacyVisibleChangeDefaultsToNoLifecycleRoot
```

Expected:

- Asset creation title data is not treated as variable.
- Same asset child has `ParentChangeId=root.ChangeId`.
- Different asset child stays unparented.

### Task 2: Lifecycle Root Tree And Cascade Reject

**Files:**
- Modify: `Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewActionService.h`
- Modify: `Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewActionService.cpp`
- Modify: `Source/BlueprintHelper/Public/UI/Review/SBlueprintHelperReviewPanel.h`
- Modify: `Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp`
- Test: `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [ ] **Step 1: Add cascade result**

Add:

```cpp
struct FBlueprintHelperReviewCascadeActionResult
{
    FBlueprintHelperReviewActionResult RootResult;
    TArray<FString> RemovedChildChangeIds;
    bool bChildrenRemoved = false;
};
```

Add service method:

```cpp
FBlueprintHelperReviewCascadeActionResult RejectLifecycleRootVisibleChange(
    const FBlueprintHelperReviewVisibleChange& Root,
    const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges) const;
```

- [ ] **Step 2: Implement root reject semantics**

Call existing `RejectVisibleChange(Root)` first.

If root reject succeeds and root has `bRejectRemovesChildren=true`, mark same-asset children with `ParentChangeId == Root.ChangeId` as rejected in the persisted record and set:

```text
NeedsActionReason = cascade_removed_by_asset_lifecycle_root:<rootId>
```

Return their change ids in `RemovedChildChangeIds`.

If root reject fails or needs action, return no removed children.

- [ ] **Step 3: Render lifecycle tree**

Tree structure:

```text
AssetPath
  新增了[BP_SmokeActor]蓝图资产
    新增了[SmokeComp]组件
    修改了[SmokeHP]变量
    替换了[EventGraph]图表
```

If no root exists, keep current flat asset children.

- [ ] **Step 4: Update panel actions**

When selected change is lifecycle root:

- Reject calls cascade method.
- On success, remove root and returned children from `ChangeItems`.
- Select next visible item from same asset if any, otherwise next asset group.
- Accept calls normal `AcceptVisibleChange` and does not remove children.

RejectAllAssetChange:

- If lifecycle root exists, reject it first.
- If root reject succeeds, skip child rejects.
- If root reject fails, continue current per-item behavior only for non-root changes if the user explicitly selected RejectAll.

- [ ] **Step 5: Add action/UI tests**

Add tests:

```text
BlueprintHelper.Review.Action.RejectLifecycleRootRemovesChildren
BlueprintHelper.Review.Action.RejectLifecycleRootFailureKeepsChildren
BlueprintHelper.Review.UI.TreeNestsChangesUnderLifecycleRoot
BlueprintHelper.Review.UI.AcceptLifecycleRootDoesNotAcceptChildren
```

### Task 3: Row Highlight Model For Non-Graph Panels

**Files:**
- Modify: `Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewSurfacePresenter.h`
- Modify: `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewSurfacePresenter.cpp`
- Modify: `Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewAssetPresenters.h`
- Modify: `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewAssetPresenters.cpp`
- Test: `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [ ] **Step 1: Add shared row highlight state**

Add a small presenter-owned model:

```cpp
struct FBlueprintHelperReviewRowHighlight
{
    FString ChangeId;
    FString TargetKey;
    EBlueprintHelperReviewChangeKind ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
    bool bSelected = false;
};
```

Each presenter builds a map:

```cpp
TMap<FString, FBlueprintHelperReviewRowHighlight> TargetKeyToHighlight;
```

- [ ] **Step 2: Add shared color helper**

Use the existing change-color source where possible. Required fill alpha:

```cpp
Color.A = 0.6f;
```

Selected Row may use a stronger outline but must not cover text.

- [ ] **Step 3: Remove row overlay usage**

For `Components`, `MyBlueprint`, `Details`, and `UMGWidgetTree`, stop calling the generic overlay frame builder for precise row frames.

Keep debug output:

```text
ReviewRowHighlight change=... surface=MyBlueprint target=... result=shown mode=row_background
ReviewRowHighlight change=... surface=MyBlueprint target=... result=pending reason=row_not_visible
```

Do not emit `ReviewFrameGeometry ... mode=review_list result=shown` for these built-in panels.

- [ ] **Step 4: Selected Row actions**

For owned rows, append an action slot on the right side only when `bSelected=true`:

```text
[row label ... spacer ... Accept Reject]
```

For native rows where right-side injection is not safe, place a small row-local floating action overlay anchored inside the row widget bounds. This is still Row-owned, not panel-level anchor overlay.

- [ ] **Step 5: Add row behavior tests**

Add tests:

```text
BlueprintHelper.Review.UI.NonGraphPanelsDoNotUseAnchorOverlay
BlueprintHelper.Review.UI.SelectedRowShowsAcceptRejectActions
BlueprintHelper.Review.UI.UnselectedRowHasNoActions
BlueprintHelper.Review.UI.RowHighlightAlphaIsPointSix
```

### Task 4: Components, MyBlueprint, WidgetTree, Details Row Application

**Files:**
- Modify: `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewSurfacePresenter.cpp`
- Modify: `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewAssetPresenters.cpp`
- Modify: `Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp`
- Test: `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [ ] **Step 1: Components**

Resolve component targets through the current `SSubobjectBlueprintEditor` row path:

```cpp
FindSlateNodeForVariableName(FName(*Candidate));
Tree->WidgetFromItem(Node);
```

When the row resolves, apply row background highlight to that row. If the row is not visible, call `RequestScrollIntoView(Node)` once and report pending.

- [ ] **Step 2: MyBlueprint**

Owned MyBlueprint `STreeView` rows read `TargetKeyToHighlight` during row generation. Highlight target kinds:

```text
blueprint_variable
signature
event_dispatcher
function
macro
blueprint_class
```

Variable default changes still show the variable in MyBlueprint and select/details-highlight the property in Details.

- [ ] **Step 3: WidgetTree**

Owned WidgetTree `STreeView` rows highlight:

```text
umg_widget
umg_widget_property
```

WidgetTree changes must not appear in Details as primary diff rows.

- [ ] **Step 4: Details**

Details only handles details/property surfaces:

```text
class_default_property
blueprint_default
blueprint_class
signature details
dispatcher details
interface/class settings
```

Use `ScrollPropertyIntoView` and `HighlightProperty` for native property focus. If a real details row widget can be resolved, apply Row background. If not, log pending and do not draw panel overlay.

- [ ] **Step 5: Regression tests**

Add tests:

```text
BlueprintHelper.Review.UI.ComponentRowHighlightsSmokeComp
BlueprintHelper.Review.UI.MyBlueprintRowHighlightsSmokeHP
BlueprintHelper.Review.UI.MyBlueprintRowHighlightsCustomEventSignature
BlueprintHelper.Review.UI.WidgetTreeRowHighlightsSmokeText
BlueprintHelper.Review.UI.DetailsHighlightsClassDefaultProperty
```

### Task 5: Graph Review Evidence And Stable Bounds

**Files:**
- Modify: `Source/BlueprintHelper/Public/Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h`
- Modify: `Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.cpp`
- Modify: `Source/BlueprintHelper/Private/Systems/Transactions/BlueprintHelperTransactionJournalService.cpp`
- Modify: `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewGraphBounds.cpp`
- Test: `Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`
- Test: `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [ ] **Step 1: Add structured graph review node anchor**

Add to graph append journal types:

```cpp
struct FBlueprintHelperGraphReviewNodeAnchor
{
    FString NodePath;
    FString NodeGuid;
    FString DisplayLabel;
    FVector2D GraphPosition = FVector2D::ZeroVector;
    FVector2D GraphSize = FVector2D(360.0f, 180.0f);
    bool bHasGraphBounds = false;
};
```

Add array to journal record:

```cpp
TArray<FBlueprintHelperGraphReviewNodeAnchor> CreatedNodeAnchors;
```

Keep `CreatedNodePaths` for backwards compatibility.

- [ ] **Step 2: Record anchors from ReplaceBlueprintGraph**

For each imported replacement node:

```cpp
Anchor.NodePath = Node->GetPathName();
Anchor.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
Anchor.DisplayLabel = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
Anchor.GraphPosition = FVector2D(Node->NodePosX, Node->NodePosY);
Anchor.GraphSize = FVector2D(
    Node->NodeWidth > 0 ? Node->NodeWidth : 360.0f,
    Node->NodeHeight > 0 ? Node->NodeHeight : 180.0f);
Anchor.bHasGraphBounds = true;
```

Only add block id targets when `OriginalBlockId` is non-empty.

- [ ] **Step 3: Convert anchors into review targets**

`TransactionJournalService` must create graph atomic targets with:

```cpp
Target.TargetKind = TEXT("graph_node");
Target.NodeGuid = Anchor.NodeGuid;
Target.TargetKey = FString::Printf(TEXT("graph:%s:node:%s"), *GraphName, *Anchor.NodeGuid);
Target.DisplayLabel = Anchor.DisplayLabel;
Target.bHasGraphBounds = Anchor.bHasGraphBounds;
Target.GraphPosition = Anchor.GraphPosition;
Target.GraphSize = Anchor.GraphSize;
```

For legacy `CreatedNodePaths`, parse GUID-like strings into `NodeGuid`. For object paths, keep the current node-name fallback.

- [ ] **Step 4: Add aggregate graph bounds**

For block-level or transaction-level Graph changes, compute the union of created node anchors and set recorded bounds on that aggregate target. This gives `ReplaceBlueprintGraph` a stable rectangle even when individual node names change.

- [ ] **Step 5: Tighten debug**

When Graph route is shown but bounds fail, include:

```text
hasNodeGuidTargets=N
hasRecordedBounds=N
anchorSource=structured|legacy|none
```

- [ ] **Step 6: Add graph tests**

Add or update tests:

```text
BlueprintHelper.GraphWrite.Replace.EmitsStructuredReviewNodeAnchors
BlueprintHelper.GraphWrite.Replace.GraphTargetsCarryRecordedBounds
BlueprintHelper.Review.GraphBounds.UsesNodeGuidBeforeDisplayLabel
BlueprintHelper.Review.GraphBounds.UsesRecordedBoundsWhenNodeMatchFails
BlueprintHelper.Review.UI.ReplaceBlueprintGraphCreatesDiffBlock
```

### Task 6: Readable Titles And Asset Factory Naming

**Files:**
- Modify: `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewSurfacePresenter.cpp`
- Test: `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [ ] **Step 1: Asset factory title**

`asset_factory` titles must use asset kind:

```text
新增了[BP_SmokeActor]蓝图资产
新增了[WBP_Smoke]Widget蓝图资产
新增了[DT_Smoke]DataTable资产
新增了[ST_SmokeRow]结构体资产
```

Do not append `变量`.

- [ ] **Step 2: Graph title**

`ReplaceBlueprintGraph` should become:

```text
替换了[EventGraph]图表
```

or for known block/function scope:

```text
替换了[BH_SmokeFunc]图表实现
```

- [ ] **Step 3: Tests**

Add tests:

```text
BlueprintHelper.Review.UI.AssetFactoryTitleUsesAssetKind
BlueprintHelper.Review.UI.ReplaceBlueprintGraphTitleIsReadable
```

### Task 7: Performance And Debug Noise Control

**Files:**
- Modify: `Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp`
- Modify: `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewSurfacePresenter.cpp`
- Test: `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [ ] **Step 1: Stop per-resize overlay churn for Row panels**

Because non-Graph panels no longer draw overlay anchors, resizing panels must not rebuild row diff overlays every frame.

- [ ] **Step 2: Dedupe debug messages**

Deduplicate repeated route/highlight messages by key:

```text
surface + changeId + result + reason
```

Do not call `DebugMessageTextBox->SetText` on every duplicate during resize.

- [ ] **Step 3: Tests**

Add tests:

```text
BlueprintHelper.Review.UI.RowHighlightDoesNotScheduleOverlayRefreshLoop
BlueprintHelper.Review.UI.DebugDedupesRepeatedGeometryPendingMessages
```

## Verification Plan

### Build

```powershell
F:\UE_5.6\Engine\Build\BatchFiles\Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject" -WaitMutex -NoHotReload
```

Expected: build succeeds. Existing unrelated build issues must be reported separately, not fixed in this plan.

### Automation

Run targeted groups:

```text
Automation RunTests BlueprintHelper.Review.VisibleChange
Automation RunTests BlueprintHelper.Review.UI
Automation RunTests BlueprintHelper.GraphWrite.Replace
Automation RunTests BlueprintHelper.Review.GraphBounds
```

Required new/updated tests:

```text
BlueprintHelper.Review.Record.AssetFactoryChangeIsLifecycleRoot
BlueprintHelper.Review.Record.PendingChangesLinkUnderLifecycleRoot
BlueprintHelper.Review.Action.RejectLifecycleRootRemovesChildren
BlueprintHelper.Review.UI.TreeNestsChangesUnderLifecycleRoot
BlueprintHelper.Review.UI.NonGraphPanelsDoNotUseAnchorOverlay
BlueprintHelper.Review.UI.ComponentRowHighlightsSmokeComp
BlueprintHelper.Review.UI.MyBlueprintRowHighlightsSmokeHP
BlueprintHelper.Review.UI.MyBlueprintRowHighlightsCustomEventSignature
BlueprintHelper.Review.UI.WidgetTreeRowHighlightsSmokeText
BlueprintHelper.GraphWrite.Replace.EmitsStructuredReviewNodeAnchors
BlueprintHelper.GraphWrite.Replace.GraphTargetsCarryRecordedBounds
BlueprintHelper.Review.UI.ReplaceBlueprintGraphCreatesDiffBlock
BlueprintHelper.Review.UI.AssetFactoryTitleUsesAssetKind
```

### Manual Smoke

- Open `/Game/BlueprintHelper/Smoke/BP_SmokeActor`.
- Confirm asset creation Review appears as root: `新增了[BP_SmokeActor]蓝图资产`.
- Confirm `SmokeComp` row in Components has background highlight and selected-row Accept / Reject.
- Confirm `BH_SmokeCustomEvent` and `SmokeHP` rows in MyBlueprint have background highlight.
- Confirm class default property changes highlight the corresponding Details row.
- Select `ReplaceBlueprintGraph`; center Graph workspace must draw a diff block around affected nodes.
- Reject lifecycle root; child Reviews disappear from pending list only if root reject succeeds.
- Drag the ReviewPanel window and splitters; no obvious lag or debug spam loop.
- Export DebugBundle; confirm routing/debug evidence contains no `debug_export_refs`.

### Static Check

```powershell
git diff --check
```

Expected: no whitespace errors.

## Assumptions

- Row background styling is allowed to be implemented through row-owned `SBorder` or `STableRow` color attributes. If a native UE row cannot expose a mutable background attribute, the fallback is a row-local decorator inside that row only, not panel-level anchor overlay.
- Graph workspace remains the only surface allowed to use anchor/bounds diff blocks.
- Child Review destruction means removal from pending visible changes and persisted status update to `Rejected` with cascade reason.
- Root reject must not remove children until the root rollback/delete succeeds.
- This plan does not redesign DataTable/DataAsset native editor reuse; those center workspace presenters stay on their existing track.

## Execution Result 2026-05-10

- Implemented Tasks 1-7 in workspace.
- Lifecycle root metadata, tree nesting, accept/reject behavior, and reject cascade were added.
- Non-Graph built-in panels now use row background highlight instead of ReviewPanel-level anchor overlay cards.
- Graph write review evidence now includes structured node anchors and recorded bounds for ReplaceBlueprintGraph.
- UE build passed.
- Targeted automation passed:
  - `BlueprintHelper.Review.VisibleChange`: 15 success, 0 failed.
  - `BlueprintHelper.Review.UI`: 39 success, 0 failed.
  - `BlueprintHelper.GraphWrite.Replace`: 3 success, 0 failed.
  - `BlueprintHelper.Review.GraphBounds`: 2 success, 0 failed.
- `git diff --check` passed with only LF/CRLF working-copy warnings.
- Manual editor smoke is still required for live Slate visual confirmation.
