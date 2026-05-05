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
asset_factory
blueprint_component
blueprint_class_settings
umg_widget
data_table
```

当前 Python/MCP Task Compiler 支持从 TaskSpec 编译出的任务类型：

```text
edit_blueprint_graph      -> graph_write，目前只支持 append_new_owned_graph/custom_event 首片
edit_blueprint_variables  -> blueprint_variable，目前只支持 ensure_member_variable
```

因此当前真正 Agent-facing 的 TaskSpec-first 写入闭环是：

```text
GraphWrite Append 首片
Blueprint Member Variable ensure 首片
```

其余已接入 UE Task Runtime 的 capability 目前主要是 TaskPlan-ready，不是 TaskSpec-ready。Agent 仍不应直接调用这些底层 MCP 工具；下一阶段应该补 Python Task Compiler 和 TaskSpec schema。

## 总体差距矩阵

| 能力簇 | v0.3.6 来源 | UE DTO/Structure | UE Service | Bridge command | UE Task Runtime | Python/MCP TaskSpec | 当前状态 | 下一步 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ToolResultBase/CommonEnvelope | Done + FieldMapping | 完成，已迁到 `Structure` | 内部 builder 完成 | 通过各 command 返回 | 被 Runtime 复用 | MCP 侧仍有 normalize 层 | 完成 | 保持为统一返回协议，不再为单簇自定义外壳 |
| TaskRuntime core | 新架构文档 | TaskPlan/validation 使用 JSON | `TaskRuntime` 完成基础执行器 | `preview_task_plan` / `execute_task_plan` / `get_task_run_journal` | 部分，只支持单 step，内存 journal | MCP 任务工具已接 Python | 部分 | 增加多 step、compile/save 执行、TOCTOU、持久 journal |
| GraphWrite Append | Done + FieldMapping | 完成 | `AppendBlueprintGraphService` 完成 | `append_blueprint_graph` 完成 | 完成，支持 `graph_write` IR lowering | 完成首片，只支持 append_new_owned_graph/custom_event | 完成首片 | 扩展更多 entry/statement，不改变 TaskPlan 为结构化 IR 的方向 |
| GraphWrite Replace | Done + FieldMapping | 完成 | `ReplaceBlueprintGraphService` 完成 | `replace_blueprint_graph` 完成 | 部分，Runtime 可 lowering | TaskSpec compiler 缺失 | 部分 | 增加 replace/patch/merge 的 TaskSpec 编译，不暴露 adapter operation 给 Agent |
| GraphWrite Patch | Done + FieldMapping | 完成 | `PatchBlueprintGraphService` 完成 | `patch_blueprint_graph` 完成 | 部分，Runtime 可 lowering | TaskSpec compiler 缺失 | 部分 | 同上 |
| GraphWrite Merge | Done + FieldMapping | 完成 | `MergeBlueprintGraphService` 完成 | `merge_blueprint_graph` 完成 | 部分，Runtime 可 lowering | TaskSpec compiler 缺失 | 部分 | 同上 |
| Cleanup BlueprintHelper Block | Done + FieldMapping | 完成 | `CleanupBlueprintHelperBlockService` 完成 | `cleanup_blueprint_helper_block` 完成 | 缺失 | 缺失 | 部分 | 作为 `graph_cleanup` 或 `ownership_cleanup` capability 接入 Runtime |
| Rollback Cleanup Transaction | Done + FieldMapping | 完成 | `RollbackCleanupTransactionService` 完成 | `rollback_cleanup_transaction` 完成 | 缺失 | 缺失 | 部分 | 作为 task rollback/journal 能力接入 Runtime，不作为普通写入默认步骤 |
| Convert Block To User Owned | Done + FieldMapping | 完成 | `ConvertBlockToUserOwnedService` 完成 | `convert_blueprint_helper_block_to_user_owned` 完成 | 缺失 | 缺失 | 部分 | 接入 `ownership` capability，并在高风险 replace/remove 前可由 TaskPlan 调用 |
| Blueprint Variables/Defaults/Local Variables | Done | 完成 | `BlueprintVariableService` 多接口完成 | 变量相关 command 完成 | 部分，只 lowering `ensure_member_variable` 到 `add_blueprint_member_variables` | 部分，只支持 `edit_blueprint_variables/ensure_member_variable` | 部分 | 补 set/remove/default/local variable 的 TaskSpec 和 Runtime lowering |
| Function/Event Signature Management | Plan 文档 | 计划存在 | 缺失 | 缺失 | 缺失 | 缺失 | 缺失 | 下一批高优先级 UE capability，作为 TaskPlan step 调用，不作为 Agent 原子入口 |
| AssetFactory | FieldMapping | 完成 | `AssetFactoryService` 完成 | `create_asset` 完成 | 完成 adapter，支持 `asset_factory/asset_create/create_asset` | 缺失 | TaskPlan-ready | 补 TaskSpec `create_asset` 语义编译；确认 dry-run 是否真实 |
| AssetDiscovery/EditorNavigation | Done + FieldMapping | 完成 | `AssetBrowseService` 完成 | `list_assets` / `search_assets` / `open_asset` / `get_asset_info` 完成 | 不需要默认写入 Runtime | `read_task_context` 只用一部分 | 部分 | 保持只读/导航能力，补 TaskContextPack 中更稳定的 asset summary |
| ProjectContext/SetupState | Done + FieldMapping | 类型存在 | `ContextService` 基础存在 | `get_editor_context` 等入口存在 | 不属于写 Runtime | `read_task_context` 使用部分上下文 | 部分 | 合并到 TaskContextPack，不扩 Agent 工具面 |
| RuntimeProfile | Done + FieldMapping | 完成 | `RuntimeProfileService` 完成 | `get_runtime_profile` 完成 | 不属于写 Runtime | MCP 默认工具已有 runtime profile | 完成 | 保持 Agent preflight 只读入口 |
| Diagnostics | Done + FieldMapping | 完成 | `DiagnosticsService` 完成 | `diagnostics_runtime` 完成 | 未被 TaskRuntime 自动执行 | MCP 默认工具已有 diagnostics | 部分 | TaskRuntime 根据 execution_policy 增加 diagnostics 阶段 |
| CompileBlueprintAsset | Done + FieldMapping | 完成 | `CompileAssetService` 完成 | `compile_blueprint_asset` 完成 | 只把 `should_compile` 写入 validation，不实际调用 compile | 缺失 | 部分 | TaskRuntime 执行末尾按 `execution_policy.should_compile` 调用 |
| SaveAsset | Done + FieldMapping | 类型存在 | Bridge 内直接实现 | `save_asset` 完成 | 只把 `should_save` 写入 validation，不实际保存 | 缺失 | 部分 | TaskRuntime 执行末尾按 `execution_policy.should_save` 调用 |
| EditorLifecycle/RiskCommand | Done + FieldMapping | 完成 | `EditorCommandService` 完成 | undo/redo/PIE/close/console 完成 | 不应默认进入写 Runtime | 缺失 | 内部/debug | 保持 high-risk/debug/expert，不纳入普通 TaskSpec |
| DebugExport/LargePayload | Done + FieldMapping | 类型存在 | 缺少完整 Service | 缺少完整 command | 缺失 | task context 中只保留 `large_payload_ref` 概念 | 部分 | 建立 debug export service，供失败定位和大 payload 分页 |
| DataAsset/Object Property | Done + FieldMapping | 类型存在 | `PropertyReflectionService` 完成通用 UObject 属性读写 | `get_object_properties` / `set_object_property` 完成 | 缺失 | 缺失 | 部分 | 接入 `data_asset` 或 `object_property` capability，统一 property path/value 字段 |
| DataTable | Done + FieldMapping | 完成 | `DataTableService` 完成 | get/add/update/delete row 完成 | 完成 adapter，支持 add/update/delete row | 缺失 | TaskPlan-ready | 补 TaskSpec 编译；确认 read 行为仍只读，不混入写 TaskPlan |
| UMG WidgetBlueprint | Done + FieldMapping | 完成 | `WidgetService` 完成 | get/add/remove/move/get_properties/set_property 完成 | 部分 adapter，支持 add/set_property/remove，不支持 move/read | 缺失 | TaskPlan-ready | 补 TaskSpec 编译；Runtime adapter 扩 move_widget 或明确不支持 |
| Blueprint Component | FieldMapping | 当前结构在 Service header 内，未完全拆到 Structure | `ComponentService` 完成，已统一 ToolResultBase | read/add/set/remove command 完成 | 部分 adapter，支持 add/set_properties/remove，preview 为 synthetic dry-run | 缺失 | TaskPlan-ready 但 dry-run 弱 | 把 component DTO 进一步迁到 Structure；实现真实 dry-run 或标注 preview limitation |
| Blueprint Class Settings | FieldMapping | 完成 | `ClassSettingsService` 完成 | read/add/remove interface/set class defaults 完成 | 部分 adapter，支持 interface/default property，不支持 reparent | 缺失 | TaskPlan-ready | 补 TaskSpec 编译；reparent 明确作为 future 或实现 |
| Internal Dependency Analysis / Reference Context | Done | 完成 | `Safety/DependencyAnalysisService` 部分完成 | `read_reference_context` 完成 | 不属于默认写 Runtime | MCP 只读工具已存在 | 部分/内部 | 保持 Agent 只读引用查看器；后续让高风险 remove/replace preview 可引用其 summary |
| LogicMD/LogicJson Read | FieldMapping + 架构文档 | 完成 | `Logic` 层完成 | read logic md/json command 完成 | 不属于写 Runtime | 用于上下文/调试，非默认写入口 | 内部/只读 | 后续由 Task Compiler 使用，避免 Agent 为写流程直读大量底层 JSON |
| TransactionJournalQuery | Done + FieldMapping | 完成 | `Transactions` 层完成 query | list/read transaction command 完成 | TaskRunJournal 目前是单独内存 journal | 缺失 | 部分 | 统一 child transaction 与 TaskRunJournal，补持久 task journal |

## 当前 Runtime 能力与 v0.3.6 的主要不一致

1. **TaskRuntime 只支持一个 TaskPlan step。**
   当前 `preview_task_plan` / `execute_task_plan` 要求 `steps` 数量为 1。v0.3.6 与新架构目标需要一次 TaskPlan 能包含多个结构化 step，例如先创建资产，再加组件，再写图，再编译/保存。

2. **`execution_policy.should_compile` / `execution_policy.should_save` 目前只进入 validation summary。**
   Runtime 没有在 task 执行末尾实际调用 `compile_blueprint_asset` 或 `save_asset`。这会让 TaskSpec 的验证策略看起来已经传递，但真实编辑器动作没有闭环。

3. **Python Compiler 覆盖面小于 UE Runtime。**
   UE Runtime 已能 lowering `asset_factory`、`blueprint_component`、`blueprint_class_settings`、`umg_widget`、`data_table`，但 Python/MCP TaskSpec 目前只编译 `graph_write` 首片和 `blueprint_variable` 首片。

4. **部分 adapter 的 preview 不是严格 dry-run。**
   Component adapter 明确存在 synthetic dry-run。AssetFactory、Widget、DataTable、ClassSettings 也需要逐项确认 dry-run 是否只预检不修改。

5. **部分 v0.3.6 能力仍只有 DTO 或底层 Bridge。**
   DebugExport/LargePayload、Function/Event Signature Management、DataAsset TaskPlan、Cleanup/Rollback/Ownership TaskPlan 还没有进入 TaskSpec-first 闭环。

## 优先级建议

### P0：先修 Runtime 闭环基础

1. TaskRuntime 支持多 step 顺序执行。
2. TaskRuntime 执行 `execution_policy.should_compile` / `execution_policy.should_save`。
3. TaskRunJournal 合并 child result、child transaction、validation、compile/save 结果。
4. Preview blocked 时返回可读的 blockers，并可引用 `ReferenceContextPack`。

这些是所有能力簇共同依赖，不应推迟到单个能力后面。

### P1：补 Python/MCP TaskSpec 编译覆盖

优先给已经 TaskPlan-ready 的 UE capability 补 TaskSpec：

1. `asset_factory`
2. `blueprint_component`
3. `blueprint_class_settings`
4. `umg_widget`
5. `data_table`
6. `blueprint_variable` 的 set/remove/default/local 扩展
7. GraphWrite replace/patch/merge

这批不需要先写大量 UE 新能力，主要补 TaskSpec schema、Python compiler、TS schema/test 与 MCP preview/execute contract。

### P2：扩 UE 新能力簇

1. Function/Event Signature Management。
2. DataAsset/ObjectProperty TaskPlan adapter。
3. Cleanup/Rollback/Ownership TaskPlan adapter。
4. DebugExport/LargePayload service。
5. DependencyAnalysis 与高风险 preview 的集成。

## 下轮可并行拆分

| 任务 | 写入范围 | 是否冲突 | 建议模型 |
| --- | --- | --- | --- |
| Runtime 多 step + execution_policy compile/save | `Source/BlueprintHelper/Public|Private/TaskRuntime`，BridgeRouter 构造依赖 | 会和 Runtime adapter 冲突 | 5.5 xhigh |
| Python TaskSpec schema/编译 AssetFactory + DataTable | `BlueprintHelper_MCP_Server/python`，`src/task-schemas.ts`，测试 | 与 UE 不冲突 | 5.5 xhigh |
| Python TaskSpec schema/编译 Component + ClassSettings | `BlueprintHelper_MCP_Server/python`，`src/task-schemas.ts`，测试 | 与上一项可能冲突在 schema | 5.5 xhigh，需分文件或串行合并 |
| UMG Widget TaskSpec compiler | `BlueprintHelper_MCP_Server/python`，`src/task-schemas.ts`，测试 | 与其他 Python compiler 有 schema 冲突 | 5.5 xhigh |
| Function/Event Signature UE 能力设计落地 | `Source/BlueprintHelper/Public|Private/Services`，`Structure`，Bridge，Tests | 与 Runtime 基础低冲突 | 5.5 xhigh |
| Component DTO 迁出 Service header | `Structure` + `ComponentService` + tests | 与 Component compiler 不冲突 | 5.3 codex-spark xhigh |

## 推荐下一步

先做 P0 的 Runtime 基础，而不是继续横向接更多 adapter：

```text
TaskRuntime multi-step
-> execution_policy compile/save
-> TaskRunJournal 聚合
-> 再扩 Python TaskSpec 编译覆盖
```

这样后续每接一个能力簇，都能自然进入真实 TaskSpec-first 闭环，不会退回到工具膨胀。
