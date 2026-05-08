# BlueprintHelper Bridge / MCP 返回协议优化规划（2026-04-28）

## 一、修改原因

### 1.1 当前 `export_to_json` 返回语义过窄

当前 Bridge Router 的导出命令主要返回：

```json
{
  "json": "{...}"
}
```

这对导入回放可用，但对 MCP / Agent 不理想：

- 返回字段名只叫 `json`，无法表达这是 raw JSON、logic JSON 还是摘要。
- JSON 被字符串包裹，Agent 需要二次解析。
- 没有 `format`、`schema`、`stats`、`diagnostics` 等元信息。
- 同一个工具难以同时服务“理解逻辑”和“准备写回”。

### 1.2 Agent 读操作和写操作需要不同默认值

Agent 读蓝图逻辑时，默认需要短摘要。
Agent 准备修改蓝图时，才需要完整 raw JSON。

如果所有读取都默认拉 raw JSON，会导致：

- 上下文占用高。
- Agent 更容易误把坐标、Pin 名、底层节点细节当成主要逻辑。
- 长图表返回过大，不利于 CLI Agent 多步推理。

---

## 二、修改目标

### 2.1 明确输出格式

所有导出类命令应显式说明输出格式：

```json
{
  "format": "raw_json | logic_json | logic_md"
}
```

### 2.2 raw JSON 保持兼容

现有 `export_to_json` 可继续返回字符串形式，避免破坏旧 MCP Server。

新增命令可以使用更合理的结构化响应。

### 2.3 逻辑视图不应伪装成可导入 JSON

`logic_json` 是理解协议，不是导入协议。响应中应明确：

```json
{
  "importable": false
}
```

---

## 三、推荐命令设计

### 3.1 新增 `export_logic`

推荐新增 Bridge 命令：

```text
export_logic
```

请求：

```json
{
  "request_id": "req_001",
  "command": "export_logic",
  "payload": {
    "target_blueprint": "/Game/BP/BP_Player.BP_Player",
    "target_graph": "EventGraph",
    "scope": "single_graph",
    "format": "logic_json",
    "detail": "normal",
    "include_data_dependencies": true,
    "include_orphans": true,
    "include_node_ids": false,
    "include_positions": false
  }
}
```

响应：

```json
{
  "request_id": "req_001",
  "success": true,
  "result": {
    "format": "logic_json",
    "schema": "BlueprintHelper.LogicGraph",
    "importable": false,
    "logic": {
      "version": "1.0",
      "graphs": []
    },
    "stats": {
      "nodes": 18,
      "exec_links": 12,
      "data_links": 7,
      "entry_points": 2,
      "orphans": 1
    }
  }
}
```

### 3.2 Markdown 响应

请求：

```json
{
  "command": "export_logic",
  "payload": {
    "target_blueprint": "/Game/BP/BP_Player.BP_Player",
    "target_graph": "EventGraph",
    "scope": "single_graph",
    "format": "logic_md",
    "detail": "brief"
  }
}
```

响应：

```json
{
  "success": true,
  "result": {
    "format": "logic_md",
    "schema": "BlueprintHelper.LogicMarkdown",
    "importable": false,
    "markdown": "# EventGraph\n\n## ReceiveBeginPlay\n...",
    "stats": {
      "nodes": 18,
      "entry_points": 2
    }
  }
}
```

---

## 四、可选方案：扩展 `export_to_json`

如不想新增命令，也可以给现有命令加：

```json
{
  "format": "raw_json | logic_json | logic_md"
}
```

但该方案有兼容风险：

- 旧调用方可能默认认为 `result.json` 一定存在。
- 命令名 `export_to_json` 与 `logic_md` 不匹配。
- raw JSON 与 logic JSON 容易被混用。

因此不作为首选。

---

## 五、MCP 工具层建议

MCP Server 可以暴露更面向 Agent 的工具名：

| MCP Tool | Bridge Command | 默认格式 | 用途 |
|----------|----------------|----------|------|
| `blueprint_get_logic` | `export_logic` | `logic_md` | 快速理解图表 |
| `blueprint_get_logic_json` | `export_logic` | `logic_json` | 结构化分析 |
| `blueprint_export_raw_json` | `export_to_json` | `raw_json` | 准备修改或回放 |
| `blueprint_import_json` | `import_json` | - | 写回蓝图 |

Agent 默认策略：

1. 先用 `blueprint_get_logic` 理解图表。
2. 需要精确修改时再用 `blueprint_export_raw_json`。
3. 写操作必须显式指定 `target_blueprint` 和 `target_graph`。

---

## 六、错误响应建议

`export_logic` 应新增以下错误场景：

| 错误码 | 场景 |
|--------|------|
| `InvalidRequest` | `format` 或 `scope` 不合法 |
| `AssetNotFound` | 目标蓝图不存在 |
| `GraphNotFound` | 目标图表不存在 |
| `ExecutionFailed` | raw JSON 导出失败 |
| `JsonParseFailed` | raw JSON 转逻辑视图失败 |
| `InternalError` | 未预期异常 |

错误响应示例：

```json
{
  "success": false,
  "error_code": "JsonParseFailed",
  "message": "LogicProcessor 无法解析 export_to_json 结果。"
}
```

---

## 七、修改计划

### Phase A：Bridge 命令新增

- 在 `FBlueprintHelperBridgeRouter::HandleRequest` 中注册 `export_logic`。
- 新增 `HandleExportLogic`。
- 复用 `FBlueprintHelperExportService` 生成 raw JSON。

### Phase B：请求参数解析

支持字段：

```text
target_blueprint
target_graph
scope
format
detail
include_data_dependencies
include_orphans
include_node_ids
include_positions
```

### Phase C：响应结构落地

- `logic_json` 返回对象。
- `logic_md` 返回 Markdown 字符串。
- 附带 `stats` 和 `importable=false`。

### Phase D：MCP Server 映射

MCP Server 工具层新增或调整：

```text
blueprint_get_logic
blueprint_get_logic_json
blueprint_export_raw_json
```

### Phase E：文档与规则更新

- 更新 JsonToBlueprintRules 中的导出说明。
- MCP 工具描述中明确 raw / logic 的边界。

---

## 八、验收标准

1. 旧 `export_to_json` 调用仍能返回 `result.json`。
2. 新 `export_logic format=logic_md` 能返回 Markdown 摘要。
3. 新 `export_logic format=logic_json` 能返回结构化逻辑对象。
4. logic 输出不被 `import_json` 当作合法输入。
5. Agent 读取 EventGraph 时上下文长度明显小于 raw JSON。

