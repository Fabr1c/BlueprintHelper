# BlueprintHelper v0.3.6 能力簇与当前实现差距矩阵

日期：2026-05-05

本文用于把 `Resources/v0.3.6/DoneImplementaion` 与 `Resources/v0.3.6/FieldMapping` 中已收敛的 UE 能力设计，对照当前源码实现状态。目标不是回到 Agent 直调原子工具，而是确定哪些能力已经能进入当前主架构：

```text
Agent TaskSpec
-> MCP Task Tools
-> Python/MCP Task Compiler
-> UE Task Runtime
-> Existing UE Capability Clusters
```

## 状态标记

| 标记 | 含义 |
| --- | --- |
| 完成 | 当前层已具备可用实现，并且命令或入口已接通 |
| 部分 | 只完成了 DTO、Service、Bridge、Runtime 或测试中的一部分 |
| 缺失 | 当前层没有对应实现 |
| 内部 | 设计上不是 Agent 主流程能力，仅作为内部支撑、debug 或只读上下文 |

## 当前主线结论

当前源码已经具备较多底层 UE Service 和 Bridge command，但 TaskSpec-first 闭环还没有覆盖所有 v0.3.6 能力。

当前 UE Task Runtime 支持的 TaskPlan capability：

```text
graph_write
blueprint_variable
blueprint_signature
asset_factory
blueprint_component
blueprint_class_settings
umg_widget
data_table
```

当前 Python/MCP Task Compiler 支持从 TaskSpec 编译出的任务类型：

```text
edit_blueprint_graph      -> graph_write，支持 append_new_owned_graph / replace_owned_graph / patch_owned_graph / merge_owned_graph
edit_blueprint_variables  -> blueprint_variable，支持 member changes/defaults/local variables 编译为结构化 IR
create_asset              -> asset_factory
edit_blueprint_components -> blueprint_component
edit_blueprint_class_settings -> blueprint_class_settings
edit_umg_widget           -> umg_widget
edit_data_table           -> data_table
create_blueprint_feature  -> composite compiler，分解为 blueprint_component / blueprint_variable / blueprint_class_settings / blueprint_signature / graph_write
```

但 2026-05-05 smoke rerun 已确认：**编译支持不等于 Bridge/UE 执行闭环可用**。当前真实跑通的 Agent-facing TaskSpec-first 写入闭环只有：

```text
edit_blueprint_graph + append_new_owned_graph + 全新图名
edit_blueprint_variables
```

GraphWrite `replace_owned_graph` / `patch_owned_graph` / `merge_owned_graph`、`edit_blueprint_components`、`create_blueprint_feature` 等已经具备 TaskSpec/Python/TaskPlan 合同或 lowering 雏形，但当前 smoke 仍被 Bridge/UE 执行层阻断。Agent 仍不应直接调用底层 MCP 原子工具；默认入口仍是 TaskSpec -> TaskPlan -> UE Task Runtime。剩余主要差距已经从“缺 TaskSpec/Python 编译”转为“Bridge/UE 执行闭环、读回定位 bug、fixture 覆盖、以及更高阶 UE capability 簇”。

## 2026-05-05 进度同步

- [x] UE Task Runtime 已支持多 step 顺序执行，并聚合 child step result。
- [x] UE Task Runtime 已按 `execution_policy.should_compile` / `execution_policy.should_save` 执行 compile/save post operation。
- [x] TaskRunJournal 已聚合 step result、post operation result，并支持进程内查询。
- [x] Blueprint Variable TaskPlan IR 已支持 `member_variables` / `member_defaults` / `local_variables` lowering。
- [x] Blueprint Variable ensure-only member batch 继续 lower 到 `add_blueprint_member_variables`。
- [x] Blueprint Variable mixed member/default/local ops lower 到内部 `blueprint_variable_batch`，不暴露 adapter operation 给 Agent。
- [x] Python/MCP TaskSpec 编译已覆盖 `create_asset`、`edit_blueprint_components`、`edit_blueprint_class_settings`、`edit_umg_widget`、`edit_data_table`。
- [x] BlueprintVariableService 的 `set_member_variable_properties` 已完成首片真实执行，支持 category/tooltip/instance_editable/expose_on_spawn。
- [x] BlueprintVariableService 的 `set_member_default(s)` 已完成首片真实执行，写入 `FBPVariableDescription::DefaultValue` 并返回 ToolResultBase。
- [x] BlueprintVariableService 已重新收敛为 ToolResultBase façade / 编排层；member default 与 member property mutation 细节已迁入 `FBlueprintHelperMemberVariableMutationHandler`。
- [x] `FBlueprintHelperMemberVariableMutationHandler` 已注册到 `FBlueprintOperationHandlerRegistry`，覆盖 `set_member_default` / `set_member_defaults` / `set_member_variable_properties`。
- [x] MCP 回归已通过 `npm.cmd test`：Node 94/94，Python 26/26。
- [x] 11 类工具簇目录分类已完成；源码 UTF-8/TEXT() 修复后，用户本地已确认项目级 `Build.bat` 通过（`Build.bat MrStoneEditor Win64 Development -Project=G:\UnrealPractise\MrStone\MrStone.uproject`）。Codex 沙盒复跑会被 MrStone 工程级 `Intermediate` 写权限限制阻塞。
- [x] BlueprintVariableService 的 local variable read/add/set/remove 已接入真实 Service/OperationHandler 路径；local variable TaskPlan preview 走真实 dry-run，不再走 synthetic preview。
- [x] Component / AssetFactory / Widget / DataTable / ClassSettings 已从 Runtime synthetic preview 升级为服务级 true dry-run；TaskPlan preview 会调用对应 Service preflight，但不会进入实际 mutation/Modify/dirty 路径。
- [x] GraphWrite `replace_owned_graph` / `patch_owned_graph` / `merge_owned_graph` 已完成 TaskSpec schema、TS fallback compiler、Python Task Compiler、协议 fixtures 和 MCP 回归测试；编译结果仍是 compiler-owned `graph_write` structured IR，不暴露 `replace_blueprint_graph` / `patch_blueprint_graph` / `merge_blueprint_graph` 给 Agent。
- [x] UE TaskRuntime 已补 `replace_body` / `set_pin_default|set_node_comment|set_node_position` / `insert_flow` 的 structured IR lowering 源码与 automation contract tests，分别 lower 到现有 Replace/Patch/Merge capability cluster adapter payload。当前 Codex 沙盒无法完成项目级 Build.bat 验证，需要用户本地复跑确认 C++ 编译。
- [x] Composite `create_blueprint_feature` 已扩到 `integration.interface` 首片：TS schema、TS fallback compiler、Python Task Compiler、MCP 回归测试已支持把一个 Agent 语义 TaskSpec 分解为现有 `blueprint_component` / `blueprint_variable` / `blueprint_class_settings` / `blueprint_signature` / `graph_write` TaskPlan steps。`integration.input` 已按当前架构确认裁剪，继续显式拒绝；`allow_create_assets=true` 仍拒绝，避免资产创建被静默跳过。
- [x] TaskSpec / TaskPlan 执行语义已确认：Agent 只写少量语义顶层 TaskSpec；TaskPlan 是 compiler-owned 内部 IR；执行前先 dry-run 全部步骤，通过后顺序 execute；中途失败写入 TaskRunJournal partial failure，并按 TaskPlan 拓扑阻断后续依赖步骤，不默认承诺全局 rollback。
- [x] 2026-05-05 smoke rerun 已确认两条完整 TaskSpec -> Execute 链路通过：`edit_blueprint_graph` 的 `append_new_owned_graph + 新图名`（`task_5806121649296A709F32088EB10C55F0`）和 `edit_blueprint_variables`（`task_38C6DC0D4AC56E1DD89F4992D9A7B3AB`）。
- [ ] 2026-05-05 smoke rerun 发现 Bridge/UE 执行层当前只支持 `append_new_owned_graph + 新图名` 这一种 GraphWrite 策略组合；`replace_owned_graph` / `patch_owned_graph` / `merge_owned_graph`、`edit_blueprint_components`、`create_blueprint_feature` 仍被 Bridge 阻断。
- [ ] 2026-05-05 smoke rerun 发现 LogicJson `target_type=custom_event` 查找只搜索 EventGraph，忽略自定义图，导致自定义图里的 Custom Event 读回返回 EventGraph 数据。

## 总体差距矩阵

| 能力簇 | v0.3.6 来源 | UE DTO/Structure | UE Service | Bridge command | UE Task Runtime | Python/MCP TaskSpec | 当前状态 | 下一步 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ToolResultBase/CommonEnvelope | Done + FieldMapping | 完成，已迁到 `Structure` | 内部 builder 完成 | 通过各 command 返回 | 被 Runtime 复用 | MCP 侧仍有 normalize 层 | 完成 | 保持为统一返回协议，不再为单簇自定义外壳 |
| TaskRuntime core | 新架构文档 | TaskPlan/validation 使用 JSON | `TaskRuntime` 完成顺序执行器 | `preview_task_plan` / `execute_task_plan` / `get_task_run_journal` | 完成多 step、compile/save post operation、内存 TaskRunJournal 聚合 | MCP 任务工具已接 Python | 完成基础闭环 | 后续补 TOCTOU、持久 journal、preview blocker 丰富化 |
| Composite Blueprint Feature | 新架构文档 | TaskSpec schema 已接 `create_blueprint_feature` | 复用现有 Service | 复用 `preview_task_plan` / `execute_task_plan` | 复用多 step Runtime，新增 `blueprint_signature/ensure_function` 首片 | 完成 components/variables/class_settings/behavior/interface integration 分解到现有 capability steps | compiler-ready / Bridge blocked | 修复 Bridge/UE preview/execute 阻断后再列为 TaskSpec-ready |
| GraphWrite Append | Done + FieldMapping | 完成 | `AppendBlueprintGraphService` 完成 | `append_blueprint_graph` 完成 | 完成，`ensure_entry(custom_event)` structured IR lowering | 完成 `append_new_owned_graph` | smoke-verified：仅 `append_new_owned_graph + 新图名` | 扩展更多 entry/statement，不改变 TaskPlan 为结构化 IR 的方向；修复 LogicJson 自定义图读回 |
| GraphWrite Replace | Done + FieldMapping | 完成 | `ReplaceBlueprintGraphService` 完成 | `replace_blueprint_graph` 完成 | 源码已补 `replace_body` -> replace adapter lowering，Build.bat 可编译 | 完成 `replace_owned_graph` TaskSpec 编译 | compiler-ready / Bridge blocked | 修复 Bridge/UE preview blocking，再补 execute smoke |
| GraphWrite Patch | Done + FieldMapping | 完成 | `PatchBlueprintGraphService` 完成 | `patch_blueprint_graph` 完成 | 源码已补 `set_pin_default` / `set_node_comment` / `set_node_position` -> patch adapter lowering，Build.bat 可编译 | 完成 `patch_owned_graph` TaskSpec 编译 | compiler-ready / Bridge blocked | 修复 Bridge/UE preview blocking；同时补 LogicJson ref -> selector 合同 |
| GraphWrite Merge | Done + FieldMapping | 完成 | `MergeBlueprintGraphService` 完成 | `merge_blueprint_graph` 完成 | 源码已补 `insert_flow` -> merge adapter lowering，Build.bat 可编译 | 完成 `merge_owned_graph` TaskSpec 编译 | compiler-ready / Bridge blocked | 修复 Bridge/UE preview blocking；补 disposable flow fixture smoke |
| Cleanup BlueprintHelper Block | Done + FieldMapping | 完成 | `CleanupBlueprintHelperBlockService` 完成 | `cleanup_blueprint_helper_block` 完成 | 缺失 | 缺失 | 部分 | 作为 `graph_cleanup` 或 `ownership_cleanup` capability 接入 Runtime |
| Rollback Cleanup Transaction | Done + FieldMapping | 完成 | `RollbackCleanupTransactionService` 完成 | `rollback_cleanup_transaction` 完成 | 缺失 | 缺失 | 部分 | 作为 task rollback/journal 能力接入 Runtime，不作为普通写入默认步骤 |
| Convert Block To User Owned | Done + FieldMapping | 完成 | `ConvertBlockToUserOwnedService` 完成 | `convert_blueprint_helper_block_to_user_owned` 完成 | 缺失 | 缺失 | 部分 | 接入 `ownership` capability，并在高风险 replace/remove 前可由 TaskPlan 调用 |
| Blueprint Variables/Defaults/Local Variables | Done | 完成 | `BlueprintVariableService` 已支持 member add/remove、member property settings 首片、member default(s) 首片，以及 local variable read/add/set/remove；member/local mutation 细节已迁入 OperationHandler，Service 保持 ToolResultBase façade | 变量相关 command 完成 | 完成变量 IR lowering：ensure-only -> `add_blueprint_member_variables`，混合 member/default/local -> `blueprint_variable_batch`；local_variables preview 支持真实 dry-run | 完成 TaskSpec 编译：member changes/defaults/local variables | smoke-verified：`edit_blueprint_variables` execute | 扩默认值和属性设置更多类型；补更多 UE automation/smoke 覆盖；用户本地项目级 `Build.bat` 已通过；最近验证 task id：`task_38C6DC0D4AC56E1DD89F4992D9A7B3AB` |
| Function/Event Signature Management | Plan 文档 | 计划存在；TaskPlan 已有 `blueprint_signature` 首片 | 首片通过 Runtime 复用 `StructureService.AddGraph` 实现 `ensure_function`；完整 SignatureService 缺失 | 无 Agent-facing 原子 command；仅 TaskRuntime 内部执行 | 完成 `ensure_function` adapter 首片 | `integration.interface` 可编译到 `blueprint_signature` + `graph_write replace_body` | 部分/TaskPlan internal | 补完整 SignatureService：函数参数、返回值、interface function/event 区分、event dispatcher、override/native event |
| AssetFactory | FieldMapping | 完成 | `AssetFactoryService` 完成，支持 dry-run 冲突/创建预检且不创建资产 | `create_asset` 完成 | 完成 adapter，支持 `asset_factory/asset_create/create_asset`；preview 调 Service true dry-run | 完成 `create_asset` TaskSpec 编译 | compiler-ready / preview smoke covered | 后续补 execute smoke，再扩 DataTable/WidgetBlueprint/Material 等资产类型 |
| AssetDiscovery/EditorNavigation | Done + FieldMapping | 完成 | `AssetBrowseService` 完成 | `list_assets` / `search_assets` / `open_asset` / `get_asset_info` 完成 | 不需要默认写入 Runtime | 后续经 `ReadSpec` / `read_context` 进入只读上下文 | 部分 | 保持只读/导航能力，但不扩散成多 Agent-facing 原子工具 |
| ProjectContext/SetupState | Done + FieldMapping | 类型存在 | `ContextService` 基础存在 | `get_editor_context` 等入口存在 | 不属于写 Runtime | `read_task_context` 当前定位不清，标记 deprecated；后续经 `read_context` 重定义 | 部分 | 合并到 ReadSpec/CapabilitySchema，不保留模糊独立入口 |
| RuntimeProfile | Done + FieldMapping | 完成 | `RuntimeProfileService` 完成 | `get_runtime_profile` 完成 | 不属于写 Runtime | MCP 默认工具已有 runtime profile | 完成 | 保持 Agent preflight 只读入口 |
| Diagnostics | Done + FieldMapping | 完成 | `DiagnosticsService` 完成 | `diagnostics_runtime` 完成 | 未被 TaskRuntime 自动执行 | MCP 默认工具已有 diagnostics | 部分 | TaskRuntime 根据 execution_policy 增加 diagnostics 阶段 |
| CompileBlueprintAsset | Done + FieldMapping | 完成 | `CompileAssetService` 完成 | `compile_blueprint_asset` 完成 | 只把 `should_compile` 写入 validation，不实际调用 compile | 缺失 | 部分 | TaskRuntime 执行末尾按 `execution_policy.should_compile` 调用 |
| SaveAsset | Done + FieldMapping | 类型存在 | Bridge 内直接实现 | `save_asset` 完成 | 只把 `should_save` 写入 validation，不实际保存 | 缺失 | 部分 | TaskRuntime 执行末尾按 `execution_policy.should_save` 调用 |
| EditorLifecycle/RiskCommand | Done + FieldMapping | 完成 | `EditorCommandService` 完成 | undo/redo/PIE/close/console 完成 | 不应默认进入写 Runtime | 缺失 | 内部/debug | `open_editor` / `close_editor` 保留并迁移到 `blueprinthelper_*` 前缀；全局 undo/redo 从默认工具集中移除，后续改做 transaction 级 undo/redo |
| DebugExport/LargePayload | Done + FieldMapping | 类型存在 | 缺少完整 Service | 缺少完整 command | 缺失 | task context 中只保留 `large_payload_ref` 概念 | 部分 | 建立 debug export service，供失败定位和大 payload 分页 |
| DataAsset/Object Property | Done + FieldMapping | 类型存在 | `PropertyReflectionService` 完成通用 UObject 属性读写 | `get_object_properties` / `set_object_property` 完成 | 缺失 | 缺失 | 部分 | 接入 `data_asset` 或 `object_property` capability，统一 property path/value 字段 |
| DataTable | Done + FieldMapping | 完成 | `DataTableService` 完成，add/update/delete row 支持 true dry-run | get/add/update/delete row 完成 | 完成 adapter，支持 add/update/delete row；preview 调 Service true dry-run | 完成 `edit_data_table` TaskSpec 编译 | compiler-ready / fixture smoke pending | 确认 read 行为仍只读，不混入写 TaskPlan；补 disposable fixture smoke；扩更完整 row schema/field 类型覆盖 |
| UMG WidgetBlueprint | Done + FieldMapping | 完成 | `WidgetService` 完成，add/set_property/remove 支持 true dry-run | get/add/remove/move/get_properties/set_property 完成 | 部分 adapter，支持 add/set_property/remove，不支持 move/read；preview 调 Service true dry-run | 完成 `edit_umg_widget` TaskSpec 编译，不支持 move_widget | compiler-ready / fixture smoke pending | Runtime adapter 扩 move_widget 或保持明确不支持；补 disposable WidgetBlueprint fixture smoke |
| Blueprint Component | FieldMapping | 当前结构在 Service header 内，未完全拆到 Structure | `ComponentService` 完成，已统一 ToolResultBase，add/set/remove 支持 true dry-run | read/add/set/remove command 完成 | 部分 adapter，支持 add/set_properties/remove；preview 调 Service true dry-run | 完成 `edit_blueprint_components` TaskSpec 编译 | compiler-ready / Bridge blocked | 修复 Bridge/UE 对 `edit_blueprint_components` 的 preview/execute 阻断；把 component DTO 进一步迁到 Structure |
| Blueprint Class Settings | FieldMapping | 完成 | `ClassSettingsService` 完成，interface/default property 写入支持 true dry-run | read/add/remove interface/set class defaults 完成 | 部分 adapter，支持 interface/default property，不支持 reparent；preview 调 Service true dry-run | 完成 `edit_blueprint_class_settings` TaskSpec 编译，reparent 明确拒绝 | compiler-ready / fixture smoke pending | reparent 作为 future 或并入 Function/Event/Class signature 能力；补 interface/default property disposable fixture smoke |
| Internal Dependency Analysis / Reference Context | Done | 完成 | `Safety/DependencyAnalysisService` 部分完成 | `read_reference_context` 完成 | 不属于默认写 Runtime | MCP 只读工具已存在 | 部分/内部 | 保持 Agent 只读引用查看器；后续让高风险 remove/replace preview 可引用其 summary |
| LogicMD/LogicJson Read | FieldMapping + 架构文档 | 完成 | `Logic` 层完成 | read logic md/json command 完成 | 不属于写 Runtime | 用于上下文/调试，保留为 Agent 只读逻辑入口 | 只读/TaskSpec 辅助；LogicJson custom_event 有读回 bug | LogicMD 保持 v0.3.6 逻辑信息样式且不携带 TaskSpec draft；修复 LogicJson `custom_event` 只查 EventGraph 的问题；`node_ref` 如何映射到 TaskSpec 另行讨论 |
| TransactionJournalQuery | Done + FieldMapping | 完成 | `Transactions` 层完成 query | list/read transaction command 完成 | TaskRunJournal 目前是单独内存 journal | 缺失 | 部分 | 统一 child transaction 与 TaskRunJournal，补持久 task journal |

## 当前 Runtime 能力与 v0.3.6 的主要不一致

1. **[x] TaskRuntime 多 step 基础已补齐。**
   当前 `preview_task_plan` / `execute_task_plan` 已能顺序执行多个 TaskPlan step，并聚合 step result。剩余问题是 TOCTOU、防重入、以及长期持久化 journal。

2. **[x] `execution_policy.should_compile` / `execution_policy.should_save` 基础执行已补齐。**
   Runtime 已在非 dry-run 执行末尾调用 `compile_blueprint_asset` / `save_asset` post operation，并写入 runtime data 与 TaskRunJournal。后续需要补更细的失败恢复和 TOCTOU 处理。

3. **[x] P1 Python Compiler 覆盖已补齐首片。**
   Python/MCP TaskSpec 已覆盖 `asset_factory`、`blueprint_component`、`blueprint_class_settings`、`umg_widget`、`data_table`，并保持 Agent-facing 字段为语义层，不暴露 adapter operation。

4. **[x] P1 adapter dry-run 已升级为服务级 true dry-run。**
   AssetFactory、BlueprintComponent、BlueprintClassSettings、UMGWidget、DataTable 的 TaskPlan adapter 已标记 true dry-run 支持，并在 preview 时调用对应 Service preflight。dry-run 路径会解析目标资产、类/接口/属性/row/widget/component 等执行前置条件，但不会进入 `FBlueprintHelperScopedAssetMutation`、`Modify`、实际 Add/Remove/ImportText 写入、MarkBlueprint、AssetRegistry 创建或 DataTable row mutation 路径。Runtime synthetic preview 仍保留给尚未完成 true dry-run 的其他 adapter。

5. **部分 UE Service 仍是 stub 或只有 DTO / 底层 Bridge。**
   BlueprintVariableService 的 member property settings、member defaults、local variables 已完成首片真实执行，且实际 mutation 已迁入 OperationHandler；DebugExport/LargePayload、Function/Event Signature Management、DataAsset TaskPlan、Cleanup/Rollback/Ownership TaskPlan 还没有进入 TaskSpec-first 闭环。
6. **UE 构建验证状态。**
   后续统一使用项目级 `Build.bat`。源码 UTF-8/TEXT() 修复后，用户本地已确认 `Build.bat MrStoneEditor Win64 Development -Project=G:\UnrealPractise\MrStone\MrStone.uproject` 构建通过；当前 Codex 沙盒复跑会因 MrStone 工程级 `Intermediate` 写权限限制停在 UBT cache 写入阶段。MCP 回归已通过。

## 优先级建议

### P0：Runtime 闭环基础

1. [x] TaskRuntime 支持多 step 顺序执行。
2. [x] TaskRuntime 执行 `execution_policy.should_compile` / `execution_policy.should_save`。
3. [x] TaskRunJournal 合并 child result、validation、compile/save 结果。
4. [ ] Preview blocked 时返回更可读的 blockers，并可引用 `ReferenceContextPack`。

这些是所有能力簇共同依赖，不应推迟到单个能力后面。

### P1：补 Python/MCP TaskSpec 编译覆盖

优先给已经 TaskPlan-ready 的 UE capability 补 TaskSpec：

1. [x] `asset_factory`
2. [x] `blueprint_component`
3. [x] `blueprint_class_settings`
4. [x] `umg_widget`
5. [x] `data_table`
6. [x] `blueprint_variable` 的 set/remove/default/local TaskSpec 与 Runtime lowering
7. [x] GraphWrite replace/patch/merge TaskSpec compiler 与 structured IR lowering 源码
8. [x] `create_blueprint_feature` composite compiler 首片：把一个功能级 TaskSpec 分解到已接入的现有 capability steps

这批不需要先写大量 UE 新能力，主要补 TaskSpec schema、Python compiler、TS schema/test 与 MCP preview/execute contract。这里的 `[x]` 表示 compiler/contract/source 首片已补齐，不等同于全部 UE execute smoke 已通过；当前真实 smoke-verified execute 闭环只有 `edit_blueprint_graph + append_new_owned_graph + 新图名` 与 `edit_blueprint_variables`。

### P2：扩 UE 新能力簇

1. Function/Event Signature Management：函数参数、返回值、interface function vs interface event、event dispatcher、override/native event。
2. DataAsset/ObjectProperty TaskPlan adapter。
3. Cleanup/Rollback/Ownership TaskPlan adapter。
4. DebugExport/LargePayload service。
5. DependencyAnalysis 与高风险 preview 的集成。

## 后续讨论待办

1. [x] Agent-facing MCP 默认工具集合最终冻结：`blueprinthelper_read_agent_guide`、`blueprinthelper_get_runtime_profile`、`blueprinthelper_diagnostics`、`blueprinthelper_read_context`、`blueprinthelper_read_reference_context`、`blueprinthelper_preview_task`、`blueprinthelper_execute_task`、`blueprinthelper_get_task_result`、`blueprinthelper_open_editor`、`blueprinthelper_close_editor`。
2. [x] 旧 MCP 原子工具处理策略确认：已实现 TaskPlan adapter + TaskSpec compiler 覆盖的能力优先移除旧 Agent-facing 原子 MCP 工具；未覆盖能力暂保留为 legacy/internal/debug/expert/test，等 adapter 与 TaskSpec 支持落地时同步移除。
3. [x] 返回体分层最终冻结：`blueprinthelper_read_agent_guide` 返回 Markdown；其他默认读/任务工具使用 `BlueprintHelper.McpToolResult.v1` 外壳；`read_context` -> `ReadContextPack.v1`，`read_reference_context` -> `ReferenceContextPack.v1`，`preview_task` -> `TaskPreviewResult.v1`，`execute_task` -> `TaskRunSummary.v1` 或 `TaskRunJournal.v1`，`get_task_result` -> `TaskRunJournal.v1`；UE façade 统一 `FBlueprintHelperToolResultBase`，debug 走 `data.debug` 或 `large_payload_ref`。
4. [x] TaskRuntime partial failure 拓扑阻断合同：TaskPlan step 使用 `steps[].depends_on` 表达依赖；TaskRunJournal step status 固定为 `completed|failed|blocked|skipped`；blocked step 使用 `blocked_by_step_ids` / `blocked_reason`；partial failure 使用 `recovery.recommended_action`、`safe_to_retry`、`rollback_available`、`notes` 给出用户可读恢复建议；不默认全局 rollback。
5. [ ] Function/Event Signature 扩展字段合同：确认参数/返回值字段、interface function 与 interface event 选择规则、event dispatcher 字段、override/native event 字段与校验细节。
6. [x] LogicJson 与 TaskSpec 组合语义：已确认 `logic_json` 不返回 `taskspec_hints`，保持只读结构化逻辑视图；其他组合语义后续单独设计。
7. [x] LogicJson reference 映射边界：已确认 `node_ref` / `link_ref` 不能默认与 TaskSpec patch/merge selector 兼容；它们只是 read-view references。
8. [ ] ReadSpec 通用读层合同：`BlueprintHelper.ReadSpec.v1`、`blueprinthelper_read_context` 的主线定位已基本确认；能力面发现不再设计运行时 schema 查询工具，改由 `blueprinthelper_read_agent_guide` 返回 AgentGuide 索引，再由 AgentGuide 文件承载具体格式。
9. [x] 通用读格式：已确认 `logic_md` / `logic_json` 固定为所有可适配 read capability 的通用 view format；默认使用 `logic_md` 节省 token，只有精确定位、diff、patch/merge/debug 时使用 `logic_json`；`summary` 用于低 token 初筛，`schema` 用于字段说明且不读取资产正文。
10. [x] 移除运行时能力 schema 查询工具方向：能力面通过文档和 AgentGuide 获得，具体格式进入 AgentGuide 文件夹。
11. [x] Rule markdown 工具改名：从 `blueprint_get_rule_markdown` 迁移到 `blueprinthelper_read_agent_guide`；该工具返回 `Resources/AgentGuide/00_Agent_Onboarding_Index_20260504.md`，不再返回 `JsonToBlueprintRules.md`。
12. [ ] Transaction 级恢复：移除默认 `blueprint_undo` / `blueprint_redo` 后，设计基于 TaskRunJournal 或 UE transaction 的 undo/redo/replay 能力。
13. [x] ReadSpec target 字段收敛：已确认 `graph_name` / `function_name` / `event_name` 压缩为 `target.target_name`，由 `target.target_type` 解释；`block_id` 因 ownership/id 语义保留独立字段。
14. [x] Read result schema 短名规则：已确认 `data.schema` 使用 `ReadContextPack.v1`、`LogicMd.v1`、`LogicJson.v1` 等短名，不重复 `BlueprintHelper.` 前缀。
15. [x] ReadSpec 首批 `read_type` 已确认：`asset_context`、`blueprint_logic`、`component_context`、`variable_context`、`graph_context`、`widget_context`、`data_table_context`、`object_property_context`。
16. [x] ReadContextPack 首片返回字段：已确认使用 `payload` 承载具体 read view；不设置独立 `read_id`；只读结果不带 `diagnostics`，错误走外层 `error`，完整性用 `truncated` / `large_payload_ref`。
17. [x] AgentGuide 工具返回合同：`blueprinthelper_read_agent_guide` 无请求字段，返回 AgentGuide 索引 Markdown；它只负责文档入口，不读取 UE 资产，也不返回动态 schema。
18. [ ] ReadRef 到 WriteAnchor 转换合同：后续单独设计 `LogicJson.node_ref/link_ref` 如何安全转换为 TaskSpec patch/merge selector；首片不允许 Agent 直接把 read ref 当作写 anchor。
19. [x] Signature 能力职责确认：`blueprint_signature` 负责创建/确保、修改、移除函数签名、Custom Event 签名、interface function / interface event 入口、event dispatcher 签名、override/native event 入口；GraphWrite 负责 body、节点、连线、调用、bind/unbind。
20. [x] Custom Event 入口与 Append 依赖边界确认：`graph_write.ensure_entry(entry_type=custom_event)` 可以保留为 append 语义的结构化 IR，但 Custom Event 入口声明/签名创建必须由 `blueprint_signature.ensure_custom_event` 或 UE 内部 BlueprintSignatureService 完成；不得新增 Agent-facing custom event 原子工具。
21. [ ] `custom_event_definition` 与 Signature 边界：旧 Replace 文档中的 `custom_event_definition` 支持重建入口节点但保持同名同签名；需要确认它拆成 Signature 的声明/签名修改加 GraphWrite 的 body rewrite，还是保留为 GraphWrite 的兼容 lowering。
22. [ ] Interface/override/native event lowering 细节：已确认入口创建/选择归 Signature、GraphWrite 只写 body；仍需确认 interface event 与 function 的判定规则、override/native event 的选择字段、dry-run blocker 与错误码。
23. [ ] Signature removal 安全合同：移除签名必须先做引用/依赖风险分析；确认哪些 remove 允许直接删除 body、哪些只删除声明、哪些需要生成 GraphWrite cleanup step，以及 partial failure 时的 journal/recovery 字段。
24. [ ] Event Dispatcher 字段细节：dispatcher 声明、参数和签名属于 Function/Event Signature；dispatcher call/bind/unbind 这类图节点属于 GraphWrite。需要确认 TaskSpec 字段如何拆分并在 TaskPlan 中表达依赖。

## 下轮可并行拆分

| 任务 | 写入范围 | 是否冲突 | 建议模型 |
| --- | --- | --- | --- |
| GraphWrite replace/patch/merge Bridge/UE blocker 修复与 smoke 验证 | `Source/BlueprintHelper/Private/TaskRuntime`，Automation tests | 与新 UE 能力可能冲突 | 5.5 xhigh |
| Preview blocker / ReferenceContextPack 集成 | `TaskRuntime`、`DependencyAnalysisService`、MCP task result | 与 Runtime 改动冲突 | 5.5 xhigh |
| ReadSpec 通用读层设计与首片落地 | `BlueprintHelper_MCP_Server/src`，AgentGuide read schema，LogicMD/LogicJson adapter | 与写 Runtime 不冲突 | 5.5 xhigh |
| Function/Event Signature UE 能力设计落地 | `Source/BlueprintHelper/Public|Private/Services`，`Structure`，Bridge，Tests | 与 Runtime 基础低冲突 | 5.5 xhigh |
| Component DTO 迁出 Service header | `Structure` + `ComponentService` + tests | 与 Component compiler 不冲突 | 5.3 codex-spark xhigh |

## 推荐下一步

P0 与 P1 compiler/contract 首片已经完成，变量簇 member property/default/local variable 首片已经落到 UE Service + OperationHandler，且 `edit_blueprint_variables` 已完成 TaskSpec -> Execute smoke。AssetFactory、Component、ClassSettings、UMG、DataTable 已完成服务级 true dry-run 或 adapter 首片，但还需要按 smoke 结果分别补 execute fixture、Bridge/UE blocker 和读回验证。Composite `create_blueprint_feature` 已能把物理门这类核心功能 TaskSpec 分解为多 step TaskPlan，并已补 `integration.interface` 首片：确保接口、确保函数入口、用 GraphWrite replace_body 写接口函数实现；但当前 Bridge/UE 执行层仍阻断 composite。`integration.input` 已确认裁剪，不进入当前路线。GraphWrite replace/patch/merge 的 TaskSpec compiler 与 Runtime lowering 源码已接上，但 smoke rerun 表明当前可用 GraphWrite execute 组合只有 `append_new_owned_graph + 新图名`。下一步优先修执行层 blocker 和读回 bug，再继续更大 UE 新能力簇：

```text
Fix Bridge/UE blockers for GraphWrite replace/patch/merge, component, composite
-> Fix LogicJson custom_event lookup across custom graphs
-> Prepare fixtures and rerun AssetFactory/ClassSettings/UMG/DataTable execute smoke
-> Function/Event Signature Management 完整簇
-> DataAsset/ObjectProperty / Cleanup/Ownership / DebugExport
```

这样能继续沿 TaskSpec -> TaskPlan -> Runtime lowering 的结构化编辑语言方向扩展，不会退回到底层工具膨胀。
