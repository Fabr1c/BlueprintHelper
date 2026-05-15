# ReviewPanel Bug 跟踪 2026-05-10

> 2026-05-14 状态转移：本文中的未达期待、待验证项和阻塞项已迁移到 [BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md](BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md)。本文保留为历史上下文；开放项跟踪迁移完成，后续当前状态以总账为准。

本文记录用户手动 ReviewPanel smoke 中发现的问题、初步根因、修复进度和验证边界。早期条目的根因描述仍保留原始分析；后续修复状态以各 bug 条目下的“当前状态”和阶段回写为准，未完成或未验证项不可标记为完成。

## BUG-20260510-01: DataTable 每个 Column 都绘制 row diff/action

**当前状态**

- 2026-05-14 源码修复已落地，编译通过，运行态预检通过；尚未做人工 UI 视觉复核。
- 修复方式：DataTable 普通数据列只绘制 row 背景高亮，不再绘制 `Accept` / `Reject`；只有 `ReviewActions` column 生成行级操作入口。

**现象**

- DataTable 中同一行的 `Accept` / `Reject` 在 `Row`、`Damage`、`DisplayName` 等每个 column 内重复出现。
- 期望只在每一行末尾绘制一次操作入口，不应被 column 数量影响。

**初步根因**

- `BlueprintHelperReviewDataTablePresenter.cpp` 中 `SBlueprintHelperReviewDataTableRow::GenerateWidgetForColumn()` 对每个 column cell 都包了一层 `BuildRowHighlightShell()`。
- `BuildRowHighlightShell()` 内部包含 row background 和 `BuildRowActions()`，因此同一个 `datatable_row:<RowName>` 被每列重复注册并重复绘制操作按钮。
- 当前 `ReviewActions` column 已存在，但 `GenerateWidgetForColumn()` 对普通数据列和 actions 列走的是同一层 highlight shell 包装，导致 actions 没有被限制在行末。

**建议修复方向**

- DataTable row background 可以按 row 或 per-cell 统一染色，但 `Accept` / `Reject` 只能在 `ReviewActions` column 生成。
- 普通 cell 只显示单元格内容和必要背景，不调用带 actions 的 `BuildRowHighlightShell()`。
- `ReviewActions` column 使用同一 `datatable_row:<RowName>` search key 查询 selected state，并只生成一次 action bar。

## BUG-20260510-02: DataTable 同一资产出现 `/DT` 与 `/DT.DT` 两个 Review scope，中央面板缺少 ReviewDiff

**当前状态**

- 2026-05-14 源码修复已落地，编译通过，运行态预检通过；尚未做人工 UI 视觉复核。
- 修复方式：Review asset context 的 UI scope 改为 canonical package path；Final Changes 资产根和 scoped diff stack 比较均使用同一 package path key，避免 `/DT` 与 `/DT.DT` 被分成两个 scope。

**现象**

- 左侧最终改动同时出现：
  - `/Game/.../DT_BHSmokeDamage`
  - `/Game/.../DT_BHSmokeDamage.DT_BHSmokeDamage`
- 选择 `DT.DT` 形态时，中央 DataTable 面板没有出现预期 ReviewDiff。

**初步根因**

- `SBlueprintHelperReviewPanel::BuildChangeTreeItemsFromChangeItems()` 直接用 `Item->AssetPath` 作为分组 key，没有先 canonical 化为 package asset path。
- `FBlueprintHelperReviewAssetContext::LoadForAssetPath()` 会解析 package/object path，但 `Context.AssetPath` 仍保留输入的 raw path，因此 `/DT` 与 `/DT.DT` 可能在 UI state、row registry、row highlight state 中成为不同 scope。
- DataTable presenter 的 row 注册使用 `Context.AssetPath`，而 selected change / visible change 可能使用另一种 path 形态，导致 row highlight 和 central overlay 查不到同一个 asset scope。

**建议修复方向**

- ReviewPanel 层新增统一 asset scope 规范化函数：所有 change tree 分组、asset context、row registry、row highlight state 都使用 canonical package path。
- 保留原始 object path 仅用于加载对象，不作为 UI scope key。
- 对历史 pending records 做兼容：查询 row highlight 时同时支持 raw path alias，但写入新 state 时只写 canonical path。

## BUG-20260510-03: Structure 左侧只显示 Structure 资产变更，字段新增未显示为子 Review

**现象**

- ST 中央面板能显示 `Damage`、`DisplayName` 两个字段并生成行操作。
- 左侧最终改动只有 `新增了[ST_BHSmokeDamageRow]Structure 资产`，内部两个添加变量/字段操作没有以独立 review 或子 review 形式显示。
- 截图中还出现两个相同的 Structure 资产级变更，字段行没有对应的左侧条目。

**初步根因**

- TaskRuntime 的 asset factory review evidence 目前只为 asset factory step 生成一个 `asset_factory` atomic target。
- `BuildTaskRuntimeReviewEvidenceForStep()` 对 `asset_factory` 分支没有读取 structure `fields` 并生成 `struct_field` / `structure_field` atomic targets。
- `FBlueprintHelperReviewStructurePresenter::ShouldShowChange()` 已经支持 `struct_field` / `structure_field`，但上游没有产出字段级 target，所以左侧 change tree 只能显示资产级 review。
- 如果同一结构创建流程被拆成多个 TaskStep 或存在 `/ST` 与 `/ST.ST` path 差异，还会导致重复的资产级 Structure review。

**建议修复方向**

- AssetFactory/TaskRuntime evidence 对 Structure 创建需要生成：
  - 一个 asset lifecycle root：`asset_factory:structure`
  - 每个字段一个 child visible change 或 atomic target：`struct_field:<FieldName>`
- 字段 child review 应设置 `ParentChangeId` 指向 Structure asset root，使 Reject root 能级联移除字段 child review。
- Change tree 文案应使用字段名显示 `新增了[Damage]变量`、`新增了[DisplayName]变量`，而不是重复显示 Structure 资产。

## BUG-20260510-04: Final Changes 同一资产内没有按执行顺序 / 依赖顺序排序

**现象**

- Widget Blueprint 资产下，左侧最终改动显示顺序为：新增 `RootCanvas`、修改 `SmokeText_Text`、新增 `SmokeText`。
- 从实际执行依赖看，必须先新增 `RootCanvas`，再新增 `SmokeText`，之后才能修改 `SmokeText_Text`。

**初步根因**

- `SBlueprintHelperReviewPanel::BuildChangeTreeItemsFromChangeItems()` 只是按 `SourceItems` 当前数组顺序生成 asset root 和 leaf，没有执行序、依赖序或父子 anchor 排序逻辑。
- `FBlueprintHelperReviewStoreService::LoadPendingVisibleChanges()` 从 ReviewRecord 直接追加 `VisibleChanges`，并在最后只做 lifecycle root child link，没有重排同一资产内的 sibling changes。
- `FBlueprintHelperReviewStoreService::AddEvidenceAtomicTargets()` / `MergeReviewRecord()` 按 `LocationKey` 合并同位置变更时会更新 `ChangeId`、`LatestTransactionId` 和 `ChangeKind`，但不会把该 visible change 移到最新执行位置，也没有保存 `TaskStepIndex` / `ExecutionOrder` 这类稳定排序字段。
- 当前 WBP widget 创建与 widget property 修改之间没有显式 `ParentChangeId` 或 target dependency，ReviewPanel 无法知道 `umg_widget_property:SmokeText_Text` 必须排在 `umg_widget:SmokeText` 之后。

**建议修复方向**

- 在 Review evidence / visible change 中增加稳定顺序字段，例如 `execution_order`、`task_step_index`、`atomic_index`，并在序列化、反序列化、merge 时保留。
- Change tree 生成时按：asset lifecycle root -> 父 anchor 创建 -> 子 anchor 创建 -> 属性修改 / 签名修改 排序；同级再按 execution order 稳定排序。
- 对 UMG WidgetTree 加依赖规则：`umg_widget_property:<Widget>.<Property>` 必须挂到或排在 `umg_widget:<Widget>` 之后；如果创建 target 不存在，debug 输出 `order_dependency_missing`。
- 增加 automation：`ReviewChangeTreeOrdersWidgetCreationBeforeWidgetPropertyModification`。

## BUG-20260510-05: MyBlueprint Panel 没有复用 UE 原生图表 / 函数 / 宏 / 变量 / 事件分发器 Row

**现象**

- 当前 MyBlueprint Panel 虽然已有 `Graphs`、`Functions`、`Macros`、`Variables`、`Event Dispatchers` 分区，但视觉和行为仍不像 UE 原生“我的蓝图”面板。
- 用户期望复用 UE 原生的 Graph Row、Function Row、Macro Row、Variable Row、Event Dispatcher Row。

**初步根因**

- 当前实现是 `FBlueprintHelperReviewMyBlueprintPresenter::BuildContent()` 自建 `STreeView<FRowItem>`，数据直接来自 `Blueprint->UbergraphPages`、`FunctionGraphs`、`MacroGraphs`、`NewVariables`、`DelegateSignatureGraphs`。
- UE 原生 `SMyBlueprint` 的真实 Row 来自 `SGraphActionMenu` + `SBlueprintPaletteItem`，其 action 数据由 `SMyBlueprint::CollectAllActions()`、`GetChildGraphs()`、`GetChildEvents()`、`GetLocalVariables()` 生成。
- `SMyBlueprint::OnCreateWidgetForAction()`、`CollectAllActions()`、`GetChildEvents()`、`GetLocalVariables()` 都是 Kismet 模块内私有实现或依赖 `FBlueprintEditor`、command list、selection、context menu、focused graph。
- 因此当前 presenter 只是“自建只读近似面板”，不是“复用 UE 原生 Row”。这能获得稳定 row ownership，但会牺牲原生 row 样式、图标、分类、局部变量和覆盖函数等完整行为。

**建议修复方向**

- 明确 MyBlueprint 的两档目标：
  - 短期：保留 owned readonly presenter，但按 UE `NodeSectionID` 顺序、图标、section header 颜色、缩进和文字规则继续逼近原生。
  - 中期：抽一个 `MyBlueprintActionModel`，复刻 `SMyBlueprint::CollectAllActions()` 的数据生成逻辑，但 Row 仍由 ReviewPanel 持有，保证可高亮和可放置 Accept / Reject。
  - 长期：如果必须真正复用 `SBlueprintPaletteItem` / `SGraphActionMenu`，需要新增 Kismet 适配层或 fork/暴露 Kismet 私有接口，风险高且维护成本高。
- 需要把“复用 UE 原生 Row”单独列为架构级需求，不应继续当作当前 Stage 8 的已完成项。
- 增加 automation：`MyBlueprintPresenterBuildsNodeSectionParityRows`、`MyBlueprintPresenterDoesNotClaimNativeRowReuse`。

## BUG-20260510-06: Function 级 Review 与 Local Variables TitleRow 支持不完整

**现象 / 问题**

- 用户询问是否支持 Review 某个 Asset 内具体函数的改动。
- 当进入 Function 审阅后，MyBlueprintPanel 是否会显示局部变量 `Local Variables` TitleRow，目前不明确且当前 presenter 未实现。

**初步根因**

- 当前 `FBlueprintHelperReviewMyBlueprintPresenter::BuildContent()` 只读 Blueprint 全局结构：`UbergraphPages`、`FunctionGraphs`、`MacroGraphs`、`NewVariables`、`DelegateSignatureGraphs`。
- 当前 state 没有 `FocusedGraph` / `SelectedFunctionGraph` / `FunctionScope`，所以无法像 UE 原生 `SMyBlueprint::GetLocalVariables()` 一样根据 focused graph 读取局部变量。
- UE 原生 local variables 只在 `GetLocalActionsListVisibility()` 可见且存在 focused graph 时加入 `NodeSectionID::LOCAL_VARIABLE`。
- Review target 侧目前能看到 `local_variable` 能力和 Graph patch 的 local variable ref，但 ReviewPanel routing 没有定义稳定 target kind，例如 `local_variable:<FunctionName>:<VarName>` 或 `function_local_variable:<FunctionName>:<VarName>`。

**建议修复方向**

- 定义 Function Review Scope 合同：selected change 带 `GraphName` / `FunctionName` / `FunctionScopeKey` 时，MyBlueprint presenter 进入函数局部视图。
- 为函数内改动增加 target kinds：
  - `function_graph:<FunctionName>`
  - `function_signature:<FunctionName>`
  - `local_variable:<FunctionName>:<VarName>`
  - `local_variable_default:<FunctionName>:<VarName>`
- MyBlueprint presenter 在函数 scope 下增加 `Local Variables` section，并从 `UK2Node_FunctionEntry::LocalVariables` 或 schema `GetLocalVariables()` 读取局部变量。
- 中间 Graph workspace 仍负责函数图内节点/连线 diff；MyBlueprint 只负责函数入口、签名、局部变量目录类 review。
- 增加 automation：`MyBlueprintFunctionScopeShowsLocalVariablesSection`、`LocalVariableReviewRoutesToFunctionScopedMyBlueprintRow`。

## BUG-20260510-07: Graph 事件 / 节点类 Review Reject 因节点锚点失效进入 needs_action

**现象**

- 选择 `BHSmoke_Inserted0` 一类 Graph 事件变更后点击 Reject，日志显示：
  - `Reject change id=tx_1778424323808 success=0 status=needs_action message="current_hash_unavailable:node_not_found:K2Node_CustomEvent_2"`
  - 同一变更还有 `GraphDiff jump missed`、`GraphDiff bounds failed ... reason=no_real_graph_anchor`。
- 这些事件类 Review 仍显示为 pending / needs_action，无法通过当前 Panel 直接 Reject。

**初步根因**

- `FBlueprintHelperReviewActionService::RejectVisibleChangeWithDefaultDispatcher()` 在真正执行 rollback 前，会先调用 `FBlueprintHelperReviewHashService::ComputeAtomicTargetHash()` 校验当前目标 hash。
- 当前 hash 计算只按记录的 `Target.TargetKey` / `Target.NodeGuid` 解析出一个节点名，再通过 `FindNodeByName()` 精确匹配 `Node->GetName()`；日志中的目标是 `K2Node_CustomEvent_2`，但当前 Graph 中该 UObject 名称已经不存在。
- `CollectRollbackNodesForTarget()` 也存在同类问题：即使 `Target.NodeGuid` 非空，也会把 GUID 放进 `NodeName`，但后续仍只比较 `Node->GetName() == NodeName`，没有按 `Node->NodeGuid` 匹配。
- 这说明 Graph Review 目前仍过度依赖易变的 K2 node object name。Graph 重建、节点删除重建、Replace/Merge/Patch 之后，`K2Node_CustomEvent_N` 序号可能变化，导致 Diff 可以通过 fallback 画出某些框，但 Reject 的真实 current hash / rollback anchor 找不到节点。
- `GraphDiff bounds failed ... hasNodeGuidTargets=0 hasRecordedBounds=0 anchorSource=none` 进一步说明失败变更缺少稳定 node guid / recorded bounds / structured anchor，只剩旧 node name。

**建议修复方向**

- Graph atomic target 必须优先持久化稳定锚点：
  - `node_guid`
  - structured graph anchor
  - recorded graph bounds
  - rollback journal 中的 created node path / guid alias
- `FBlueprintHelperReviewHashService::FindNodeByName()` 或新增 resolver 需要支持 `NodeGuid` 匹配，且 `ComputeGraphNodeHash()` 不能把 GUID 当 UObject name 使用。
- `CollectRollbackNodesForTarget()` 需要同样支持 `NodeGuid`、structured anchor、block id，多目标回滚时不能只靠 `Node->GetName()`。
- 如果旧记录只有 `K2Node_*` 名称且当前图找不到节点，应明确进入 `needs_action`，但 Debug 需要输出 `unstable_node_name_anchor`，并建议重新生成 Review 记录；不要静默尝试错误节点。
- 增加 automation：
  - `ReviewRejectGraphNodeUsesNodeGuidWhenObjectNameChanges`
  - `ReviewRejectGraphNodeReportsUnstableNodeNameAnchor`
  - `GraphDiffAndRejectShareSameAnchorResolver`

## BUG-20260510-08: Reject 创建资产 root 后当前 Panel 持有资产引用，子 Review 和资产显示不会立即消失

**现象**

- Reject 创建 `BP_BHSmokeActor` 的资产级 Review 后，资产内容被回退成空蓝图，但 Content Browser / ReviewPanel 当前页仍能看到该资产或其子 Review。
- 关闭并重新打开 ReviewPanel 后，Reject 的资产和相关 Review 才全部消失。
- 用户判断当前仍停留在该 Asset 审阅页面，导致指针未释放，资产删除和 Review 刷新没有即时完成。

**初步根因**

- `SBlueprintHelperReviewPanel::OnRejectChange()` 对 asset lifecycle root 的流程是先调用 `ReviewActionService->RejectLifecycleRootVisibleChange()`，之后才更新 `ChangeItems`、`SelectedChange`、`ReviewAssetContext`、Graph/Details/Components/MyBlueprint 内容。
- `RejectAssetFactoryTargetWithDefaultDispatcher()` 直接 `FindObject/LoadObject` 后调用 `ObjectTools::ForceDeleteObjects()`。此时 ReviewPanel 仍可能持有：
  - `ReviewAssetContext.AssetObject / Blueprint / DefaultObject`
  - Graph preview/editor widget
  - Components / MyBlueprint presenter state
  - Details / KismetInspector 当前选择对象
- 生命周期 root cascade 只移除 `ParentChangeId == Root.ChangeId` 且仍为 `Pending` 的 child change。若子 Review 没有稳定 `ParentChangeId`，或 root `ChangeId` 在 merge 后变化，内存树不会把这些事件 / 组件 / 变量 child 一起移除。
- 重新打开后之所以消失，是因为 `LoadPendingVisibleChanges()` 在全局查询时会跳过缺失资产包的 record；这说明持久层结果可能已经变化，但当前 Panel 没有在 root reject 后按 store 重新拉取并释放旧 UI 引用。

**建议修复方向**

- Asset lifecycle root Reject 前，Panel 应先执行目标资产的 UI detach：
  - 清空 `SelectedChange`
  - 清空 `ReviewAssetContext`
  - `GraphEditorBox` 切到空状态
  - Details / KismetInspector 清空 selection
  - Components / MyBlueprint / WidgetTree presenter 清空 state
- Reject root 成功后，不应只本地 `RemoveAll` 已知 child；需要按 canonical asset path 从 ReviewStore 重新 `LoadPendingVisibleChanges()`，再重建 change tree。
- lifecycle root 的 child 关系需要改成可恢复合同：同资产内由 asset_factory root 派生的 graph/component/signature/variable/widget/datatable/structure child 必须有稳定 `ParentChangeId` 或 `ParentAssetLifecycleKey`，root ChangeId merge 后仍能 cascade。
- 对删除资产失败和 delayed unload 要分开标记：
  - `asset_delete_failed_referenced_by_review_panel`
  - `asset_deleted_pending_editor_unload`
  - `asset_deleted_and_review_reloaded`
- 增加 automation / manual smoke：
  - `ReviewRejectAssetLifecycleRootClearsSelectedAssetContextBeforeDelete`
  - `ReviewRejectAssetLifecycleRootReloadsPendingChangesFromStore`
  - `ReviewRejectAssetLifecycleRootCascadesChildrenWithoutOpeningPanelAgain`
  - 手动验证：Reject 新建 BP 后，不关闭窗口也应立即清空该资产 root 和所有 child Review，Content Browser 不再显示半删除状态。

## 2026-05-13 Physics Door ReviewPanel live bug dedupe

### PD-RP-01: Review visible change collapse fails for repeated variables/components

- Source: physics-door CLI TaskSpec live Editor ReviewPanel observation.
- Dedupes user-reported Bug1 and Bug6 into one root issue.
- Observed: four variables each show three identical `修改了[xxx]变量` Review records; the three added components also each show two Review records.
- Expected: Review system should collapse repeated writes to the same atomic target and show one final visible change per variable/component/function/etc., using the latest write operation.
- Relation to prior docs: existing ReviewStore / visible-change contract already expects final visible changes rather than raw transaction rows; this entry records the live regression.

### PD-RP-02: Component rows miss row-background diff highlight

- Source: physics-door CLI TaskSpec live Editor ReviewPanel observation.
- Observed: DoorFrame, DoorPanel, and DoorHingeConstraint are added, but the Components panel does not color the changed component rows.
- Expected: apply BG-color to the changed row itself; do not render a panel-level overlay card.
- Relation to prior docs: row-background highlight is already the intended design; this is a live failure instance.

### PD-RP-03: GraphPanel diff block opacity should be 0.35

- Source: physics-door CLI TaskSpec live Editor ReviewPanel observation.
- Observed: center GraphPanel diff block opacity is too high.
- Expected: lower GraphPanel diff block fill alpha to 0.35.
- Relation to prior docs: previous row-highlight docs mention 0.6 for non-Graph rows; no exact prior bug for GraphPanel alpha 0.35 was found.

### PD-RP-04: ReviewAnchor appears in MyBlueprint where native rows are expected

- Source: physics-door CLI TaskSpec live Editor ReviewPanel observation.
- Observed: MyBlueprint panel displays ReviewAnchor entries, which does not match the UE editor's My Blueprint panel content.
- Expected: use native-parity rows for normal Blueprint content; ReviewAnchor fallback should appear only for targets that genuinely have no stable row.
- Relation to prior docs: related to MyBlueprint native-parity and ReviewAnchor fallback notes; this entry records the concrete live bug.

### PD-RP-05: Rejecting InitializeDoor also rejects sibling events

- Source: physics-door CLI TaskSpec live Editor ReviewPanel observation.
- Observed: Rejecting InitializeDoor also rejected OpenDoor and CloseDoor transactions.
- Expected: normal selected-row Reject affects only the selected visible change / atomic target. Cascade is only valid for asset lifecycle root Reject after root success.
- Relation to prior docs: existing Review E2E docs state selected Reject should not cascade to unselected targets; this entry records the live violation.

### PD-RP-06: New signature reviews are not grouped under asset-creation root

- Source: physics-door CLI TaskSpec live Editor ReviewPanel observation.
- Observed: four newly added signature Review records are not grouped under the created-asset Review record.
- Expected: asset creation Review should act as the lifecycle root for same-asset child changes; new signatures created as part of the same asset creation / feature flow should be nested under that root.
- Relation to prior docs: lifecycle-root grouping is already part of the ReviewPanel V2 contract; this entry records the concrete live failure.
## 2026-05-13 修复进度回写

状态说明：本轮仅做源码修复，未执行构建或编辑器验证；因此未将未验证项标记为完全完成。

1. PD-RP-01 重复 Review 记录
   - 状态：源码修复已落地，待验证。
   - 处理：pending visible changes 加载阶段增加 latest-wins 收敛，按资产/Surface/Graph/TargetKey 去重并保留最新记录。

2. PD-RP-03 GraphPanel diff 框透明度过高
   - 状态：源码修复已落地，待验证。
   - 处理：Graph diff block 填充透明度调整为 0.35。

3. PD-RP-04 ReviewAnchor 泄漏到 MyBlueprint
   - 状态：源码修复已落地，待验证。
   - 处理：MyBlueprint presenter 增加 Ubergraph CustomEvent 行，物理门测试中的事件签名不再需要退回 ReviewAnchor。

4. PD-RP-05 Reject 单个事件误拒绝同事务内其他事件
   - 状态：源码修复已落地，待验证。
   - 处理：TaskSpec evidence 的 visible change id 改为 transaction id + visual group key，避免多个可见改动共享 ChangeId。

5. PD-RP-06 新增签名未归到创建资产 Review root
   - 状态：源码修复已落地，待验证。
   - 处理：生命周期 root 链接和 ChangeTree 资产分组使用规范化 package path。

6. CLI TaskSpec Review Panel 动态刷新
   - 状态：源码修复已落地，待验证。
   - 处理：ReviewPanel 不再轮询；改为 `execute_task_plan` 结束后由 ReviewStoreService 广播一次 pending review changed 事件，ReviewPanel 收到事件后刷新 UI。

7. PD-RP-02 Components Panel Row 背景 diff
   - 状态：源码修复已落地，待验证。
   - 处理：native Components row 实际为 STableRow/SBorder 派生；组件行定位成功后直接设置该 row border background color，不再只接受 `SBorder` 类型名。

9. 最终改动面板新增签名未挂到新增资产 root
   - 状态：源码修复已落地，待验证。
   - 处理：ChangeTree 构建阶段增加同资产新增资产 root 的兜底挂载；即使签名记录缺少 `ParentChangeId`，也会作为新增资产 root 的叶子显示。

10. MyBlueprint 宏分类缺失
    - 状态：源码修复已落地，待验证。
    - 处理：Macros section 改为常驻显示，避免空宏分类被清理。

11. 暂时跳过项
   - 状态：未完成。
   - 范围：字段变更/架构变更类问题继续跳过，包括旧文档中需要新增排序字段、重做 MyBlueprint 原生 row 复用策略、函数局部变量 Section 数据模型、Graph 节点稳定锚点等问题。
## 2026-05-13 ReviewPanel Accept/Reject Persistence Fix

状态说明：本轮仅做源码修复和 CLI/Bridge 命令接入，未执行构建、CLI build 或编辑器验证。

1. Final Changes 面板 Accept/Reject GraphPanel 同一条 Review 无效
   - 状态：源码修复已落地，待验证。
   - 处理：ReviewActionService 不再只解析第一个持久化 ReviewRecord；对一个可见 Review 项会解析所有匹配的 ReviewRecord + target_keys，并逐个回写 Accept/Reject。

2. 组件/变量 Review Accept/Reject 后重开 ReviewPanel 又出现
   - 状态：源码修复已落地，待验证。
   - 处理：针对 latest-wins 合并后的可见项，Accept/Reject 会回写所有同资产同 target_key 的 pending 持久化记录，避免旧 pending record 在重新加载时复活。

3. DebugBundle 排查能力
   - 状态：源码接入已落地，待验证。
   - 处理：Bridge 增加 `list_debug_cases` 与 `export_debug_bundle`；CLI tool surface 增加 `blueprinthelper_list_debug_cases` 与 `blueprinthelper_export_debug_bundle`。

## 2026-05-14 ReviewPanel DataTable 修复闭环

状态说明：本轮完成 BUG-20260510-01 与 BUG-20260510-02 的源码修复、C++ 编译、编辑器启动和运行态预检；未做人工 UI 视觉复核，因此不标记为 UI 完全验收。

1. BUG-20260510-01 DataTable 每个 Column 都绘制 row action
   - 状态：源码修复已落地，编译通过，待人工 UI 复核。
   - 处理：`SBlueprintHelperReviewDataTableRow::GenerateWidgetForColumn()` 区分普通列和 `ReviewActions` 列；普通列只用 row background highlight shell，`ReviewActions` 列才绘制 `Accept` / `Reject`。
   - 期望结果：同一 DataTable row 的 `Accept` / `Reject` 只在行末 actions column 出现一次，不再随 column 数量重复。

2. BUG-20260510-02 `/DT` 与 `/DT.DT` Review scope 分裂
   - 状态：源码修复已落地，编译通过，待人工 UI 复核。
   - 处理：`FBlueprintHelperReviewAssetContext::LoadForAssetPath()` 将 `Context.AssetPath` 规范化为 package path；`SBlueprintHelperReviewPanel` 的资产树显示和 scoped diff stack 过滤统一使用 canonical package path key。
   - 期望结果：Final Changes 中同一 DataTable 不再分裂成 `/DT` 和 `/DT.DT` 两个 root；中央 DataTable 面板的 row diff 能按同一 asset scope 命中。

3. 验证记录
   - UE `TemplateEditor Win64 Development` 编译：通过。
   - MCP `blueprint_open_editor`：通过，Bridge 可用。
   - CLI `blueprint_get_runtime_profile`：通过，`warnings=0`、`errors=0`。
   - CLI `blueprinthelper_diagnostics_runtime`：通过，`warnings=0`、`errors=0`。

距离期望差距：

1. 未执行人工 ReviewPanel UI smoke，因此无法确认视觉层面“只出现一次按钮”和“中央 DataTable row diff 命中”的最终表现。
2. 未处理 BUG-20260510-03 以后需要 evidence 字段、排序字段、MyBlueprint 原生 parity、函数局部变量 scope、Graph 稳定锚点等更大范围变更。

能力缺失记录：

1. 当前缺少可由 CLI 自动断言 ReviewPanel Slate 行按钮数量、DataTable row diff 命中范围的自动化能力；本轮只能做到编译、编辑器启动和运行态预检。

遭遇 Bug 记录：

1. 本轮未新增非文档已知 Bug；主要修复对象即 BUG-20260510-01 与 BUG-20260510-02。

## 2026-05-13 ReviewPanel Accept/Reject Persistence Fix

状态说明：本轮仅做源码修复和 CLI/Bridge 命令接入，未执行构建、CLI build 或编辑器验证。

1. Final Changes 面板 Accept/Reject GraphPanel 同一条 Review 无效
   - 状态：源码修复已落地，待验证。
   - 处理：ReviewActionService 不再只解析第一个持久化 ReviewRecord；对一个可见 Review 项会解析所有匹配的 ReviewRecord + target_keys，并逐个回写 Accept/Reject。

2. 组件/变量 Review Accept/Reject 后重开 ReviewPanel 又出现
   - 状态：源码修复已落地，待验证。
   - 处理：针对 latest-wins 合并后的可见项，Accept/Reject 会回写所有同资产同 target_key 的 pending 持久化记录，避免旧 pending record 在重新加载时复活。

3. DebugBundle 排查能力
   - 状态：源码接入已落地，待验证。
   - 处理：Bridge 增加 `list_debug_cases` 与 `export_debug_bundle`；CLI tool surface 增加 `blueprinthelper_list_debug_cases` 与 `blueprinthelper_export_debug_bundle`。
