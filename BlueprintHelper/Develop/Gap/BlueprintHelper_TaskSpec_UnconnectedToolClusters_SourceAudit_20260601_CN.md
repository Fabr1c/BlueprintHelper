# BlueprintHelper 现有工具簇 TaskSpec 接入差距源代码审查

日期：2026-06-01  
性质：只读源代码审查；未修改运行时代码。  
问题：还有哪些现有工具簇没有接入 TaskSpec；哪些已经在 schema/runtime/TaskPlan 接线，但 canonical TS compiler 顶层尚未放行。

## 1. 结论

按 canonical TypeScript compiler 顶层 `task_type` 口径，当前明确放行 7 个 TaskSpec 类型：

1. `create_asset`
2. `create_blueprint_feature`
3. `edit_blueprint_graph`
4. `edit_blueprint_variables`
5. `edit_object_properties`
6. `edit_blueprint_signature`
7. `edit_blueprint_class_settings`

按 TaskSpec schema union 口径，当前定义了 10 个 `task_type`。其中 3 个属于“schema/TaskPlan/C++ runtime 簇已接入，但 canonical TS compiler 顶层未放行”，会在当前 canonical compiler 路径落到 `unsupported_task_type`：

1. `edit_blueprint_components`
2. `edit_umg_widget`
3. `edit_data_table`

因此，`edit_umg_widget` 确实属于 UMG 工具簇，但它只是 schema/runtime/TaskPlan 侧存在，不能算 canonical TS compiler 顶层已接入。上一版把 UMGWidget/DataTable/Component 按 runtime 簇存在误归为“已接入 TaskSpec”，这个口径不准确，已在本版修正。

重要边界：`blueprint_signature` 的写入子集必须排除 GraphWrite 已有能力。Signature 只承载声明/签名级语义；函数体、事件体、图内节点、执行流、pin/default/comment patch、delegate bind/call/clear use-site 等图内逻辑仍归 `graph_write`。

## 2. TaskSpec schema 与 compiler 放行清单

| TaskSpec `task_type` | schema 状态 | canonical TS compiler 状态 | 判断 |
| --- | --- | --- | --- |
| `create_asset` | 已定义 | 顶层直接处理 | 已放行 |
| `create_blueprint_feature` | 已定义 | 顶层直接处理 | 已放行 |
| `edit_blueprint_graph` | 已定义 | 顶层主路径处理 | 已放行 |
| `edit_blueprint_variables` | 已定义 | 顶层直接处理 | 已放行 |
| `edit_object_properties` | 已定义 | 顶层直接处理 | 已放行 |
| `edit_blueprint_signature` | 已定义 | 顶层直接处理 | 已放行 |
| `edit_blueprint_class_settings` | 已定义 | 顶层直接处理 | 已放行 |
| `edit_blueprint_components` | 已定义 | 顶层未处理；落到 `unsupported_task_type` | 部分接入，compiler 未放行 |
| `edit_umg_widget` | 已定义 | 顶层未处理；落到 `unsupported_task_type` | 部分接入，compiler 未放行 |
| `edit_data_table` | 已定义 | 顶层未处理；落到 `unsupported_task_type` | 部分接入，compiler 未放行 |

证据：

- `AgentFaceService/task-core/src/task/schema/task-schemas.ts:714-944` 定义 10 个 TaskSpec schema。
- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:83-104` 顶层 compiler 只分支处理 7 个类型，其他非 `edit_blueprint_graph` 类型抛 `unsupported_task_type`。
- `AgentFaceService/task-core/src/task/compiler/task-compiler-registry.ts:10-18` 的 `CANONICAL_TS_TASK_TYPES` 也只包含同样 7 个类型。

## 3. 已接线但 compiler 未放行的完整清单

| 工具簇 | TaskSpec `task_type` | TaskPlan capability | C++ runtime 簇 | 当前真实状态 |
| --- | --- | --- | --- | --- |
| `Component` | `edit_blueprint_components` | `blueprint_component` | `Component` | 独立顶层 `task_type` 未放行；仅 `create_blueprint_feature.components` 复合路径会编译出 component step。 |
| `UMGWidget` | `edit_umg_widget` | `umg_widget` | `UMGWidget` | schema、TaskPlan step、runtime 簇存在；canonical TS compiler 顶层未放行。 |
| `DataTable` | `edit_data_table` | `data_table` | `DataTable` | schema、TaskPlan step、runtime 簇存在；canonical TS compiler 顶层未放行。 |

证据：

- `AgentFaceService/task-core/src/task/schema/task-schemas.ts:797` 定义 `edit_blueprint_components`。
- `AgentFaceService/task-core/src/task/schema/task-schemas.ts:835` 定义 `edit_umg_widget`。
- `AgentFaceService/task-core/src/task/schema/task-schemas.ts:852` 定义 `edit_data_table`。
- `AgentFaceService/task-core/src/task/schema/task-schemas.ts:1233-1258` 定义 `blueprint_component`、`umg_widget`、`data_table` 等 TaskPlan step schema。
- `BlueprintHelper/Source/BlueprintHelper/Public/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.h:35-43` 声明 9 个 runtime cluster，包括 Component、UMGWidget、DataTable。
- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.cpp:92-119` 执行分发表已接入 Component、UMGWidget、DataTable。
- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.cpp:163-176` Review evidence 分发表也包含 Component、UMGWidget、DataTable。

## 4. Component 的特殊情况

`edit_blueprint_components` 作为独立顶层 TaskSpec 未放行，但 component 能力并非完全不可用。当前 compiler 只在复合任务 `create_blueprint_feature` 的 `components` 字段中生成 `blueprint_component` TaskPlan step。

证据：

- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:86-87` 放行 `create_blueprint_feature`。
- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:180` 在复合编译中调用 `compileCompositeComponentSteps`。
- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:250-285` 将 `components[]` 降低为 `blueprint_component` / `component_tree` TaskPlan step。
- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:101-104` 独立 `edit_blueprint_components` 仍会落到 `unsupported_task_type`。

结论：Component 应标为“复合路径已放行，独立顶层 task_type 未放行”，不能与 `edit_blueprint_variables`、`edit_blueprint_class_settings` 这类顶层已放行类型混为一类。

## 5. 对外模板/工作流暴露风险

当前 Agent-facing 文档与模板仍暴露这 3 个未放行顶层 `task_type`，这会让普通 Agent 构造出 schema 能 parse、但 preview/compile 被 canonical compiler 拒绝的任务。

| 暴露面 | 暴露内容 | 风险 |
| --- | --- | --- |
| `AgentFaceService/agent-guide/Templates/write/taskspec_edit_blueprint_components_template.json` | `task_type: edit_blueprint_components` | 独立组件编辑模板当前会 compiler unsupported。 |
| `AgentFaceService/agent-guide/Templates/write/taskspec_edit_umg_widget_template.json` | `task_type: edit_umg_widget` | UMG 模板当前会 compiler unsupported。 |
| `AgentFaceService/agent-guide/Templates/write/taskspec_edit_data_table_rows_template.json` | `task_type: edit_data_table` | DataTable 行编辑模板当前会 compiler unsupported。 |
| `AgentFaceService/agent-guide/Templates/write/SEMANTIC_INDEX.md` | 列出 Component/UMG/DataTable 编辑模板 | 语义索引会引导 Agent 选择未放行模板。 |
| `AgentFaceService/agent-guide/Workflows/06_UMG_Data_Workflows.md` | 指导 UMG/DataTable 使用 `edit_umg_widget` / `edit_data_table` | 工作流描述与 compiler 当前行为不一致。 |
| `ClaudePlugin` / `CodexPlugin` 复制的 BlueprintHelper references | 同步传播 UMG/DataTable TaskSpec-first 说明 | 插件侧 sideagent 可能按过期能力构造任务。 |
| `AgentFaceService/task-core/src/task/fixtures/task-protocol.fixtures.ts` | fixture 中包含这 3 个 `task_type` | fixture/协议样本会强化“可用”假象，但不是 canonical compiler 放行证据。 |
| C++ adapter/result 测试 | 硬编码这 3 个 `task_type` | 这些测试验证 adapter/result 层，不验证 canonical TS compiler 顶层放行。 |
| Debug 样本 | 记录 `edit_umg_widget` 的 `unsupported_task_type` | 已有真实回归样本证明 UMG 顶层 TaskSpec 当前不可执行。 |

证据：

- `AgentFaceService/agent-guide/Templates/write/taskspec_edit_blueprint_components_template.json:3`
- `AgentFaceService/agent-guide/Templates/write/taskspec_edit_umg_widget_template.json:3`
- `AgentFaceService/agent-guide/Templates/write/taskspec_edit_data_table_rows_template.json:3`
- `AgentFaceService/agent-guide/Templates/write/SEMANTIC_INDEX.md:31`
- `AgentFaceService/agent-guide/Templates/write/SEMANTIC_INDEX.md:65-66`
- `AgentFaceService/agent-guide/Workflows/06_UMG_Data_Workflows.md:17-21`
- `AgentFaceService/agent-guide/Workflows/06_UMG_Data_Workflows.md:41-49`
- `AgentFaceService/task-core/src/task/fixtures/task-protocol.fixtures.ts:1149-1159`
- `AgentFaceService/task-core/src/task/fixtures/task-protocol.fixtures.ts:1383-1393`
- `AgentFaceService/task-core/src/task/fixtures/task-protocol.fixtures.ts:1491-1501`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/BlueprintComponent/BlueprintHelperComponentToolResultBaseTests.cpp:309-312`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/UMGWidget/BlueprintHelperTaskPlanWidgetAdapterTests.cpp:92-97`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp:1579-1657`
- `Debug/BlueprintHelper_ReviewPanelRegenEvents_20260601.md:40-55`
- `Debug/TaskSpecs/ReviewPanelRegen_20260601_125756/04_widget_tree.json:1-4`

## 6. 已放行且端到端口径较一致的簇

| 簇 | TaskSpec | compiler | TaskPlan/runtime |
| --- | --- | --- | --- |
| `GraphWrite` | `edit_blueprint_graph` | 已放行 | 已接 runtime |
| `BlueprintVariables` | `edit_blueprint_variables` | 已放行 | 已接 runtime |
| `AssetFactory` | `create_asset` | 已放行 | 已接 runtime |
| `ClassSettings` | `edit_blueprint_class_settings` | 已放行 | 已接 runtime |
| `Signature` | `edit_blueprint_signature` | 已放行 | 已接 runtime |
| `ObjectProperty` | `edit_object_properties` | 已放行 | 已接 runtime |
| `CompositeFeature` | `create_blueprint_feature` | 已放行 | 降低到 Component/Variables/ClassSettings/Signature/GraphWrite 等 step |

## 7. 未接入 TaskSpec 且通常不应接入的 route cluster

按 `BridgeRoutePlanner` 的 route cluster 口径，以下簇存在于 Bridge/工具层，但通常不应直接纳入 TaskSpec body：

1. `Core`：环境、规则、写会话等执行前服务。
2. `Debug`：诊断、debug case、compile/save 手动入口。
3. `SharedServices`：读取上下文、导入导出、校验等辅助服务。
4. `AssetBrowser`：打开、保存、资产信息等 UI/导航/手动操作。
5. `AssetDiscovery`：未知 `asset_path` 的前置发现。
6. `EditorCommand`：undo/redo/PIE/console/lifecycle 控制。
7. `Review`：TaskSpec 执行后的查询、Accept/Reject、回滚消费面。

这些不属于“接入但 compiler 未放行”，而是 TaskSpec 之外的上下文、诊断、生命周期或结果消费面。

## 8. 需要收敛或替换的 direct write 语义

`BlueprintStructure` 的写入子集仍需要单独治理，重点是避免绕过 TaskSpec/Review/owned-policy：

1. `blueprint_add_graph` / `blueprint_remove_graph`：不应作为普通 Agent 顶层自由图结构写入；如仅为声明/签名入口服务，应纳入 Signature 的声明级子集。
2. `blueprint_add_event_dispatcher`：已有 `edit_blueprint_signature.ensure_event_dispatcher`，direct command 不应成为并行入口。
3. `blueprint_add_variable` / `blueprint_remove_variable`：已有 `edit_blueprint_variables`，direct command 属于 legacy 结构面。
4. `blueprint_delete_nodes`：不应开放为自由删除 TaskSpec；GraphWrite 仍应通过 owned/external policy 与 anchor 表达。
5. `blueprint_create_blueprint`：如需任务化，应通过 `create_asset` / `asset_factory`，不是 EditorCommand direct route。
6. `blueprint_compile_blueprint` / `blueprint_save_asset`：由 TaskSpec validation/post-operation 控制；direct route 保持诊断或手动入口。

Signature 子集仅限：

- `ensure_function`
- `ensure_interface_function`
- `ensure_custom_event`
- `ensure_interface_event`
- `ensure_event_dispatcher`
- `ensure_override_event`
- `remove_signature`

明确排除 GraphWrite 已覆盖内容：

- function/event/custom event body
- node creation/deletion
- execution flow insert/replace/merge
- pin/default/comment patch
- delegate bind/assign/unbind/call use-site

## 9. 测试覆盖缺口

当前未找到针对以下 3 个 schema-only 顶层 `task_type` 的 compiler 测试：

1. `edit_blueprint_components`
2. `edit_umg_widget`
3. `edit_data_table`

证据：`rg -n "edit_blueprint_components|edit_umg_widget|edit_data_table|unsupported_task_type" AgentFaceService/task-core/src -g "*.test.ts"` 无匹配。

补充：C++ 测试中存在这 3 个 `task_type`，但覆盖的是 adapter/result 层，不是 canonical TS compiler 顶层拒绝或放行行为。

建议至少补两类测试：

1. 若短期维持未放行：增加 explicit unsupported 测试，确保错误消息稳定且文档/模板不再引导普通 Agent 使用。
2. 若准备放行：补 canonical compiler 降低测试，并同步模板、AgentGuide、ClaudePlugin、CodexPlugin、C++ adapter/runtime 端到端 preview 证据。

## 10. 最终分类

### 已由 canonical TS compiler 顶层放行

- `create_asset` / `AssetFactory`
- `create_blueprint_feature` / composite
- `edit_blueprint_graph` / `GraphWrite`
- `edit_blueprint_variables` / `BlueprintVariables`
- `edit_object_properties` / `ObjectProperty`
- `edit_blueprint_signature` / `Signature`
- `edit_blueprint_class_settings` / `ClassSettings`

### 已接线但 canonical TS compiler 顶层未放行

- `edit_blueprint_components` / `Component`
- `edit_umg_widget` / `UMGWidget`
- `edit_data_table` / `DataTable`

### 未接入 TaskSpec 且通常不应接入

- `Core`
- `Debug`
- `SharedServices`
- `AssetBrowser`
- `AssetDiscovery`
- `EditorCommand`
- `Review`

### 需要收敛 legacy/direct write 入口

- `BlueprintStructure` 写入子集：`add_graph`、`remove_graph`、`add_event_dispatcher`、`delete_nodes`、legacy `add_variable/remove_variable`。其中 Signature 候选只限声明/签名级写入；GraphWrite 已覆盖的图内逻辑写入必须排除。

## 11. 审查状态

已完成：

- 派发 3 个只读子任务：schema/compiler、runtime/TaskPlan、外部模板/工作流暴露面。
- 已合并 schema/compiler 子任务结论。
- 已合并 runtime/TaskPlan 子任务结论。
- 已合并外部模板/工作流暴露面子任务结论，并补入 fixtures、C++ adapter/result 测试、真实 Debug 样本证据。
- 输出本 Gap 审查文档与 Debug 证据文档。

未执行：

- 未修改运行时代码。
- 未运行自动化测试或 E2E；本轮是只读审查与文档修正。
