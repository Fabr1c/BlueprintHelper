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

## Assumptions

- 已采用设计文档的推荐决策：Stage 1 保持现有四列布局，先不扩展 `EBlueprintHelperReviewSurface`。
- UMG/DataTable/DataAsset 的专用 presenter 放到 Stage 3；Stage 1 只保证可加载、非空显示和正确路由。
- Signature 仅在实际创建或修改 graph node/block 时才发 Graph target。
- DataTable 创建按 asset summary 审查；DataTable 行写入按 row anchor 审查。
- 本计划不处理 UE Build 问题；验证以 targeted automation、用户侧手动 smoke 和 `git diff --check` 为准。
