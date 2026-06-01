# BlueprintHelper CLI Screenshot Evidence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增 Agent-facing CLI 截图证据工具 `blueprinthelper_capture_screenshot`，按输入的 `asset_path` 打开资产，并在给定 `graph_name + block_ref/node_ref` 时定位到蓝图目标后保存编辑器截图证据。

**Architecture:** CLI/task-core 是整合工具簇，负责串联打开资产、可选蓝图定位、截图命令和结果归一化；UE Bridge 内部只暴露小而纯的 primitive：`focus_blueprint_editor_target` 只负责编辑器定位，`capture_editor_screenshot` 只负责对当前编辑器状态截图。截图实现落在 Debug/Evidence service 边界，不接入 UI widget，不使用 Windows 截图工具，不把资产打开/蓝图解析塞进截图服务。

**Tech Stack:** TypeScript + Zod + node:test, BlueprintHelper CLI/TaskCore tool-surface, BlueprintHelper Bridge TCP command routing, UE 5.6 Editor C++ Plugin, Slate `FSlateApplication::TakeScreenshot`, `FImageUtils::PNGCompressImageArray`, UE Automation, live editor E2E.

---

## 0. Authoritative Decisions

- Agent-facing command name: `blueprinthelper_capture_screenshot`.
- Agent-facing request schema: `BlueprintHelper.CaptureScreenshotRequest.v1`.
- Agent-facing location input uses readable semantic fields:
  - `asset_path` is required.
  - `graph_name` is optional unless `block_ref` or `node_ref` is provided.
  - `block_ref` is optional and must remain human-readable.
  - `node_ref` is optional and must support readable values already accepted by graph/node resolution paths, such as `nodes[0]` or node name/path references.
- CLI/task-core orchestration sequence:
  1. `open_asset`
  2. `focus_blueprint_editor_target` only when `graph_name`, `block_ref`, or `node_ref` is provided
  3. optional bounded settle delay
  4. `capture_editor_screenshot`
- UE screenshot primitive receives only screenshot options and captures the current editor state. It must not parse `asset_path`, `graph_name`, `block_ref`, or `node_ref`.
- Fixed screenshot output root: `FBlueprintHelperDebugCaseStoreService::GetDebugRootDir() / Screenshots`.
- P0 capture mode: Slate active editor window. `FScreenshotRequest::RequestScreenshot(...)` remains a compatibility fallback for viewport capture, not the primary editor UI evidence path.
- Local UE 5.3, 5.4, and 5.5 source trees all expose `FScreenshotRequest::RequestScreenshot(bool)` and `FScreenshotRequest::RequestScreenshot(const FString&, bool, bool, bool)`. Keep any fallback behind a small adapter seam.
- C++ code must not add anonymous namespaces.
- This repository's task completion rule forbids agents from running `git add`, `git commit`, or `git push`. Every checkpoint below records suggested manual commands only.

## 1. P0 Scope

P0 must include:

- New default Agent-facing CLI tool `blueprinthelper_capture_screenshot`.
- New task-core schema and handler that compose existing Bridge commands instead of mapping the CLI tool to one Bridge command.
- New UE Bridge command `focus_blueprint_editor_target` for opening/focusing the requested graph/node context.
- New UE Bridge command `capture_editor_screenshot` for screenshot capture only.
- New `FBlueprintHelperScreenshotCaptureService` with a pure screenshot API.
- New `FBlueprintHelperEditorFocusService` with asset/graph/node positioning API.
- Screenshot files written under `Saved/BlueprintHelper/Debug/Screenshots` or the configured DebugBundle root equivalent.
- Result payload containing schema, absolute path, relative path, width, height, target, created timestamp, and orchestration step summaries.
- Automation tests and one live editor E2E test that confirms an actual PNG file exists and is non-empty.

P0 explicitly excludes:

- Windows Snipping Tool automation.
- New toolbar/UI button.
- Automatic bug discovery or visual diffing.
- Screenshot service opening assets or parsing blueprint location inputs.
- Agent-facing `BlueprintHelperBlockId` requirement.
- Legacy direct MCP-only exposure or old Agent compatibility fields.
- Any Review v1, legacy anchor, or Transaction fallback path.

## 2. Subagent Development Split

Use this split when executing the plan:

- Worker A, small code task, model `5.4 high`: TypeScript schema, handler, registry, CLI docs/templates, and node tests.
- Worker B, architecture/code task, model `5.5 xhigh`: UE C++ services, Bridge route adapters, validators, and UE automation tests.
- Worker C, small code/docs task, model `5.4 high`: live E2E script/documentation, Report/Debug updates, and command evidence collection.
- Final audit workers after implementation:
  - Small readonly worker 1, model `5.3codex-spark xhigh`: verify agent-facing CLI/tool registry exposure and no legacy shortcut.
  - Small readonly worker 2, model `5.3codex-spark xhigh`: verify UE screenshot primitive stays pure and C++ has no anonymous namespace additions.
  - Medium readonly worker 3, model `5.4mini xhigh`: verify tests/E2E evidence and UE 5.3-5.6 compatibility notes.

## 3. File Structure

### New TypeScript files

- Create: `AgentFaceService/task-core/src/tool-surface/bridge/screenshot/capture-screenshot-schema.ts`
  - Zod schema and exported TypeScript request type for `BlueprintHelper.CaptureScreenshotRequest.v1`.
- Create: `AgentFaceService/task-core/src/tool-surface/bridge/screenshot/capture-screenshot-schema.test.ts`
  - Schema validation tests for required fields, semantic location requirements, and strict unknown-field rejection.
- Create: `AgentFaceService/task-core/src/tool-surface/bridge/screenshot/capture-screenshot-handler.ts`
  - Orchestration handler that sends `open_asset`, optional `focus_blueprint_editor_target`, optional sleep, and `capture_editor_screenshot`.
- Create: `AgentFaceService/task-core/src/tool-surface/bridge/screenshot/capture-screenshot-handler.test.ts`
  - Handler tests for command order, error propagation, and proof that screenshot primitive receives no asset/navigation fields.
- Create: `AgentFaceService/agent-guide/Templates/blueprinthelper_capture_screenshot_template.json`
  - Minimal Agent-facing CLI input template.

### Modified TypeScript files

- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/bridge-tool-schemas.ts`
  - Register `blueprinthelper_capture_screenshot` with the new schema.
- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/bridge-tool-dispatcher.ts`
  - Add a special branch for the screenshot orchestration handler before the generic bridge command map.
- Modify: `AgentFaceService/task-core/src/tool-surface/registry/tool-metas.ts`
  - Add default, low-risk tool metadata for `blueprinthelper_capture_screenshot`.
- Modify: `AgentFaceService/task-core/src/tests/tool-surface/tool-registry.contract.test.ts`
  - Add registry exposure assertions and explicitly avoid requiring a generic bridge command map entry.
- Modify: `AgentFaceService/cli/src/cli/help.ts`
  - Add help text and template hint for the new tool.
- Modify: `AgentFaceService/cli/src/tests/cli/cli-tool-bridge.test.ts`
  - Add CLI invocation test that validates orchestration calls and JSON output.
- Modify: `AgentFaceService/docs/CLI_Tools_API_Reference.md`
  - Document request/response shape and CLI examples.
- Modify: `AgentFaceService/agent-guide/Templates/SEMANTIC_INDEX.md`
  - Add screenshot template to the semantic index.
- Modify: `AgentFaceService/agent-guide/Templates/INDEX.md`
  - Add screenshot evidence category entry if the index is currently category-based.

### New UE C++ files

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Shared/Debug/BlueprintHelperScreenshotTypes.h`
  - DTOs for screenshot capture request/result and editor focus request/result.
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Debug/BlueprintHelperScreenshotCaptureService.h`
  - Pure screenshot capture service interface.
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Debug/BlueprintHelperScreenshotCaptureService.cpp`
  - Active editor window capture, PNG writing, and metadata construction.
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Debug/BlueprintHelperEditorFocusService.h`
  - Asset/Blueprint graph/node focus service interface.
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Debug/BlueprintHelperEditorFocusService.cpp`
  - Uses existing asset opening and graph/node resolver boundaries to position the editor.
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/Routes/BlueprintHelperScreenshotBridgeRoutes.h`
  - Bridge route adapter interface for screenshot/focus commands.
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Routes/BlueprintHelperScreenshotBridgeRoutes.cpp`
  - Payload parsing, service invocation, and Bridge JSON result serialization.
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics/BlueprintHelperScreenshotCaptureServiceTests.cpp`
  - Automation tests for output path generation, invalid active window behavior, and metadata shape.
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics/BlueprintHelperEditorFocusServiceTests.cpp`
  - Automation tests for graph/node target payload parsing and resolver failure messages.

### Modified UE C++ files

- Modify: `BlueprintHelper/Source/BlueprintHelper/BlueprintHelper.Build.cs`
  - Add `ImageWrapper` only if PNG compression linkage requires it after the first compile failure proves it.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/BlueprintHelper.h`
  - Own and initialize screenshot/focus services through module lifecycle.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/BlueprintHelper.cpp`
  - Construct services and inject them into Bridge routes/router.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/BlueprintHelperBridgeRouter.h`
  - Hold `FBlueprintHelperScreenshotBridgeRoutes`.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp`
  - Dispatch `focus_blueprint_editor_target` and `capture_editor_screenshot`.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h`
  - Add route cluster enum/value if the current planner enum requires one.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.cpp`
  - Add route mapping for `focus_blueprint_editor_target` and `capture_editor_screenshot`.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperRequestValidator.cpp`
  - Validate payload shapes and keep both commands classified as read/debug, not write.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Bridge/BlueprintHelperBridgeRoutePlannerTests.cpp`
  - Assert route cluster and GameThread/read-lane classification.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Bridge/BlueprintHelperObjectFirstBridgeTests.cpp`
  - Add Bridge payload validation tests if this file already owns object-first request cases.

### Documentation and evidence files

- Modify: `Debug/BlueprintHelper_EditorScreenshotEvidence_Brainstorm_20260601.md`
  - Append implementation evidence, command outputs, test summaries, and E2E screenshot path.
- Create after implementation: `BlueprintHelper/Develop/Report/BlueprintHelper_CLI_ScreenshotEvidence_Report_20260602_CN.md`
  - Required because execution of this plan changes code. Include reason, process, result, and exact code-change scope.

## 4. Task A: TypeScript Schema

**Files:**

- Create: `AgentFaceService/task-core/src/tool-surface/bridge/screenshot/capture-screenshot-schema.test.ts`
- Create: `AgentFaceService/task-core/src/tool-surface/bridge/screenshot/capture-screenshot-schema.ts`
- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/bridge-tool-schemas.ts`

- [x] **Step A1: Create failing schema tests**

Create `AgentFaceService/task-core/src/tool-surface/bridge/screenshot/capture-screenshot-schema.test.ts`:

```ts
import { strict as assert } from 'node:assert';
import test from 'node:test';

import { CaptureScreenshotInputSchema } from './capture-screenshot-schema.js';

test('capture screenshot schema accepts asset-only request', () => {
  const parsed = CaptureScreenshotInputSchema.parse({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    label: 'bp_player_asset',
  });

  assert.equal(parsed.asset_path, '/Game/Blueprints/BP_Player.BP_Player');
  assert.equal(parsed.capture_target, 'active_window');
});

test('capture screenshot schema accepts readable blueprint location request', () => {
  const parsed = CaptureScreenshotInputSchema.parse({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    graph_name: 'EventGraph',
    block_ref: 'BeginPlaySetup',
    node_ref: 'nodes[0]',
    settle_delay_ms: 250,
  });

  assert.equal(parsed.graph_name, 'EventGraph');
  assert.equal(parsed.block_ref, 'BeginPlaySetup');
  assert.equal(parsed.node_ref, 'nodes[0]');
  assert.equal(parsed.settle_delay_ms, 250);
});

test('capture screenshot schema requires graph_name when block_ref is provided', () => {
  const parsed = CaptureScreenshotInputSchema.safeParse({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    block_ref: 'BeginPlaySetup',
  });

  assert.equal(parsed.success, false);
  assert.match(String(parsed.error?.issues[0]?.message), /graph_name/);
});

test('capture screenshot schema requires graph_name when node_ref is provided', () => {
  const parsed = CaptureScreenshotInputSchema.safeParse({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    node_ref: 'nodes[0]',
  });

  assert.equal(parsed.success, false);
  assert.match(String(parsed.error?.issues[0]?.message), /graph_name/);
});

test('capture screenshot schema rejects unknown fields and unsafe labels', () => {
  assert.equal(CaptureScreenshotInputSchema.safeParse({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    unexpected: true,
  }).success, false);

  assert.equal(CaptureScreenshotInputSchema.safeParse({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    label: '../escape',
  }).success, false);
});
```

- [x] **Step A2: Run the failing test gate**

Run:

```powershell
npm.cmd --prefix AgentFaceService\task-core run build
npm.cmd --prefix AgentFaceService\task-core run test:node
```

Expected before implementation:

- `npm run build` fails because `capture-screenshot-schema.ts` does not exist, or
- `npm run test:node` fails because `CaptureScreenshotInputSchema` is not exported.

- [x] **Step A3: Implement the schema**

Create `AgentFaceService/task-core/src/tool-surface/bridge/screenshot/capture-screenshot-schema.ts`:

```ts
import { z } from 'zod';

export const CaptureScreenshotTargetSchema = z.enum(['active_window', 'active_viewport']);

export const CaptureScreenshotInputSchema = z.object({
  schema: z.literal('BlueprintHelper.CaptureScreenshotRequest.v1'),
  asset_path: z.string().min(1),
  graph_name: z.string().min(1).optional(),
  block_ref: z.string().min(1).optional(),
  node_ref: z.string().min(1).optional(),
  label: z.string().min(1).max(80).regex(/^[A-Za-z0-9_.-]+$/).optional(),
  capture_target: CaptureScreenshotTargetSchema.default('active_window'),
  settle_delay_ms: z.number().int().min(0).max(5000).default(250),
}).strict().superRefine((value, ctx) => {
  if ((value.block_ref || value.node_ref) && !value.graph_name) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: value.block_ref ? ['block_ref'] : ['node_ref'],
      message: 'graph_name is required when block_ref or node_ref is provided.',
    });
  }
});

export type CaptureScreenshotInput = z.infer<typeof CaptureScreenshotInputSchema>;
```

- [x] **Step A4: Register schema in the bridge schema registry**

Modify `AgentFaceService/task-core/src/tool-surface/bridge/bridge-tool-schemas.ts`:

```ts
import { CaptureScreenshotInputSchema } from './screenshot/capture-screenshot-schema.js';
```

Add to `bridgeToolSchemas`:

```ts
blueprinthelper_capture_screenshot: CaptureScreenshotInputSchema,
```

- [x] **Step A5: Run schema gate again**

Run:

```powershell
npm.cmd --prefix AgentFaceService\task-core run build
npm.cmd --prefix AgentFaceService\task-core run test:node
```

Expected:

- Build succeeds.
- New schema tests pass.
- Existing task-core tests remain passing.

## 5. Task B: TypeScript Orchestration Handler

**Files:**

- Create: `AgentFaceService/task-core/src/tool-surface/bridge/screenshot/capture-screenshot-handler.test.ts`
- Create: `AgentFaceService/task-core/src/tool-surface/bridge/screenshot/capture-screenshot-handler.ts`
- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/bridge-tool-dispatcher.ts`

- [x] **Step B1: Create handler tests for command sequence**

Create `AgentFaceService/task-core/src/tool-surface/bridge/screenshot/capture-screenshot-handler.test.ts`:

```ts
import { strict as assert } from 'node:assert';
import test from 'node:test';

import type { BridgeResponse } from '../../../bridge/bridge-client.js';
import type { BlueprintHelperToolContext } from '../../types.js';
import { executeCaptureScreenshot } from './capture-screenshot-handler.js';

function response(command: string, result: Record<string, unknown> = {}): BridgeResponse {
  return {
    request_id: `req_${command}`,
    success: true,
    result,
  };
}

function makeContext(): BlueprintHelperToolContext & {
  calls: Array<{ command: string; payload: Record<string, unknown> }>;
  sleeps: number[];
} {
  const calls: Array<{ command: string; payload: Record<string, unknown> }> = [];
  const sleeps: number[] = [];
  return {
    cwd: 'D:/UEProjects/Template/Plugins/BlueprintHelper',
    taskRunner: {} as BlueprintHelperToolContext['taskRunner'],
    bridge: {
      sendCommand: async (command: string, payload: Record<string, unknown>) => {
        calls.push({ command, payload });
        if (command === 'capture_editor_screenshot') {
          return response(command, {
            schema: 'BlueprintHelper.ScreenshotCapture.v1',
            screenshot_path: 'D:/UEProjects/Template/Saved/BlueprintHelper/Debug/Screenshots/test.png',
            relative_path: 'Screenshots/test.png',
            width: 1600,
            height: 900,
          });
        }
        return response(command, { status: 'completed' });
      },
    } as BlueprintHelperToolContext['bridge'],
    sleep: async (ms: number) => {
      sleeps.push(ms);
    },
    calls,
    sleeps,
  };
}

test('capture screenshot opens asset, focuses readable blueprint target, waits, then captures pure screenshot', async () => {
  const context = makeContext();

  const result = await executeCaptureScreenshot({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    graph_name: 'EventGraph',
    block_ref: 'BeginPlaySetup',
    node_ref: 'nodes[0]',
    label: 'bp_player_eventgraph',
    settle_delay_ms: 125,
  }, context);

  assert.equal(result.ok, true);
  assert.deepEqual(context.calls.map((call) => call.command), [
    'open_asset',
    'focus_blueprint_editor_target',
    'capture_editor_screenshot',
  ]);
  assert.deepEqual(context.calls[0]?.payload, {
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
  });
  assert.deepEqual(context.calls[1]?.payload, {
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    graph_name: 'EventGraph',
    block_ref: 'BeginPlaySetup',
    node_ref: 'nodes[0]',
  });
  assert.deepEqual(context.calls[2]?.payload, {
    target: 'active_window',
    label: 'bp_player_eventgraph',
  });
  assert.deepEqual(context.sleeps, [125]);
  assert.equal(result.data?.['schema'], 'BlueprintHelper.ScreenshotEvidence.v1');
});

test('capture screenshot skips focus command for asset-only request', async () => {
  const context = makeContext();

  const result = await executeCaptureScreenshot({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    settle_delay_ms: 0,
  }, context);

  assert.equal(result.ok, true);
  assert.deepEqual(context.calls.map((call) => call.command), [
    'open_asset',
    'capture_editor_screenshot',
  ]);
  assert.deepEqual(context.sleeps, []);
});
```

- [x] **Step B2: Add failure propagation test**

Append to `capture-screenshot-handler.test.ts`:

```ts
test('capture screenshot stops on open_asset failure', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> }> = [];
  const context = {
    cwd: 'D:/UEProjects/Template/Plugins/BlueprintHelper',
    taskRunner: {} as BlueprintHelperToolContext['taskRunner'],
    bridge: {
      sendCommand: async (command: string, payload: Record<string, unknown>) => {
        calls.push({ command, payload });
        return {
          request_id: `req_${command}`,
          success: false,
          error_code: 'asset_not_found',
          message: 'Asset was not found.',
        } satisfies BridgeResponse;
      },
    } as BlueprintHelperToolContext['bridge'],
  } satisfies BlueprintHelperToolContext;

  const result = await executeCaptureScreenshot({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Missing.Missing',
  }, context);

  assert.equal(result.ok, false);
  assert.equal(result.error?.code, 'asset_not_found');
  assert.equal(result.error?.stage, 'bridge');
  assert.deepEqual(calls.map((call) => call.command), ['open_asset']);
});
```

- [x] **Step B3: Run the failing handler tests**

Run:

```powershell
npm.cmd --prefix AgentFaceService\task-core run build
npm.cmd --prefix AgentFaceService\task-core run test:node
```

Expected before handler implementation:

- Build or test fails because `capture-screenshot-handler.ts` does not exist or dispatcher does not call it.

- [x] **Step B4: Implement the orchestration handler**

Create `AgentFaceService/task-core/src/tool-surface/bridge/screenshot/capture-screenshot-handler.ts`:

```ts
import { failureResult, normalizeToolResult, successRead, type ToolResultBase } from '../../../result/tool-result.js';
import type { BridgeResponse } from '../../../bridge/bridge-client.js';
import type { BlueprintHelperToolContext } from '../../types.js';
import { omitUndefined } from '../bridge-tool-result-utils.js';
import { CaptureScreenshotInputSchema, type CaptureScreenshotInput } from './capture-screenshot-schema.js';

const operation = 'blueprinthelper_capture_screenshot';

export async function executeCaptureScreenshot(
  input: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Promise<ToolResultBase> {
  const parsed = CaptureScreenshotInputSchema.safeParse(input);
  if (!parsed.success) {
    return failureResult(operation, {
      code: 'invalid_capture_screenshot_input',
      stage: 'parse_input',
      message: parsed.error.issues.map((issue) => `${issue.path.join('.')}: ${issue.message}`).join('; '),
      retryable: false,
      rollback_result: 'not_needed',
    });
  }

  const request = parsed.data;
  const target = {
    target_type: 'asset' as const,
    asset_path: request.asset_path,
    graph: request.graph_name,
  };

  const openResponse = await context.bridge.sendCommand('open_asset', {
    asset_path: request.asset_path,
  });
  if (!openResponse.success) {
    return failedBridgeStep(openResponse, 'open_asset', target);
  }

  let focusResponse: BridgeResponse | undefined;
  if (needsFocus(request)) {
    focusResponse = await context.bridge.sendCommand('focus_blueprint_editor_target', omitUndefined({
      asset_path: request.asset_path,
      graph_name: request.graph_name,
      block_ref: request.block_ref,
      node_ref: request.node_ref,
    }));
    if (!focusResponse.success) {
      return failedBridgeStep(focusResponse, 'focus_blueprint_editor_target', target);
    }
  }

  if (request.settle_delay_ms > 0) {
    if (context.sleep) {
      await context.sleep(request.settle_delay_ms);
    } else {
      await new Promise((resolve) => setTimeout(resolve, request.settle_delay_ms));
    }
  }

  const screenshotResponse = await context.bridge.sendCommand('capture_editor_screenshot', omitUndefined({
    target: request.capture_target,
    label: request.label,
  }));
  if (!screenshotResponse.success) {
    return failedBridgeStep(screenshotResponse, 'capture_editor_screenshot', target);
  }

  return successRead(operation, target, {
    schema: 'BlueprintHelper.ScreenshotEvidence.v1',
    asset_path: request.asset_path,
    graph_name: request.graph_name,
    block_ref: request.block_ref,
    node_ref: request.node_ref,
    steps: omitUndefined({
      open_asset: openResponse.result ?? {},
      focus_blueprint_editor_target: focusResponse?.result,
    }),
    screenshot: screenshotResponse.result ?? {},
  });
}

function needsFocus(request: CaptureScreenshotInput): boolean {
  return Boolean(request.graph_name || request.block_ref || request.node_ref);
}

function failedBridgeStep(
  response: BridgeResponse,
  step: string,
  target: { target_type: 'asset'; asset_path: string; graph?: string },
): ToolResultBase {
  return normalizeToolResult(response, operation, {
    target,
    error: {
      stage: 'bridge',
      code: response.error_code ?? `${step}_failed`,
      message: response.message ?? `${step} failed.`,
      retryable: false,
      rollback_result: 'not_needed',
    },
    data: {
      failed_step: step,
    },
  });
}
```

- [x] **Step B5: Route the tool through custom dispatcher branch**

Modify `AgentFaceService/task-core/src/tool-surface/bridge/bridge-tool-dispatcher.ts`:

```ts
import { executeCaptureScreenshot } from './screenshot/capture-screenshot-handler.js';
```

Add before `const bridgeCommand = bridgeCommandByToolName[toolName];`:

```ts
if (toolName === 'blueprinthelper_capture_screenshot') {
  return executeCaptureScreenshot(input, context);
}
```

Do not add `blueprinthelper_capture_screenshot` to `bridge-tool-command-map.ts`; that map is only for one-to-one generic Bridge tools.

- [x] **Step B6: Run handler gate**

Run:

```powershell
npm.cmd --prefix AgentFaceService\task-core run build
npm.cmd --prefix AgentFaceService\task-core run test:node
```

Expected:

- Build succeeds.
- Handler tests pass.
- No generic map assertion incorrectly requires `blueprinthelper_capture_screenshot`.

## 6. Task C: Agent-Facing Registry, CLI, Templates

**Files:**

- Modify: `AgentFaceService/task-core/src/tool-surface/registry/tool-metas.ts`
- Modify: `AgentFaceService/task-core/src/tests/tool-surface/tool-registry.contract.test.ts`
- Modify: `AgentFaceService/cli/src/cli/help.ts`
- Modify: `AgentFaceService/cli/src/tests/cli/cli-tool-bridge.test.ts`
- Create: `AgentFaceService/agent-guide/Templates/blueprinthelper_capture_screenshot_template.json`
- Modify: `AgentFaceService/agent-guide/Templates/SEMANTIC_INDEX.md`
- Modify: `AgentFaceService/agent-guide/Templates/INDEX.md`
- Modify: `AgentFaceService/docs/CLI_Tools_API_Reference.md`

- [x] **Step C1: Add registry contract expectations**

Modify `AgentFaceService/task-core/src/tests/tool-surface/tool-registry.contract.test.ts`:

```ts
assert.ok(expectedToolNames.includes('blueprinthelper_capture_screenshot'));
```

If the file has a sorted explicit array, insert:

```ts
'blueprinthelper_capture_screenshot',
```

Add a custom-tool assertion next to generic map checks:

```ts
assert.equal(bridgeCommandByToolName['blueprinthelper_capture_screenshot'], undefined);
assert.equal(Boolean(bridgeToolSchemas['blueprinthelper_capture_screenshot']), true);
```

- [x] **Step C2: Add tool metadata**

Modify `AgentFaceService/task-core/src/tool-surface/registry/tool-metas.ts`:

```ts
{
  name: 'blueprinthelper_capture_screenshot',
  description: 'Open an asset, optionally focus a readable Blueprint graph/block/node target, and capture editor screenshot evidence.',
  audience: 'default',
  risk: 'low',
}
```

Use the exact local metadata shape already used in this file. Do not add a legacy alias.

- [x] **Step C3: Add CLI test**

Append a test to `AgentFaceService/cli/src/tests/cli/cli-tool-bridge.test.ts` that invokes the built CLI with stdin JSON:

```ts
const input = JSON.stringify({
  schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
  asset_path: '/Game/Blueprints/BP_Player.BP_Player',
  graph_name: 'EventGraph',
  node_ref: 'nodes[0]',
  label: 'bp_player_eventgraph',
  settle_delay_ms: 0,
});

const result = await runCli([
  'blueprinthelper_capture_screenshot',
  '--stdin',
  '--format',
  'json',
], {
  stdin: input,
});

assert.equal(result.exitCode, 0);
assert.match(result.stdout, /BlueprintHelper\.ScreenshotEvidence\.v1/);
```

Adjust only the `runCli` helper call signature to match the existing file. Keep the asserted behavior the same.

- [x] **Step C4: Add template**

Create `AgentFaceService/agent-guide/Templates/blueprinthelper_capture_screenshot_template.json`:

```json
{
  "schema": "BlueprintHelper.CaptureScreenshotRequest.v1",
  "asset_path": "/Game/Blueprints/BP_Player.BP_Player",
  "graph_name": "EventGraph",
  "node_ref": "nodes[0]",
  "label": "bp_player_eventgraph",
  "capture_target": "active_window",
  "settle_delay_ms": 250
}
```

- [x] **Step C5: Update help and docs**

Update `AgentFaceService/cli/src/cli/help.ts` so `bh blueprinthelper_capture_screenshot --help` documents:

```text
blueprinthelper_capture_screenshot
  Opens an asset, optionally focuses graph_name + block_ref/node_ref, and captures editor screenshot evidence.
  Required: asset_path
  Optional: graph_name, block_ref, node_ref, label, capture_target, settle_delay_ms
```

Update `AgentFaceService/docs/CLI_Tools_API_Reference.md` with:

```md
### blueprinthelper_capture_screenshot

Captures editor screenshot evidence after opening an asset and optionally focusing a Blueprint graph/block/node target.

Input schema: `BlueprintHelper.CaptureScreenshotRequest.v1`

Required:
- `asset_path`

Optional:
- `graph_name`
- `block_ref`
- `node_ref`
- `label`
- `capture_target`: `active_window` or `active_viewport`
- `settle_delay_ms`: `0..5000`

The CLI tool is an orchestration tool. It sends `open_asset`, optional `focus_blueprint_editor_target`, and `capture_editor_screenshot`. The UE screenshot primitive does not receive asset or Blueprint location fields.
```

Update the template indexes to include `blueprinthelper_capture_screenshot_template.json`.

- [x] **Step C6: Run CLI and task-core gates**

Run:

```powershell
npm.cmd --prefix AgentFaceService\task-core run build
npm.cmd --prefix AgentFaceService\task-core run test:node
npm.cmd --prefix AgentFaceService\cli run build
npm.cmd --prefix AgentFaceService\cli run test:node
```

Expected:

- All TypeScript builds pass.
- All node tests pass.
- CLI help/test surfaces the new tool as a default Agent-facing tool.

## 7. Task D: UE Shared Types and Screenshot Capture Service

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Shared/Debug/BlueprintHelperScreenshotTypes.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Debug/BlueprintHelperScreenshotCaptureService.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Debug/BlueprintHelperScreenshotCaptureService.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics/BlueprintHelperScreenshotCaptureServiceTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/BlueprintHelper.Build.cs` only if compile proves `ImageWrapper` is required.

- [x] **Step D1: Add shared DTO header**

Create `BlueprintHelper/Source/BlueprintHelper/Public/Shared/Debug/BlueprintHelperScreenshotTypes.h`:

```cpp
#pragma once

#include "CoreMinimal.h"

enum class EBlueprintHelperScreenshotTarget : uint8
{
    ActiveWindow,
    ActiveViewport
};

struct FBlueprintHelperScreenshotCaptureRequest
{
    EBlueprintHelperScreenshotTarget Target = EBlueprintHelperScreenshotTarget::ActiveWindow;
    FString Label;
};

struct FBlueprintHelperScreenshotCaptureResult
{
    bool bSuccess = false;
    FString ErrorCode;
    FString Message;
    FString AbsolutePath;
    FString RelativePath;
    int32 Width = 0;
    int32 Height = 0;
    FString Target;
    FString CreatedAtUtc;
};

struct FBlueprintHelperEditorFocusRequest
{
    FString AssetPath;
    FString GraphName;
    FString BlockRef;
    FString NodeRef;
};

struct FBlueprintHelperEditorFocusResult
{
    bool bSuccess = false;
    FString ErrorCode;
    FString Message;
    FString AssetPath;
    FString GraphName;
    FString BlockRef;
    FString NodeRef;
};
```

- [x] **Step D2: Write service header**

Create `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Debug/BlueprintHelperScreenshotCaptureService.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shared/Debug/BlueprintHelperScreenshotTypes.h"

class FBlueprintHelperScreenshotCaptureService
{
public:
    FBlueprintHelperScreenshotCaptureResult Capture(const FBlueprintHelperScreenshotCaptureRequest& Request) const;

private:
    FBlueprintHelperScreenshotCaptureResult CaptureActiveWindow(const FBlueprintHelperScreenshotCaptureRequest& Request) const;
    FBlueprintHelperScreenshotCaptureResult CaptureActiveViewport(const FBlueprintHelperScreenshotCaptureRequest& Request) const;
    FString BuildScreenshotFilePath(const FBlueprintHelperScreenshotCaptureRequest& Request, const FString& Extension) const;
    FString SanitizeLabel(const FString& Label) const;
};
```

- [x] **Step D3: Implement active-window screenshot service**

Create `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Debug/BlueprintHelperScreenshotCaptureService.cpp`:

```cpp
#include "Systems/Debug/BlueprintHelperScreenshotCaptureService.h"

#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"
#include "Widgets/SWindow.h"

FBlueprintHelperScreenshotCaptureResult FBlueprintHelperScreenshotCaptureService::Capture(
    const FBlueprintHelperScreenshotCaptureRequest& Request) const
{
    return Request.Target == EBlueprintHelperScreenshotTarget::ActiveViewport
        ? CaptureActiveViewport(Request)
        : CaptureActiveWindow(Request);
}

FBlueprintHelperScreenshotCaptureResult FBlueprintHelperScreenshotCaptureService::CaptureActiveWindow(
    const FBlueprintHelperScreenshotCaptureRequest& Request) const
{
    FBlueprintHelperScreenshotCaptureResult Result;
    Result.Target = TEXT("active_window");
    Result.CreatedAtUtc = FDateTime::UtcNow().ToIso8601();

    if (!FSlateApplication::IsInitialized())
    {
        Result.ErrorCode = TEXT("slate_not_initialized");
        Result.Message = TEXT("Slate is not initialized.");
        return Result;
    }

    TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow();
    if (!Window.IsValid())
    {
        Result.ErrorCode = TEXT("active_window_not_found");
        Result.Message = TEXT("No active top-level editor window was available for screenshot capture.");
        return Result;
    }

    TArray<FColor> Colors;
    FIntVector Size;
    if (!FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(), Colors, Size))
    {
        Result.ErrorCode = TEXT("take_screenshot_failed");
        Result.Message = TEXT("FSlateApplication::TakeScreenshot failed.");
        return Result;
    }

    TArray64<uint8> PngBytes;
    FImageUtils::PNGCompressImageArray(Size.X, Size.Y, MakeArrayView(Colors), PngBytes);

    Result.AbsolutePath = BuildScreenshotFilePath(Request, TEXT("png"));
    Result.RelativePath = FPaths::ConvertRelativePathToFull(Result.AbsolutePath).Replace(
        *FBlueprintHelperDebugCaseStoreService::GetDebugRootDir(),
        TEXT(""));
    Result.RelativePath.RemoveFromStart(TEXT("/"));
    Result.RelativePath.RemoveFromStart(TEXT("\\"));

    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Result.AbsolutePath), true);
    if (!FFileHelper::SaveArrayToFile(TArrayView64<const uint8>(PngBytes), *Result.AbsolutePath))
    {
        Result.ErrorCode = TEXT("save_screenshot_failed");
        Result.Message = FString::Printf(TEXT("Failed to write screenshot to %s."), *Result.AbsolutePath);
        return Result;
    }

    Result.bSuccess = true;
    Result.Width = Size.X;
    Result.Height = Size.Y;
    Result.Message = TEXT("Screenshot captured.");
    return Result;
}

FBlueprintHelperScreenshotCaptureResult FBlueprintHelperScreenshotCaptureService::CaptureActiveViewport(
    const FBlueprintHelperScreenshotCaptureRequest& Request) const
{
    FBlueprintHelperScreenshotCaptureResult Result;
    Result.Target = TEXT("active_viewport");
    Result.CreatedAtUtc = FDateTime::UtcNow().ToIso8601();
    Result.ErrorCode = TEXT("active_viewport_not_implemented");
    Result.Message = TEXT("P0 captures active editor window. Active viewport remains behind this explicit adapter.");
    return Result;
}

FString FBlueprintHelperScreenshotCaptureService::BuildScreenshotFilePath(
    const FBlueprintHelperScreenshotCaptureRequest& Request,
    const FString& Extension) const
{
    const FString Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
    const FString Label = SanitizeLabel(Request.Label).IsEmpty() ? TEXT("editor") : SanitizeLabel(Request.Label);
    return FPaths::Combine(
        FBlueprintHelperDebugCaseStoreService::GetDebugRootDir(),
        TEXT("Screenshots"),
        FString::Printf(TEXT("%s_%s.%s"), *Timestamp, *Label, *Extension));
}

FString FBlueprintHelperScreenshotCaptureService::SanitizeLabel(const FString& Label) const
{
    FString Sanitized;
    for (const TCHAR Char : Label)
    {
        if (FChar::IsAlnum(Char) || Char == TEXT('_') || Char == TEXT('-') || Char == TEXT('.'))
        {
            Sanitized.AppendChar(Char);
        }
    }
    return Sanitized.Left(80);
}
```

If `MakeArrayView(Colors)` is not accepted by the local UE compiler, replace only that expression with:

```cpp
TArrayView64<const FColor> ColorView(Colors.GetData(), Colors.Num());
FImageUtils::PNGCompressImageArray(Size.X, Size.Y, ColorView, PngBytes);
```

- [x] **Step D4: Add screenshot service tests**

Create `BlueprintHelper/Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics/BlueprintHelperScreenshotCaptureServiceTests.cpp` with tests that can run headless for path/label behavior and skip active Slate capture when Slate is not initialized:

```cpp
#include "Misc/AutomationTest.h"
#include "Systems/Debug/BlueprintHelperScreenshotCaptureService.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlueprintHelperScreenshotCaptureServiceLabelTest,
    "BlueprintHelper.RuntimeDiagnostics.Screenshot.SanitizesLabelThroughCaptureFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperScreenshotCaptureServiceLabelTest::RunTest(const FString& Parameters)
{
    FBlueprintHelperScreenshotCaptureService Service;
    FBlueprintHelperScreenshotCaptureRequest Request;
    Request.Label = TEXT("../bad_label");

    const FBlueprintHelperScreenshotCaptureResult Result = Service.Capture(Request);
    if (!Result.bSuccess)
    {
        TestTrue(TEXT("failure has diagnostic code"), !Result.ErrorCode.IsEmpty());
        return true;
    }

    TestTrue(TEXT("screenshot path is under Screenshots"), Result.AbsolutePath.Contains(TEXT("Screenshots")));
    TestFalse(TEXT("screenshot path does not include traversal"), Result.AbsolutePath.Contains(TEXT("..")));
    TestTrue(TEXT("width positive"), Result.Width > 0);
    TestTrue(TEXT("height positive"), Result.Height > 0);
    return true;
}
```

- [x] **Step D5: Compile UE and add dependency only if required**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected:

- If compile passes, do not touch `BlueprintHelper.Build.cs`.
- If linker/compiler reports missing PNG/ImageWrapper symbols, add `"ImageWrapper"` to the relevant dependency list in `BlueprintHelper/Source/BlueprintHelper/BlueprintHelper.Build.cs`, rerun the same command, and record the exact failure/resolution in Debug.

## 8. Task E: UE Editor Focus Service

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Debug/BlueprintHelperEditorFocusService.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Debug/BlueprintHelperEditorFocusService.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics/BlueprintHelperEditorFocusServiceTests.cpp`

- [x] **Step E1: Write focus service header**

Create `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Debug/BlueprintHelperEditorFocusService.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shared/Debug/BlueprintHelperScreenshotTypes.h"

class FBlueprintHelperAssetBrowseService;
class UEdGraph;
class UEdGraphNode;

class FBlueprintHelperEditorFocusService
{
public:
    explicit FBlueprintHelperEditorFocusService(FBlueprintHelperAssetBrowseService& InAssetBrowseService);

    FBlueprintHelperEditorFocusResult FocusBlueprintEditorTarget(const FBlueprintHelperEditorFocusRequest& Request) const;

private:
    FBlueprintHelperAssetBrowseService& AssetBrowseService;
    FBlueprintHelperEditorFocusResult MakeFailure(
        const FBlueprintHelperEditorFocusRequest& Request,
        const FString& ErrorCode,
        const FString& Message) const;
    bool ResolveNodeForRequest(
        UEdGraph* Graph,
        const FBlueprintHelperEditorFocusRequest& Request,
        UEdGraphNode*& OutNode,
        FString& OutErrorCode,
        FString& OutMessage) const;
};
```

- [x] **Step E2: Implement focus service through existing boundaries**

Create `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Debug/BlueprintHelperEditorFocusService.cpp`:

```cpp
#include "Systems/Debug/BlueprintHelperEditorFocusService.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Systems/Debug/BlueprintHelperAssetBrowseService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteBlockScopedResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"

FBlueprintHelperEditorFocusService::FBlueprintHelperEditorFocusService(
    FBlueprintHelperAssetBrowseService& InAssetBrowseService)
    : AssetBrowseService(InAssetBrowseService)
{
}

FBlueprintHelperEditorFocusResult FBlueprintHelperEditorFocusService::FocusBlueprintEditorTarget(
    const FBlueprintHelperEditorFocusRequest& Request) const
{
    FBlueprintHelperEditorFocusResult Result;
    Result.AssetPath = Request.AssetPath;
    Result.GraphName = Request.GraphName;
    Result.BlockRef = Request.BlockRef;
    Result.NodeRef = Request.NodeRef;

    if (Request.AssetPath.IsEmpty())
    {
        return MakeFailure(Request, TEXT("missing_asset_path"), TEXT("asset_path is required."));
    }
    if ((Request.BlockRef.Len() > 0 || Request.NodeRef.Len() > 0) && Request.GraphName.IsEmpty())
    {
        return MakeFailure(Request, TEXT("missing_graph_name"), TEXT("graph_name is required when block_ref or node_ref is provided."));
    }

    const bool bOpened = AssetBrowseService.OpenAsset(Request.AssetPath);
    if (!bOpened)
    {
        return MakeFailure(
            Request,
            TEXT("open_asset_failed"),
            FString::Printf(TEXT("Failed to open asset %s."), *Request.AssetPath));
    }

    if (Request.GraphName.IsEmpty())
    {
        Result.bSuccess = true;
        Result.Message = TEXT("Asset editor focused.");
        return Result;
    }

    FBlueprintHelperGraphTarget Target;
    Target.BlueprintPath = Request.AssetPath;
    Target.GraphName = Request.GraphName;

    FBlueprintHelperDiagnosticSet Diagnostics;
    FBlueprintHelperGraphResolver GraphResolver;
    UEdGraph* Graph = GraphResolver.ResolveGraph(Target, Diagnostics);
    if (!Graph)
    {
        const FString Message = Diagnostics.Items.Num() > 0
            ? Diagnostics.Items.Last().Message
            : FString::Printf(TEXT("Failed to resolve graph %s."), *Request.GraphName);
        const FString Code = Diagnostics.Items.Num() > 0 && !Diagnostics.Items.Last().Code.IsEmpty()
            ? Diagnostics.Items.Last().Code
            : TEXT("graph_not_found");
        return MakeFailure(Request, Code, Message);
    }

    UObject* ObjectToFocus = Graph;
    if (!Request.BlockRef.IsEmpty() || !Request.NodeRef.IsEmpty())
    {
        UEdGraphNode* Node = nullptr;
        FString ErrorCode;
        FString Message;
        if (!ResolveNodeForRequest(Graph, Request, Node, ErrorCode, Message))
        {
            return MakeFailure(Request, ErrorCode, Message);
        }
        ObjectToFocus = Node;
    }

    FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(ObjectToFocus, false);
    Result.bSuccess = true;
    Result.Message = TEXT("Blueprint editor target focused.");
    return Result;
}

FBlueprintHelperEditorFocusResult FBlueprintHelperEditorFocusService::MakeFailure(
    const FBlueprintHelperEditorFocusRequest& Request,
    const FString& ErrorCode,
    const FString& Message) const
{
    FBlueprintHelperEditorFocusResult Result;
    Result.AssetPath = Request.AssetPath;
    Result.GraphName = Request.GraphName;
    Result.BlockRef = Request.BlockRef;
    Result.NodeRef = Request.NodeRef;
    Result.ErrorCode = ErrorCode;
    Result.Message = Message;
    return Result;
}

bool FBlueprintHelperEditorFocusService::ResolveNodeForRequest(
    UEdGraph* Graph,
    const FBlueprintHelperEditorFocusRequest& Request,
    UEdGraphNode*& OutNode,
    FString& OutErrorCode,
    FString& OutMessage) const
{
    FBlueprintHelperLogicJsonPathService PathService;
    FBlueprintHelperPatchResolveError ResolveError;

    if (!Request.BlockRef.IsEmpty())
    {
        FBlueprintHelperBlockIdService BlockIdService;
        FBlueprintHelperGraphWriteAnchorRef Anchor;
        Anchor.BlockId = BlockIdService.MakeFullBlockId(Request.GraphName, Request.BlockRef);
        Anchor.NodeRef = Request.NodeRef;
        Anchor.GroupEntryNodePath = Request.NodeRef.IsEmpty() ? TEXT("nodes[0]") : FString();

        if (FBlueprintHelperGraphWriteBlockScopedResolver::ResolveNode(PathService, Graph, Anchor, OutNode, ResolveError))
        {
            return true;
        }

        OutErrorCode = ResolveError.Code.IsEmpty() ? TEXT("block_node_not_found") : ResolveError.Code;
        OutMessage = ResolveError.Message.IsEmpty()
            ? FString::Printf(TEXT("Failed to resolve block_ref %s."), *Request.BlockRef)
            : ResolveError.Message;
        return false;
    }

    if (PathService.ResolveNode(Graph, Request.NodeRef, FString(), OutNode, ResolveError))
    {
        return true;
    }

    OutErrorCode = ResolveError.Code.IsEmpty() ? TEXT("node_not_found") : ResolveError.Code;
    OutMessage = ResolveError.Message.IsEmpty()
        ? FString::Printf(TEXT("Failed to resolve node_ref %s."), *Request.NodeRef)
        : ResolveError.Message;
    return false;
}
```

The implementation above is the required final shape for this service task. It may be adjusted for exact local include paths or compiler signatures, but the behavior must remain:

- asset open succeeds,
- graph resolution succeeds when `GraphName` is non-empty,
- `BlockRef` resolves through `FBlueprintHelperBlockIdService::MakeFullBlockId(GraphName, BlockRef)` plus `FBlueprintHelperGraphWriteBlockScopedResolver`,
- `NodeRef` without `BlockRef` resolves through `FBlueprintHelperLogicJsonPathService::ResolveNode`,
- the editor has been asked to focus/jump to the resolved graph or node.

If only `asset_path` is supplied, focus service opens/focuses the asset and returns success without graph resolution.

- [x] **Step E3: Add focus service tests**

Create `BlueprintHelper/Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics/BlueprintHelperEditorFocusServiceTests.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "Shared/Debug/BlueprintHelperScreenshotTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlueprintHelperEditorFocusRequestValidationTest,
    "BlueprintHelper.RuntimeDiagnostics.Screenshot.FocusRequestValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEditorFocusRequestValidationTest::RunTest(const FString& Parameters)
{
    FBlueprintHelperEditorFocusRequest Request;
    Request.AssetPath = TEXT("/Game/Blueprints/BP_Player.BP_Player");
    Request.NodeRef = TEXT("nodes[0]");

    TestTrue(TEXT("node_ref without graph_name is invalid by route/service validation"), Request.GraphName.IsEmpty());
    return true;
}
```

Extend this test after route parsing exists so it calls the actual validation/route helper and asserts `missing_graph_name`. Do not close this task with only the DTO smoke assertion.

- [x] **Step E4: Compile UE after focus implementation**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected:

- Build succeeds.
- No anonymous namespace was introduced.
- Focus service stays outside UI widget code.

## 9. Task F: UE Bridge Routes and Validator

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/Routes/BlueprintHelperScreenshotBridgeRoutes.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Routes/BlueprintHelperScreenshotBridgeRoutes.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/BlueprintHelper.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/BlueprintHelper.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/BlueprintHelperBridgeRouter.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperRequestValidator.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Bridge/BlueprintHelperBridgeRoutePlannerTests.cpp`

- [x] **Step F1: Add route adapter header**

Create `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/Routes/BlueprintHelperScreenshotBridgeRoutes.h`:

```cpp
#pragma once

#include "CoreMinimal.h"

class FBlueprintHelperBridgeResponse;
class FBlueprintHelperEditorFocusService;
class FBlueprintHelperScreenshotCaptureService;
struct FJsonObject;

class FBlueprintHelperScreenshotBridgeRoutes
{
public:
    FBlueprintHelperScreenshotBridgeRoutes(
        FBlueprintHelperEditorFocusService& InFocusService,
        FBlueprintHelperScreenshotCaptureService& InScreenshotService);

    FBlueprintHelperBridgeResponse HandleFocusBlueprintEditorTarget(const FJsonObject& Payload) const;
    FBlueprintHelperBridgeResponse HandleCaptureEditorScreenshot(const FJsonObject& Payload) const;

private:
    FBlueprintHelperEditorFocusService& FocusService;
    FBlueprintHelperScreenshotCaptureService& ScreenshotService;
};
```

- [x] **Step F2: Implement route adapter serialization**

Create `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Routes/BlueprintHelperScreenshotBridgeRoutes.cpp` with these responsibilities:

```cpp
#include "Entry/Bridge/Routes/BlueprintHelperScreenshotBridgeRoutes.h"

#include "Dom/JsonObject.h"
#include "Entry/Bridge/BlueprintHelperBridgeProtocol.h"
#include "Shared/Debug/BlueprintHelperScreenshotTypes.h"
#include "Systems/Debug/BlueprintHelperEditorFocusService.h"
#include "Systems/Debug/BlueprintHelperScreenshotCaptureService.h"

FBlueprintHelperScreenshotBridgeRoutes::FBlueprintHelperScreenshotBridgeRoutes(
    FBlueprintHelperEditorFocusService& InFocusService,
    FBlueprintHelperScreenshotCaptureService& InScreenshotService)
    : FocusService(InFocusService)
    , ScreenshotService(InScreenshotService)
{
}
```

Implement `HandleFocusBlueprintEditorTarget` to:

- read string fields `asset_path`, `graph_name`, `block_ref`, `node_ref`,
- call `FocusService.FocusBlueprintEditorTarget`,
- return success JSON:

```json
{
  "schema": "BlueprintHelper.EditorFocus.v1",
  "status": "completed",
  "asset_path": "...",
  "graph_name": "...",
  "block_ref": "...",
  "node_ref": "..."
}
```

- return failure with `Result.ErrorCode` and `Result.Message`.

Implement `HandleCaptureEditorScreenshot` to:

- read `target`, where missing or `active_window` maps to `EBlueprintHelperScreenshotTarget::ActiveWindow`,
- reject unsupported target values with `invalid_capture_target`,
- read optional `label`,
- call `ScreenshotService.Capture`,
- return success JSON:

```json
{
  "schema": "BlueprintHelper.ScreenshotCapture.v1",
  "status": "completed",
  "screenshot_path": "...",
  "relative_path": "...",
  "width": 1600,
  "height": 900,
  "target": "active_window",
  "created_at_utc": "2026-06-02T00:00:00Z"
}
```

- return failure with the service error code/message.

- [x] **Step F3: Wire routes through module and router**

Modify `BlueprintHelper/Source/BlueprintHelper/Public/Entry/BlueprintHelper.h`:

```cpp
TUniquePtr<FBlueprintHelperScreenshotCaptureService> ScreenshotCaptureService;
TUniquePtr<FBlueprintHelperEditorFocusService> EditorFocusService;
```

Modify `BlueprintHelper/Source/BlueprintHelper/Private/Entry/BlueprintHelper.cpp` where debug services are created:

```cpp
ScreenshotCaptureService = MakeUnique<FBlueprintHelperScreenshotCaptureService>();
EditorFocusService = MakeUnique<FBlueprintHelperEditorFocusService>(*AssetBrowseService);
```

Inject both into Bridge router/route adapter using the router's existing constructor or setter pattern. Follow the current router dependency-injection style; do not reach through globals from route handlers.

- [x] **Step F4: Add router dispatch**

Modify `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp` with command branches matching the current `BLUEPRINTHELPER_ROUTE` pattern:

```cpp
BLUEPRINTHELPER_ROUTE(TEXT("focus_blueprint_editor_target"), ScreenshotRoutes.HandleFocusBlueprintEditorTarget(Payload));
BLUEPRINTHELPER_ROUTE(TEXT("capture_editor_screenshot"), ScreenshotRoutes.HandleCaptureEditorScreenshot(Payload));
```

Use the exact local macro/dispatch style already present in the file.

- [x] **Step F5: Add route planner mappings**

Modify `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.cpp`:

```cpp
{ TEXT("focus_blueprint_editor_target"), EBlueprintHelperBridgeCommandCluster::Debug },
{ TEXT("capture_editor_screenshot"), EBlueprintHelperBridgeCommandCluster::Debug },
```

If `Debug` is not an available cluster enum for this table, add a narrowly named `RuntimeDiagnostics` or `ScreenshotEvidence` cluster consistently in:

- `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Bridge/BlueprintHelperBridgeRoutePlannerTests.cpp`

- [x] **Step F6: Add validator cases**

Modify `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperRequestValidator.cpp`:

- `focus_blueprint_editor_target`:
  - requires string `asset_path`,
  - rejects `block_ref` or `node_ref` without `graph_name`,
  - accepts optional string `graph_name`, `block_ref`, `node_ref`.
- `capture_editor_screenshot`:
  - accepts optional string `target`,
  - accepts optional string `label`,
  - rejects unknown `target` values outside `active_window` and `active_viewport`.

Keep both commands read/debug classified. They must not require write auth.

- [x] **Step F7: Add route planner tests**

Modify `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Bridge/BlueprintHelperBridgeRoutePlannerTests.cpp`:

```cpp
TestEqual(TEXT("focus command cluster"), ResolveCluster(TEXT("focus_blueprint_editor_target")), EBlueprintHelperBridgeCommandCluster::Debug);
TestEqual(TEXT("screenshot command cluster"), ResolveCluster(TEXT("capture_editor_screenshot")), EBlueprintHelperBridgeCommandCluster::Debug);
```

Use the actual helper names in the existing test file. The assertions must prove both commands are routed and remain in a non-write cluster.

- [x] **Step F8: Compile and run focused UE automation**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Then run focused automation:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.RuntimeDiagnostics.Screenshot;Automation RunTests BlueprintHelper.Bridge;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\ScreenshotEvidence_RuntimeDiagnostics_20260602_001"
```

Expected:

- Build succeeds.
- Automation report exists at `D:\UEProjects\Template\Saved\Automation\ScreenshotEvidence_RuntimeDiagnostics_20260602_001\index.json`.
- Screenshot route tests pass.
- If NullRHI cannot perform active-window capture, service test must assert diagnostic failure, and live E2E remains the actual screenshot proof.

## 10. Task G: End-to-End CLI Evidence

**Files:**

- Modify: `Debug/BlueprintHelper_EditorScreenshotEvidence_Brainstorm_20260601.md`
- Create: `BlueprintHelper/Develop/Report/BlueprintHelper_CLI_ScreenshotEvidence_Report_20260602_CN.md`

- [x] **Step G1: Build all local surfaces**

Run:

```powershell
npm.cmd --prefix AgentFaceService\task-core run build
npm.cmd --prefix AgentFaceService\task-core run test:node
npm.cmd --prefix AgentFaceService\cli run build
npm.cmd --prefix AgentFaceService\cli run test:node
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected:

- All commands exit `0`.

- [x] **Step G2: Start or reuse a live UE editor with BlueprintHelper Bridge**

If no live editor is already running, start:

```powershell
Start-Process -FilePath "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList "`"D:\UEProjects\Template\Template.uproject`"" -WindowStyle Hidden
```

Then verify Bridge:

```powershell
.\bh.cmd bridge ping
```

Expected:

- Bridge ping returns success.
- Do not continue to screenshot E2E if Bridge ping fails; record the ping failure in Debug.

- [x] **Step G3: Discover a real Blueprint asset for screenshot E2E**

Run:

```powershell
@'
{
  "schema": "BlueprintHelper.FindAssetsRequest.v1",
  "query": "BP_",
  "asset_types": ["blueprint"],
  "limit": 10
}
'@ | .\bh.cmd blueprinthelper_find_assets --stdin --format full --select status,artifacts.full_result
```

Expected:

- At least one returned `asset_path` with `asset_type` or class indicating Blueprint.
- Use the first returned Blueprint asset for Step G4.
- If zero Blueprint assets are returned, create a Debug blocker entry and do not fake an E2E pass.

- [x] **Step G4: Run screenshot CLI with asset-only input**

Replace `/Game/Blueprints/BP_Player.BP_Player` below with the asset path discovered in Step G3:

```powershell
@'
{
  "schema": "BlueprintHelper.CaptureScreenshotRequest.v1",
  "asset_path": "/Game/Blueprints/BP_Player.BP_Player",
  "label": "e2e_asset_only",
  "capture_target": "active_window",
  "settle_delay_ms": 250
}
'@ | .\bh.cmd blueprinthelper_capture_screenshot --stdin --format full --select status,artifacts.full_result
```

Expected:

- CLI status is completed.
- Result contains `schema: BlueprintHelper.ScreenshotEvidence.v1`.
- Result contains `screenshot.screenshot_path`.
- PNG file exists at the returned path and has non-zero byte length.

- [x] **Step G5: Run screenshot CLI with graph/node input**

Use a Blueprint asset with an `EventGraph`. If Step G3 asset does not have `EventGraph`, first run `blueprinthelper_read_context` for that asset and choose an existing graph name from the returned context.

```powershell
@'
{
  "schema": "BlueprintHelper.CaptureScreenshotRequest.v1",
  "asset_path": "/Game/Blueprints/BP_Player.BP_Player",
  "graph_name": "EventGraph",
  "node_ref": "nodes[0]",
  "label": "e2e_eventgraph_node0",
  "capture_target": "active_window",
  "settle_delay_ms": 250
}
'@ | .\bh.cmd blueprinthelper_capture_screenshot --stdin --format full --select status,artifacts.full_result
```

Expected:

- CLI status is completed.
- Bridge command trace or handler test evidence shows the sequence `open_asset -> focus_blueprint_editor_target -> capture_editor_screenshot`.
- Returned screenshot path exists and is non-empty.
- Debug doc records the absolute screenshot path.

- [x] **Step G6: Inspect screenshot dimensions**

Run:

```powershell
Add-Type -AssemblyName System.Drawing
$path = "D:\UEProjects\Template\Saved\BlueprintHelper\Debug\Screenshots\e2e_eventgraph_node0.png"
$img = [System.Drawing.Image]::FromFile($path)
"$($img.Width)x$($img.Height)"
$img.Dispose()
```

Expected:

- Printed dimensions are both greater than `0`.
- If the file name includes a timestamp, use the exact returned `screenshot.screenshot_path` from Step G5.

## 11. Task H: Documentation, Debug, and Report Closure

**Files:**

- Modify: `Debug/BlueprintHelper_EditorScreenshotEvidence_Brainstorm_20260601.md`
- Create: `BlueprintHelper/Develop/Report/BlueprintHelper_CLI_ScreenshotEvidence_Report_20260602_CN.md`

- [x] **Step H1: Update Debug evidence**

Append to `Debug/BlueprintHelper_EditorScreenshotEvidence_Brainstorm_20260601.md`:

```md
## Implementation Evidence - 2026-06-02

- TypeScript build/test:
  - `npm.cmd --prefix AgentFaceService\task-core run build`: PASS/FAIL with exact result.
  - `npm.cmd --prefix AgentFaceService\task-core run test:node`: PASS/FAIL with exact result.
  - `npm.cmd --prefix AgentFaceService\cli run build`: PASS/FAIL with exact result.
  - `npm.cmd --prefix AgentFaceService\cli run test:node`: PASS/FAIL with exact result.
- UE build:
  - command and PASS/FAIL result.
- UE automation:
  - report path.
  - pass/fail summary.
- Live E2E:
  - asset_path used.
  - graph_name/block_ref/node_ref used if any.
  - returned screenshot path.
  - file size and image dimensions.
- Boundary confirmation:
  - CLI/task-core orchestrates open/focus/capture.
  - UE screenshot service captures only current editor state.
```

- [x] **Step H2: Create implementation Report**

Create `BlueprintHelper/Develop/Report/BlueprintHelper_CLI_ScreenshotEvidence_Report_20260602_CN.md`:

```md
# BlueprintHelper CLI Screenshot Evidence Report - 2026-06-02

## 改动原因

新增面向 Agent 的 CLI 截图证据工具，用于在真实编辑器内打开资产、定位蓝图上下文并保存截图，作为循环开发中的可复查证据。

## 改动范围

- AgentFaceService task-core:
  - 新增 `blueprinthelper_capture_screenshot` schema 与 orchestration handler。
  - 注册 tool metadata 与 schema。
- AgentFaceService CLI/docs/templates:
  - 新增 help、template、CLI API reference。
- BlueprintHelper UE:
  - 新增 screenshot capture service。
  - 新增 editor focus service。
  - 新增 Bridge routes、route planner、validator。
- Tests:
  - 新增 task-core/CLI node tests。
  - 新增 UE automation tests。
  - 完成 live editor E2E 截图验证。

## 过程

- 先以 schema/handler tests 锁定 Agent-facing contract。
- 再以 UE service/route tests 锁定 Bridge primitive 边界。
- 最后用真实 UE 编辑器验证返回的截图文件存在且尺寸有效。

## 结果

- `blueprinthelper_capture_screenshot` 可以通过 CLI 打开资产、可选定位蓝图图表/节点，并返回截图证据路径。
- UE 端截图 primitive 不包含资产打开和蓝图定位逻辑。
- 截图证据落入 DebugBundle root 下的 `Screenshots` 目录。

## 验证

- 填入本次执行的 build/test/E2E 命令和结果。
```

- [x] **Step H3: Final readonly audits**

Dispatch final audit workers:

1. Small readonly worker:
   - Check `tool-metas.ts`, `bridge-tool-schemas.ts`, `bridge-tool-dispatcher.ts`, CLI help, and templates.
   - Confirm `blueprinthelper_capture_screenshot` is default Agent-facing and has no legacy alias.
2. Small readonly worker:
   - Check new C++ files.
   - Confirm no anonymous namespace.
   - Confirm screenshot service does not parse `asset_path`, `graph_name`, `block_ref`, or `node_ref`.
3. Medium readonly worker:
   - Check Debug/Report evidence, node tests, UE automation report path, and live E2E screenshot file metadata.
   - Confirm UE 5.3-5.6 compatibility notes remain behind adapter seams.

- [x] **Step H4: Manual commit guidance only**

Do not run git commands automatically. At the end, provide manual commands with only this task's files:

```powershell
git status --short
git add -- AgentFaceService/task-core/src/tool-surface/bridge/screenshot/capture-screenshot-schema.ts AgentFaceService/task-core/src/tool-surface/bridge/screenshot/capture-screenshot-schema.test.ts AgentFaceService/task-core/src/tool-surface/bridge/screenshot/capture-screenshot-handler.ts AgentFaceService/task-core/src/tool-surface/bridge/screenshot/capture-screenshot-handler.test.ts AgentFaceService/task-core/src/tool-surface/bridge/bridge-tool-schemas.ts AgentFaceService/task-core/src/tool-surface/bridge/bridge-tool-dispatcher.ts AgentFaceService/task-core/src/tool-surface/registry/tool-metas.ts AgentFaceService/task-core/src/tests/tool-surface/tool-registry.contract.test.ts AgentFaceService/cli/src/cli/help.ts AgentFaceService/cli/src/tests/cli/cli-tool-bridge.test.ts AgentFaceService/agent-guide/Templates/blueprinthelper_capture_screenshot_template.json AgentFaceService/agent-guide/Templates/SEMANTIC_INDEX.md AgentFaceService/agent-guide/Templates/INDEX.md AgentFaceService/docs/CLI_Tools_API_Reference.md BlueprintHelper/Source/BlueprintHelper/Public/Shared/Debug/BlueprintHelperScreenshotTypes.h BlueprintHelper/Source/BlueprintHelper/Public/Systems/Debug/BlueprintHelperScreenshotCaptureService.h BlueprintHelper/Source/BlueprintHelper/Private/Systems/Debug/BlueprintHelperScreenshotCaptureService.cpp BlueprintHelper/Source/BlueprintHelper/Public/Systems/Debug/BlueprintHelperEditorFocusService.h BlueprintHelper/Source/BlueprintHelper/Private/Systems/Debug/BlueprintHelperEditorFocusService.cpp BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/Routes/BlueprintHelperScreenshotBridgeRoutes.h BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Routes/BlueprintHelperScreenshotBridgeRoutes.cpp BlueprintHelper/Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics/BlueprintHelperScreenshotCaptureServiceTests.cpp BlueprintHelper/Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics/BlueprintHelperEditorFocusServiceTests.cpp BlueprintHelper/Source/BlueprintHelper/Private/Tests/Bridge/BlueprintHelperBridgeRoutePlannerTests.cpp BlueprintHelper/Develop/Report/BlueprintHelper_CLI_ScreenshotEvidence_Report_20260602_CN.md Debug/BlueprintHelper_EditorScreenshotEvidence_Brainstorm_20260601.md
git commit -m "新增内容：添加 CLI 截图证据工具"
```

If `BlueprintHelper.Build.cs`, router, validator, or module lifecycle files changed during implementation, add only the touched files from this task before committing.

## 12. Verification Matrix

Run these before claiming implementation complete:

```powershell
npm.cmd --prefix AgentFaceService\task-core run build
npm.cmd --prefix AgentFaceService\task-core run test:node
npm.cmd --prefix AgentFaceService\cli run build
npm.cmd --prefix AgentFaceService\cli run test:node
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.RuntimeDiagnostics.Screenshot;Automation RunTests BlueprintHelper.Bridge;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\ScreenshotEvidence_RuntimeDiagnostics_20260602_001"
.\bh.cmd bridge ping
```

Then run the live editor screenshot CLI commands from Task G.

Completion criteria:

- `blueprinthelper_capture_screenshot` is listed as a default Agent-facing tool.
- Schema rejects `block_ref`/`node_ref` without `graph_name`.
- Handler test proves screenshot primitive payload contains only `target` and `label`.
- UE route planner knows `focus_blueprint_editor_target` and `capture_editor_screenshot`.
- UE request validator treats both commands as read/debug, not write.
- Live E2E creates a PNG under the configured Debug root.
- Debug doc contains the real screenshot path, size, and image dimensions.
- Report doc exists and records reason, process, result, and code-change scope.

## 2026-06-02 execution update: graph-only multi-PNG

The plan was corrected after visual readback of the first graph/node E2E screenshot. The first result proved that opening and focusing the asset worked, but it still captured the full editor window and included too much EventGraph context. The accepted requirement is now:

- Asset-only screenshot: capture the current editor Window.
- Graph/Event/Node screenshot: capture only the Graph panel area.
- Event screenshot: cover the whole event logic and return multiple independent PNG files when one PNG cannot cover all selected nodes.

Implementation delta:

- Added `capture_focused_graph_screenshot` as the graph-only UE primitive.
- Kept `capture_editor_screenshot` as the asset-window primitive.
- Updated task-core handler so `graph_name`, `block_ref`, or `node_ref` routes to graph-only capture, while asset-only routes to window capture.
- Added `screenshots[]`, `screenshot_count`, `capture_scope`, and first-screenshot alias fields to the CLI result.
- Added `debug.screenshot.graph_max_nodes_per_image`, default `8`, for independent PNG split behavior.
- Fixed focus/capture coupling by storing the semantic graph selection in `FBlueprintHelperEditorFocusService` and passing it into `FBlueprintHelperScreenshotCaptureService`.

Evidence:

- Asset-only result remains window capture:
  - `D:\UEProjects\Template\Saved\BlueprintHelper\Cli\cli_graph_only_asset_20260602_001\cli_1780335087667\result.json`
- Node-target result is graph-only and no longer falls back to the full EventGraph:
  - `D:\UEProjects\Template\Saved\BlueprintHelper\Cli\cli_graph_only_node_20260602_003\cli_1780335514113\result.json`
  - `selected_node_count=7`, `screenshot_count=1`, `target=graph_panel`
  - PNG: `D:\UEProjects\Template\Saved\BlueprintHelper\Debug\Screenshots\cli_e2e_eventgraph_node0_graph_focusstate_001_20260601_173834_yP9_jEXzu02T2ImvpAo-2Q.png`
- EventGraph result returns multiple independent PNG files:
  - `D:\UEProjects\Template\Saved\BlueprintHelper\Cli\cli_graph_only_event_20260602_002\cli_1780335577296\result.json`
  - `selected_node_count=27`, `screenshot_count=4`, `tile_count=4`, `target=graph_panel`

Verification:

- `npm.cmd --prefix AgentFaceService\task-core run build`: passed.
- `npm.cmd --prefix AgentFaceService\task-core run test:node`: 327 passed, 0 failed.
- `npm.cmd --prefix AgentFaceService\cli run build`: passed.
- `npm.cmd --prefix AgentFaceService\cli run test:node`: 51 passed, 0 failed.
- UE build: `TemplateEditor Win64 Development`, passed.
- UE automation:
  - `D:\UEProjects\Template\Saved\Automation\ScreenshotEvidence_GraphOnly_20260602_002\index.json`: 4 succeeded, 0 failed.
  - `D:\UEProjects\Template\Saved\Automation\ScreenshotEvidence_Router_20260602_001\index.json`: 2 succeeded, 0 failed.
  - `D:\UEProjects\Template\Saved\Automation\ScreenshotEvidence_Settings_20260602_001\index.json`: 8 succeeded, 0 failed.
- Visual readback was performed for the asset-only PNG, fixed node graph-only PNG, and first/last EventGraph multi-PNG tiles. The graph PNGs are GraphPanel area captures with GraphPanel navigation/breadcrumb/zoom overlays visible; they are not full editor window captures and not tight selected-node crops.
