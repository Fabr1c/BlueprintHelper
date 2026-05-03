# BridgePayload Object-First 迁移说明

> **日期:** 2026-05-01
> **关联计划:** BlueprintHelper_BridgePayload_ObjectFirst_ParallelExecutionPlan_20260501.md

## 1. 变更概述

BlueprintHelper Bridge 协议从 string-first 升级为 object-first。RawJson 数据现在以结构化 `FJsonObject` 形式在 Bridge 响应中传递，不再默认序列化为字符串。

## 2. 旧消费者迁移

### 2.1 直接读取 `result.json` 字符串

**旧代码:**
```js
const rawJson = JSON.parse(result.json);
const nodes = rawJson.nodes;
```

**新代码:**
```js
// payload 是主要字段，json 是兼容性别名
const rawJson = result.payload ?? result.json;
const nodes = rawJson.nodes;
```

### 2.2 兼容模式

如果暂时无法更新消费者代码，可通过以下方式获取旧格式：

- **Bridge 请求:** 添加 `"include_json_text": true`
- **MCP 请求:** 使用 `"response_mode": "legacy_text_json"`

## 3. MCP 行为变更

### 3.1 RawJson 导出 (blueprint_export_to_json)

| 属性 | 旧行为 | 新行为 |
|------|--------|--------|
| 默认输出 | inline RawJson text | resource_link (raw_json_ref) |
| RawJson body | 可能包裹在 `{ json: ... }` 中 | 通过 resource 读取时直接返回 |
| 文本 content | 完整 JSON 字符串 | 摘要文本 |
| structuredContent | 无或有限 | `{ format: raw_json_ref, rawUri, assetPath, ... }` |

### 3.2 RawJson Resource (blueprint://asset/...?view=raw-json)

| 属性 | 旧行为 | 新行为 |
|------|--------|--------|
| 返回内容 | `{ json: { nodes: [], links: [] } }` | `{ nodes: [], links: [] }` |
| 包裹层 | 一层 Bridge 包裹 | 直接返回 RawJson 本体 |

### 3.3 导入 (blueprint_import_json_to_graph)

| 属性 | 旧行为 | 新行为 |
|------|--------|--------|
| json 参数类型 | 仅 `string` | `string \| object` |
| LogicJson 拒绝 | C++ 层 | MCP 层 + C++ 层双重守卫 |
| importable=false 拒绝 | 无 | MCP 层 + C++ 层 |

## 4. 字段参考

### export_to_json 响应字段

| 字段 | 类型 | 说明 |
|------|------|------|
| `payload` | object | 结构化 RawJson (主要字段) |
| `json` | object | 兼容性别名，等同 payload |
| `json_text` | string | 仅在 `include_json_text: true` 时出现 |
| `format` | string | `"raw_json"` |
| `schema` | string | `"BlueprintHelper.JsonToBlueprint.v2.2"` |
| `importable` | boolean | `true` |
| `assetPath` | string | 蓝图资产路径 |
| `graph` | string | 图表名称 |
| `effective_scope` | string | `"graph"` / `"blueprint"` / `"selection"` |
| `stats` | object | `{ nodes: N, links: M }` |
| `diagnostics` | array | 诊断消息 |

### export_logic 响应字段

| 字段 | 类型 | 说明 |
|------|------|------|
| `importable` | boolean | `false` (只读视图) |

## 5. 破坏性变更

1. **`result.json` 类型变更**: 从 `string` 变为 `object`。消费者如果直接对 `result.json` 调用 `JSON.parse()` 将失败。使用 `result.payload ?? result.json` 替代。
2. **RawJson resource 返回格式变更**: 不再包裹在 `{ json: ... }` 中。
3. **MCP 默认输出模式变更**: `blueprint_export_to_json` 默认返回 resource_link 而非 inline text。

## 6. 兼容性保留

- `include_json_text: true` 请求参数 —— 返回旧格式 `json_text` 字符串
- `response_mode: "legacy_text_json"` MCP 参数 —— 返回旧 MCP 输出格式
- `payload.json` 字符串输入 —— import 仍然接受字符串 RawJson
- 所有旧版 C++ API (`ConvertGraphToJson`, `ExportBlueprintToJson`, `ProcessRawJson`) 仍作为 wrapper 可用

## 7. MCP 配置路径要求

**`UE_ENGINE_DIR` 和 `UE_PROJECT_FILE` 必须使用绝对路径。** 不支持相对路径。

MCP Server v0.4.0+ 在启动时会自动展开以下模板变量：

| 模板变量 | 展开为 |
|----------|--------|
| `${workspaceFolder}` | `process.cwd()`（MCP Server 工作目录） |
| `${workspaceRoot}` | 同 `${workspaceFolder}` |
| `${userHome}` | `USERPROFILE`（Windows）或 `HOME`（Unix） |

示例配置（`.vscode/mcp.json` 或 Claude Code `settings.json`）：

```json
{
  "servers": {
    "blueprint-helper": {
      "env": {
        "UE_ENGINE_DIR": "F:/UE_5.6",
        "UE_PROJECT_FILE": "G:/UnrealPractise/MrStone/MrStone.uproject"
      }
    }
  }
}
```

或使用模板变量（会在启动时自动展开）：

```json
{
  "env": {
    "UE_ENGINE_DIR": "F:/UE_5.6",
    "UE_PROJECT_FILE": "${workspaceFolder}/MrStone.uproject"
  }
}
```

**如果出现 `Failed to open descriptor file .../${workspaceFolder}/...` 错误**，说明模板变量未展开。确保 MCP Server 版本 >= v0.4.0 或改用绝对路径。
