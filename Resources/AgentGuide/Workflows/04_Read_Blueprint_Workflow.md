# 04 - 读取蓝图工作流

## 1. 目标

让 Agent 在理解蓝图时读取正确的信息量，避免一开始拉取完整 raw JSON 导致上下文膨胀和误判。

## 2. 默认流程

```text
1. Resolve asset path
2. Get asset info
3. List blueprint graphs / variables / dispatchers
4. Choose target graph
5. Export logic_md or logic_json
6. Summarize logic and risks
7. Only request raw_json if exact node/pin operation is required
```

## 3. 何时使用 logic_md

适合：

- 用户问“这个蓝图做了什么”。
- 需要给人类解释逻辑。
- 需要快速定位入口事件、分支、循环、变量读写。

输出应关注：

- 入口事件。
- 主执行链。
- 分支条件。
- 变量读写。
- 外部函数 / 组件 / 资产引用。
- 潜在孤立节点或未连接节点。

## 4. 何时使用 logic_json

适合：

- Agent 需要结构化推理。
- 需要比较改动前后逻辑。
- 需要为后续图表修改生成计划。

输出应关注：

- `events`
- `exec_flow`
- `data_flow`
- `variable_reads`
- `variable_writes`
- `function_calls`
- `diagnostics`

## 5. 何时使用 raw_json

适合：

- 需要精确定位节点 GUID。
- 需要检查 Pin 类型、默认值、连线端点。
- 需要导入 / 回放 / 批量迁移。
- 需要生成兼容 JsonToBlueprint 的输入。

不适合：

- 只做高层逻辑说明。
- 用户仅需要知道蓝图功能。
- Agent 还没有明确改动目标。

### 5.1 读取优先级

1. **`blueprint_get_logic` (LogicMd)** — 快速审阅蓝图逻辑，默认推荐。
2. **`blueprint_get_logic_json` (LogicJson)** — 结构化分析执行流和数据依赖。
3. **`blueprint_export_to_json` (RawJson)** — 仅用于回放、导入、连线调试场景。RawJson 不应作为默认读取方式。

### 5.2 MCP 默认输出

`blueprint_export_to_json` 默认返回 `raw_json_ref` (resource link)，而非内联完整 JSON 文本。通过 resource 读取时，RawJson 本体直接返回（不再包裹在 `{ json: ... }` 层中）。这可以避免上下文膨胀。

## 6. 读取后的输出模板

Agent 给用户回复时建议包含：

```text
已读取：<asset_path> / <graph_name>
入口：...
主流程：...
关键变量：...
外部调用：...
发现的问题：...
建议下一步：...
```

不要把完整 raw JSON 原文直接贴给用户，除非用户明确要求。
