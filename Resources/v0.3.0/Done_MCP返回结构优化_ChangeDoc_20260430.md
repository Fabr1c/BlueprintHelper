# BlueprintHelper MCP 结构化返回变更文档（2026-04-30）

## 1. 变更名称

`BlueprintHelper MCP Structured Return Envelope Optimization`

中文名称：BlueprintHelper MCP 结构化返回封装优化。

## 2. 变更摘要

BlueprintHelper v0.3.0 已经具备 `LogicJson`、`LogicMd`、`RawJson` 三种蓝图视图。本次变更不新增第四种蓝图格式，而是调整 MCP 工具结果的封装方式：

```text
LogicMd   -> content[].text
LogicJson -> structuredContent
RawJson   -> resource_link / resources/read
```

核心目标是减少 JSON 二次编码造成的冗余转义字符，并让 Agent 默认读取更短、更稳定、更符合语义的蓝图上下文。

## 3. 修改原因

### 3.1 当前问题

当 `LogicJson` 或 `RawJson` 被 `JSON.stringify` 后放入 MCP `content[].text`，结果会变成：

```json
{
  "type": "text",
  "text": "{\"nodes\":[{\"id\":\"n1\"}]}"
}
```

该模式导致：

1. 大量 `\"` 转义字符。
2. Agent 需要二次解析 JSON 字符串。
3. token 成本上升。
4. Debug 输出和 Agent 上下文难以阅读。
5. RawJson 这类大对象默认进入上下文，干扰 Agent 推理。

### 3.2 变更依据

MCP 工具结果支持结构化内容和资源链接。BlueprintHelper 应利用：

- `content`：承载人类可读摘要或 Markdown。
- `structuredContent`：承载机器可读 JSON 对象。
- `resource_link`：承载可按需读取的大型资源引用。
- `outputSchema`：声明结构化结果形状。

## 4. 变更范围

### 4.1 涉及模块

优先涉及 MCPServer：

```text
MCPServer
- 工具注册
- 工具结果构造
- Bridge 返回结果归一化
- outputSchema 声明
- resource URI 构造
- resources/read 处理
```

可能涉及 Bridge Router：

```text
Source/BlueprintHelper
- export_logic 返回对象化结果
- export_to_json 兼容 raw json
- 错误码与诊断信息补充
```

### 4.2 不涉及模块

本次变更不要求修改：

```text
- LogicJson 内部字段语义
- LogicMd 生成规则
- RawJson 兼容格式
- 蓝图节点创建算法
- 自动布局算法
- UMG / DataAsset / DataTable 工具语义
```

## 5. 变更前后对比

### 5.1 变更前

```json
{
  "content": [
    {
      "type": "text",
      "text": "{\"format\":\"logic_json\",\"assetPath\":\"/Game/BP/BP_Player\",\"logic\":{\"nodes\":[]}}"
    }
  ]
}
```

### 5.2 变更后

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
      "nodes": []
    },
    "stats": {
      "nodes": 12,
      "links": 14
    }
  }
}
```

### 5.3 RawJson 变更前

```json
{
  "content": [
    {
      "type": "text",
      "text": "{\"format\":\"raw_json\",\"json\":{\"Nodes\":[...]}}"
    }
  ]
}
```

### 5.4 RawJson 变更后

```json
{
  "content": [
    {
      "type": "text",
      "text": "RawJson is available as a resource. Use only for debugging, compatibility, or replay."
    },
    {
      "type": "resource_link",
      "uri": "blueprint://asset/Game/BP/BP_Player?view=raw-json&graph=EventGraph",
      "name": "BP_Player RawJson",
      "mimeType": "application/json"
    }
  ],
  "structuredContent": {
    "format": "raw_json_ref",
    "schema": "BlueprintHelper.RawJsonRef.v1",
    "assetPath": "/Game/BP/BP_Player",
    "graph": "EventGraph",
    "rawUri": "blueprint://asset/Game/BP/BP_Player?view=raw-json&graph=EventGraph"
  }
}
```

## 6. 新增或调整的参数

### 6.1 `response_mode`

建议为读取类工具增加可选参数：

```ts
response_mode?: "summary_text" | "structured_json" | "resource_ref" | "legacy_text_json";
```

含义：

| 值 | 含义 |
|---|---|
| `summary_text` | `content.text` 返回 Markdown 或摘要 |
| `structured_json` | 主要数据进入 `structuredContent` |
| `resource_ref` | 主要数据通过 `resource_link` 引用 |
| `legacy_text_json` | 兼容旧客户端，将 JSON stringify 后放入 text |

### 6.2 环境变量

```text
BPH_MCP_LEGACY_TEXT_JSON=1
```

用于全局启用旧式返回，便于回滚。

## 7. 工具行为变更

| 工具 | 变更前 | 变更后 |
|---|---|---|
| `blueprint_get_logic` | 可能返回 JSON 字符串或 Markdown | 默认返回 `LogicMd` 到 `content.text` |
| `blueprint_get_logic_json` | 可能返回 stringified LogicJson | 默认返回 `LogicJson` 到 `structuredContent` |
| `blueprint_export_raw_json` | 可能 inline 返回完整 RawJson | 默认返回 `resource_link` |
| `blueprint_import_json` | 可能接受 JSON 字符串 | 推荐接受 object，避免 JSON 字符串 |
| 后续 Patch/Ops 工具 | 未定义 | 推荐使用结构化 object 参数 |

## 8. 兼容性影响

### 8.1 对新 Agent

正向影响：

1. 上下文中转义字符显著减少。
2. 读取蓝图逻辑更稳定。
3. 可以通过 `structuredContent` 直接访问对象。
4. RawJson 不再默认污染上下文。
5. 更容易实现审阅、Diff 和 Patch。

### 8.2 对旧 Agent / 旧 MCP Host

潜在影响：

1. 如果旧 Host 只展示 `content.text`，可能看不到完整 `structuredContent`。
2. 如果旧流程依赖 `content.text` 中完整 JSON，需要使用 legacy 模式。

缓解：

```json
{
  "response_mode": "legacy_text_json"
}
```

或：

```text
BPH_MCP_LEGACY_TEXT_JSON=1
```

### 8.3 对 UE Bridge

理想情况下 Bridge 返回对象，不返回 JSON 字符串。但短期可以由 MCPServer 解析兼容。

## 9. 实施计划

### Phase A：文档与工具描述更新

- 更新 MCP 工具描述。
- 明确 `LogicMd / LogicJson / RawJson` 的承载方式。
- 标注 `RawJson` 默认不 inline。

### Phase B：MCPServer 返回构造器

- 新增 `buildBlueprintToolResult`。
- 新增 `normalizeBridgeResult`。
- 新增 `normalizeBlueprintPayload`。
- 给读取类工具接入统一返回构造器。

### Phase C：Output Schema

- 给 `blueprint_get_logic_json` 添加 output schema。
- 给 `blueprint_export_raw_json` 添加 RawJsonRef output schema。
- 确保工具结果 `structuredContent` 符合 schema。

### Phase D：Resource Link

- 新增 `blueprint://asset/...` resource URI。
- 实现 `resources/read`：
  - `view=logic-md`
  - `view=logic-json`
  - `view=raw-json`
- RawJson 初期可实时导出，后续再缓存。

### Phase E：兼容与回滚

- 增加 `response_mode=legacy_text_json`。
- 增加 `BPH_MCP_LEGACY_TEXT_JSON=1`。
- 保留旧工具名称和旧 Bridge 命令。

### Phase F：测试与验收

- 增加单元测试。
- 增加 MCP 工具返回快照测试。
- 增加 token / 字符量对比测试。

## 10. 影响文件建议

实际文件名以当前仓库为准，预期可能涉及：

```text
MCPServer/src/server.ts
MCPServer/src/config.ts
MCPServer/src/bridgeClient.ts
MCPServer/src/tools/blueprint.ts
MCPServer/src/resources/blueprintResources.ts
MCPServer/src/schemas/blueprintOutputs.ts
MCPServer/src/utils/normalizeBridgeResult.ts
```

如果当前 MCPServer 是单文件结构，则先在现有文件内落地工具函数，后续再拆分模块。

可选涉及：

```text
Source/BlueprintHelper/Private/BlueprintHelperBridgeRouter.cpp
Source/BlueprintHelper/Private/BlueprintHelperExportService.cpp
Source/BlueprintHelper/Public/BlueprintHelperExportTypes.h
```

## 11. 验收清单

### 11.1 MCP 返回

- [ ] `blueprint_get_logic` 默认 `content.text` 是 Markdown。
- [ ] `blueprint_get_logic_json` 默认 `content.text` 只是摘要。
- [ ] `blueprint_get_logic_json` 默认有 `structuredContent.logic`。
- [ ] `blueprint_export_raw_json` 默认返回 `resource_link`。
- [ ] `blueprint_export_raw_json` 默认不 inline 完整 RawJson。
- [ ] legacy 模式可恢复旧行为。

### 11.2 转义字符

- [ ] 默认 `content.text` 中不出现大段 `\"nodes\"`。
- [ ] 默认 `content.text` 中不出现完整 RawJson。
- [ ] 同一蓝图下，默认读取输出字符数小于 RawJson inline。

### 11.3 资源读取

- [ ] `blueprint://asset/...view=logic-md` 可读取 Markdown。
- [ ] `blueprint://asset/...view=logic-json` 可读取 JSON。
- [ ] `blueprint://asset/...view=raw-json` 可读取 RawJson。
- [ ] 非法 URI 被拒绝。
- [ ] 不允许通过资源 URI 读取本地任意文件。

### 11.4 导入与写操作

- [ ] 新增写工具不要求 JSON 字符串参数。
- [ ] 写工具要求显式资产路径。
- [ ] 写工具要求显式图表名。
- [ ] 写操作结果返回 summary + diagnostics，不回显完整输入。

## 12. 风险与回滚

| 风险 | 等级 | 回滚方式 |
|---|---|---|
| MCP Host 不支持 `structuredContent` 展示 | 中 | 启用 `legacy_text_json` |
| RawJson resource 读取失败 | 中 | 临时允许 `response_mode=legacy_text_json` |
| 旧 Agent 依赖 text JSON | 中 | 环境变量回退 |
| Bridge 返回格式不一致 | 低 | MCPServer normalize 兼容 |
| 资源 URI 安全边界不清晰 | 高 | 只允许 UE 资产路径，不允许文件路径 |

## 13. 版本建议

建议版本标记：

```text
BlueprintHelper v0.3.1
```

或如果同时引入 Patch/Ops 写入协议：

```text
BlueprintHelper v0.4.0
```

判断标准：

- 仅 MCP 返回 envelope 优化：`v0.3.1`
- 同时改变写工具输入协议并新增 Patch/Ops：`v0.4.0`

## 14. 最终结论

本次变更应被视为 MCP 承载层优化，而不是蓝图逻辑格式重构。

推荐最终规则：

```text
LogicMd   默认给 Agent 读
LogicJson 默认给 structuredContent
RawJson   默认给 resource_link
写操作    默认使用 object / Patch / Ops
旧行为    仅 legacy 模式启用
```
