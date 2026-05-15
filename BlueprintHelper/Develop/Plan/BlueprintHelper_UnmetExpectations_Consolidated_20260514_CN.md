# BlueprintHelper 未达期待总账 2026-05-14

## 文档定位

本文是 `Develop/Plan` 下未达期待、待验证项、阻塞项和未来项的统一总账。

迁移规则：

1. 原文档保留历史分析、执行证据和设计上下文。
2. 原文档顶部已加“状态转移”指针，表示开放项跟踪迁移完成。
3. “原文档标记完成”仅表示迁移完成，不表示这里列出的未达期待已经完成。
4. 后续闭环修复优先更新本文；只有完成验证后，才能把本文对应条目标记为完成。

## 当前总览

> 2026-05-14 本轮闭环更新：已开始 P0 ReviewPanel DataTable live smoke。编辑器通过 MCP 启动成功，DataTable 创建通过；Struct 创建暴露 `CreateStructure` 字段应用失败后留下半成品资产的问题，导致后续 DataTable row 写入只看到 `Damage` 字段而缺少 `DisplayName`。源码已修复，待关闭编辑器、编译、重启后用新资产路径复测；A1 不可标记完成。

1. 已真实闭环：GraphStatementFramework first-slice、CLI 物理门非 ReviewPanel 范围、AssetFactory BPI/PrimaryDataAsset 两个 smoke bug、`close_editor` 蓝图编辑器关闭崩溃修复。
2. 源码已修但待 UI/人工复核：ReviewPanel DataTable BUG-01/02、多个 2026-05-13 ReviewPanel live bug 修复项。
3. 仍未完成：ReviewPanel live smoke、Debug/DebugBundle 手动环中需要 UI/needs_action 参与的部分、Data/UMG dry-run 中 UMG 未来状态模拟、`read_context` 数据/对象属性统一入口、ReviewPanel BUG-03 以后架构级问题。ObjectProperty 正向 TaskSpec 写入、DataTable JSON number/bool 写入、缺失资产 negative preview 已完成自动闭环验证；2026-05-15 DebugBundle 已暴露 Reject 结果状态不一致与目标已缺失清理策略问题，源码已修正并编译通过，待编辑器内复测。
4. 文档层待整理：CLI/MCP 边界旧文档冲突已于 2026-05-14 修正；旧 v0.3.6 gap matrix 有乱码且被新总账覆盖。

## A. ReviewPanel 未达期待

### A1. ReviewPanel live Editor smoke 未完成

来源文档：

- `BlueprintHelper_Current_TODO_20260506.md`
- `BlueprintHelper_Unified_SmokeRun_Verification_20260509.md`
- `SmokeBug_VerificationGaps_20260510_CN.md`
- `ReviewPanel_Native_Row_Geometry_PLAN_20260510_CN.md`

状态：已完成 TaskSpec 正向写入 smoke；读回入口缺口归入 D3。

未达期待：

1. 需要用真实 pending ReviewRecord 打开 ReviewPanel。
2. 需要验证 row highlights。
3. 需要验证 selected-row Accept / Reject。
4. 需要验证 asset-root Reject cascade。
5. 需要验证 Graph diff block bounds。
6. 需要验证 Debug export 不含 `debug_export_refs`。
7. 需要验证 ReviewPanel pending load、Accept、Reject、RejectAll、Graph diff block 绘制满足合同。

距离期望差距：本轮已进入真实编辑器闭环，并已在 `/Game/BlueprintHelperCliSmoke/ReviewPanelLoop_20260515_000049` 成功生成 Struct、DataTable 和 rows；新增 `blueprinthelper_query_review_records` 后已确认该 DataTable 有 2 条 pending ReviewRecord，row task 有 1 条 pending ReviewRecord，包含 `Alpha` modified 和 `Beta` added。仍需打开 ReviewPanel 做真实 UI 验证 row highlights、Accept/Reject、root cascade、pending load 和 Debug export。

### A2. DataTable ReviewPanel BUG-01/02 待人工 UI 复核

补充记录：当前 ReviewRecord / DebugBundle 后端证据已可由 CLI 查询，但 ReviewPanel live UI 仍缺少可由 CLI 直接打开并切换 Review 页的自动化入口；本轮未对 UI row highlight / Accept / Reject 做人工视觉验收，A1/A2 不可标记完成。


来源文档：

- `ReviewPanelBug_20260510_CN.md`

状态：源码已修复，编译通过，运行态预检通过，待人工 UI 复核。

未达期待：

1. BUG-20260510-01：DataTable 同一 row 的 `Accept` / `Reject` 需要只在 `ReviewActions` column 出现一次。
2. BUG-20260510-02：同一 DataTable 不应分裂成 `/DT` 与 `/DT.DT` 两个 Review scope，中央 DataTable row diff 应命中同一 asset scope。

距离期望差距：未做人工 ReviewPanel UI smoke，因此不能标记 UI 完全验收。

### A3. Structure 字段子 Review 挂载到结构资产创建 Review 下

- 状态：已完成。
- 本轮处理：`create_asset` / `asset_type=structure` 的 TaskRuntime Review evidence 现在为结构体字段生成 `struct_field:<FieldName>` 原子目标，并把字段可见变更挂到同资产 `asset_factory` 生命周期根变更下。
- 排序/归属字段：`FBlueprintHelperReviewAtomicTarget` 与 `FBlueprintHelperReviewVisibleChange` 增加 `execution_order`、`task_step_index`、`atomic_index`，Review Store 在构建、合并、保存、查询 Summary Artifact 前都会执行生命周期父子挂载和稳定排序。
- 编译证据：2026-05-15 `Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild` 返回 `Result: Succeeded`，目标 up-to-date。
- 复测证据：2026-05-15 执行 `Saved\CodexTest\a3a4_struct_review_20260515_101234.json`，创建 `/Game/BlueprintHelperCliSmoke/A3A4/ST_A3A4_Row_20260515_101234` 后查询 pending Review，返回 `record_count=1`、`visible_change_count=4`。
- 复测结果：结构资产根变更 `is_asset_lifecycle_root=true`、`reject_removes_children=true`、`visual_group_key=asset_factory:_Game_BlueprintHelperCliSmoke_A3A4_ST_A3A4_Row_20260515_101234`；字段 `Damage`、`DisplayName`、`bEnabled` 均生成 `struct_field:*` 子变更，且 `parent_change_id` 指向结构资产根变更。
- 距离期望差距：代码层和 CLI artifact 层已满足；本条未包含手动 ReviewPanel UI 视觉验收，UI 原生 Row 复用归入 A5。
### A4. Final Changes 同资产依赖顺序稳定

- 状态：已完成。
- 本轮处理：Review Store 不再只依赖容器插入顺序；同资产变更按 `asset_path -> execution_order -> task_step_index -> atomic_index -> lifecycle root first -> location_key` 排序。
- 复测证据：A3 同一结构创建任务返回的 `visible_changes` 顺序为：结构资产根 `execution_order=0, atomic_index=0`，字段 `Damage=1`，字段 `DisplayName=2`，字段 `bEnabled=3`。
- 复测结果：同一个资产内，创建资产根 Review 稳定排在字段子 Review 前；字段顺序保持 TaskSpec 中的 fields 顺序。
- 距离期望差距：当前已覆盖结构创建与字段子 Review 的依赖顺序；未扩展到所有未来复杂资产写入类型，但排序字段和 Store 主路径已通用化。

A5 追加记录：2026-05-15 MyBlueprint/Components Row polish update 已进入源码并编译通过。具体包括 Row 25px 最小高度与 bottom padding 5.0、Components hover tooltip、保留空 Macros/Event Dispatchers 分类、变量/事件分发器只读 UE pin type 显示，以及 Graphs 分类下 graph row 支持包含事件子项并折叠。距离期望差距：仍需用户在 ReviewPanel 中做视觉验收。
### A5. MyBlueprint / Components / WidgetTree / Details / GraphPanel 原生复用方案

- 状态：MyBlueprint / Components / GraphPanel 源码实现已落地并编译通过；WidgetTree / Details 按本轮范围暂缓；ReviewPanel 人工 UI 验证未完成，因此 A5 不能整体标记完成。
- 子计划文档：`BlueprintHelper_A5_NativePanel_CopyPlan_20260515_CN.md` 已更新为当前实现记录和差距。
- 本轮完成 1：新增 `SBlueprintHelperReviewMyBlueprintPanel` / `SBlueprintHelperReviewMyBlueprintRow`，MyBlueprint Presenter 改为构建数据后挂接 native panel，移除 Review-only placeholder 注入，避免 `ReviewAnchor` 泄漏到 MyBlueprint 正常内容。
- 本轮完成 2：新增 `SBlueprintHelperReviewComponentsPanel` / `SBlueprintHelperReviewComponentRow`，Components Presenter 不再使用 UE final 的 `SSubobjectBlueprintEditor` 作为主 UI，改为 BlueprintHelper 自有 read-only component tree/row，并在 Row 内部绘制 Review 背景色和 Accept / Reject 操作。
- 本轮完成 3：`UBlueprintHelperReviewDiffBlockNode` 改为 `UEdGraphNode_Comment` 派生，借用 UE 原生 `SGraphPanel` comment layer 让 Graph diff block 进入节点/连线底层绘制，alpha 保持 0.35。
- 编译证据：2026-05-15 执行 `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild`，结果 `Result: Succeeded`。
- 距离期望差距：Components 当前覆盖 Blueprint SCS component tree，native / inherited / CDO component 与 UE 原生 Components 面板完全等价仍需 ReviewPanel 验证；GraphPanel 未复制 `SGraphPanel` / `SGraphEditorImpl`，采用 comment-layer underlay，仍需验证缩放、平移、pin hit test 和 node selection；WidgetTree / Details 未做；ReviewPanel UI 验证未完成。

A5 本轮 ReviewPanel 反馈修正：
- 用户验证发现 Row Diff 色被替换后变暗，MyBlueprint / Components 视觉与 UE 原生不一致，hover 到 row 时 Accept / Reject 没有稳定显示在 row 右侧。
- 已修正 Row 颜色路径：Diff 状态只绘制 Review Diff Overlay 自身颜色，不再叠加 `Brushes.Recessed` / `BorderBackgroundColor`。
- 已修正 MyBlueprint Row 风格：section 采用 UE `SGraphActionMenu` 类似 header brush，row 采用 native table row style，按钮槽锚到整行右侧 hover 显示。
- 已修正 Components Row 风格：采用 UE `SSubobject_RowWidget` 使用的 `SceneOutliner.TableViewRow` 风格，组件显示名改为变量名清洗，补组件 class icon，按钮槽锚到整行右侧 hover 显示。
- 2026-05-15 用户验证发现 Components/MyBlueprint 折叠三角重复；原因是自定义 Row 手工绘制 SExpanderArrow，同时 STreeView 已提供内置 expander。已移除两个 native Row 内的手工 expander，改为只保留 STreeView 的单一路径。
- 真实差距：当前仍不是完整复制 UE `SGraphActionMenu` / `SSubobjectEditor` 私有源码后的最终形态，而是 native style 修正切片；完整源码级迁入仍未完成。按当前会话要求，最终编译验证放到所有改动结束后统一执行。
### A6. Function scope / Local Variables Review 未完成

来源文档：

- `ReviewPanelBug_20260510_CN.md`

状态：未完成。

未达期待：

1. Function 级 Review 需要定义稳定 scope 合同。
2. MyBlueprint presenter 在函数 scope 下需要显示 `Local Variables` section。
3. 需要 target kinds：`function_graph`、`function_signature`、`local_variable`、`local_variable_default`。
4. 函数图节点/连线 diff 仍由 Graph workspace 负责。

距离期望差距：当前 state 没有稳定 `FocusedGraph` / `SelectedFunctionGraph` / `FunctionScopeKey`。

### A7. Graph 事件/节点 Reject 稳定锚点未完成

来源文档：

- `ReviewPanelBug_20260510_CN.md`
- `ReviewPanel_V2_Implementation_PLAN.md`
- `BlueprintHelper_GraphStatementFramework_Progress_20260513_CN.md`

状态：未完成。

未达期待：

1. Graph atomic target 必须优先持久化稳定 `node_guid`、structured graph anchor、recorded bounds、rollback journal node alias。
2. Hash / rollback resolver 需要支持 `NodeGuid`，不能只靠易变 UObject name。
3. GraphDiff 和 Reject 需要共用同一 anchor resolver。
4. 旧记录只有 `K2Node_*` 名称且当前图找不到节点时，应明确输出 `unstable_node_name_anchor`。

距离期望差距：当前仍有路径依赖易变节点名，Graph Review Reject 可进入 `needs_action`。

### A8. Reject asset lifecycle root 后 UI detach / reload / cascade 未完成

来源文档：

- `ReviewPanelBug_20260510_CN.md`

状态：未完成。

未达期待：

1. Reject 创建资产 root 前，Panel 应先清空选中变化和资产上下文。
2. Graph/Details/Components/MyBlueprint/WidgetTree presenter 应释放当前资产引用。
3. root reject 成功后，应从 ReviewStore 重新加载 pending changes，而不是只本地移除已知 child。
4. 同资产 child review 需要可恢复父子合同，避免关闭重开后才消失。

距离期望差距：当前文档仍记录用户侧需要关闭重开 Panel 才能完全刷新。

### A9. 2026-05-13 Physics Door ReviewPanel live bug 修复项待 UI 验证

来源文档：

- `ReviewPanelBug_20260510_CN.md`
- `BlueprintHelper_CLI_PhysicsDoor_TestExecution_20260513_CN.md`

状态：源码修复已落地或部分修复已记录，待 UI 验证。

未达期待：

1. PD-RP-01：重复变量/组件 visible changes collapse。
2. PD-RP-02：Components row 背景 diff。
3. PD-RP-03：GraphPanel diff block alpha 0.35。
4. PD-RP-04：ReviewAnchor 不应泄漏到 MyBlueprint 正常内容。
5. PD-RP-05：Reject 单个事件不应误拒绝 sibling events。
6. PD-RP-06：新增签名 Review 应归到创建资产 root 下。
7. CLI TaskSpec ReviewPanel 动态刷新需要 live UI 验证。
8. Final Changes 中 Accept/Reject GraphPanel 同一条 Review 和组件/变量复活问题需要 live UI 验证。

距离期望差距：多数项已有源码修复记录，但缺少真实 Panel 操作复核。

## B. Debug / DebugBundle / Review diagnostics 未达期待

### B1. Debug / DebugBundle 手动环未执行

来源文档：
- `SmokeBug_VerificationGaps_20260510_CN.md`
- `BlueprintHelper_Unified_SmokeRun_Verification_20260509.md`
- `ReviewPanel_Native_Row_Geometry_PLAN_20260510_CN.md`

状态：后端与 CLI 边界已进一步补强，仍保留人工 ReviewPanel / needs_action 环验证缺口。

已完成内容：
1. `blueprinthelper_list_debug_cases` / `blueprinthelper_export_debug_bundle` 已接入 CLI surface 和 Bridge Debug cluster，列表默认裁切并支持 `limit`。
2. `blueprinthelper_export_debug_bundle` 仍只接受 `debug_case_id`，错误参数场景已写入 CLI Tips。
3. DebugBundle manifest 继续声明 `summary_only=true`、`redacted=true`，且不暴露 token、本地绝对路径、完整 raw JSON、源码内容。
4. 本轮新增 manifest 安全补强：过滤 `../` escape artifact ref、过滤 legacy `debug_export_refs` artifact ref，并新增 `artifact_summary`，显式输出 safe content count、review summary count、skipped count，以及 `contains_legacy_debug_export_refs=false`、`contains_local_absolute_paths=false`。
5. 本轮修复 bundle artifact 路径边界：`WriteJsonArtifact` 和 review summary artifact 写入不再使用裸 `StartsWith` 判断目录归属，改为带目录分隔符和大小写兼容的 `IsPathInsideDirectory(...)`，避免相邻目录名前缀误判。
6. 本轮补自动化断言：Debug manifest DTO 测试覆盖本地路径、`../escape.json` 和 `artifacts/debug_export_refs.json` 都不能进入 manifest `contents`，并验证 `privacy` / `artifact_summary` 的 legacy debug export refs 标志为 false。

仍未达到期望：
1. 未在本轮运行 `BlueprintHelper.Review.Integration.*DebugCase*` 自动化。
2. 未在本轮运行 `BlueprintHelper.RuntimeDiagnostics.Debug.*` 自动化。
3. 未在真实 ReviewPanel 操作中手动触发一次 reject `needs_action` 并导出对应 DebugBundle。
4. `get_debug_case` summary-only、DebugBundle Review summary artifact、no `debug_export_refs` 已有源码/测试覆盖和历史 CLI 证据，但本轮尚未做最终统一编译验证；编译按当前会话要求放在全部 B 系列改动之后执行。

距离期望差距：B1 的非 UI 后端安全边界已补强；完整完成仍依赖后续手动 ReviewPanel / needs_action 环和目标 Automation 执行结果。

验证记录：
1. B1 编译证据：2026-05-15 11:47 `Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild` 返回 `Result: Succeeded`。
2. B1 CLI 诊断证据：2026-05-15 `bh.cmd blueprinthelper_diagnostics_runtime --json "{}" --select status,summary` 返回 `status=completed`、`warnings=0`、`errors=0`。
3. B1 CLI DebugCase 列表证据：2026-05-15 `bh.cmd blueprinthelper_list_debug_cases --json "{}" --select status,summary` 返回 `status=completed`、`warnings=0`、`errors=0`。
### B2. Baseline snapshot future plan 部分落地

来源文档：
- `BlueprintHelper_Review_BaselineSnapshot_FuturePlan_20260509.md`

状态：Stage 1 dirty target policy、DebugBundle baseline trust metadata、最小 `baseline.semantic.json` 输出和 TaskRunJournal baseline 诊断已进入源码；Stage 2/3 的 ReviewPanel/Diff/Reject 全量采用 semantic baseline 仍未完成，不能整体标记完成。

已完成内容：
1. TaskPlan schema 增加 `execution_policy.review_baseline_dirty_asset_policy`，支持 `block`、`save_before_archive`、`allow_stale_disk_snapshot`，默认 `block`。
2. TaskRuntime 在 ArchiveSession 创建前解析 dirty policy；未知值返回 `invalid_taskplan_execution_policy`。
3. 默认 `block`：若 target asset package 在编辑器中 dirty，则在 ArchiveSession 创建和写入前返回 `review_baseline_dirty_target_assets`，避免 disk-only baseline 捕获 stale 状态。
4. `save_before_archive`：若 target asset dirty，则在 ArchiveSession 创建前调用现有 SaveAsset 路径保存目标资产；保存失败或保存后仍 dirty 时失败，不创建 ArchiveSession。
5. `allow_stale_disk_snapshot`：允许继续执行，但 ArchiveSession 写入 `snapshot_trust=stale_disk_copy` 和 warning，明确该 disk snapshot 只可作为诊断证据，不可当作权威 rollback baseline。
6. ArchiveSession 增加 baseline metadata：`dirty_asset_policy`、`snapshot_trust`、`dirty_target_assets`、`warnings`、`disk_snapshot_refs`、`semantic_snapshot_refs`。
7. Review summary artifact 会读取 ArchiveSession 并导出 baseline metadata；因此 DebugBundle 中的 Review summary artifact 可以显示 dirty policy、dirty asset list、disk snapshot refs、semantic snapshot refs 和 trust level。
8. DebugCase 自动化用例补充 baseline trust metadata 断言，覆盖 DebugBundle Review summary artifact 中 `allow_stale_disk_snapshot`、`stale_disk_copy` 与 `disk_snapshot_refs`。
9. 新增 `FBlueprintHelperReviewBaselineSnapshotService`，在 ArchiveSession 创建阶段为每个 target asset 输出 `Saved/BlueprintHelper/Review/Snapshots/<archive_session_id>/<asset_key>/baseline.semantic.json`，并把 `review://archive/<id>/baseline/<asset_key>/baseline.semantic.json` 写入 `baseline_semantic_snapshot_refs`。
10. 最小 semantic snapshot 当前覆盖 Blueprint 的 parent/generated class、变量、SCS 组件、graph/node/pin/link 摘要，WidgetBlueprint 的 WidgetTree 摘要，DataTable 的 row struct、row name/value，以及 UObject 可编辑属性导出值。
11. TaskRunJournal 新增 `review_baseline` 区块，记录 dirty policy、snapshot trust、dirty target assets、`save_before_archive` 实际保存资产列表、warning 和 pre-archive save operation 结果。`n12. DebugBundle 导出 Review summary 时会解析 ArchiveSession 的 `baseline_semantic_snapshot_refs`，把安全且可解析的 `baseline.semantic.json` 复制进 DebugBundle `artifacts/review/*.baseline.semantic.N.json`；非法 ref、缺失文件或不可解析 JSON 会进入 skipped artifacts。`n13. DebugCase 自动化测试 fixture 已补 semantic baseline artifact 断言，覆盖 bundle 中 `artifacts/review/*.baseline.semantic.N.json` 可读且携带 `BlueprintHelper.ReviewBaselineSemanticSnapshot.v1`。

仍未达到期望：
1. 当前 `baseline.semantic.json` 是最小 live semantic snapshot，还不是 Review/Diff/Debug 的唯一 before/after 主数据源。
2. ReviewPanel / Debug export 尚未将 `baseline.semantic.json` 作为首选 before/after 来源。
3. Reject safety 尚未接入 baseline semantic hash checks。
4. DataAsset / UObject semantic snapshot 目前依赖通用可编辑属性导出，尚未形成按 Review atomic target key 的稳定 semantic hash。
5. 本轮已完成统一编译，`Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild` 返回 `Result: Succeeded`；尚未运行完整 Automation 和人工 ReviewPanel / needs_action 环。

距离期望差距：B2 Stage 1 与最小 Stage 2 文件输出已进入源码；完整 Stage 2/3 仍需要 ReviewPanel/Diff/Reject 消费 semantic baseline 并补 semantic hash safety，因此 B 系列不能声明全部达到期望。

验证记录：
1. B2 编译证据：2026-05-15 11:47 `Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild` 返回 `Result: Succeeded`。
2. Bridge/CLI 运行态证据：重启编辑器后 `bh.cmd blueprint_get_runtime_profile --json "{}" --select status,summary` 返回 `status=completed`、`warnings=0`、`errors=0`。
## C. Smoke / Automation / 验证缺口

### C1. Unified SmokeRun 后续 rings 未全部完成

来源文档：

- `BlueprintHelper_Unified_SmokeRun_Verification_20260509.md`
- `SmokeBug_VerificationGaps_20260510_CN.md`

状态：未完成。

未达期待：

1. Ring 1 grouped failures 当前已关闭，但后续 rings 仍需补跑。
2. UE Automation final matrix 中曾有 `NOT RUN` 项，需要更新真实结果。
3. Review / Debug targeted Automation 需要补充最新报告。
4. 每个失败应保留 task_run_id、preview_id、Automation report、ReviewRecord id、DebugCase id 或 DebugBundle manifest。

距离期望差距：当前不是全线 smoke PASS 状态。

### C2. New Project Full SmokeRun 未执行为最终结果

来源文档：

- `BlueprintHelper_NewProject_Full_SmokeRun_20260510.md`

状态：runbook，不是 PASS 报告。

未达期待：

1. 需要在干净 Unreal project 中执行完整 disposable assets smoke。
2. 需要覆盖 TaskSpec-first writes、UE Automation、ReviewPanel、Debug/DebugBundle checks。
3. 需要确认 no path leak、no Automation failures、no MCP/CLI contract regressions。

距离期望差距：文档目前主要是流程和判定规则，不是当前执行完成证据。

### C3. ObjectProperty 正向写入验证未完成

来源文档：

- `SmokeBug_VerificationGaps_20260510_CN.md`
- `BlueprintHelper_v0.3.6_Current_Implementation_Gap_Matrix_20260505.md`

状态：未完成。

未达期待：

1. invalid object property case 已有 blocked 方向，但 valid write 曾因 BPI parent bug 阻断未运行。
2. 需要使用不依赖 Interface 的 fixture 验证 valid object-property write。
3. 需要补 DataAsset fixture 和 value 类型覆盖。

距离期望差距：2026-05-15 自动闭环已创建独立 DataAsset fixture `/Game/BlueprintHelperCliSmoke/AutoExpectations_20260515_003213/DA_AE_ObjectProperty_20260515_003213`，其 Blueprint DataAsset class 先创建并加入 `SmokeLabel:string`、`SmokeFloat:float`、`bSmokeFlag:bool` 三个 instance editable 变量，再通过 `edit_object_properties` TaskSpec 写入三项属性。preview passed，execute applied，`task_195B378C46F78BA148F0C582341E9140` 的 artifact 显示 `requested_count=3`、`applied_count=3`、`changed_count=3`。未使用 supported `read_context` 做属性读回，因为 `object_property_context` 当前返回 `unsupported_read_type`，该统一读入口缺口已归入 D3。

### C4. P2 first batch 多能力仍有 source integrated / smoke pending 项

来源文档：

- `BlueprintHelper_v0.3.6_Current_Implementation_Gap_Matrix_20260505.md`

状态：旧矩阵，部分乱码，仍保留历史缺口。

未达期待：

1. Cleanup / Rollback / Convert ownership 仍需要统一 smoke 后才能标记完成。
2. Function/Event Signature remove 执行和 dispatcher 迁移策略仍未完善。
3. ObjectProperty / DataTable / UMG / Component / ClassSettings 多项在旧矩阵中仍为 fixture smoke pending。
4. Debug diagnostics verification tail 未完成。

距离期望差距：旧矩阵已被新 smoke/进度文档覆盖，后续只从本文派生任务。

## D. TaskSpec / CLI / context 能力缺口

## D0. 本轮新增 TaskSpec/AssetFactory 诊断缺口

状态：修复中，待编译和复测。

未达期待：
1. `create_asset` 创建 `structure` 时，如果字段应用失败，不应留下可被后续 dry-run 识别为已存在的半成品资产。
2. 空字符串默认值不应导致 `DisplayName` 等 string 字段创建失败。
3. `execute_task` 的 artifact 不应只给 `creation_failed`，应携带字段名、字段类型、默认值应用失败等可定位信息。

距离期望差距：本轮已修复半成品回收以及基础类型零值/空值默认值 no-op 路径，并完成编译、重启编辑器和全新路径复测；Struct、DataTable、DataTable rows 三段 TaskSpec 均已成功。artifact 字段级诊断增强尚未实现；DebugCase 列表响应裁切已实现；随后定位到 BridgeRoutePlanner 缺少 `list_debug_cases` / `export_debug_bundle` 的 Debug cluster 映射，已补映射并完成编译、重启、复测。



### D1. UMG / DataTable dry-run 未来状态模拟未完成

来源文档：

- `SmokeBug_TaskSpecDataUMG_20260510_CN.md`

状态：DataTable 半程已验证，UMG 和完整未来状态模拟仍未完成。

未达期待：

1. 同一 TaskSpec 内先创建 widget，后续 property update dry-run 应看到计划内未来状态。
2. DataTable 同一 TaskSpec 内先 add row，后续 update row dry-run 应看到计划内未来 row。
3. 需要区分 `missing_in_current_asset_but_created_by_plan` 与真实缺失。
4. 需要 TaskSpec 级虚拟状态模拟。

距离期望差距：2026-05-15 自动闭环已验证 DataTable 同一 TaskSpec 内 `add row` 后接 `update row` 的 preview 不再被真实缺失行阻断。`/Game/BlueprintHelperCliSmoke/AutoExpectations_20260515_003213/DT_AE_NumberBool_20260515_003213` 的 rows preview passed，并返回 `planned_datatable_row_update_validation_deferred`，明确区分“当前资产缺失但计划内创建”。差距是该路径仍是 deferred validation，不是完整虚拟行字段状态模拟；UMG widget 创建后 property update 的未来状态仍未运行/实现，因此 D1 不可标记完成。

### D2. JSON number / bool 到 UE import text 转换缺口

来源文档：

- `SmokeBug_TaskSpecDataUMG_20260510_CN.md`

状态：已完成。

未达期待：

1. DataTable / UObject property 等写入中，JSON number / bool 需要稳定转换为 UE import text。
2. 错误输入需要结构化 blocked issue。

距离期望差距：2026-05-15 自动闭环先复现 JSON number 写入 DataTable int 字段被转成 `"42.0"` 导致 `字段 Damage 值导入失败`；随后修复 TaskRuntime JSON number 到 UE import text 的整数格式化路径，关闭编辑器、编译、重启后复测通过。`/Game/BlueprintHelperCliSmoke/AutoExpectations_20260515_003213/DT_AE_NumberBool_20260515_003213` 的 rows preview passed、execute applied，覆盖 `Damage:int` JSON number、`bEnabled:bool` JSON bool、`Ratio:float` JSON number 和 `DisplayName:string`。同轮 ObjectProperty 正向 TaskSpec 也覆盖 `string/float/bool` literal 写入并 applied。

### D3. `read_context` 仍不是所有资产类型统一上下文入口

来源文档：

- `SmokeBug_MCPContract_20260510_CN.md`
- `BlueprintHelper_CLI_OrdinaryAgent_TestPlan_20260512_CN.md`

状态：未完成。

未达期待：

1. `read_context` 目前主要覆盖 blueprint logic。
2. UMG / Component / Data 类任务缺少统一 read context。
3. 普通 Agent TaskSpec-first 写入前仍需要更完整上下文入口。

距离期望差距：2026-05-15 自动闭环确认该缺口仍存在：`blueprinthelper_read_context` 的 schema 不接受 `target_type=data_table`；改用 `target_type=asset` 后，`read_type=data_table_context` 返回 `unsupported_read_type`，提示当前只支持 `blueprint_logic`。`read_type=object_property_context` 同样返回 `unsupported_read_type`。直接尝试 `blueprint_get_datatable_rows` 和 `blueprint_get_object_properties` 也被当前 CLI registry 拒绝为 unsupported command。该项属于统一上下文/工具面能力缺口，本轮按“不拓展工具、不改架构”范围不处理。

### D4. 缺失资产 negative read 诊断不够强

来源文档：

- `SmokeBug_MCPContract_20260510_CN.md`
- `SmokeBug_VerificationGaps_20260510_CN.md`

状态：已完成。

未达期待：

1. 缺失目标资产应统一返回 `target_asset_not_found` 或等价 blocked issue。
2. 不应返回 partial / empty `导出失败`。
3. `blueprinthelper_preview_task` 应把该错误提升为 blocked。

距离期望差距：2026-05-15 自动闭环已修复并验证。缺失 DataAsset 目标 `/Game/BlueprintHelperCliSmoke/AutoExpectations_20260515_003213/DA_AE_Missing_20260515_003213` 的 `blueprinthelper_preview_task` 返回 `preview_blocked`，issue 为 `code=target_asset_not_found`、`path=asset_path`、message 明确包含缺失资产路径；不再只返回泛化的 `object_property_operation_failed`。

### D5. 普通 Agent CLI 全测计划未完成为最终 PASS 报告

来源文档：

- `BlueprintHelper_CLI_OrdinaryAgent_TestPlan_20260512_CN.md`

状态：计划文档，待完整执行/刷新。

未达期待：

1. 当前文档是普通 Agent 全面测试计划，不是当前所有项目全 PASS 结果。
2. 需要按最新 CLI/MCP 边界刷新执行方式。
3. 需要确保 ToolResult schema、artifact、select 裁切、DebugBundle 边界均按最新实现验证。

距离期望差距：多处步骤是通过标准和失败判定，不是最新执行证据。

## E. GraphStatementFramework 非阻塞限制

来源文档：

- `BlueprintHelper_GraphStatementFramework_Progress_20260513_CN.md`
- `BlueprintHelper_GraphStatementFramework_Design_20260513_CN.md`

状态：first-slice 已闭环；以下为非阻塞限制或后续工作。

未达期待：

1. `connect_pins` / `disconnect_link` / `replace_link` 低层 patch 能力未暴露到当前 AgentFace first-slice schema。
2. user-node anchor 合同尚未设计完成，当前范围限定 BlueprintHelper-owned graph anchor。
3. Review UI 按 function/event/macro 聚合仍需单独 UI 验收。
4. layout model 主要用于 debug/evidence 与 fragment 描述，尚未声明为真实 UE 节点排布驱动器。
5. Review / Debug 使用 statement/body identity 聚合仍需 UI/Debug 侧继续闭环。

距离期望差距：图语句主链路可用，但不是所有图编辑、所有 UI 反馈、所有低层 patch 都完成。

## F. 物理门剩余验证边界

来源文档：

- `BlueprintHelper_CLI_PhysicsDoor_TestExecution_20260513_CN.md`

状态：非 ReviewPanel 范围已完成。

未达期待：

1. 未做 PIE / 物理模拟层面的行为观测。
2. 未做 ReviewPanel UI 回归。
3. `blueprinthelper_read_context` 对 component summary 仍不能直接证明组件属性细节，只能依赖 task result / 读回证据。

距离期望差距：当前验证到 Blueprint 资产结构、图逻辑、组件物理属性写入和 Blueprint compile/execute 成功。

## G. 文档与流程整理缺口

### G1. CLI/MCP 边界文档冲突

来源文档：

- `BlueprintHelper_CLI_MCP_Boundary_20260514_CN.md`

状态：已完成。

已处理：

1. 旧文档中 “MCP 重新废弃冻结” 的结论已删除。
2. 已修正为：CLI 仍为普通 BlueprintHelper 读写主线；MCP 只保留 editor lifecycle，即 `blueprint_open_editor` / `blueprint_close_editor`。
3. 已明确 MCP 不承载 TaskSpec preview/execute/read/debug bundle，也不恢复旧 MCP 普通工具面。

距离期望差距：无。

验证记录：

1. 文档已更新为当前插件技能和记忆一致的边界。
2. 本项为文档治理，不涉及源码编译。

### G2. 旧 gap matrix 乱码与状态覆盖

来源文档：

- `BlueprintHelper_v0.3.6_Current_Implementation_Gap_Matrix_20260505.md`

状态：历史文档，需降级为历史参考。

未达期待：

1. 文件存在明显编码乱码。
2. 多数状态已被 Unified Smoke、GraphStatementProgress、ReviewPanelBug、本文覆盖。
3. 后续不应继续把该文件作为当前权威状态源。

距离期望差距：需要保留历史证据，但当前开放项以本文为准。

### G3. ReviewPanel 早期 implementation plan 中的旧 build blocker 需要历史化

来源文档：

- `BlueprintHelper_ReviewPanel_UserSide_Implementation_Plan_20260506.md`

状态：历史文档，需降级为历史参考。

未达期待：

1. 文档中多处旧环境 build blocked 记录已不代表当前工作区状态。
2. 后续应从 ReviewPanelBug / Native Row Geometry / 本文追踪当前缺口。

距离期望差距：旧计划对当前判断噪声较大。

## 优先级建议

1. P0：执行 ReviewPanel live UI smoke，先验证 DataTable BUG-01/02 和 2026-05-13 Physics Door ReviewPanel 修复项。
2. P1：补 Debug/DebugBundle 手动环与 Review Debug export。
3. P1：补 ObjectProperty 正向写入验证。
4. P1：实现或明确延期 UMG/DataTable dry-run 未来状态模拟。
5. P2：处理 Structure field child review、Final Changes 排序字段、Graph stable anchor、asset root reject detach/reload/cascade。
6. P2：MyBlueprint native parity / Function local variables scope 作为架构级任务单独设计。

## 原文档迁移完成标记

已在相关原文档顶部加入状态转移指针。后续这些原文档只保留历史上下文，不再作为当前开放项状态源。

## 2026-05-15 A5 补充：DetailsView 联动

- 状态：实现中，待编译与编辑器内验证。
- 已完成：MyBlueprint/最终变更面板选择变量 Review、Components/最终变更面板选择组件 Review 时，右侧 DetailsView 会重新绑定到可显示设置值的对象；变量继续使用蓝图 CDO，组件解析到 SCS 组件模板对象。
- 已完成：Details diff 路由扩展到可映射 Details 的变量/组件原子目标，选择后触发 UE 原生 DetailsView 的属性滚动与高亮刷新路径。
- 距离期望差距：尚未在编辑器内确认所有变量设置类 Review 都能定位到 UE 原生 DetailsView 的具体 property row；新增组件这类无具体 property path 的 Review 只能先显示组件模板详情，是否需要整块 DetailsView diff 仍需人工确认。
- 阻塞内容：无代码阻塞；等待本轮编译与编辑器验证。

## 2026-05-15 A5 补充验证状态

- 状态：代码实现完成，编译通过，编辑器已通过 MCP 重新启动并确认 Bridge 可用；等待人工 UI 验证。
- 编译结果：`TemplateEditor Win64 Development` 编译成功。
- 距离期望差距：尚未由人工确认 MyBlueprint/最终变更面板选择变量 Review 时是否能稳定定位到 UE 原生 DetailsView 的具体 property row；组件新增这类无具体 property path 的 Review 当前会显示组件模板详情，但不保证存在可定位的单个属性行。
- 阻塞内容：无当前代码阻塞。

## 2026-05-15 ReviewPanel 待修补充

- 状态：待修，未开始实现。
- ST Row：ST 中央/详情 Row 需要复用 MyBlueprint 面板的 Variable Row 风格与只读呈现能力。
- DT 中央面板结构：需要拆成上下两部分；上方负责行列表选择，下方根据当前选择行渲染具体 ST/字段值，从而复用 ST 面板并适配更多列/更多变量的 DataTable。
- DA 中央面板：Row 已能显示变更，但当前仍可编辑，需要改为只读模式。
- DT 中央面板：当前没有铺满整个可用区域，需要修正布局填充。
- DT Row 操作：中央面板 Row 需要支持 hover/overlap 显示 Accept/Reject。
- DT Row padding：中央面板 Row 的 border 区域需要增加上下 3.0 padding。
- DetailsView 联动修正：变量/组件选择后的 DetailsView 定位能力可用；当前问题不是定位失败，而是 Details surface 未绘制 diff 框或 diff 框未命中可见 row。
- 距离期望差距：尚未实现上述待修项，Details diff 框绘制还需要排查 row geometry/overlay predicate/DetailsView 刷新时序。
- 阻塞内容：DT 拆分方案需先按用户确认的理解推进；其余无已知代码阻塞。

## 2026-05-15 ReviewPanel 循环修理 - 第 1 轮

- 状态：代码实现完成，编译通过，等待编辑器内验证。
- 已完成：DA 中央面板 property row 改为只读显示，保留 Review 高亮与 Accept/Reject 操作区域。
- 已完成：DT 中央面板拆为上方行列表与下方 Selected Row Details；上方负责行选择，下方按选中行展开字段值，复用 ST/DA 字段 Row 渲染路径。
- 已完成：DT Row 在 border 区域增加上下 3.0 padding。
- 已完成：DT Row 的 Accept/Reject 支持选中或 hover 时显示在行操作列。
- 已完成：ST 字段行补充 MyBlueprint Variable Row 同源的 pin type 图标/文本显示逻辑；DT 选中行详情复用同一字段 Row 路径。
- 已完成：DetailsView 的变量/组件 target 路由补充为可进入 Details surface；Details overlay 不再返回空 widget，能基于已解析几何绘制 diff overlay。
- 编译结果：`TemplateEditor Win64 Development` 编译成功；仅有既存 `STreeView::ItemHeight` deprecated warning。
- 距离期望差距：尚未在编辑器内确认 DA 是否完全不可编辑、DT 上下拆分交互是否符合预期、DT hover 操作是否只在目标 Row 显示、Details diff 框是否与原生 DetailsView row 对齐。
- 阻塞内容：无代码阻塞；等待编辑器内验证结果。

## 2026-05-15 ReviewPanel DebugBundle 链路

- 状态：代码实现完成，编译通过，等待编辑器内验证。
- 已完成：新增 `FBlueprintHelperReviewDebugBundleService`，负责 ReviewPanel DebugBundle JSON 的默认路径、路径安全限制、读取、追加事件和保存。
- 已完成：DebugBundle 默认写入 `Saved/BlueprintHelper/Debug/ReviewPanelBundles/*.json`，并限制路径不能逃出 `Saved/BlueprintHelper/Debug`。
- 已完成：`AddDebugMessage()` 继续写入 DebugPanel 内存文本，同时追加结构化 `debug_log` 事件到 DebugBundle。
- 已完成：DebugPanel 新增 Bundle 路径输入框、`LoadBundle`、`CopyPath`、`CaptureFocus` 按钮。
- 已完成：`LoadBundle` 可从路径读取 DebugBundle JSON 并显示在 DebugPanel。
- 已完成：`CaptureFocus` 会逐帧遍历当前资产的最终改动 Row，依次触发 Review 选择/Focus，并记录 `focus_traversal` 事件；不会触发 Accept/Reject，不改变 Review 状态。
- 编译结果：`TemplateEditor Win64 Development` 编译成功。
- 距离期望差距：尚未在编辑器内验证 CaptureFocus 是否能稳定等待到所有 surface 的几何刷新；当前实现是逐帧遍历，不是显式等待每个 surface geometry complete。
- 2026-05-15 DebugBundle 排查：最新 bundle `review_panel_20260515_055606.json` 显示多次 Reject 中存在 `status=rejected` 但 `success=0` 的状态不一致，导致 UI 仍按失败路径保留 Review 项；另有 `current_hash_unavailable:graph_not_found:*` 和 `current_state_changed:*` 两类真实 needs_action。
- 已修正：`RejectReviewTargets` 改为以 target 最终 `NewStatus==Rejected` 聚合成功状态和清理条件，避免已拒绝目标因为 `bSucceeded=false` 残留；`graph_not_found` / `anchor_not_found` 视为 `target_already_missing` 并允许清理；`current_state_changed` 仍保留 needs_action，避免覆盖用户或后续写入造成的真实状态漂移。
- 距离期望差距：本轮源码已修正并通过 `TemplateEditor Win64 Development` 编译；尚未完成编辑器内再次点击 Reject 的视觉/状态复测，需要确认 Final Changes Row、Components Row、Graph diff Row 的 Reject 后都能从 pending 列表稳定移除。- 阻塞内容：无代码阻塞；等待编辑器内验证结果。
