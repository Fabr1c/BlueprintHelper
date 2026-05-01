# BlueprintHelper MCP 结构化返回设计文档（2026-04-30）

## 1. 背景

BlueprintHelper v0.3.0 已经具备三种蓝图读取视图：

| 视图 | 当前定位 | 是否继续保留 |
|---|---|---|
| `LogicJson` | 面向 Agent 的结构化逻辑视图，用于精确分析、编辑规划、后续 Patch/Ops 输入 | 保留 |
| `LogicMd` | 面向 Agent / 用户的文本逻辑视图，用于快速理解蓝图流程 | 保留 |
| `RawJson` | 面向调试、兼容和回放的完整蓝图导出视图 | 保留 |

当前主要问题不再是“缺少更精简的蓝图格式”，而是 MCP 工具返回结果仍可能将 `LogicJson` 或 `RawJson` 作为字符串放入 `content[].text`。这会造成 JSON 二次编码，产生大量 `\"`、`\n`、路径反斜杠和嵌套转义字符。

MCP 外层通信必须遵循 JSON-RPC 2.0，因此不能通过更换外层协议彻底消除 JSON。但可以通过 MCP 工具结果的 `structuredContent`、`resource_link` 和资源读取机制，避免把蓝图 JSON 再塞入文本字符串。

## 2. 问题定义

### 2.1 当前低效模式

典型低效返回：

```json
{
  "content": [
    {
      "type": "text",
      "text": "{\"format\":\"LogicJson\",\"nodes\":[{\"id\":\"n1\",\"kind\":\"Branch\"}]}"
    }
  ]
}
```

该模式存在以下问题：

1. `LogicJson` 被当作字符串嵌入，所有双引号被转义。
2. Agent 需要先从自然语言文本中识别 JSON 字符串，再执行二次解析。
3. `content.text` 同时承担“人类可读摘要”和“机器可读数据”两个职责。
4. RawJson 体积较大时，转义与重复字段会显著增加 token 消耗。
5. 工具结果缺少稳定的输出 schema，客户端和 Agent 不容易区分 `LogicMd`、`LogicJson`、`RawJson`。

### 2.2 需要解决的问题

本设计只解决 MCP 承载层问题：

- 如何减少 JSON 二次编码导致的冗余转义字符。
- 如何让 Agent 默认读取更短、更符合语义的蓝图信息。
- 如何在不破坏 v0.3.0 三种视图的前提下优化工具返回。
- 如何保持旧客户端兼容。

本设计不重新设计 `LogicJson`、`LogicMd`、`RawJson` 的内部字段。

## 3. 设计目标

### 3.1 功能目标

1. `LogicJson` 默认通过 `structuredContent` 返回。
2. `LogicMd` 默认通过 `content[].text` 返回。
3. `RawJson` 默认通过 `resource_link` 暴露，不默认内联到工具结果。
4. 所有导出类工具都返回格式元数据，例如 `format`、`schema`、`assetPath`、`graph`、`stats`。
5. 写操作输入使用结构化对象，不使用 JSON 字符串。
6. 保留显式 legacy 模式，允许旧客户端继续接收 `content.text = JSON.stringify(...)`。

### 3.2 非目标

1. 不替换 MCP 的 JSON-RPC 外层协议。
2. 不引入 YAML、MessagePack、CBOR 作为 Agent 默认上下文。
3. 不移除 `RawJson`。
4. 不把 `LogicMd` 伪装为可导入 JSON。
5. 不要求 Agent 继续提供节点坐标 `Pos`；布局仍由插件规则处理。
6. 不在本设计中修改 UE 蓝图编辑算法。

## 4. 核心设计原则

### 4.1 视图格式与 MCP 承载分离

`LogicJson / LogicMd / RawJson` 是蓝图信息视图；`content.text / structuredContent / resource_link` 是 MCP 承载方式。二者不应混淆。

### 4.2 默认最小上下文

Agent 默认读取 `LogicMd` 或摘要，只有在需要精确编辑、调试、回放时才请求 `LogicJson` 或 `RawJson`。

### 4.3 机器数据走结构化字段

所有需要机器解析的对象都应进入 `structuredContent` 或资源内容，不进入 `content.text`。

### 4.4 大对象按需读取

`RawJson` 默认通过 `resource_link` 暴露。工具调用只返回 URI、摘要和统计信息。

### 4.5 兼容模式显式启用

旧行为可以保留，但必须通过参数或环境变量显式开启，避免新 Agent 默认走低效路径。

## 5. 推荐承载矩阵

| 使用场景 | 蓝图视图 | MCP 承载方式 | 默认 inline | 说明 |
|---|---|---|---|---|
| 快速理解蓝图 | `LogicMd` | `content[].text` | 是 | 最适合 Agent 直接阅读 |
| 精确分析蓝图 | `LogicJson` | `structuredContent` | 是 | 不再作为字符串返回 |
| 查看完整原始导出 | `RawJson` | `resource_link` / `resources/read` | 否 | 仅调试和兼容使用 |
| 旧客户端兼容 | `LogicJson` / `RawJson` | `content[].text` stringified JSON | 可选 | 显式 legacy 模式 |
| 写入蓝图 | Patch/Ops JSON object | `tools/call.arguments` 对象 | 是 | 不传 JSON 字符串 |

## 6. MCP 返回 Profile

### 6.1 `summary_text` Profile

用于 `LogicMd` 或普通状态返回。

```json
{
  "content": [
    {
      "type": "text",
      "text": "# EventGraph\n\nBeginPlay -> Branch(IsValid) -> PrintString"
    }
  ],
  "structuredContent": {
    "format": "logic_md",
    "assetPath": "/Game/BP/BP_Player",
    "graph": "EventGraph",
    "stats": {
      "nodes": 12,
      "links": 14
    }
  }
}
```

### 6.2 `structured_json` Profile

用于 `LogicJson`。

```json
{
  "content": [
    {
      "type": "text",
      "text": "Exported LogicJson: /Game/BP/BP_Player.EventGraph, nodes=12, links=14."
    }
  ],
  "structuredContent": {
    "format": "logic_json",
    "schema": "BlueprintHelper.LogicJson.v1",
    "assetPath": "/Game/BP/BP_Player",
    "graph": "EventGraph",
    "logic": {
      "nodes": [],
      "links": []
    },
    "stats": {
      "nodes": 12,
      "links": 14
    }
  }
}
```

### 6.3 `resource_ref` Profile

用于 `RawJson` 或过大的 `LogicJson`。

```json
{
  "content": [
    {
      "type": "text",
      "text": "RawJson is available as a resource. Use it only for debugging or replay."
    },
    {
      "type": "resource_link",
      "uri": "blueprint://asset/Game/BP/BP_Player?view=raw-json&graph=EventGraph&rev=1024",
      "name": "BP_Player RawJson",
      "description": "Full raw blueprint export for EventGraph.",
      "mimeType": "application/json"
    }
  ],
  "structuredContent": {
    "format": "raw_json_ref",
    "assetPath": "/Game/BP/BP_Player",
    "graph": "EventGraph",
    "rawUri": "blueprint://asset/Game/BP/BP_Player?view=raw-json&graph=EventGraph&rev=1024",
    "stats": {
      "nodes": 12,
      "links": 14,
      "bytes": 28472
    }
  }
}
```

### 6.4 `legacy_text_json` Profile

仅用于旧客户端。

```json
{
  "content": [
    {
      "type": "text",
      "text": "{\"format\":\"logic_json\",\"logic\":{\"nodes\":[],\"links\":[]}}"
    }
  ],
  "structuredContent": {
    "format": "logic_json",
    "logic": {
      "nodes": [],
      "links": []
    }
  }
}
```

## 7. 工具级默认策略

| MCP Tool | 默认返回 | 可选返回 | 说明 |
|---|---|---|---|
| `blueprint_get_logic` | `LogicMd` in `content.text` | `LogicJson` resource | 快速理解默认工具 |
| `blueprint_get_logic_json` | `LogicJson` in `structuredContent` | `LogicMd` 摘要 | 精确分析默认工具 |
| `blueprint_export_raw_json` | `RawJson` resource_link | legacy inline | 调试、兼容、回放 |
| `blueprint_import_json` | 执行结果 summary + diagnostics | 不返回导入原文 | 避免回显大 JSON |
| `blueprint_apply_patch` / 后续工具 | Patch result structuredContent | Diff resource | 推荐新增方向 |

## 8. 资源 URI 设计

推荐保留现有 `blueprint://` scheme，并扩展资源视图。

```text
blueprint://asset/{assetPath}?view=logic-md&graph={graph}
blueprint://asset/{assetPath}?view=logic-json&graph={graph}
blueprint://asset/{assetPath}?view=raw-json&graph={graph}
blueprint://asset/{assetPath}?view=diff&rev={revision}
blueprint://context/active-graph
blueprint://rules/json-to-blueprint
```

URI 设计要求：

1. `assetPath` 必须是 UE 资产路径，不允许任意本地文件路径。
2. `view` 必须枚举校验。
3. `graph` 可选，但写操作仍应显式指定目标图表。
4. `rev` 可选，用于缓存和审阅。
5. 资源读取失败必须返回可被 Agent 修正的错误信息。

## 9. Agent 推荐行为

默认读取流程：

1. 先调用 `blueprint_get_logic` 获取 `LogicMd`。
2. 需要精确定位节点、Pin、变量依赖时再调用 `blueprint_get_logic_json`。
3. 仅在导入失败、兼容旧 JSON、调试 UE 原始结构时读取 `RawJson`。
4. 写操作优先提交 Patch/Ops，不提交整份 RawJson。
5. 写操作必须明确 `assetPath` 和 `graph`，不得只依赖当前焦点。

## 10. 兼容性策略

### 10.1 向后兼容

保留旧返回方式：

```text
responseMode = "legacy_text_json"
```

或者通过环境变量：

```text
BPH_MCP_LEGACY_TEXT_JSON=1
```

旧客户端继续收到 stringified JSON，新客户端默认使用结构化返回。

### 10.2 向前兼容

所有新返回对象增加：

```json
{
  "format": "logic_json",
  "schema": "BlueprintHelper.LogicJson.v1"
}
```

未来 `LogicJson.v2` 可以并存，不必破坏旧解析器。

## 11. 风险评估

| 风险 | 影响 | 缓解 |
|---|---|---|
| 部分 MCP Host 不展示 `structuredContent` | Agent 无法直接看到结构化对象 | `content.text` 保留摘要；提供 legacy 模式 |
| 旧调用方依赖 `content.text` 中完整 JSON | 行为变化 | 增加 `responseMode` 和环境变量回退 |
| RawJson resource 未实现缓存 | 资源 URI 读取失败 | 初期可按需实时导出；后续增加 snapshot cache |
| Bridge 返回仍是 JSON 字符串 | MCPServer 仍需二次解析 | 增加 `normalizeBridgeResult` |
| 资源 URI 被构造为本地路径访问 | 安全风险 | 只允许 UE asset path，禁止 file path |

## 12. 验收标准

1. `LogicJson` 默认不会出现在 `content.text` 的 stringified JSON 中。
2. `content.text` 中不再出现大段 `\"format\":\"logic_json\"` 或 `\"nodes\"`。
3. `blueprint_get_logic_json` 的主要数据位于 `structuredContent.logic`。
4. `blueprint_export_raw_json` 默认返回 `resource_link`。
5. 显式 legacy 模式仍可返回旧式 text JSON。
6. Agent 读取常规蓝图时默认 token 数小于 v0.3.0 RawJson inline 模式。
7. 导入工具参数 schema 不要求用户提供 JSON 字符串。
8. 写操作仍必须显式指定目标资产与图表。

## 13. 参考

- Model Context Protocol 2025-06-18 Basic Protocol：所有消息遵循 JSON-RPC 2.0。
- Model Context Protocol 2025-06-18 Tools：工具结果支持 `content`、`resource_link`、`structuredContent`、`outputSchema`。
