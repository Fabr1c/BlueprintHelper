# BlueprintHelper 现有工具簇 TaskSpec 接入差距源代码审查

日期：2026-06-01  
性质：源代码审查与 implementation closure 更新。
结论状态：`edit_blueprint_components`、`edit_umg_widget`、`edit_data_table` 已在本轮接通 canonical TS compiler。

## 1. 当前结论

按 canonical TypeScript compiler 顶层 `task_type` 口径，当前 10 个 TaskSpec 类型均已进入 canonical TS compiler allow-list，并有对应顶层 lowering 分支：

1. `create_asset`
2. `create_blueprint_feature`
3. `edit_blueprint_graph`
4. `edit_blueprint_variables`
5. `edit_object_properties`
6. `edit_blueprint_signature`
7. `edit_blueprint_class_settings`
8. `edit_blueprint_components`
9. `edit_umg_widget`
10. `edit_data_table`

本轮之前的真实差距是：Component / UMGWidget / DataTable 已有 schema、TaskPlan capability 和 C++ TaskRuntime adapter，但 canonical TS compiler 顶层没有放行，导致 `preview_task` / `execute_task` 在 compile 阶段返回 `unsupported_task_type`。本轮已补齐该差距。

## 2. TaskSpec schema 与 compiler 放行清单

| TaskSpec `task_type` | schema 状态 | canonical TS compiler 状态 | 判断 |
| --- | --- | --- | --- |
| `create_asset` | 已定义 | 顶层直接处理 | 已放行 |
| `create_blueprint_feature` | 已定义 | 顶层直接处理 | 已放行 |
| `edit_blueprint_graph` | 已定义 | GraphWrite 主路径处理 | 已放行 |
| `edit_blueprint_variables` | 已定义 | 顶层直接处理 | 已放行 |
| `edit_object_properties` | 已定义 | 顶层直接处理 | 已放行 |
| `edit_blueprint_signature` | 已定义 | 顶层直接处理 | 已放行 |
| `edit_blueprint_class_settings` | 已定义 | 顶层直接处理 | 已放行 |
| `edit_blueprint_components` | 已定义 | 顶层直接处理 | 已放行 |
| `edit_umg_widget` | 已定义 | 顶层直接处理 | 已放行 |
| `edit_data_table` | 已定义 | 顶层直接处理 | 已放行 |

关键源代码证据：

- `AgentFaceService/task-core/src/task/schema/task-schemas.ts` 定义 10 个 TaskSpec schema。
- `AgentFaceService/task-core/src/task/compiler/task-compiler-registry.ts` 的 `CANONICAL_TS_TASK_TYPES` 现在包含 10 个 `task_type`。
- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts` 的 `compileTaskSpecToTaskPlan` 现在包含 `edit_blueprint_components`、`edit_umg_widget`、`edit_data_table` 顶层分支。
- `AgentFaceService/task-core/src/task/compiler/task-compiler.unblocked-clusters.test.ts` 固定三类新放行能力的 registry、fixture lowering 和语义错误负例。

## 3. 本轮接通的三个能力

| 工具簇 | TaskSpec `task_type` | TaskPlan capability | Runtime adapter | 本轮状态 |
| --- | --- | --- | --- | --- |
| Component | `edit_blueprint_components` | `blueprint_component` | `Component` | 已放行，lower 到 `component_tree` 单 op step |
| UMGWidget | `edit_umg_widget` | `umg_widget` | `UMGWidget` | 已放行，`create/delete` lower 到 `widget_tree_edit`，`update_widget_property` lower 到 `widget_property_edit` |
| DataTable | `edit_data_table` | `data_table` | `DataTable` | 已放行，lower 到 `row_edit` 单 row op step |

Component 的复合路径 `create_blueprint_feature.components` 仍保留；本轮新增的是独立顶层 `edit_blueprint_components` 的 canonical compiler path。

## 4. Signature 写入子集边界

`edit_blueprint_signature` 只承载声明级写入语义，必须排除 GraphWrite 已覆盖的图内写入。

Signature 子集限于：

- `ensure_function`
- `ensure_interface_function`
- `ensure_custom_event`
- `ensure_interface_event`
- `ensure_event_dispatcher`
- `ensure_override_event`
- `remove_signature`

明确排除 GraphWrite 已覆盖内容：

- function / event / custom event body
- node creation / deletion
- execution flow insert / replace / merge
- pin default / node comment patch
- delegate bind / assign / unbind / call use-site

## 5. 仍不应接入 TaskSpec 的 route cluster

以下 Bridge/tool cluster 仍属于 TaskSpec 之外的上下文、诊断、生命周期或结果消费面，不应作为普通 TaskSpec body 接入：

- `Core`
- `Debug`
- `SharedServices`
- `AssetBrowser`
- `AssetDiscovery`
- `EditorCommand`
- `Review`

其中 `AssetDiscovery` 是写入前的 `asset_path` 发现工具；`Review` 是 TaskSpec 执行后的查询、Accept/Reject 与回滚消费面。

## 6. 需要收敛或保持受限的 direct write 入口

`BlueprintStructure` direct write 子集仍需要避免绕过 TaskSpec / Review / owned-policy：

1. `blueprint_add_graph` / `blueprint_remove_graph`：不作为普通 Agent 顶层自由图结构写入；声明级入口归 Signature，图体归 GraphWrite。
2. `blueprint_add_event_dispatcher`：已有 `edit_blueprint_signature.ensure_event_dispatcher`，direct command 不应成为并行入口。
3. `blueprint_add_variable` / `blueprint_remove_variable`：已有 `edit_blueprint_variables`，direct command 属于 legacy 结构面。
4. `blueprint_delete_nodes`：不开放为自由删除 TaskSpec；GraphWrite 必须通过 owned/external policy 与 anchor 表达。
5. `blueprint_create_blueprint`：如需任务化，应通过 `create_asset` / `asset_factory`。
6. `blueprint_compile_blueprint` / `blueprint_save_asset`：由 TaskSpec validation/post-operation 控制；direct route 保持诊断或手动入口。

## 7. Agent-facing 模板状态

本轮同步：

- `taskspec_edit_umg_widget_template.json` 的 `target.target_type` 改为 `widget_blueprint`。
- `taskspec_edit_data_table_rows_template.json` 的 `target.target_type` 改为 `data_table`。

复查命令：

```powershell
rg -n "edit_blueprint_components|edit_umg_widget|edit_data_table|unsupported_task_type|not supported|unsupported|未放行|不支持" AgentFaceService/agent-guide/Templates/write AgentFaceService/agent-guide/Workflows ClaudePlugin/skills/blueprint-helper/references CodexPlugin/skills/blueprint-helper/references
```

结果：未发现把这三类能力描述为 compiler unsupported 的残留。命令命中的 `unsupported` 均为 GraphWrite policy、legacy `call_function`、用户偏好等无关边界说明。

## 8. 验证结果

| 验证项 | 命令 | 结果 | 证据摘要 |
| --- | --- | --- | --- |
| task-core build | `cd AgentFaceService/task-core; npm.cmd run build` | PASS | TypeScript build 通过 |
| task-core node tests | `cd AgentFaceService/task-core; npm.cmd run test:node` | PASS | `tests 316`, `pass 316`, `fail 0` |
| CLI build | `cd AgentFaceService/cli; npm.cmd run build` | PASS | CLI build 通过，并重新构建 task-core |
| CLI preview smoke: Component | `node AgentFaceService/cli/build/cli/index.js task preview --file .tmp/taskspec-compiler-connect/edit_blueprint_components.real-preview.json --format full` | PASS | `/Game/LevelPrototyping/Interactable/Door/BP_DoorFrame` preview passed，`task_type=edit_blueprint_components`，`capability=blueprint_component` |
| CLI preview smoke: UMGWidget | `node AgentFaceService/cli/build/cli/index.js task preview --file .tmp/taskspec-compiler-connect/edit_umg_widget.real-preview.json --format full` | PASS | `/Game/Variant_Combat/UI/UI_LifeBar.UI_LifeBar` preview passed，`task_type=edit_umg_widget`，`capability=umg_widget` |
| CLI preview smoke: DataTable | `node AgentFaceService/cli/build/cli/index.js task preview --file .tmp/taskspec-compiler-connect/edit_data_table.preview.json --format full` | PASS for compiler path / BLOCKED by fixture asset | 已生成 `data_table/row_edit` TaskPlan；UE runtime 阻塞原因为 `/Game/Data/DT_Weapons` 不存在，不是 `unsupported_task_type` |
| DataTable asset discovery | `blueprinthelper_find_assets --file .tmp/taskspec-compiler-connect/find_data_tables.json` | PASS | `/Game` 下返回 `assets: []`，当前项目缺少可用于真实 DataTable preview 的资产 |
| UE automation: TaskRuntime cluster | `Automation RunTests BlueprintHelper.TaskRuntime.Cluster` | PASS | 6 succeeded, 0 failed |
| UE automation: Component adapter | `Automation RunTests BlueprintHelper.TaskPlan.ComponentAdapter` | PASS | 4 succeeded, 0 failed |
| UE automation: Widget adapter | `Automation RunTests BlueprintHelper.TaskPlan.WidgetAdapter` | PASS | 4 succeeded, 0 failed |
| UE automation: DataTable adapter | `Automation RunTests BlueprintHelper.TaskPlan.DataTableAdapter` | PASS | 9 succeeded, 0 failed |

## 9. 最终分类

### 已由 canonical TS compiler 顶层放行

- `create_asset` / `AssetFactory`
- `create_blueprint_feature` / composite
- `edit_blueprint_graph` / `GraphWrite`
- `edit_blueprint_variables` / `BlueprintVariables`
- `edit_object_properties` / `ObjectProperty`
- `edit_blueprint_signature` / `Signature`
- `edit_blueprint_class_settings` / `ClassSettings`
- `edit_blueprint_components` / `Component`
- `edit_umg_widget` / `UMGWidget`
- `edit_data_table` / `DataTable`

### 已接线但 canonical TS compiler 顶层未放行

无。

### 未接入 TaskSpec 且通常不应接入

- `Core`
- `Debug`
- `SharedServices`
- `AssetBrowser`
- `AssetDiscovery`
- `EditorCommand`
- `Review`

## 10. 审查状态

已完成：

- 3 个未放行能力的 compiler allow-list 接通。
- 3 个未放行能力的 TaskSpec 到 TaskPlan lowering。
- 新增 compiler tests 并跑通完整 task-core Node 测试。
- 同步 UMG / DataTable 模板 target type。
- CLI preview smoke 验证不再出现 `unsupported_task_type`。
- UE adapter/runtime 自动化验证通过。

保留限制：

- 当前项目 `/Game` 下没有 DataTable 资产，因此 DataTable 真实资产 preview 只能验证到 compiler + TaskPlan + UE runtime 缺资产阻塞；DataTable adapter 的真实 UE 自动化已覆盖 row add/update/delete dry-run 与 shape rejection。
