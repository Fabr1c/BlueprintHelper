# TaskSpec Compiler 接通未放行工具簇报告

日期：2026-06-01

## 改动原因

`edit_blueprint_components`、`edit_umg_widget`、`edit_data_table` 已有 TaskSpec schema、TaskPlan capability 与 C++ TaskRuntime adapter，但 canonical TypeScript compiler 顶层没有放行。Agent 使用这些模板时会在 preview/execute 编译阶段遇到 `unsupported_task_type`。

本轮目标是只补齐 compiler 层的语义 lowering，不新增 runtime 工具簇，不绕过 TaskRuntime，不恢复 direct Bridge 写入口。

## 改动范围

- `AgentFaceService/task-core/src/task/compiler/task-compiler-registry.ts`
- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- `AgentFaceService/task-core/src/task/compiler/task-compiler.unblocked-clusters.test.ts`
- `AgentFaceService/task-core/src/tests/task/task-compiler-policy.test.ts`
- `AgentFaceService/task-core/src/tests/task/task-compiler.canonical-ts.test.ts`
- `AgentFaceService/agent-guide/Templates/write/taskspec_edit_umg_widget_template.json`
- `AgentFaceService/agent-guide/Templates/write/taskspec_edit_data_table_rows_template.json`
- `BlueprintHelper/Develop/Gap/BlueprintHelper_TaskSpec_UnconnectedToolClusters_SourceAudit_20260601_CN.md`
- `Debug/BlueprintHelper_TaskSpec_CompilerConnect_UnblockedClusters_20260601.md`

临时验证文件位于 `.tmp/taskspec-compiler-connect/`，用于 CLI preview smoke，不应提交。

## 改动过程

1. 先新增 `task-compiler.unblocked-clusters.test.ts`，固定三类能力的 registry 放行、fixture lowering 与语义负例。
2. 将三类 `task_type` 加入 canonical TS compiler allow-list。
3. 在 `compileTaskSpecToTaskPlan` 增加三类顶层分支。
4. 将 Component TaskSpec lower 到 `blueprint_component` / `component_tree` / 单 op step。
5. 将 UMGWidget TaskSpec lower 到 `umg_widget`，其中树编辑走 `widget_tree_edit`，属性编辑走 `widget_property_edit`。
6. 将 DataTable TaskSpec lower 到 `data_table` / `row_edit` / 单 row op step，并在 compiler 层要求 update row 必须有非空 `fields`。
7. 修正旧 policy/canonical tests 中把 `edit_blueprint_components` 当 unsupported 负例的过期前提。
8. 同步 UMG/DataTable 模板 target type。
9. 更新 Gap 与 Debug 文档。

## 改动结果

1. canonical TS compiler 顶层现在放行 10 个 TaskSpec 类型。
2. `edit_blueprint_components` 可从独立 TaskSpec 编译到 `blueprint_component` TaskPlan step。
3. `edit_umg_widget` 可从独立 TaskSpec 编译到 `umg_widget` TaskPlan step。
4. `edit_data_table` 可从独立 TaskSpec 编译到 `data_table` TaskPlan step。
5. Agent-facing UMG/DataTable 模板 target type 与 schema contract 对齐。
6. 旧 unsupported 负例已改为真正 registry-unsupported sentinel，避免继续残留“Component 未放行”的错误测试语义。

## 验证结果

| 验证项 | 命令 | 结果 | 摘要 |
| --- | --- | --- | --- |
| task-core build | `cd AgentFaceService/task-core; npm.cmd run build` | PASS | TypeScript build 通过 |
| task-core node tests | `cd AgentFaceService/task-core; npm.cmd run test:node` | PASS | `tests 316`, `pass 316`, `fail 0` |
| CLI build | `cd AgentFaceService/cli; npm.cmd run build` | PASS | CLI build 通过 |
| Component real preview | `node AgentFaceService/cli/build/cli/index.js task preview --file .tmp/taskspec-compiler-connect/edit_blueprint_components.real-preview.json --format full --max-bytes 20000` | PASS | `/Game/LevelPrototyping/Interactable/Door/BP_DoorFrame` preview passed |
| UMGWidget real preview | `node AgentFaceService/cli/build/cli/index.js task preview --file .tmp/taskspec-compiler-connect/edit_umg_widget.real-preview.json --format full --max-bytes 20000` | PASS | `/Game/Variant_Combat/UI/UI_LifeBar.UI_LifeBar` preview passed |
| DataTable preview | `node AgentFaceService/cli/build/cli/index.js task preview --file .tmp/taskspec-compiler-connect/edit_data_table.preview.json --format full --max-bytes 20000` | Compiler path PASS / runtime asset BLOCKED | 已生成 `data_table/row_edit` TaskPlan；阻塞原因为目标 DataTable 不存在 |
| DataTable asset discovery | `node AgentFaceService/cli/build/cli/index.js blueprinthelper_find_assets --file .tmp/taskspec-compiler-connect/find_data_tables.json --format full --max-bytes 12000` | PASS | 当前项目 `/Game` 下 `data_table` 资产为空 |
| UE automation: cluster | `Automation RunTests BlueprintHelper.TaskRuntime.Cluster` | PASS | 6 succeeded, 0 failed |
| UE automation: component adapter | `Automation RunTests BlueprintHelper.TaskPlan.ComponentAdapter` | PASS | 4 succeeded, 0 failed |
| UE automation: widget adapter | `Automation RunTests BlueprintHelper.TaskPlan.WidgetAdapter` | PASS | 4 succeeded, 0 failed |
| UE automation: data table adapter | `Automation RunTests BlueprintHelper.TaskPlan.DataTableAdapter` | PASS | 9 succeeded, 0 failed |

## 限制与后续

- 当前项目没有可用于真实 DataTable preview 的 `/Game` DataTable 资产；本轮通过 compiler-path preview 与 UE adapter automation 覆盖 DataTable 行编辑路径。
- `.tmp/taskspec-compiler-connect/` 下的 JSON 文件只用于本轮验证，不属于提交范围。
