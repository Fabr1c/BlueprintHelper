# ReviewPanel v2 执行计划

## Summary

目标是按已批准的 `BlueprintHelper_ReviewPanel_V2_Architecture_Design_20260509_CN.md` 执行第一轮架构改造：先修正资产上下文和 surface 路由合同，再把 Graph/Components/MyBlueprint/Details 从单体面板中拆出 presenter 边界。Stage 1 必须先交付可验证行为：非 Blueprint 资产能进入 ReviewPanel，Graph 不再为非 Graph anchor 画 fallback 框，现有 true Graph review 不回退。

## Public API / Types

- 新增 `FBlueprintHelperReviewAssetContext` 和 `EBlueprintHelperReviewAssetKind`，用于从 `AssetPath` 加载 `UObject`，并派生 `Blueprint`、`DataTable`、`DefaultObject` 等上下文。
- 收紧 `BlueprintHelperReviewShouldShowInGraph` 语义：当 `VisibleChange.AtomicTargets` 非空时，只有存在 `Surface == Graph` 的 target 才允许 Graph 路由；`GraphName` 只作为无 atomic target 的遗留 fallback。
- 新增轻量 presenter 接口或等价 helper 层：`GraphPresenter`、`BlueprintComponentsPresenter`、`MyBlueprintPresenter`、`ObjectDetailsPresenter`。第一轮先不扩展 `EBlueprintHelperReviewSurface`，UMG/DataTable/DataAsset 暂通过 `Details + target_kind` 兼容路由。
- `SBlueprintHelperReviewPanel` 保留 selection、Accept/Reject、Debug export 和整体布局所有权；资产加载、surface 判断、内容构建和 overlay 构建逐步下沉到 context/presenter。

## Implementation Plan

### Stage 1: Asset Context + Strict Graph Routing

1. 新增 Review asset context loader。
   - 加载顺序：规范化 object path -> 先 `FindObject<UObject>` -> 再确认 package 存在 -> `LoadObject<UObject>`。
   - `WidgetBlueprint` 判定为 `UWidgetBlueprint` 或 generated class 继承 `UUserWidget`。
   - `DataTable` 判定为 `UDataTable`。
   - `DataAsset` 判定为 `UDataAsset`。
   - 其他 UObject 归为 `GenericObject`。
   - 缺包或类型未知时返回 invalid context，不触发 Blueprint-only load warning。

2. 更新 `SBlueprintHelperReviewPanel` 的资产状态。
   - 用 `FBlueprintHelperReviewAssetContext ReviewAssetContext` 替代主路径中的 `ReviewBlueprint`。
   - 需要 Graph/MyBlueprint/Components 时从 context 取 `Blueprint`。
   - Details 默认显示 `AssetObject`；Blueprint 资产仍可按现有逻辑显示 Blueprint 或 CDO。
   - 非 Blueprint 资产选择时，Graph 区域显示非 Graph 占位，不创建空 K2 preview graph。

3. 修正 Graph routing 和 debug。
   - `AddGraphDiffBlocks` 只遍历 true Graph-routable changes。
   - selected change 不是 Graph-routable 时不创建 selected fallback rect。
   - Debug 增加 `ReviewRoute change=... surface=Graph explicitTargets=... graphTargets=... result=hidden|shown reason=...`。
   - 保留 true Graph case，例如 `tx_1778317276165`。

4. 让 Details 支持非 Blueprint。
   - DataTable asset creation 显示 asset summary/details。
   - GenericObject/DataAsset 显示 readonly details。
   - 暂不实现 DataTable row grid 和 UMG widget tree，只保证非空、可审查、可 Accept/Reject。

### Stage 2: Presenter Extraction

1. 把 Graph 构建、preview graph clone、diff block、jump 逻辑移到 Graph presenter。
2. 把 Components/MyBlueprint/Details 的 `BuildContent + BuildOverlay + ShouldShowChange` 移到对应 presenter。
3. 旧硬编码 overlay 几何保留为 `fallback_geometry`，并在 Debug 中明确标注。
4. `SBlueprintHelperReviewPanel` 只负责：布局、选择、服务调用、刷新 presenter、Debug 汇总。

### Stage 3: UMG/DataTable/DataAsset Presenters

1. `UMGWidgetTreePresenter`
   - WidgetBlueprint 资产显示 readonly widget tree。
   - `target_kind=umg_widget|umg_widget_property` 路由到 widget tree 或 property fallback list。
2. `DataTablePresenter`
   - `UDataTable` 显示 row struct summary 和 readonly row list。
   - asset creation 使用 `AssetSummary` 行为；row write 使用 `datatable_row` 行为。
3. `DataAssetPresenter`
   - DataAsset/generic UObject 显示 readonly details。
   - `object_property|data_asset_property` 通过 `property_path` 路由，无法稳定定位时显示 deterministic review-list fallback。

### Stage 4: Real Geometry Anchors

1. 对 Graph 继续使用已有 node bounds 和 recorded graph bounds。
2. 对 Components/MyBlueprint/Details/UMG/DataTable，优先尝试读取稳定 Slate row geometry。
3. 如果 Unreal 不暴露稳定 row geometry，移除“假精确”硬编码框，改为 deterministic review-list/card，不再伪装成精确行框。

### Stage 5: Panel Placement Contract Fixes

1. Final change list grouping and readable row text.
   - Left final-change list groups rows by the visible change's true `AssetPath`; a Widget Blueprint review must not absorb DataTable or DataAsset changes from the same transaction.
   - Each review row should show user-facing text first, for example `修改了[SmokeText]`, `修改了[DamageSmall]行`, `修改了[SmokeHealth]变量`.
   - Transaction id, `target_kind`, surface name, and raw anchor details remain available in debug export or tooltip-style diagnostics, not as the primary row title.

2. Blueprint and Widget Blueprint panel ownership.
   - Blueprint uses `Components + MyBlueprint + Graph`: component changes draw their diff overlay in the Components panel, variable / function / dispatcher / signature changes draw in My Blueprint, and graph changes draw in the center Graph workspace.
   - Widget Blueprint uses `WidgetTree + MyBlueprint + Graph`: Widget Tree replaces the Components position, My Blueprint stays in the lower left-middle panel, and the center workspace continues to display Graph.
   - Widget widget/property changes draw only inside Widget Tree. They must not be rerouted into the right Details panel and must not create Graph fallback overlays.
   - DataTable and DataAsset changes remain independent asset groups. They must not appear under a selected Widget Blueprint; their dedicated panel placement remains a separate follow-up decision.
   - The right Details panel is auxiliary for the selected review item: readable property summary, before/after text, reject/debug context, and diagnostic metadata. It does not own UMG, DataTable, or DataAsset primary diff overlays.

3. Diff fill style, padding, and fallback behavior.
   - Diff visuals should follow code-review style block highlighting: a translucent filled background over the changed row/block is the primary signal, with a subtle selected-state edge or stronger fill when selected.
   - Added changes use green fill, modified changes use yellow/amber fill, and removed changes use red fill. The existing semi-transparent base should stay readable over Slate text.
   - Geometry may expand beyond the exact row/block bounds. Default padding is 10 px; graph blocks or large block-level anchors may use up to 20 px.
   - Expanded geometry must stay clipped to its owning surface and must not cover unrelated rows as if they were part of the same change.
   - When a stable row/block geometry cannot be resolved, render the deterministic review-list/card inside the owning surface instead of drawing a fake precise overlay in Details.

### Stage 6: DataTable/DataAsset Main Workspace

1. Main workspace surface routing.
   - Blueprint and WidgetBlueprint keep Graph as the center workspace.
   - DataTable uses the DataTable presenter in the center workspace.
   - DataAsset and GenericObject use the DataAsset presenter in the center workspace.

2. Main workspace overlay ownership.
   - DataTable and DataAsset overlays are owned by the center workspace host.
   - WidgetTree remains in the structure panel, and Details remains details-only.
   - Graph keeps its existing graph-presenter diff block path instead of using the generic panel overlay host.

3. Fallback behavior.
   - DataTable/DataAsset first try stable Slate row geometry inside their center presenter content.
   - If row geometry is unavailable, the deterministic review-list/card is rendered in the center workspace, not in Details.

### Stage 7: Row Highlight, Graph Bounds Evidence, Lifecycle Root

Detailed execution plan: `Develop/Plan/ReviewPanel_RowHighlight_GraphBounds_LifecycleRoot_PLAN_20260510_CN.md`.

1. Non-Graph panel diff contract.
   - Components, MyBlueprint, Details, and WidgetTree no longer use the old anchor overlay path.
   - These panels directly highlight the resolved Row background with added/modified/removed colors.
   - Only the selected Row shows Accept / Reject on the right side.
   - Missing Row geometry logs pending/hidden debug and does not draw fake precise frames or text fallback cards.

2. Graph workspace contract.
   - The center Graph workspace keeps the existing anchor/bounds diff block model.
   - Graph route success is not enough to draw a block; graph review evidence must include node guid matches or recorded bounds.
   - Graph write tools, especially ReplaceBlueprintGraph, must emit structured node anchors and aggregate recorded bounds so `GraphDiff bounds failed matchedNodes=0 recordBounds=0` is eliminated for valid graph changes.

3. Asset lifecycle root contract.
   - `asset_factory` + added visible changes are lifecycle root reviews.
   - While the root is pending, same-asset child reviews are nested under it.
   - Rejecting the root successfully removes same-asset child reviews from pending review flow.
   - Accepting the root does not accept children; rejecting a child does not affect the root.

## Execution Progress

### 2026-05-09 Stage 1-6 Status

- Stage 1 Asset Context + Strict Graph Routing: Completed in workspace. Non-Blueprint assets can construct ReviewPanel, explicit non-Graph targets no longer route through Graph fallback, and true Graph visible changes remain routable.
- Stage 2 Presenter Extraction: Completed for the current four-panel boundary. Graph, Components, MyBlueprint, Details content/overlay routing now live behind presenter/helper boundaries while `SBlueprintHelperReviewPanel` keeps selection, layout, actions, refresh, and debug ownership.
- Stage 3 UMG/DataTable/DataAsset Presenters: Completed for independent presenter routing and readonly review surfaces. UMG widget tree, DataTable rows, and DataAsset properties have presenter-level routing and deterministic fallback behavior.
- Stage 4 Real Geometry Anchors: Completed for current Slate row geometry coverage. Stable row geometry is preferred; unresolved geometry falls back to deterministic review-list/card instead of fake precise Details overlays.
- Stage 5 Panel Placement Contract Fixes: Completed in workspace. Widget Blueprint uses `WidgetTree + MyBlueprint + Graph`; Widget Tree replaces the Components slot, Details no longer owns UMG/DataTable/DataAsset primary overlays, final-change rows use readable titles, and visible changes are grouped by true target `AssetPath`.
- Stage 6 DataTable/DataAsset Main Workspace: Completed in workspace. DataTable, DataAsset, and GenericObject reviews now replace the center Graph workspace with their dedicated presenter content and center-owned overlay host.

### 2026-05-09 Verification

- UE build passed with `F:\UE_5.6\Engine\Build\BatchFiles\Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject" -WaitMutex -NoHotReload`.
- Full grouped automation passed: `BlueprintHelper.Review` reported `69 success`, `3 succeededWithWarnings`, `0 failed`.
- Automation report: `G:\UnrealPractise\MrStone\Saved\Automation\BlueprintHelper_Review_Stage5_Final_20260509_224505`.
- Targeted UI automation passed after Stage 6: `BlueprintHelper.Review.UI` reported `29 success`, `0 warnings`, `0 failed`.
- Stage 6 UI automation report: `G:\UnrealPractise\MrStone\Saved\Automation\BlueprintHelper_Review_UI_MainWorkspace_20260509_230037`.
- `git diff --check` passed. Only LF/CRLF working-copy warnings were reported.
- Known automation warnings are environment or legacy fixture noise: `/Game/BP_Door` missing package and EOS no-connection log entries. No ReviewPanel assertion or automation error remains in this run.

### 2026-05-09 Bugfix: Automation Record Pollution

- User reported final-change list still showing `tx_save_umg_surface`, `tx_save_datatable_surface`, and `tx_save_dataasset_surface` under `/Game/BlueprintHelper/Smoke/WBP_WidgetSmoke`.
- Root cause: `BlueprintHelper.Review.Record.PreservesIndependentSurfaceStrings` persisted synthetic automation records into `Saved/BlueprintHelper/Review/Records` and left them pending. The synthetic record used the Widget Blueprint record path for DataTable/DataAsset changes, so ReviewPanel loaded automation leftovers as real pending review rows.
- Fix in workspace: the test now writes each synthetic visible change and atomic target with its true target asset path and deletes its temporary review record file after reload validation.
- Local cleanup performed: stale `review_archive_independent_surfaces_*.json` files containing those `tx_save_*` changes were removed from `Saved/BlueprintHelper/Review/Records`.
- Verification pending: rebuild and targeted automation are blocked if UnrealEditor/Rider is holding `UnrealEditor-BlueprintHelper.dll`.

### 2026-05-09 Bugfix: Blueprint Package Path Asset Context

- User reported `/Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke` true Graph review opening as `generic_object`, with Graph targets hidden and the center workspace showing `Object Details Summary`.
- Root cause: `FBlueprintHelperReviewAssetContext::MakeObjectPathFromAssetPath` treated pure long package paths such as `/Game/.../BP_TaskSpecSmoke` as already-valid object paths. The loader therefore did not normalize them to `/Game/.../BP_TaskSpecSmoke.BP_TaskSpecSmoke`, so Blueprint package-path review records could fall through to `GenericObject`.
- Fix in workspace: only paths containing an object separator `.` are accepted as object paths; pure package paths are expanded to `<Package>.<AssetName>`.
- Regression coverage added: `BlueprintHelper.Review.UI.AssetContextLoadsBlueprintFromPackagePath`.
- Verification:
  - UE build passed with `F:\UE_5.6\Engine\Build\BatchFiles\Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject" -WaitMutex -NoHotReload`.
  - `BlueprintHelper.Review.UI.AssetContextLoadsBlueprintFromPackagePath`: `1 success`, `0 warnings`, `0 failed`.
  - `BlueprintHelper.Review.UI.AssetContextLoads`: `5 success`, `0 warnings`, `0 failed`.
  - `BlueprintHelper.Review.UI.ReviewPanelKeepsTrueGraphVisibleChangeRoutable`: `1 success`, `0 warnings`, `0 failed`.

### 2026-05-10 Bugfix: Graph Anchor And Built-In Panel Fallback

- User reported that selecting `ReplaceBlueprintGraph` routed to Graph but did not draw a center workspace diff frame.
- Root cause: `ReplaceBlueprintGraph` wrote a graph review target even when `OriginalBlockId` was empty, but it did not write the newly imported graph nodes into `CreatedNodePaths`. The ReviewPanel route therefore had `graphTargets=1` while `GraphBounds` had no node, block metadata, or recorded bounds to match.
- Fix in workspace: `ReplaceBlueprintGraph` now records imported replacement nodes as review graph node anchors and skips empty block ids. The new graph node targets are usable by `FBlueprintHelperReviewGraphBounds::BuildBoundsForTargets`.
- User also reported that Components/MyBlueprint/WidgetTree/Details diff frames showed text and behaved like overlay cards instead of precise embedded highlights.
- Contract update: those built-in panels may only draw diff frames when stable Slate row geometry is available. If geometry is missing or partial, the presenter logs `ReviewFrameGeometry ... result=hidden reason=no_stable_slate_geometry|partial_slate_row_geometry` and does not render a text review-list fallback. DataTable/DataAsset center workspace fallback remains unchanged for now.
- Regression coverage added:
  - `BlueprintHelper.GraphWrite.Replace.EmitsReviewNodeAnchorsForDiffBounds`
  - `BlueprintHelper.Review.VisibleChange.PresenterOverlayHidesBuiltInPanelFallbackWithoutSlateRowGeometry`
  - updated `BlueprintHelper.Review.VisibleChange.PresenterOverlayFallsBackWhenSlateRowGeometryIsPartial` to assert hidden fallback rather than text-card rendering.
- Verification status: initial build was blocked before running the new tests by an existing `BlueprintHelperRequestValidator::GetConfiguredToken` declaration mismatch and UnrealEditor-held DLL link locks; those build issues are outside this ReviewPanel bugfix and are not handled here.

### 2026-05-10 Stage 7 Implementation Result

- Stage 7 row-highlight / graph-bounds / lifecycle-root plan has been implemented in workspace.
- Non-Graph built-in panels now use Row background highlight instead of ReviewPanel anchor overlay cards for Components, MyBlueprint, Details, and WidgetTree. Owned MyBlueprint and WidgetTree rows show selected-row Accept / Reject actions.
- Graph workspace keeps graph diff blocks, but `ReplaceBlueprintGraph` now records structured node anchors and recorded bounds so valid graph changes can resolve by node guid or persisted bounds.
- `asset_factory + added` visible changes are lifecycle roots. Same-asset pending child reviews are nested under the root; root reject cascades child removal only after root reject succeeds; root accept does not accept children.
- Fixed integration issue where lifecycle-root cascade was briefly wired into Accept during parallel implementation; final code keeps cascade only on Reject.
- Verification:
  - UE build passed: `F:\UE_5.6\Engine\Build\BatchFiles\Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject" -WaitMutex -NoHotReload`.
  - `BlueprintHelper.Review.VisibleChange`: 15 found / 15 success / 0 failed.
  - `BlueprintHelper.Review.UI`: 39 found / 39 success / 0 failed.
  - `BlueprintHelper.GraphWrite.Replace`: 3 found / 3 success / 0 failed.
  - `BlueprintHelper.Review.GraphBounds`: 2 found / 2 success / 0 failed.
  - `git diff --check` passed with only LF/CRLF working-copy warnings.
  - UE startup logs still include environment noise such as EOS no-connection and pre-session `LogAutomationTest: Error: Condition failed`; the targeted automation runs completed with exit code 0.

### 2026-05-10 Stage 8 Investigation: Title Cleanup, Asset-Scoped Row Highlight, MyBlueprint Native Parity

This note is research and plan sync only. No C++ behavior change is included in this update.

Code inspected:

- `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewSurfacePresenter.cpp`
- `Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewSurfacePresenter.h`
- `Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp`
- `F:\UE_5.6\Engine\Source\Editor\Kismet\Private\SMyBlueprint.cpp`
- `F:\UE_5.6\Engine\Source\Editor\Kismet\Public\SMyBlueprint.h`

Findings:

1. Asset creation title pollution.
   - `BuildReadableChangeTitle()` uses `GetReadableTargetName()` and `ExtractReadableTail()`.
   - `ExtractReadableTail()` strips `:`, `/`, and `.` segments, but it does not normalize underscore-encoded package names such as `_Game_BlueprintHelper_Smoke_BP_SmokeActor`.
   - `asset_factory` root changes therefore can show path noise inside `新增了[...]`.
   - Asset creation titles must prefer the reviewed `AssetPath` short asset name and asset kind, for example `新增了[BP_SmokeActor]蓝图资产`, not `新增了[_Game_BlueprintHelper_Smoke_BP_SmokeActor]变量`.

2. Built-in panel row highlight should be asset-scoped, not selected-change scoped.
   - `BuildRowHighlightOverlay()` already scans all `ChangeItems` under the current asset and uses `bSelected` only for row actions.
   - The contract should be locked in tests: selecting an asset root or any same-asset review row must still make Components, MyBlueprint, Details, and WidgetTree show all routable pending row highlights for that asset.
   - Accept / Reject buttons remain selected-row only.

3. Section rows can be falsely highlighted.
   - Section rows in owned MyBlueprint currently use empty `SearchText`.
   - `FindRowHighlightEntry()` falls through to `GeometrySearchTextMatches()`.
   - `GeometrySearchTextMatches(SearchText, Target)` can match an empty normalized row text because `TargetTerm.Contains(NormalizedRow)` is true for an empty string.
   - This explains section headers such as `Graphs` / `Variables` turning into diff-colored rows.
   - Empty search text must never resolve a row highlight or row action. Section rows should be non-review rows with gray header background.

4. Current MyBlueprint presenter is not native-parity.
   - `FBlueprintHelperReviewMyBlueprintPresenter::BuildContent()` currently creates one `Graphs` section and adds `UbergraphPages`, `FunctionGraphs`, `MacroGraphs`, and `DelegateSignatureGraphs` under it, then adds `Variables` and `Dispatchers`.
   - UE native `SMyBlueprint` uses distinct sections from `NodeSectionID`: `Graphs`, `Functions`, `Macros`, `Variables`, `Event Dispatchers`, and related optional sections.
   - UE native rows are generated by private `SMyBlueprint::CollectAllActions()` / `OnCreateWidgetForAction()` and a private `SGraphActionMenu` context tied to `FBlueprintEditor`, command lists, edit mode, action selection, and context menus.
   - Directly reusing native generated row widgets inside ReviewPanel is high risk without embedding a real Blueprint editor context. The safer short path is source-data and visual parity in the owned readonly presenter.

5. Graph workspace diff block still needs live-record verification.
   - Stage 7 added structured graph node anchors and recorded bounds for new `ReplaceBlueprintGraph` writes.
   - If live pending records still show `GraphDiff bounds failed matchedNodes=0 recordBounds=0`, those records are either old-contract records created before Stage 7, or the current write path still failed to persist node guid / recorded bounds for that case.
   - Route success alone is not enough. Debug must show `hasNodeGuidTargets=1` or `hasRecordedBounds=1`, and the center workspace must draw a diff block.

6. Asset-level Reject currently fails in manual smoke.
   - User reported that asset-level Reject is ineffective after selecting asset creation/root review rows.
   - This is a Stage 8 blocker because `asset_factory + added` reviews are lifecycle roots: rejecting the root must reject/delete or roll back the created asset first, then remove same-asset child reviews only after the root operation succeeds.
   - The UI must distinguish normal child reject from lifecycle-root reject. Root-row Reject and `RejectAllAssetChange` must call the lifecycle-root cascade path, not silently fall through to a normal visible-change reject.
   - Failure must be explicit: if asset deletion/rollback cannot complete, keep the root and children pending or `needs_action`, write a clear debug reason, and do not remove child reviews.

Stage 8 implementation tasks:

1. Friendly title cleanup.
   - Add a sanitizer used by `BuildReadableChangeTitle()` for `asset_factory` targets.
   - Prefer `Change.AssetPath` short name for asset root changes.
   - Strip `_Game_BlueprintHelper_Smoke_` style package prefixes and any long package/object path residue from display text.
   - Use asset kind aware suffixes: `蓝图资产`, `Widget Blueprint 资产`, `DataTable 资产`, `Structure 资产`, `DataAsset 资产`, or `UObject 资产`.
   - Add regression coverage for Blueprint, WidgetBlueprint, DataTable, and Structure asset factory titles.

2. Asset-scoped row highlight contract.
   - Keep row background highlights based on all same-asset pending changes.
   - Keep Accept / Reject visibility based on the selected change only.
   - Add tests proving that selecting the lifecycle root still highlights same-asset component, variable, signature, dispatcher, and widget rows.
   - Add tests proving another asset's rows do not highlight.

3. Empty-search and section-row guard.
   - Make `GeometrySearchTextMatches()` return false if either normalized side is empty.
   - Make section rows skip `GetRowBackgroundColor()` and `GetRowActionsVisibility()` entirely.
   - Give owned section headers a UE-like gray background and no review key.
   - Add tests for `SectionRowsNeverHighlightFromEmptySearchText` and `EmptySearchTextDoesNotMatchAnyReviewTarget`.

4. MyBlueprint owned presenter native-parity pass.
   - Split sections to match UE order: `Graphs`, `Functions`, `Macros`, `Variables`, `Event Dispatchers`, plus `Review Anchors` only for targets that have no real Blueprint row.
   - Populate `Graphs` from `Blueprint->UbergraphPages`.
   - Populate `Functions` from `Blueprint->FunctionGraphs` and interface/overridable signature anchors when no graph row exists.
   - Populate `Macros` from `Blueprint->MacroGraphs`.
   - Populate `Variables` from non-delegate Blueprint-visible variables.
   - Populate `Event Dispatchers` from multicast delegate variables and `DelegateSignatureGraphs`.
   - For custom event signatures, prefer the same Graph/event child placement as UE `SMyBlueprint::GetChildEvents()` when a graph/event node exists; otherwise add a deterministic `Review Anchors` row.
   - Register aliases per row: `signature:Name`, `function:Name`, `event:Name`, `blueprint_variable:Name`, `event_dispatcher:Name`, `dispatcher:Name`, and raw `Name`.
   - Use UE style brushes/icons where available and keep section header backgrounds gray, not diff-colored.

5. WidgetTree parity guard.
   - Keep WidgetTree row highlight asset-scoped just like MyBlueprint.
   - Section/group rows in WidgetTree, if any, must not highlight from empty search text.
   - Add regression coverage for selecting a Widget Blueprint asset root while widget row changes remain highlighted.

6. Graph diff live-record follow-up.
   - Inspect current pending `ReplaceBlueprintGraph` records from `Saved/BlueprintHelper/Review/Records`.
   - If a record predates Stage 7 and lacks node anchors or bounds, mark it as old-contract debug evidence rather than treating the current code path as failed.
   - Add a targeted test that builds a ReviewPanel-visible `ReplaceBlueprintGraph` change from the same record shape emitted by the write service, then asserts a drawable graph diff block can be produced.
   - Debug success condition: `ReviewRoute surface=Graph result=shown` plus `GraphDiff bounds ... built=1` with `matchedNodes>0` or `recordBounds>0`.

7. Asset-level Reject lifecycle fix.
   - Trace `SBlueprintHelperReviewPanel::OnRejectChange`, root-row Reject buttons, and `OnRejectAll` to confirm which path is used for `bIsAssetLifecycleRoot`.
   - Ensure root-row Reject calls `RejectLifecycleRootVisibleChange()` for `bIsAssetLifecycleRoot` changes.
   - Ensure `RejectAllAssetChange` prefers the same lifecycle-root cascade when the selected asset has a pending lifecycle root.
   - Require a success-first cascade: root reject/delete must succeed before same-asset child reviews are removed.
   - Add debug evidence: `RejectLifecycleRoot id=... rootSuccess=... removedChildren=... reason=...`.
   - Add failure behavior: failed asset root reject must leave child reviews visible and set root status/reason rather than no-op.

Stage 8 verification targets:

- `BlueprintHelper.Review.UI.AssetFactoryTitleStripsPackagePrefix`
- `BlueprintHelper.Review.UI.AssetFactoryTitleUsesAssetKindSuffix`
- `BlueprintHelper.Review.UI.EmptySearchTextDoesNotMatchAnyReviewTarget`
- `BlueprintHelper.Review.UI.SectionRowsNeverHighlightFromEmptySearchText`
- `BlueprintHelper.Review.UI.AssetRootSelectionHighlightsSameAssetRows`
- `BlueprintHelper.Review.UI.RowActionsRemainSelectedChangeOnly`
- `BlueprintHelper.Review.UI.MyBlueprintPresenterBuildsNativeParitySections`
- `BlueprintHelper.Review.UI.MyBlueprintPresenterClassifiesFunctionsMacrosVariablesDispatchers`
- `BlueprintHelper.Review.UI.WidgetTreeAssetRootSelectionHighlightsWidgetRows`
- `BlueprintHelper.Review.UI.ReplaceBlueprintGraphLiveRecordCreatesDiffBlock`
- `BlueprintHelper.Review.UI.AssetRootRejectUsesLifecycleCascade`
- `BlueprintHelper.Review.UI.RejectAllAssetChangeUsesLifecycleCascade`
- `BlueprintHelper.Review.Action.AssetRootRejectFailureKeepsChildrenPending`

### Pending Follow-Up

- Manual editor smoke should confirm the visual result in live Slate: Blueprint Components/MyBlueprint/Graph overlays, WidgetBlueprint WidgetTree/MyBlueprint/Graph overlays, DataTable/DataAsset center workspace overlays, readable final-change row text, selected diff fill, Accept/Reject/RejectAll, and Debug export without `debug_export_refs`.
- Next execution candidate: implement Stage 8 above before returning to richer DataTable/DataAsset presenter content.

## Test Plan

- Stage 1 automation:
  - `ReviewShouldShowInGraphRequiresGraphTargetWhenTargetsAreExplicit`
  - `ReviewPanelConstructsWithDataTableVisibleChange`
  - `ReviewPanelConstructsWithGenericObjectVisibleChange`
  - `ReviewPanelDoesNotGraphRouteMyBlueprintOnlySignatureChange`
  - `ReviewPanelKeepsTrueGraphVisibleChangeRoutable`

- Stage 2 automation:
  - Graph presenter 只接受 Graph anchors。
  - Components presenter 只接受 component anchors。
  - MyBlueprint presenter 接受 signature/variable/dispatcher anchors。
  - ObjectDetails presenter 接受 details/object/data summary anchors。

- Stage 3 automation:
  - UMG widget anchor 路由到 UMG presenter。
  - DataTable row anchor 路由到 DataTable presenter。
  - DataAsset property anchor 路由到 DataAsset presenter。

- Manual smoke:
  - `/Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke` true Graph change 能跳转并绘制 Graph diff block。
  - `/Game/BlueprintHelper/Smoke/BP_ClassSettingsSmoke` MyBlueprint-only signature 不再绘制 Graph fallback。
  - `SmokeSceneComp` 和 `SmokeHealth` 显示在对应 presenter 或 fallback review-list。
  - `/Game/BlueprintHelper/Smoke/WBP_WidgetSmoke` 不再作为空 Blueprint review。
  - `/Game/BlueprintHelper/Smoke/DT_SmokeDamageTable` 显示非空 DataTable/details review。
  - Accept/Reject/RejectAll 状态更新不变。
  - Debug export 包含 routing 诊断，不出现 `debug_export_refs`。

## Stage 8 Execution Status - 2026-05-10

Implemented in code:

- Asset factory visible-change titles now prefer the short `AssetPath` asset name and strip encoded package prefixes such as `_Game_BlueprintHelper_Smoke_`.
- Asset factory titles now use asset-kind suffixes for Blueprint, Widget Blueprint, DataTable, Structure, DataAsset, and generic UObject instead of treating asset creation as a variable.
- Components, MyBlueprint, Details, and WidgetTree no longer render old text/anchor overlay frames for row-level review. They use row highlight state with 0.6 alpha; only the selected row exposes Accept / Reject actions.
- Empty row/search text no longer matches any review target. Section rows are non-review rows and keep gray section backgrounds.
- MyBlueprint readonly presenter now separates `Graphs`, `Functions`, `Macros`, `Variables`, and `Event Dispatchers`, with `Review Anchors` only for targets that do not have a stable row.
- WidgetTree keeps asset-scoped row highlight behavior when the selected change is the Widget Blueprint asset lifecycle root.
- `asset_factory + Added` reviews are lifecycle roots. The review tree nests same-asset children under the root when `ParentChangeId` is present.
- Root-row Reject and RejectAll asset flow call lifecycle-root cascade logic. Child review rows are removed/rejected only after root reject succeeds.
- Default asset-root Reject now handles `asset_factory` targets by deleting the created asset object through `ObjectTools::ForceDeleteObjects`; if delete fails, children remain pending or needs-action.
- Row highlight and frame-geometry debug messages are deduped to avoid repeated per-tick log spam during layout resizing.

Verified:

- UE build passed with `F:/UE_5.6/Engine/Build/BatchFiles/Build.bat MrStoneEditor Win64 Development -Project=G:/UnrealPractise/MrStone/MrStone.uproject -WaitMutex -FromMsBuild`.
- `Automation RunTests BlueprintHelper.Review.UI; BlueprintHelper.Review.Action; BlueprintHelper.Review.Record` passed: `succeeded=47`, `failed=0`, report at `G:/UnrealPractise/MrStone/Saved/Automation/ReviewPanelStage8/index.json`.
- `git diff --check` passed before the final doc update.

Still requires live editor smoke:

- Confirm Components/MyBlueprint/Details/WidgetTree row background highlight appears visually in the real ReviewPanel, not only through automation state.
- Confirm selected row Accept / Reject buttons appear on the right side of the selected row for native Slate rows.
- Confirm `ReplaceBlueprintGraph` live pending records contain node GUID or recorded bounds and produce `GraphDiff bounds ... built=1`. Old pending records without Stage 7 graph evidence should be treated as old-contract records.
- Confirm asset-level Reject on a real newly-created Blueprint root deletes the asset and clears same-asset child reviews.
- DataTable/DataAsset/Structure native editor row reuse is still not complete in this Stage 8 pass; current code keeps their existing summary/content path unless a later plan explicitly replaces those presenters.

## Assumptions

- 已采用设计文档的推荐决策：Stage 1 保持现有四列布局，先不扩展 `EBlueprintHelperReviewSurface`。
- UMG/DataTable/DataAsset 的专用 presenter 放到 Stage 3；Stage 1 只保证可加载、非空显示和正确路由。
- Signature 仅在实际创建或修改 graph node/block 时才发 Graph target。
- DataTable 创建按 asset summary 审查；DataTable 行写入按 row anchor 审查。
- 本计划不处理 UE Build 问题；验证以 targeted automation、用户侧手动 smoke 和 `git diff --check` 为准。
