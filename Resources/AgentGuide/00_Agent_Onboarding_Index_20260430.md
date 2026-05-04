# BlueprintHelper Agent 使用引导索引（2026-04-30）

## 1. 文档目的

本目录是给 AI Agent / IDE Agent / CLI Agent 阅读的入口文档。目标是让新安装插件后的 Agent 不再通过猜测调用 MCP 工具，而是先理解 BlueprintHelper 的边界、连接方式、任务路由、功能索引和安全写入流程。

该文档不是面向最终用户的宣传 README，也不是 C++ API 参考。它面向执行任务的 Agent：当用户说“用这个插件改蓝图 / 写功能 / 修 UI / 改 DataTable”时，Agent 应先读这里。

## 2. 先给 Agent 的一句话结论

BlueprintHelper MCP 的默认主线正在收敛为 TaskSpec-first：Agent -> MCP Task Tools -> Python/MCP Task Compiler -> UE Task Runtime -> Existing Capability Clusters。Agent 面向少量任务级工具提交结构化 TaskSpec，Python / MCP 编译 TaskPlan，UE Task Runtime 执行现有能力簇并负责事务、Review、rollback、compile/save。底层工具继续存在，但默认作为内部 capability、debug / expert 工具和测试入口。

## 3. 推荐阅读顺序

1. [BlueprintHelper_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md](../Plan/BlueprintHelper_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md)
   先理解 Agent -> MCP Task Tools -> Python/MCP Task Compiler -> UE Task Runtime -> Existing Capability Clusters 的新主线。
2. [01_Preflight_And_Boundary.md](Reference/01_Preflight_And_Boundary.md)
   先判断是否应该使用 BlueprintHelper MCP，以及调用前需要确认什么。
3. [02_Capability_Index.md](Reference/02_Capability_Index.md)
   按任务级工具和底层能力簇理解 MCP 能做什么。
4. [03_Tool_Selection_Rules.md](Reference/03_Tool_Selection_Rules.md)
   将用户意图映射到 TaskSpec、TaskContextPack 和必要的 debug / expert 工具。
5. [04_Read_Blueprint_Workflow.md](Workflows/04_Read_Blueprint_Workflow.md)
   读取蓝图、理解逻辑、选择 raw_json / logic_json / logic_md 的流程。
6. [05_Edit_Blueprint_Workflow.md](Workflows/05_Edit_Blueprint_Workflow.md)
   通过 TaskSpec / preview / execute 修改蓝图图表、变量、函数、节点和连线的流程。
7. [06_UMG_Data_Workflows.md](Workflows/06_UMG_Data_Workflows.md)
   UMG、DataAsset、UObject、DataTable 的读写流程。
8. [07_Safety_Validation_And_Recovery.md](Workflows/07_Safety_Validation_And_Recovery.md)
   写操作后的编译、验证、保存、撤销和恢复策略。

## 4. Agent 默认行为规则

### 4.1 读操作默认规则

- 复杂任务先调用 `blueprinthelper_read_task_context` 获取 TaskContextPack，不要靠反复提交错误参数猜上下文。
- 只读理解仍可使用资产搜索、资产信息、logic_md、logic_json 等能力。
- 读蓝图逻辑时优先请求 `logic_md` 或 `logic_json`；只有准备导入、回放、精确连线、定位节点 GUID 时才请求 `raw_json`。
- 用户只问“这个蓝图做了什么”时，不应默认导出完整 raw JSON。

### 4.2 写操作默认规则

- 普通 Agent 默认生成 TaskSpec，再调用 `blueprinthelper_preview_task`。
- preview 通过后再调用 `blueprinthelper_execute_task`。
- TaskSpec 必须显式描述目标资产、允许修改范围、资源引用、验证策略和失败策略。
- Agent 不提交 TaskPlan；TaskPlan 由 Python / MCP Task Compiler 从 TaskSpec 生成，UE Task Runtime 执行后负责 compile / diagnostics / save 策略，Agent 最终报告任务级摘要。
- 失败时优先保留现场，报告错误和已执行步骤；不要连续盲目重试破坏资产。

### 4.3 禁止性规则

- 不要把 BlueprintHelper MCP 当成源码搜索工具。
- 不要用 MCP 修改 C++ / TypeScript / Python / JSON 配置文件。
- 不要让普通 Agent 直接把复杂任务拆成几十个底层原子 MCP 调用。
- 不要在没有目标资产路径的情况下执行破坏性操作。
- 不要依赖当前焦点编辑器上下文来做删除、改名、连线等操作。
- 不要把 `logic_json` 当成可导入 / 可回放格式；导入仍应使用兼容的 raw JSON / JsonToBlueprint 协议。

## 5. 常见用户请求到文档的映射

| 用户请求 | 先读文档 | 默认路径 |
|---|---|---|
| “看看这个蓝图逻辑” | 04_Read_Blueprint_Workflow | 搜索资产 -> 导出 logic_md / logic_json -> 总结 |
| “帮我给 BP_Player 加变量/函数/节点” | 05_Edit_Blueprint_Workflow | read_task_context -> TaskSpec -> preview_task -> execute_task |
| “帮我改 Widget 布局” | 06_UMG_Data_Workflows | TaskSpec-first；必要时读取 WidgetTree 作为上下文 |
| “改 DataTable 一行” | 06_UMG_Data_Workflows | TaskSpec-first；必要时读取表结构作为上下文 |
| “写 C++ 实现” | 01_Preflight_And_Boundary | 使用普通代码工具，不使用 MCP 写源码 |
| “打开编辑器/编译项目” | 03_Tool_Selection_Rules | 确认 env -> open_editor / build_project |
| “插件怎么用” | 本索引 | 解释边界 + 给出任务式入口 |

## 6. 与现有计划文档的关系

`Resources/Plan/` 下的文档主要描述架构、协议、字段映射、UE 能力簇实现计划和测试计划。当前主线以 `BlueprintHelper_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md` 为准。AgentGuide 目录负责告诉 Agent 如何开始使用插件、如何在 TaskSpec-first 主线下选择任务级工具、以及何时进入底层能力簇的 debug / expert 路径。

两者关系：

```text
Resources/AgentGuide/     给 Agent 的使用入口与任务流程
Resources/Plan/           给开发者的设计、优化、测试和实施计划
```
