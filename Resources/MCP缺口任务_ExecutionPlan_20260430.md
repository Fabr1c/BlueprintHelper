# BlueprintHelper MCP Missing Capabilities Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 补齐 MCP 结构化承载、RawJson resource、Change Review、Patch/Ops、P2 job/status/cancel/queue 这五类缺口，让 Agent 读取更稳定、写入可审阅、长任务可追踪。

**Architecture:** 先在 MCPServer 层建立统一返回 envelope 与资源读取入口，再把写操作审阅能力接入 UE Bridge Router，最后在审阅与稳定节点引用基础上实现 Patch/Ops 和异步任务队列。读路径优先落地，写路径遵循 read -> plan -> write -> validate -> save 的项目规则。

**Tech Stack:** TypeScript MCPServer、`@modelcontextprotocol/sdk`、Zod、Node test runner、Unreal Engine 5.6 C++ Editor Plugin、UE Automation Tests。

---

## 1. 当前确认状态

来源：

- `Resources/当前总体实施计划顺序_Assessment_20260430.md`
- `Resources/Plan/MCP返回结构优化_Design_20260430.md`
- `Resources/Plan/MCP返回结构优化_TechSpec_20260430.md`
- `Resources/Plan/ChangeReviewExecution/*`
- 当前源码检索结果

| 缺口 | 当前状态 | 主要证据 | 本计划处理方式 |
|---|---|---|---|
| `MCP structuredContent.logic` | 未完成 | `MCPServer/src/tools.ts` 中 `blueprint_get_logic_json` 仍返回 `toToolResult(resp)`，`toToolResult` 将响应 `JSON.stringify` 到 `content[0].text` | Task 1 |
| `RawJson resource_link / resources/read` | 未完成 | `MCPServer/src/resources.ts` 只注册规则文档和 active context，没有 `blueprint://asset/...` 资源读取 | Task 2 |
| `Change Review v1` | 有执行文档，源码未发现实现 | `Resources/Plan/ChangeReviewExecution/` 已有 worker 文档；源码中未发现 `ChangeReview` / `blueprint_review_*` | Task 3 |
| `Patch / Ops` | 未发现 | 源码只存在旧 `blueprint_operations` 校验和 Event Dispatcher 命名误匹配，没有 `apply_patch` / PatchOps 工具 | Task 4 |
| `P2 job/status/cancel/queue` | 未发现 | `BridgeClient` 每次请求新建 TCP 连接；Bridge Router 无 `job_status` / `job_cancel` / request queue 命令 | Task 5 |

---

## 2. 总体实施顺序

```text
Task 1 MCP structuredContent.logic
-> Task 2 RawJson resource_link / resources/read
-> Task 3 Change Review v1
-> Task 4 Patch / Ops
-> Task 5 P2 job/status/cancel/queue
```

排序理由：

1. Task 1 和 Task 2 是读路径和返回承载，风险低，且是后续 Patch/Ops、Review summary、Diff resource 的共同基础。
2. Task 3 建立写操作审阅、快照、回滚闭环，是 Patch/Ops 的前置安全条件。
3. Task 4 会直接修改已有图表，必须依赖稳定 LogicJson、node id、Review 和 strict mutation。
4. Task 5 面向大项目和长任务，可在 Task 3 后并行做队列基础，但必须在 Patch/Ops 扩大使用前完成 cancel/status 语义。

---

## 3. 文件结构

### MCPServer

| 文件 | 操作 | 职责 |
|---|---|---|
| `MCPServer/src/bridge-client.ts` | Modify | 增加 Bridge payload 归一化类型或导出通用响应类型；保持 TCP 协议不变 |
| `MCPServer/src/tool-result.ts` | Create | MCP tool result envelope、`structuredContent`、legacy mode、资源链接构造辅助 |
| `MCPServer/src/blueprint-resource-uri.ts` | Create | `blueprint://asset/...` URI 构造、解析、安全校验 |
| `MCPServer/src/resources.ts` | Modify | 注册 `blueprint://asset/...` resource handler，支持 `logic-md`、`logic-json`、`raw-json` |
| `MCPServer/src/tools.ts` | Modify | `blueprint_get_logic_json`、`blueprint_export_to_json`、review、patch、job 工具接入 |
| `MCPServer/src/tools.regression.test.ts` | Modify | 覆盖结构化返回、resource_link、review/patch/job 工具注册和参数传递 |

### UE C++ Plugin

| 文件 | 操作 | 职责 |
|---|---|---|
| `Source/BlueprintHelper/Public/Bridge/BlueprintHelperBridgeRouter.h` | Modify | 新增 review、patch、job 命令 handler 声明 |
| `Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp` | Modify | Router 命令分发、写命令审阅包裹、job 命令接入 |
| `Source/BlueprintHelper/Public/Services/BlueprintHelperChangeReviewTypes.h` | Create | Review session、operation、asset change、state 类型 |
| `Source/BlueprintHelper/Public/Services/BlueprintHelperChangeReviewManager.h` | Create | Review session 生命周期公开接口 |
| `Source/BlueprintHelper/Private/Services/BlueprintHelperChangeReviewManager.cpp` | Create | session 创建、记录、持久化、approve/reject |
| `Source/BlueprintHelper/Public/Services/BlueprintHelperChangeSnapshot.h` | Create | 资产快照和可回滚信息接口 |
| `Source/BlueprintHelper/Private/Services/BlueprintHelperChangeSnapshot.cpp` | Create | Blueprint/DataTable/UObject/UMG 快照与恢复 |
| `Source/BlueprintHelper/Public/Services/BlueprintHelperPatchOpsService.h` | Create | Patch/Ops 校验与应用接口 |
| `Source/BlueprintHelper/Private/Services/BlueprintHelperPatchOpsService.cpp` | Create | Patch/Ops 执行，复用 `FBlueprintHelperScopedAssetMutation` |
| `Source/BlueprintHelper/Public/Services/BlueprintHelperJobTypes.h` | Create | job 状态、请求、响应、取消状态类型 |
| `Source/BlueprintHelper/Public/Services/BlueprintHelperJobManager.h` | Create | 队列、状态查询、取消请求公开接口 |
| `Source/BlueprintHelper/Private/Services/BlueprintHelperJobManager.cpp` | Create | 单写队列、状态持久化、协作式取消 |
| `Source/BlueprintHelper/Private/Tests/*` | Modify/Create | UE Automation 覆盖 review、patch、job |

### Documentation

| 文件 | 操作 | 职责 |
|---|---|---|
| `Resources/AgentGuide/Reference/02_Capability_Index.md` | Modify | 新增 structuredContent/resource/review/patch/job 能力索引 |
| `Resources/AgentGuide/Reference/03_Tool_Selection_Rules.md` | Modify | 更新 RawJson、review、patch、job 工具选择规则 |
| `Resources/AgentGuide/Workflows/04_Read_Blueprint_Workflow.md` | Modify | 说明 LogicJson 走 `structuredContent.logic`，RawJson 走 resource |
| `Resources/AgentGuide/Workflows/05_Edit_Blueprint_Workflow.md` | Modify | 说明写操作默认 pending review，Patch/Ops 使用条件 |
| `Resources/AgentGuide/Workflows/07_Safety_Validation_And_Recovery.md` | Modify | 增加 reject、mixed changes、job cancellation 行为 |

---

## 4. Task 1: MCP structuredContent.logic

**目标：** `blueprint_get_logic_json` 默认返回摘要文本 + `structuredContent.logic`，不再把完整 LogicJson `JSON.stringify` 到 `content.text`。

**Files:**

- Create: `MCPServer/src/tool-result.ts`
- Modify: `MCPServer/src/tools.ts`
- Modify: `MCPServer/src/bridge-client.ts`
- Test: `MCPServer/src/tools.regression.test.ts`

- [ ] **Step 1: 写失败测试**

在 `MCPServer/src/tools.regression.test.ts` 增加用例：

```ts
test('blueprint_get_logic_json returns structuredContent.logic without stringified JSON text', async () => {
  const tools = registerWithBridge(async () => ({
    request_id: 'test',
    success: true,
    result: {
      format: 'logic_json',
      schema: 'BlueprintHelper.LogicJson.v1',
      assetPath: '/Game/BP/BP_Player',
      graph: 'EventGraph',
      importable: false,
      logic: { nodes: [{ id: 'n1', kind: 'event' }], links: [] },
      stats: { nodes: 1, links: 0 },
    },
  }));
  const tool = tools.get('blueprint_get_logic_json');
  assert.ok(tool);

  const result = await invokeTool(tool, {});

  assert.equal(result.content[0].type, 'text');
  assert.match(result.content[0].text, /Exported LogicJson/);
  assert.equal(result.content[0].text.includes('\\"nodes\\"'), false);
  assert.equal((result as any).structuredContent.format, 'logic_json');
  assert.deepEqual((result as any).structuredContent.logic.nodes, [{ id: 'n1', kind: 'event' }]);
});
```

运行：

```powershell
cd MCPServer
npm.cmd test
```

预期：新增测试失败，因为当前 handler 返回 `toToolResult(resp)`，没有 `structuredContent`。

- [ ] **Step 2: 新增 MCP result helper**

创建 `MCPServer/src/tool-result.ts`：

```ts
import type { BridgeResponse } from './bridge-client.js';

export type McpResponseMode =
  | 'summary_text'
  | 'structured_json'
  | 'resource_ref'
  | 'legacy_text_json';

export interface ResourceLink {
  uri: string;
  name: string;
  description?: string;
  mimeType: string;
}

export interface BuildToolResultOptions {
  mode?: McpResponseMode;
  summary?: string;
  markdown?: string;
  structured?: Record<string, unknown>;
  resourceLinks?: ResourceLink[];
  isError?: boolean;
}

export function normalizeBridgeResult(value: unknown): unknown {
  if (typeof value !== 'string') {
    return value;
  }

  const trimmed = value.trim();
  const looksLikeJson =
    (trimmed.startsWith('{') && trimmed.endsWith('}')) ||
    (trimmed.startsWith('[') && trimmed.endsWith(']'));

  if (!looksLikeJson) {
    return value;
  }

  try {
    return JSON.parse(trimmed) as unknown;
  } catch {
    return value;
  }
}

export function normalizeBlueprintPayload(resp: BridgeResponse): Record<string, unknown> {
  const normalized = normalizeBridgeResult(resp.result ?? resp);

  if (normalized && typeof normalized === 'object') {
    const record = normalized as Record<string, unknown>;
    if (typeof record['json'] === 'string') {
      return {
        ...record,
        json: normalizeBridgeResult(record['json']),
      };
    }
    return record;
  }

  return {
    success: resp.success,
    value: normalized,
  };
}

export function buildBlueprintToolResult(options: BuildToolResultOptions) {
  const mode = options.mode ?? 'summary_text';
  const content: Array<Record<string, unknown>> = [];

  if (mode === 'legacy_text_json' && options.structured) {
    content.push({ type: 'text', text: JSON.stringify(options.structured, null, 2) });
  } else if (options.markdown) {
    content.push({ type: 'text', text: options.markdown });
  } else {
    content.push({
      type: 'text',
      text: options.summary ?? 'BlueprintHelper operation completed.',
    });
  }

  for (const link of options.resourceLinks ?? []) {
    content.push({
      type: 'resource_link',
      uri: link.uri,
      name: link.name,
      description: link.description,
      mimeType: link.mimeType,
    });
  }

  return {
    content,
    isError: options.isError || undefined,
    ...(options.structured ? { structuredContent: options.structured } : {}),
  };
}
```

- [ ] **Step 3: 调整测试类型**

把 `tools.regression.test.ts` 中 `ToolHandler` 的返回类型扩展为允许 `structuredContent`：

```ts
type ToolHandler = (args: Record<string, unknown>) => Promise<{
  content: Array<{ type: string; text?: string; uri?: string; name?: string; mimeType?: string }>;
  isError?: boolean;
  structuredContent?: Record<string, unknown>;
}>;
```

- [ ] **Step 4: 接入 `blueprint_get_logic_json`**

在 `MCPServer/src/tools.ts` 导入 helper：

```ts
import {
  buildBlueprintToolResult,
  normalizeBlueprintPayload,
  type McpResponseMode,
} from './tool-result.js';
```

在 `blueprint_get_logic_json` 输入 schema 增加：

```ts
response_mode: z
  .enum(['summary_text', 'structured_json', 'resource_ref', 'legacy_text_json'])
  .optional()
  .default('structured_json')
  .describe('MCP response carrier mode. Default keeps LogicJson in structuredContent.'),
```

在 handler 参数加入 `response_mode`，替换 `return toToolResult(resp);`：

```ts
const payload = normalizeBlueprintPayload(resp);
const structured = {
  format: payload['format'] ?? 'logic_json',
  schema: payload['schema'] ?? 'BlueprintHelper.LogicJson.v1',
  assetPath: payload['assetPath'] ?? target_blueprint ?? '',
  graph: payload['graph'] ?? target_graph,
  importable: false,
  logic: payload['logic'] ?? payload,
  stats: payload['stats'],
  diagnostics: payload['diagnostics'],
};

return buildBlueprintToolResult({
  mode: response_mode as McpResponseMode,
  summary: `Exported LogicJson: ${String(structured.assetPath)}${structured.graph ? `.${String(structured.graph)}` : ''}.`,
  structured,
  isError: !resp.success,
});
```

- [ ] **Step 5: 保持 `blueprint_get_logic` Markdown 首位**

保留 `content[0].text = markdown`，但将 safety/result fields 放入 `structuredContent`，不再追加第二段 JSON 文本。

新增测试：

```ts
test('blueprint_get_logic returns markdown text and structured metadata', async () => {
  const tools = registerWithBridge(async () => ({
    request_id: 'test',
    success: true,
    result: {
      markdown: '# Logic',
      status: 'failed',
      rolled_back: true,
    },
  }));
  const tool = tools.get('blueprint_get_logic');
  assert.ok(tool);

  const result = await invokeTool(tool, {});

  assert.equal(result.content[0].text, '# Logic');
  assert.equal(result.content.length, 1);
  assert.equal((result as any).structuredContent.status, 'failed');
  assert.equal((result as any).structuredContent.rolled_back, true);
});
```

- [ ] **Step 6: 验证**

运行：

```powershell
cd MCPServer
npm.cmd test
npm.cmd run build
```

预期：

- `blueprint_get_logic_json` 主数据在 `structuredContent.logic`。
- `content[0].text` 是短摘要。
- legacy 模式显式传入时仍返回 stringified JSON。

---

## 5. Task 2: RawJson resource_link / resources/read

**目标：** RawJson 默认不再内联到工具结果，`blueprint_export_to_json` 返回 `resource_link` 和 `structuredContent.rawUri`，完整内容通过 `resources/read` 按需获取。

**Files:**

- Create: `MCPServer/src/blueprint-resource-uri.ts`
- Modify: `MCPServer/src/resources.ts`
- Modify: `MCPServer/src/tools.ts`
- Test: `MCPServer/src/tools.regression.test.ts`

- [ ] **Step 1: 写失败测试**

增加测试：

```ts
test('blueprint_export_to_json returns raw json resource link by default', async () => {
  const tools = registerWithBridge(async () => ({
    request_id: 'test',
    success: true,
    result: {
      json: '{"nodes":[],"links":[]}',
      stats: { nodes: 0, links: 0, bytes: 22 },
    },
  }));
  const tool = tools.get('blueprint_export_to_json');
  assert.ok(tool);

  const result = await invokeTool(tool, {
    target_blueprint: '/Game/BP/BP_Player',
    target_graph: 'EventGraph',
  });

  assert.equal(result.content[0].type, 'text');
  assert.equal(result.content[1].type, 'resource_link');
  assert.equal((result as any).structuredContent.format, 'raw_json_ref');
  assert.match((result as any).structuredContent.rawUri, /^blueprint:\/\/asset\//);
});
```

- [ ] **Step 2: 新增 URI helper**

创建 `MCPServer/src/blueprint-resource-uri.ts`：

```ts
export type BlueprintResourceView =
  | 'logic-md'
  | 'logic-json'
  | 'raw-json'
  | 'diff'
  | 'compile-result';

export interface BlueprintResourceRequest {
  assetPath: string;
  graph?: string;
  view: BlueprintResourceView;
  rev?: string;
}

export function makeBlueprintResourceUri(input: BlueprintResourceRequest): string {
  const params = new URLSearchParams();
  params.set('view', input.view);
  if (input.graph) params.set('graph', input.graph);
  if (input.rev) params.set('rev', input.rev);

  const assetPath = input.assetPath.replace(/^\/+/, '');
  return `blueprint://asset/${encodeURIComponent(assetPath)}?${params.toString()}`;
}

export function parseBlueprintResourceUri(uri: string): BlueprintResourceRequest {
  const parsed = new URL(uri);
  if (parsed.protocol !== 'blueprint:') {
    throw new Error('Invalid blueprint resource protocol.');
  }
  if (parsed.hostname !== 'asset') {
    throw new Error('Invalid blueprint resource host.');
  }

  const view = parsed.searchParams.get('view');
  if (!['logic-md', 'logic-json', 'raw-json', 'diff', 'compile-result'].includes(view ?? '')) {
    throw new Error(`Unsupported blueprint resource view: ${view}`);
  }

  const assetPath = `/${decodeURIComponent(parsed.pathname.replace(/^\/+/, ''))}`;
  if (!assetPath.startsWith('/Game/') && !assetPath.startsWith('/Plugin/')) {
    throw new Error('Only Unreal asset paths are allowed.');
  }
  if (assetPath.includes('..') || /^[A-Za-z]:/.test(assetPath)) {
    throw new Error('Local file paths are not allowed in blueprint resource URI.');
  }

  return {
    assetPath,
    graph: parsed.searchParams.get('graph') ?? undefined,
    view: view as BlueprintResourceView,
    rev: parsed.searchParams.get('rev') ?? undefined,
  };
}
```

- [ ] **Step 3: 修改 `blueprint_export_to_json` 默认返回 resource**

在 `tools.ts` 的 `blueprint_export_to_json` 输入 schema 增加：

```ts
response_mode: z
  .enum(['resource_ref', 'legacy_text_json'])
  .optional()
  .default('resource_ref')
  .describe('Default returns a resource_link; legacy_text_json inlines RawJson for old clients.'),
```

handler 中：

```ts
const resp = await bridge.sendCommand('export_to_json', payload);
const structuredPayload = normalizeBlueprintPayload(resp);

if (response_mode === 'legacy_text_json') {
  return buildBlueprintToolResult({
    mode: 'legacy_text_json',
    structured: structuredPayload,
    isError: !resp.success,
  });
}

const rawUri = makeBlueprintResourceUri({
  assetPath: target_blueprint ?? String(structuredPayload['assetPath'] ?? ''),
  graph: target_graph,
  view: 'raw-json',
});

return buildBlueprintToolResult({
  mode: 'resource_ref',
  summary: 'RawJson is available as a resource. Use it for debugging, compatibility, or replay.',
  structured: {
    format: 'raw_json_ref',
    schema: 'BlueprintHelper.RawJsonRef.v1',
    assetPath: target_blueprint ?? structuredPayload['assetPath'] ?? '',
    graph: target_graph,
    importable: true,
    rawUri,
    stats: structuredPayload['stats'],
  },
  resourceLinks: [{
    uri: rawUri,
    name: `${target_blueprint ?? 'Blueprint'} RawJson`,
    description: 'Full raw BlueprintHelper JSON export.',
    mimeType: 'application/json',
  }],
  isError: !resp.success,
});
```

- [ ] **Step 4: 注册 `blueprint://asset/...` resource read**

在 `MCPServer/src/resources.ts` 中导入：

```ts
import { ResourceTemplate } from '@modelcontextprotocol/sdk/server/mcp.js';
import { parseBlueprintResourceUri } from './blueprint-resource-uri.js';
import { normalizeBlueprintPayload } from './tool-result.js';
```

新增 resource：

```ts
server.registerResource(
  'blueprint-asset-view',
  new ResourceTemplate('blueprint://asset/{assetPath}', { list: undefined }),
  {
    description: 'Blueprint asset views: logic-md, logic-json, raw-json.',
    mimeType: 'application/json',
  },
  async (uri) => {
    const request = parseBlueprintResourceUri(uri.href);

    if (request.view === 'logic-md') {
      const resp = await bridge.sendCommand('export_logic', {
        target_blueprint: request.assetPath,
        target_graph: request.graph,
        format: 'logic_md',
      });
      const payload = normalizeBlueprintPayload(resp);
      return {
        contents: [{
          uri: uri.href,
          mimeType: 'text/markdown',
          text: String(payload['markdown'] ?? ''),
        }],
      };
    }

    if (request.view === 'logic-json') {
      const resp = await bridge.sendCommand('export_logic', {
        target_blueprint: request.assetPath,
        target_graph: request.graph,
        format: 'logic_json',
      });
      return {
        contents: [{
          uri: uri.href,
          mimeType: 'application/json',
          text: JSON.stringify(normalizeBlueprintPayload(resp)),
        }],
      };
    }

    if (request.view === 'raw-json') {
      const resp = await bridge.sendCommand('export_to_json', {
        target_blueprint: request.assetPath,
        target_graph: request.graph,
        scope: 'graph',
      });
      const payload = normalizeBlueprintPayload(resp);
      const raw = typeof payload['json'] === 'string' ? payload['json'] : JSON.stringify(payload['json'] ?? payload);
      return {
        contents: [{
          uri: uri.href,
          mimeType: 'application/json',
          text: raw,
        }],
      };
    }

    throw new Error(`Unsupported blueprint resource view: ${request.view}`);
  },
);
```

- [ ] **Step 5: 验证**

运行：

```powershell
cd MCPServer
npm.cmd test
npm.cmd run build
```

预期：

- `blueprint_export_to_json` 默认返回 `resource_link`。
- `structuredContent.rawUri` 存在。
- `legacy_text_json` 能显式回退。
- `resources/read(rawUri)` 返回 `application/json`，内容可 `JSON.parse`。

---

## 6. Task 3: Change Review v1

**目标：** 写操作默认进入 `pending review`，用户可查看、Approve、Reject；Reject 在安全条件下回滚，不安全时进入 `mixed_changes` 或 `manual_resolution_required`。

**Files:**

- Create: `Source/BlueprintHelper/Public/Services/BlueprintHelperChangeReviewTypes.h`
- Create: `Source/BlueprintHelper/Public/Services/BlueprintHelperChangeReviewManager.h`
- Create: `Source/BlueprintHelper/Private/Services/BlueprintHelperChangeReviewManager.cpp`
- Create: `Source/BlueprintHelper/Public/Services/BlueprintHelperChangeSnapshot.h`
- Create: `Source/BlueprintHelper/Private/Services/BlueprintHelperChangeSnapshot.cpp`
- Modify: `Source/BlueprintHelper/Public/Bridge/BlueprintHelperBridgeRouter.h`
- Modify: `Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp`
- Modify: `MCPServer/src/tools.ts`
- Test: `Source/BlueprintHelper/Private/Tests/*`
- Test: `MCPServer/src/tools.regression.test.ts`

- [ ] **Step 1: 对齐现有执行文档**

先阅读并执行已有分工文档：

```text
Resources/Plan/ChangeReviewExecution/ChangeReview_Execution_Index_20260430.md
Resources/Plan/ChangeReviewExecution/ChangeReview_WorkerA_CommandInventoryContract_20260430.md
Resources/Plan/ChangeReviewExecution/ChangeReview_WorkerB_ReviewManagerPersistence_20260430.md
Resources/Plan/ChangeReviewExecution/ChangeReview_WorkerC_SnapshotRollback_20260430.md
Resources/Plan/ChangeReviewExecution/ChangeReview_WorkerD_BridgeIntegration_20260430.md
Resources/Plan/ChangeReviewExecution/ChangeReview_WorkerG_MCPToolsDocs_20260430.md
Resources/Plan/ChangeReviewExecution/ChangeReview_WorkerH_ValidationIntegration_20260430.md
```

验收：执行前输出一份命令清单，标记 read/write/ui/save/build/editor-process。

- [ ] **Step 2: 写 ReviewManager 持久化测试**

新增 UE Automation 测试：

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlueprintHelperReviewManagerPersistsSessionTest,
    "BlueprintHelper.ChangeReview.Persistence.CreatesAndReloadsPendingSession",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewManagerPersistsSessionTest::RunTest(const FString& Parameters)
{
    FBlueprintHelperChangeReviewManager Manager;
    const FString SessionId = Manager.BeginSession(TEXT("Automation Review"), TEXT("Automation"));

    TestFalse(TEXT("SessionId should not be empty"), SessionId.IsEmpty());
    TestTrue(TEXT("Session should be pending"), Manager.MarkPending(SessionId));
    TestTrue(TEXT("Save should succeed"), Manager.SaveSession(SessionId));

    FBlueprintHelperChangeReviewManager Reloaded;
    TestTrue(TEXT("Reload should find session"), Reloaded.LoadSession(SessionId).IsSet());
    TestEqual(TEXT("Session state should remain pending"), Reloaded.LoadSession(SessionId)->State, EBlueprintHelperReviewSessionState::PendingReview);
    return true;
}
```

预期：先失败，因为类型不存在。

- [ ] **Step 3: 实现 Review 数据模型**

在 `BlueprintHelperChangeReviewTypes.h` 定义：

```cpp
enum class EBlueprintHelperReviewSessionState : uint8
{
    Open,
    PendingReview,
    Approved,
    Rejected,
    Reverted,
    Failed,
    MixedChanges,
    ManualResolutionRequired
};

enum class EBlueprintHelperRollbackMode : uint8
{
    Transaction,
    SnapshotRestore,
    DeleteCreatedAsset,
    NotSupported,
    ManualOnly
};

enum class EBlueprintHelperReviewPolicy : uint8
{
    Pending,
    Bypass,
    AutoApproveAndSave
};

struct FBlueprintHelperReviewOperationRecord
{
    FString OperationId;
    FString CommandName;
    FString TargetAssetPath;
    FString TargetGraphName;
    FString RequestJson;
    FString ResponseJson;
    bool bSuccess = false;
    FString ErrorMessage;
};

struct FBlueprintHelperReviewAssetRecord
{
    FString AssetPath;
    FString AssetClass;
    FString ChangeKind;
    FString BeforeSnapshotId;
    FString AfterSnapshotId;
    FString SummaryMarkdown;
    FString CompileStatus;
    EBlueprintHelperRollbackMode RollbackMode = EBlueprintHelperRollbackMode::NotSupported;
};

struct FBlueprintHelperReviewSession
{
    FString SessionId;
    FString DisplayName;
    FString Initiator;
    FDateTime StartedAt;
    FDateTime LastUpdatedAt;
    EBlueprintHelperReviewSessionState State = EBlueprintHelperReviewSessionState::Open;
    TArray<FBlueprintHelperReviewOperationRecord> Operations;
    TArray<FBlueprintHelperReviewAssetRecord> Assets;
    TArray<FString> Diagnostics;
    bool bHasMixedUserChanges = false;
    bool bRequiresManualResolution = false;
};
```

- [ ] **Step 4: 实现 ReviewManager**

`BlueprintHelperChangeReviewManager` 必须提供：

```cpp
FString BeginSession(const FString& DisplayName, const FString& Initiator);
TOptional<FBlueprintHelperReviewSession> LoadSession(const FString& SessionId) const;
TArray<FBlueprintHelperReviewSession> ListPendingSessions() const;
bool RecordOperation(const FString& SessionId, const FBlueprintHelperReviewOperationRecord& Operation);
bool RecordAssetChange(const FString& SessionId, const FBlueprintHelperReviewAssetRecord& AssetChange);
bool MarkPending(const FString& SessionId);
bool ApproveSession(const FString& SessionId, bool bSaveAssets, FString& OutError);
bool RejectSession(const FString& SessionId, FString& OutError);
bool SaveSession(const FString& SessionId) const;
```

持久化目录固定为：

```text
Saved/BlueprintHelper/ReviewSessions/
```

- [ ] **Step 5: Bridge Router 包裹写命令**

在 `BlueprintHelperBridgeRouter.cpp` 中增加：

```cpp
bool IsReviewableWriteCommand(const FString& Command);
EBlueprintHelperReviewPolicy ParseReviewPolicy(const TSharedPtr<FJsonObject>& Payload);
FBlueprintHelperBridgeResponse RunWithReview(const FBlueprintHelperBridgeRequest& Request, TFunctionRef<FBlueprintHelperBridgeResponse()> Execute);
```

规则：

- 读命令不进入 Review。
- 写命令默认 `review_policy = pending`。
- `save_asset` 类操作必须显式 `review_policy = auto_approve_and_save` 或用户触发。
- `bypass` 只允许低风险或显式用户授权场景。

- [ ] **Step 6: 新增 Bridge review 命令**

Router 支持：

```text
review_begin_session
review_list_sessions
review_get_session
review_approve_session
review_reject_session
review_open_asset_diff
review_export_summary
```

每个响应都返回：

```json
{
  "review": {
    "session_id": "BPHR_...",
    "state": "pending_review",
    "requires_user_review": true
  }
}
```

- [ ] **Step 7: 新增 MCP review tools**

`MCPServer/src/tools.ts` 注册：

```text
blueprint_review_begin_session
blueprint_review_list_pending
blueprint_review_get_summary
blueprint_review_approve
blueprint_review_reject
blueprint_review_open_diff
```

MCP 行为：

- list/get/open_diff 是读或 UI 操作。
- approve/reject 必须要求 `session_id`。
- approve/reject 的工具描述必须说明需要用户明确指令。

- [ ] **Step 8: 验证**

运行：

```powershell
cd MCPServer
npm.cmd test
npm.cmd run build
```

运行 UE 编译：

```powershell
& 'F:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

运行 UE Automation：

```powershell
& 'F:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'G:/UnrealPractise/MrStone/MrStone.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.ChangeReview;Quit'
```

验收：

- 添加变量、添加节点、修改 DataTable 行至少三类写操作进入 pending review。
- `blueprint_review_list_pending` 能看到 session。
- Reject 在未混入用户修改时可恢复。
- 已保存或用户混入修改时不静默回滚，状态进入 `mixed_changes` 或 `manual_resolution_required`。

---

## 7. Task 4: Patch / Ops

**目标：** 新增对象化 Patch/Ops 协议，支持 Agent 对已有图表做小粒度增量修改，默认进入 Change Review，不使用整份 RawJson 覆盖。

**Files:**

- Create: `Source/BlueprintHelper/Public/Services/BlueprintHelperPatchOpsService.h`
- Create: `Source/BlueprintHelper/Private/Services/BlueprintHelperPatchOpsService.cpp`
- Modify: `Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp`
- Modify: `Source/BlueprintHelper/Public/Bridge/BlueprintHelperBridgeRouter.h`
- Modify: `MCPServer/src/tools.ts`
- Test: `Source/BlueprintHelper/Private/Tests/*`
- Test: `MCPServer/src/tools.regression.test.ts`

- [ ] **Step 1: 定义 Patch/Ops v1 范围**

第一版只支持：

```json
{
  "schema": "BlueprintHelper.PatchOps",
  "version": "1.0",
  "target_blueprint": "/Game/BP/BP_Player.BP_Player",
  "target_graph": "EventGraph",
  "base_logic_rev": "optional",
  "ops": [
    { "op": "add_node", "id": "print_1", "kind": "call", "function": "/Script/Engine.KismetSystemLibrary:PrintString" },
    { "op": "set_pin_default", "node": "print_1", "pin": "InString", "value": "Hello" },
    { "op": "connect_exec", "from": { "node": "begin_play", "pin": "Then" }, "to": { "node": "print_1", "pin": "execute" } }
  ],
  "options": {
    "compile": true,
    "save": false,
    "review_policy": "pending",
    "strict": true
  }
}
```

第一版明确不支持：

```text
跨 Blueprint Patch
跨 graph link
大范围自动 merge
无 target_graph 的隐式写入
按 title 模糊匹配既有节点
```

- [ ] **Step 2: 写 MCP schema 测试**

`tools.regression.test.ts` 增加：

```ts
test('blueprint_apply_patch requires explicit target blueprint and graph', async () => {
  const tools = registerWithBridge(async () => ({ request_id: 'test', success: true }));
  const tool = tools.get('blueprint_apply_patch');
  assert.ok(tool);

  assert.throws(() => tool.inputSchema.parse({
    schema: 'BlueprintHelper.PatchOps',
    version: '1.0',
    target_blueprint: '/Game/BP/BP_Player',
    ops: [],
  }));

  const parsed = tool.inputSchema.parse({
    schema: 'BlueprintHelper.PatchOps',
    version: '1.0',
    target_blueprint: '/Game/BP/BP_Player',
    target_graph: 'EventGraph',
    ops: [],
  });
  assert.equal(parsed.target_graph, 'EventGraph');
});
```

- [ ] **Step 3: MCP 工具转发 Bridge**

新增工具：

```text
blueprint_apply_patch
```

handler：

```ts
const resp = await bridge.sendCommand('apply_patch_ops', args);
return buildBlueprintToolResult({
  mode: 'structured_json',
  summary: resp.success ? 'Patch/Ops applied and entered review.' : 'Patch/Ops failed.',
  structured: normalizeBlueprintPayload(resp),
  isError: !resp.success,
});
```

- [ ] **Step 4: C++ PatchOpsService 校验**

`FBlueprintHelperPatchOpsService` 提供：

```cpp
struct FBlueprintHelperPatchOpsResult
{
    bool bSuccess = false;
    FString Status;
    int32 OperationsApplied = 0;
    TArray<FString> Warnings;
    TArray<FString> Errors;
    FString ReviewSessionId;
};

class FBlueprintHelperPatchOpsService
{
public:
    FBlueprintHelperPatchOpsResult ApplyPatchOps(const TSharedPtr<FJsonObject>& PatchRequest);
};
```

校验规则：

- `target_blueprint` 必填。
- `target_graph` 必填。
- `ops` 必须是数组。
- 每个 op 必须有 `op` 字段。
- 引用既有节点必须使用稳定 node id 或 GUID。
- strict 下任一 op 失败整体回滚。

- [ ] **Step 5: Bridge Router 接入**

新增命令：

```text
apply_patch_ops
```

执行路径：

```text
Validate request
-> Resolve blueprint + graph
-> Begin scoped mutation
-> Begin review session
-> Apply ops
-> Compile if requested
-> Record review
-> Return structured diagnostics
```

- [ ] **Step 6: UE Automation 测试**

测试用例：

```text
BlueprintHelper.PatchOps.ApplyAddPrintString.EntersReview
BlueprintHelper.PatchOps.RejectRestoresBeforeGraph
BlueprintHelper.PatchOps.MissingTargetGraphHardFails
BlueprintHelper.PatchOps.UnknownNodeIdRollsBackStrict
BlueprintHelper.PatchOps.CrossGraphLinkRejected
```

- [ ] **Step 7: 验证**

运行：

```powershell
cd MCPServer
npm.cmd test
npm.cmd run build
```

```powershell
& 'F:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

```powershell
& 'F:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'G:/UnrealPractise/MrStone/MrStone.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.PatchOps;Quit'
```

验收：

- Patch/Ops 不接受缺失 graph 的请求。
- strict 失败不留下半改动。
- 成功 Patch 进入 pending review。
- Reject 后 LogicJson 摘要与 before snapshot 匹配。

---

## 8. Task 5: P2 job/status/cancel/queue

**目标：** 支持长任务异步执行、状态查询、取消请求和串行写队列，避免 Node 侧超时后 UE 仍在不可见地继续写资产。

**Files:**

- Create: `Source/BlueprintHelper/Public/Services/BlueprintHelperJobTypes.h`
- Create: `Source/BlueprintHelper/Public/Services/BlueprintHelperJobManager.h`
- Create: `Source/BlueprintHelper/Private/Services/BlueprintHelperJobManager.cpp`
- Modify: `Source/BlueprintHelper/Public/Bridge/BlueprintHelperBridgeRouter.h`
- Modify: `Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp`
- Modify: `MCPServer/src/bridge-client.ts`
- Modify: `MCPServer/src/tools.ts`
- Test: `Source/BlueprintHelper/Private/Tests/*`
- Test: `MCPServer/src/tools.regression.test.ts`

- [ ] **Step 1: 定义 job 状态语义**

```cpp
enum class EBlueprintHelperJobState : uint8
{
    Queued,
    Running,
    Succeeded,
    Failed,
    CancelRequested,
    Cancelled,
    TimedOut,
    Abandoned
};
```

状态规则：

- `Queued` 可直接取消。
- `Running` 只支持协作式取消，不强杀线程，不中断 UE UObject 写入的临界区。
- MCP client 超时后，UE job 状态变为 `Abandoned` 或继续可查询。
- 同一时间只允许一个写 job 进入 mutation 临界区。

- [ ] **Step 2: 写 MCP job 工具注册测试**

`tools.regression.test.ts` 增加：

```ts
test('job tools forward job commands to bridge', async () => {
  const calls: Array<{ command: string; payload?: Record<string, unknown> }> = [];
  const tools = registerWithBridge(async (command, payload) => {
    calls.push({ command, payload });
    return {
      request_id: 'test',
      success: true,
      result: { job_id: 'job_1', state: 'queued' },
    };
  });

  await invokeTool(tools.get('blueprint_job_status')!, { job_id: 'job_1' });
  await invokeTool(tools.get('blueprint_job_cancel')!, { job_id: 'job_1' });

  assert.deepEqual(calls.map((call) => call.command), ['job_status', 'job_cancel']);
});
```

- [ ] **Step 3: 实现 JobManager**

公开接口：

```cpp
class FBlueprintHelperJobManager
{
public:
    FString Enqueue(const FString& CommandName, const TSharedPtr<FJsonObject>& Payload);
    TOptional<FBlueprintHelperJobRecord> GetStatus(const FString& JobId) const;
    TArray<FBlueprintHelperJobRecord> ListJobs() const;
    bool RequestCancel(const FString& JobId, FString& OutError);
    void TickJobs();
};
```

执行约束：

- 所有 UObject/Blueprint 修改必须在 Game Thread 执行。
- `TickJobs` 从队列取一个 job 执行。
- 写 job 执行时阻塞后续写 job，读 job 可在安全时同步返回。
- job record 保留 request summary、start/end time、progress、diagnostics、review session id。

- [ ] **Step 4: Bridge Router 新增 job 命令**

支持：

```text
job_status
job_list
job_cancel
job_queue_snapshot
```

支持异步提交参数：

```json
{
  "async": true,
  "queue_policy": "enqueue",
  "timeout_ms": 120000
}
```

对支持 async 的长任务，首次响应：

```json
{
  "success": true,
  "result": {
    "job_id": "BPHJ_...",
    "state": "queued",
    "status_uri": "blueprint://job/BPHJ_..."
  }
}
```

- [ ] **Step 5: MCP 新增 job tools**

`MCPServer/src/tools.ts` 注册：

```text
blueprint_job_status
blueprint_job_list
blueprint_job_cancel
blueprint_job_queue_snapshot
```

工具返回使用 Task 1 的 `structuredContent`：

```ts
return buildBlueprintToolResult({
  mode: 'structured_json',
  summary: `Job ${job_id} state: ${String(payload['state'])}.`,
  structured: payload,
  isError: !resp.success,
});
```

- [ ] **Step 6: 资源读取扩展**

Task 2 的 URI 机制扩展：

```text
blueprint://job/{jobId}
```

`resources/read` 返回 job record JSON，用于大结果按需读取。

- [ ] **Step 7: UE Automation 测试**

测试用例：

```text
BlueprintHelper.Job.Queue.EnqueuesLongWrite
BlueprintHelper.Job.Status.QueuedRunningSucceeded
BlueprintHelper.Job.Cancel.QueuedJobCancelled
BlueprintHelper.Job.Cancel.RunningJobCooperativeOnly
BlueprintHelper.Job.Queue.SingleWriter
BlueprintHelper.Job.Timeout.MarkedAbandoned
```

- [ ] **Step 8: 验证**

运行：

```powershell
cd MCPServer
npm.cmd test
npm.cmd run build
```

```powershell
& 'F:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

```powershell
& 'F:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'G:/UnrealPractise/MrStone/MrStone.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.Job;Quit'
```

验收：

- 长任务可返回 `job_id`。
- `blueprint_job_status` 可查询 queued/running/succeeded/failed/cancelled。
- queued job 可取消。
- running job 不被强杀，只在安全 checkpoint 取消。
- 同一时间只有一个写 job 修改资产。

---

## 9. 文档更新任务

**Files:**

- Modify: `Resources/AgentGuide/Reference/02_Capability_Index.md`
- Modify: `Resources/AgentGuide/Reference/03_Tool_Selection_Rules.md`
- Modify: `Resources/AgentGuide/Workflows/04_Read_Blueprint_Workflow.md`
- Modify: `Resources/AgentGuide/Workflows/05_Edit_Blueprint_Workflow.md`
- Modify: `Resources/AgentGuide/Workflows/07_Safety_Validation_And_Recovery.md`

- [ ] **Step 1: 更新读路径规则**

写入以下规则：

```text
默认读蓝图逻辑使用 blueprint_get_logic。
需要机器分析时使用 blueprint_get_logic_json，并从 structuredContent.logic 读取。
只有调试、兼容、回放时才读取 RawJson resource。
不要把 LogicJson 当成 importable RawJson。
```

- [ ] **Step 2: 更新写路径规则**

写入以下规则：

```text
写操作默认进入 pending review。
Patch/Ops 必须显式 target_blueprint 和 target_graph。
Agent 不得在没有用户明确指令时 approve/reject。
Reject 失败或 mixed_changes 时必须报告人工处理状态。
```

- [ ] **Step 3: 更新长任务规则**

写入以下规则：

```text
长任务优先 async 提交并轮询 job status。
超时后先查 job_status，不重复提交同一写请求。
cancel 是请求取消，不代表 running UObject mutation 被强制终止。
```

---

## 10. 总体验收矩阵

| 验收项 | 命令 / 操作 | 预期 |
|---|---|---|
| MCP build | `cd MCPServer; npm.cmd run build` | Exit code 0 |
| MCP regression | `cd MCPServer; npm.cmd test` | 所有 Node tests PASS |
| UE build | `Build.bat MrStoneEditor Win64 Development` | Exit code 0 |
| StructuredContent | 调 `blueprint_get_logic_json` | `structuredContent.logic` 存在，`content.text` 是摘要 |
| RawJson resource | 调 `blueprint_export_to_json` | 返回 `resource_link` 和 `rawUri` |
| resources/read | 读 `rawUri` | 返回 `application/json` 且可解析 |
| Change Review | 执行添加变量 | pending review session 出现 |
| Reject | 对 pending session reject | 未混入修改时资产恢复 |
| Patch/Ops | 应用 add PrintString patch | 进入 review，编译诊断结构化返回 |
| Patch strict rollback | Patch 引用不存在 node | 返回 failed，资产无半改动 |
| Job queue | async 提交长任务 | 返回 `job_id` |
| Job status | 查询 `job_id` | 返回 queued/running/succeeded/failed |
| Job cancel | cancel queued job | 状态变为 cancelled |

---

## 11. 版本建议

```text
v0.3.4
  - MCP structuredContent.logic
  - RawJson resource_link
  - blueprint://asset resources/read
  - legacy_text_json 兼容模式

v0.3.6
  - Change Review v1
  - review MCP tools
  - snapshot / reject / mixed_changes

v0.4.0
  - Patch / Ops v1
  - P2 job/status/cancel/queue
  - job resource read
```

---

## 12. 自检结论

- 覆盖了用户列出的五个缺口。
- 每个缺口都有明确任务、文件范围、测试入口和验收标准。
- 没有要求使用 BlueprintHelper MCP 修改源码或文档；本计划遵守项目边界，文档和代码修改均走普通仓库工具。
- Change Review 已有执行文档，本计划不替代已有分工，而是把它接入当前五项缺口的总顺序。
