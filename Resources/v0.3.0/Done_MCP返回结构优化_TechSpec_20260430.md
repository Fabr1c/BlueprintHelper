# BlueprintHelper MCP 结构化返回技术文档（2026-04-30）

## 1. 范围

本文档描述 BlueprintHelper v0.3.0 之后针对 MCP 返回协议的实现方案。目标是在保留 `LogicJson / LogicMd / RawJson` 三种现有蓝图视图的前提下，减少 MCP 通信中的 JSON 二次转义，并提高 Agent 对工具输出的解析稳定性。

本文档主要涉及 MCPServer 层。UE Bridge 和 C++ 侧仅在必要时提供更干净的结构化返回，不要求立即重写蓝图导出逻辑。

## 2. 当前假设

1. MCPServer 通过 stdio 暴露 MCP。
2. MCPServer 默认连接 UE Bridge `127.0.0.1:54321`。
3. BlueprintHelper v0.3.0 已提供：
   - `LogicJson`
   - `LogicMd`
   - `RawJson`
4. 现有工具中可能仍存在以下模式：

```ts
return {
  content: [
    {
      type: "text",
      text: JSON.stringify(result)
    }
  ]
};
```

或 Bridge 返回：

```json
{
  "success": true,
  "result": "{\"format\":\"logic_json\",\"nodes\":[]}"
}
```

## 3. 目标架构

```text
Agent / MCP Client
        |
        | tools/call JSON-RPC
        v
MCPServer
        |
        | Bridge request JSON
        v
UE Bridge
        |
        | raw / logic result
        v
MCPServer normalize + envelope
        |
        | content.text summary
        | structuredContent object
        | resource_link for large payload
        v
Agent / MCP Client
```

核心变化：

1. MCPServer 不再将完整 `LogicJson` / `RawJson` 默认放入 `content.text`。
2. MCPServer 增加统一返回构造器。
3. MCPServer 增加 Bridge 结果归一化函数。
4. MCPServer 增加资源 URI 构造与资源读取处理。
5. 工具 `outputSchema` 明确声明结构化输出。

## 4. 类型定义

### 4.1 基础枚举

```ts
export type BlueprintViewFormat =
  | "logic_md"
  | "logic_json"
  | "raw_json";

export type McpResponseMode =
  | "summary_text"
  | "structured_json"
  | "resource_ref"
  | "legacy_text_json";

export type BlueprintResourceView =
  | "logic-md"
  | "logic-json"
  | "raw-json"
  | "diff"
  | "compile-result";
```

### 4.2 工具返回元信息

```ts
export interface BlueprintResultMeta {
  format: BlueprintViewFormat | "raw_json_ref";
  schema: string;
  assetPath: string;
  graph?: string;
  importable: boolean;
  stats?: {
    nodes?: number;
    links?: number;
    execLinks?: number;
    dataLinks?: number;
    variables?: number;
    bytes?: number;
  };
  diagnostics?: BlueprintDiagnostic[];
}

export interface BlueprintDiagnostic {
  severity: "info" | "warning" | "error";
  code: string;
  message: string;
  nodeId?: string;
  pin?: string;
}
```

### 4.3 LogicMd 返回

```ts
export interface BlueprintLogicMdResult extends BlueprintResultMeta {
  format: "logic_md";
  schema: "BlueprintHelper.LogicMd.v1";
  markdown: string;
}
```

### 4.4 LogicJson 返回

```ts
export interface BlueprintLogicJsonResult extends BlueprintResultMeta {
  format: "logic_json";
  schema: "BlueprintHelper.LogicJson.v1";
  logic: unknown;
}
```

### 4.5 RawJson 引用返回

```ts
export interface BlueprintRawJsonRefResult extends BlueprintResultMeta {
  format: "raw_json_ref";
  schema: "BlueprintHelper.RawJsonRef.v1";
  rawUri: string;
}
```

## 5. Bridge 结果归一化

### 5.1 设计目的

短期内 UE Bridge 可能仍返回 JSON 字符串。MCPServer 需要在不破坏旧 Bridge 的情况下，将可解析的 JSON 字符串转成对象。

### 5.2 实现

```ts
export function normalizeBridgeResult(value: unknown): unknown {
  if (typeof value !== "string") {
    return value;
  }

  const trimmed = value.trim();

  const looksLikeJson =
    (trimmed.startsWith("{") && trimmed.endsWith("}")) ||
    (trimmed.startsWith("[") && trimmed.endsWith("]"));

  if (!looksLikeJson) {
    return value;
  }

  try {
    return JSON.parse(trimmed);
  } catch {
    return value;
  }
}
```

### 5.3 处理 Bridge 包裹结构

如果 Bridge 返回：

```json
{
  "success": true,
  "result": {
    "json": "{...}"
  }
}
```

应额外识别 `result.json`：

```ts
export function normalizeBlueprintPayload(result: unknown): unknown {
  const normalized = normalizeBridgeResult(result);

  if (
    normalized &&
    typeof normalized === "object" &&
    "json" in normalized &&
    typeof (normalized as { json?: unknown }).json === "string"
  ) {
    const raw = normalizeBridgeResult((normalized as { json: string }).json);
    return {
      ...normalized,
      json: raw
    };
  }

  return normalized;
}
```

注意：该函数只解析 JSON 字符串，不执行任何代码，不解释 UE 路径。

## 6. MCP 工具结果构造器

### 6.1 通用接口

```ts
interface BuildToolResultOptions {
  mode: McpResponseMode;
  summary?: string;
  structured?: Record<string, unknown>;
  markdown?: string;
  resourceLinks?: Array<{
    uri: string;
    name: string;
    description?: string;
    mimeType: string;
  }>;
  legacyTextJson?: boolean;
}
```

### 6.2 构造函数

```ts
export function buildBlueprintToolResult(options: BuildToolResultOptions) {
  const content: Array<Record<string, unknown>> = [];

  if (options.mode === "legacy_text_json" && options.structured) {
    content.push({
      type: "text",
      text: JSON.stringify(options.structured)
    });
  } else if (options.markdown) {
    content.push({
      type: "text",
      text: options.markdown
    });
  } else {
    content.push({
      type: "text",
      text: options.summary ?? "BlueprintHelper operation completed."
    });
  }

  for (const link of options.resourceLinks ?? []) {
    content.push({
      type: "resource_link",
      uri: link.uri,
      name: link.name,
      description: link.description,
      mimeType: link.mimeType
    });
  }

  const result: Record<string, unknown> = {
    content,
    isError: false
  };

  if (options.structured) {
    result.structuredContent = options.structured;
  }

  return result;
}
```

## 7. 工具实现建议

### 7.1 `blueprint_get_logic`

默认返回 `LogicMd`。

输入 schema：

```ts
const BlueprintGetLogicInputSchema = z.object({
  target_blueprint: z.string(),
  target_graph: z.string().optional(),
  detail: z.enum(["brief", "normal", "full"]).default("normal"),
  response_mode: z
    .enum(["summary_text", "structured_json", "resource_ref", "legacy_text_json"])
    .optional()
});
```

返回策略：

```ts
const bridgeResult = await bridge.call("export_logic", {
  target_blueprint,
  target_graph,
  format: "logic_md",
  detail
});

const payload = normalizeBlueprintPayload(bridgeResult.result) as BlueprintLogicMdResult;

return buildBlueprintToolResult({
  mode: response_mode ?? "summary_text",
  markdown: payload.markdown,
  structured: {
    format: "logic_md",
    schema: "BlueprintHelper.LogicMd.v1",
    assetPath: payload.assetPath,
    graph: payload.graph,
    importable: false,
    stats: payload.stats,
    diagnostics: payload.diagnostics
  }
});
```

### 7.2 `blueprint_get_logic_json`

默认返回 `LogicJson` 到 `structuredContent`。

```ts
const bridgeResult = await bridge.call("export_logic", {
  target_blueprint,
  target_graph,
  format: "logic_json",
  detail
});

const payload = normalizeBlueprintPayload(bridgeResult.result) as BlueprintLogicJsonResult;

return buildBlueprintToolResult({
  mode: response_mode ?? "structured_json",
  summary: `Exported LogicJson: ${payload.assetPath}.${payload.graph ?? ""}, nodes=${payload.stats?.nodes ?? "unknown"}.`,
  structured: payload
});
```

禁止默认：

```ts
text: JSON.stringify(payload)
```

### 7.3 `blueprint_export_raw_json`

默认返回资源链接，不内联 RawJson。

```ts
const rawUri = makeBlueprintResourceUri({
  assetPath: target_blueprint,
  graph: target_graph,
  view: "raw-json"
});

return buildBlueprintToolResult({
  mode: response_mode ?? "resource_ref",
  summary: "RawJson is available as a resource. Use only for debugging, compatibility, or replay.",
  structured: {
    format: "raw_json_ref",
    schema: "BlueprintHelper.RawJsonRef.v1",
    assetPath: target_blueprint,
    graph: target_graph,
    importable: true,
    rawUri
  },
  resourceLinks: [
    {
      uri: rawUri,
      name: `${target_blueprint} RawJson`,
      description: "Full raw BlueprintHelper JSON export.",
      mimeType: "application/json"
    }
  ]
});
```

如用户显式传入：

```json
{
  "response_mode": "legacy_text_json"
}
```

才允许返回 stringified RawJson。

## 8. 资源 URI 实现

### 8.1 URI 构造

```ts
export function makeBlueprintResourceUri(input: {
  assetPath: string;
  graph?: string;
  view: BlueprintResourceView;
  rev?: string | number;
}) {
  const params = new URLSearchParams();
  params.set("view", input.view);

  if (input.graph) {
    params.set("graph", input.graph);
  }

  if (input.rev !== undefined) {
    params.set("rev", String(input.rev));
  }

  const normalizedAssetPath = input.assetPath.replace(/^\/+/, "");
  return `blueprint://asset/${encodeURIComponent(normalizedAssetPath)}?${params.toString()}`;
}
```

### 8.2 URI 校验

```ts
export function parseBlueprintResourceUri(uri: string) {
  const parsed = new URL(uri);

  if (parsed.protocol !== "blueprint:") {
    throw new Error("Invalid blueprint resource protocol.");
  }

  if (parsed.hostname !== "asset") {
    throw new Error("Invalid blueprint resource host.");
  }

  const view = parsed.searchParams.get("view");

  if (!["logic-md", "logic-json", "raw-json", "diff", "compile-result"].includes(view ?? "")) {
    throw new Error(`Unsupported blueprint resource view: ${view}`);
  }

  const assetPath = "/" + decodeURIComponent(parsed.pathname.replace(/^\/+/, ""));

  if (!assetPath.startsWith("/Game/") && !assetPath.startsWith("/Plugin/")) {
    throw new Error("Only Unreal asset paths are allowed.");
  }

  return {
    assetPath,
    graph: parsed.searchParams.get("graph") ?? undefined,
    view: view as BlueprintResourceView,
    rev: parsed.searchParams.get("rev") ?? undefined
  };
}
```

### 8.3 `resources/read` 处理

伪代码：

```ts
server.registerResource(
  "blueprint-asset-view",
  new ResourceTemplate("blueprint://asset/{assetPath}", {
    list: undefined
  }),
  async (uri) => {
    const request = parseBlueprintResourceUri(uri.href);

    if (request.view === "logic-md") {
      const result = await bridge.call("export_logic", {
        target_blueprint: request.assetPath,
        target_graph: request.graph,
        format: "logic_md"
      });

      const payload = normalizeBlueprintPayload(result.result) as BlueprintLogicMdResult;

      return {
        contents: [
          {
            uri: uri.href,
            mimeType: "text/markdown",
            text: payload.markdown
          }
        ]
      };
    }

    if (request.view === "logic-json") {
      const result = await bridge.call("export_logic", {
        target_blueprint: request.assetPath,
        target_graph: request.graph,
        format: "logic_json"
      });

      const payload = normalizeBlueprintPayload(result.result);

      return {
        contents: [
          {
            uri: uri.href,
            mimeType: "application/json",
            text: JSON.stringify(payload)
          }
        ]
      };
    }

    if (request.view === "raw-json") {
      const result = await bridge.call("export_to_json", {
        target_blueprint: request.assetPath,
        target_graph: request.graph
      });

      const payload = normalizeBlueprintPayload(result.result);

      return {
        contents: [
          {
            uri: uri.href,
            mimeType: "application/json",
            text: JSON.stringify(payload)
          }
        ]
      };
    }

    throw new Error(`Unsupported resource view: ${request.view}`);
  }
);
```

说明：资源内容本身仍可能是 JSON 文本，但它是按需读取，不再默认进入每次工具调用上下文。

## 9. Output Schema

### 9.1 `blueprint_get_logic_json`

```ts
const BlueprintLogicJsonOutputSchema = {
  type: "object",
  properties: {
    format: { const: "logic_json" },
    schema: { const: "BlueprintHelper.LogicJson.v1" },
    assetPath: { type: "string" },
    graph: { type: "string" },
    importable: { type: "boolean" },
    logic: { type: "object" },
    stats: {
      type: "object",
      additionalProperties: true
    },
    diagnostics: {
      type: "array",
      items: {
        type: "object",
        additionalProperties: true
      }
    }
  },
  required: ["format", "schema", "assetPath", "importable", "logic"]
};
```

### 9.2 `blueprint_export_raw_json`

```ts
const BlueprintRawJsonRefOutputSchema = {
  type: "object",
  properties: {
    format: { const: "raw_json_ref" },
    schema: { const: "BlueprintHelper.RawJsonRef.v1" },
    assetPath: { type: "string" },
    graph: { type: "string" },
    importable: { type: "boolean" },
    rawUri: { type: "string" },
    stats: {
      type: "object",
      additionalProperties: true
    }
  },
  required: ["format", "schema", "assetPath", "importable", "rawUri"]
};
```

## 10. 错误处理

### 10.1 协议错误

用于 MCP 工具名不存在、参数 schema 不合法等情况。

### 10.2 工具执行错误

用于 UE Bridge 不可用、资产不存在、图表不存在、导出失败等业务错误。

```ts
function toBlueprintToolError(error: unknown) {
  return {
    content: [
      {
        type: "text",
        text: error instanceof Error ? error.message : String(error)
      }
    ],
    isError: true,
    structuredContent: {
      ok: false,
      errorCode: inferBlueprintErrorCode(error),
      message: error instanceof Error ? error.message : String(error)
    }
  };
}
```

### 10.3 推荐错误码

| 错误码 | 说明 |
|---|---|
| `BridgeUnavailable` | UE Bridge 未启动或端口不可达 |
| `AssetNotFound` | 目标蓝图资产不存在 |
| `GraphNotFound` | 目标图表不存在 |
| `UnsupportedFormat` | 请求的视图格式不支持 |
| `UnsupportedResponseMode` | 请求的 MCP 返回模式不支持 |
| `JsonParseFailed` | Bridge 返回的 JSON 字符串无法解析 |
| `ResourceReadFailed` | 资源 URI 读取失败 |
| `InternalError` | 未分类异常 |

## 11. 安全约束

1. `blueprint://asset/...` 只能映射到 UE 资产路径，不能映射本地文件系统。
2. 不允许通过 URI 参数传入 `C:\`、`/Users/`、`../` 等文件路径。
3. 写工具必须要求显式 `target_blueprint` 和 `target_graph`。
4. RawJson resource 不应包含本地绝对路径，除非该路径已经是 UE 必需字段且确认可暴露。
5. 所有 Bridge 请求都应设置超时。
6. 资源读取不能自动执行写操作。

## 12. 测试计划

### 12.1 单元测试

| 测试 | 输入 | 预期 |
|---|---|---|
| normalize object | `{ format: "logic_json" }` | 原样返回对象 |
| normalize JSON string | `'{"format":"logic_json"}'` | 返回对象 |
| normalize non-JSON string | `'hello'` | 原样返回字符串 |
| normalize invalid JSON | `'{bad'` | 原样返回字符串 |
| make resource URI | asset + graph + view | 返回 `blueprint://asset/...` |
| parse invalid resource URI | `file:///...` | 抛错 |
| build LogicJson result | structured payload | `content.text` 仅摘要，`structuredContent` 有对象 |
| build legacy result | legacy mode | `content.text` 为 stringified JSON |

### 12.2 集成测试

1. 启动 UE Editor 和 Bridge。
2. 调用 `blueprint_get_logic`：
   - `content[0].type === "text"`
   - `content[0].text` 为 Markdown。
   - `structuredContent.format === "logic_md"`。
3. 调用 `blueprint_get_logic_json`：
   - `content[0].text` 不含大段 `\"nodes\"`。
   - `structuredContent.format === "logic_json"`。
   - `structuredContent.logic` 存在。
4. 调用 `blueprint_export_raw_json`：
   - 返回 `resource_link`。
   - `structuredContent.rawUri` 存在。
5. 调用 `resources/read(rawUri)`：
   - 返回 `mimeType: "application/json"`。
   - 内容可被 `JSON.parse`。
6. 显式 `response_mode: "legacy_text_json"`：
   - 旧 text JSON 行为可用。

### 12.3 token 观察测试

同一个 10KB 蓝图分别读取：

1. RawJson inline text。
2. LogicJson inline text stringified。
3. LogicJson structuredContent + text summary。
4. LogicMd text。
5. RawJson resource_link。

验收方向：

- `LogicMd text` 最短。
- `LogicJson structuredContent` 不再出现明显双重转义。
- `RawJson resource_link` 工具结果 token 明显低于 RawJson inline。

## 13. 迁移步骤

### Phase 1：MCPServer 内部工具函数

- 新增 `normalizeBridgeResult`。
- 新增 `normalizeBlueprintPayload`。
- 新增 `buildBlueprintToolResult`。
- 新增 URI 构造与解析函数。
- 新增 output schema 常量。

### Phase 2：只改读取工具返回 envelope

- `blueprint_get_logic` 默认 `LogicMd` in `content.text`。
- `blueprint_get_logic_json` 默认 `LogicJson` in `structuredContent`。
- `blueprint_export_raw_json` 默认 `resource_link`。

### Phase 3：增加资源读取

- 注册 `blueprint://asset/...` resource handler。
- 支持 `view=logic-md`、`view=logic-json`、`view=raw-json`。
- RawJson 初期可以实时导出，后续再加入缓存。

### Phase 4：兼容模式

- 增加 `response_mode` 参数。
- 增加环境变量 `BPH_MCP_LEGACY_TEXT_JSON`。
- 工具描述中标明 legacy 模式只用于旧客户端。

### Phase 5：写工具输入优化

- 禁止新增工具要求 `logic_json` string。
- 新增或调整工具使用 object 参数。
- 后续支持 Patch/Ops 输入。

## 14. 回滚方案

如果某些 MCP Host 无法正确处理 `structuredContent`：

1. 设置环境变量：

```text
BPH_MCP_LEGACY_TEXT_JSON=1
```

2. 或在工具参数中指定：

```json
{
  "response_mode": "legacy_text_json"
}
```

3. 保持 `structuredContent` 同时返回，方便支持新协议的客户端继续使用。
