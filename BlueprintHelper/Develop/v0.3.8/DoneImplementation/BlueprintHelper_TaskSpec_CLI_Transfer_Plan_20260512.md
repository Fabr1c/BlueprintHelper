# BlueprintHelper TaskSpec CLI Transfer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a TaskSpec-first CLI entry as the only supported Agent transport for token-sensitive workflows while preserving the existing TaskSpec, Python orchestration, TaskPlan preview, Bridge execution, and UE Task Runtime gates.

**Architecture:** Extract the current task flow into `ClaudePlugin/task-core` and expose CLI as the only supported Agent entry. Deprecated MCP wrappers and the CLI call the same runner, so CLI cannot bypass Python compilation or preview. CLI stdout defaults to a compact summary, while full journals and large payloads are written to local artifacts and returned by path.

**Tech Stack:** Node.js, TypeScript ESM, existing `BridgeClient` TCP framed JSON protocol, existing Python `blueprinthelper_task` orchestrator, existing TaskSpec and TaskPlan zod schemas, existing node and Python regression tests.

---

## Closure Note - 2026-05-12

This plan has been implemented for the v0.3.8 documentation archive.

- `ClaudePlugin/task-core` now owns the shared Bridge client, TaskSpec schemas, Python orchestration, TaskSpec runner, and normalized tool result helpers.
- `ClaudePlugin/mcp` is deprecated transport only; task tools call `createTaskSpecRunner` from `@blueprinthelper/task-core`.
- `ClaudePlugin/cli` is a separate CLI transport with compact `BlueprintHelper.CliResult.v1` output, artifact directory support, and CLI regression tests.
- The original TaskSpec-first flow is preserved: TaskSpec -> Python compiler -> TaskPlan -> Bridge preview/execute -> UE Task Runtime.
- Deprecated MCP wrappers and CLI both keep the Python orchestration layer in the critical path; CLI does not submit raw TaskPlan or bypass preview.

Verification already run in this workspace:

- `ClaudePlugin/mcp npm test`
- `ClaudePlugin/cli npm test`
- CLI help smoke
- CLI no-Bridge smoke

The checkbox body below is kept as the original execution record.

## Scope

This plan changes the supported Agent entry and output contract. It does not replace `BlueprintHelper.TaskSpec.v1`, does not let Agents submit `TaskPlan` directly, and does not bypass `preview_task_plan` before writes.

Target chain:

```text
Agent -> BlueprintHelper CLI -> shared TaskSpec runner -> Python Task compiler -> TaskPlan -> Bridge -> UE Task Runtime
```

Deprecated compatibility chain:

```text
Deprecated MCP wrapper -> shared TaskSpec runner -> Python Task compiler -> TaskPlan -> Bridge -> UE Task Runtime
```

## Output Contract

CLI stdout is reserved for one JSON object. Logs go to stderr. Default output is `summary`, not the full tool result.

Default summary shape:

```json
{
  "ok": true,
  "schema": "BlueprintHelper.CliResult.v1",
  "operation": "task.preview",
  "status": "preview_passed",
  "task_run_id": null,
  "preview_id": "preview_123",
  "summary": {
    "target_assets": ["/Game/BP_Player"],
    "task_type": "edit_blueprint_graph",
    "planned_steps": 2,
    "warnings": 0,
    "errors": 0
  },
  "artifacts": {
    "full_result": "Saved/BlueprintHelper/Cli/preview_123/result.json",
    "task_plan": "Saved/BlueprintHelper/Cli/preview_123/task_plan.json"
  }
}
```

CLI output modes:

```text
--format summary  default compact result
--format json     normalized full JSON in stdout
--format full     full JSON in stdout plus artifacts
--max-bytes N     fail closed if stdout would exceed N bytes
```

Artifact root:

```text
--artifact-dir <path>
BPH_CLI_ARTIFACT_DIR
<cwd>/Saved/BlueprintHelper/Cli
```

The order above is the resolution order. The CLI must create the directory if missing.

## File Structure

Create:

- `ClaudePlugin/task-core/src/task/service/task-spec-runner.ts`
- `ClaudePlugin/cli/src/cli/index.ts`
- `ClaudePlugin/cli/src/cli/run.ts`
- `ClaudePlugin/cli/src/cli/output.ts`
- `ClaudePlugin/cli/src/cli/artifacts.ts`
- `ClaudePlugin/cli/src/tests/cli/cli-output.test.ts`
- `ClaudePlugin/cli/src/tests/cli/cli-task-runner.test.ts`
- `ClaudePlugin/task-core/package.json`
- `ClaudePlugin/task-core/tsconfig.json`
- `ClaudePlugin/cli/package.json`
- `ClaudePlugin/cli/tsconfig.json`

Modify:

- `ClaudePlugin/mcp/src/mcp/tools/task-tools.ts`
- `ClaudePlugin/mcp/package.json`
- `BlueprintHelper/Docs/Install_CLI_QuickStart.md`
- `BlueprintHelper/Docs/CLI_Tools_API_Reference.md`

Do not modify:

- `ClaudePlugin/task-core/python/blueprinthelper_task/**` except when a failing parity test proves the Python compiler needs a bug fix.
- UE Bridge route implementations for this transport change.

---

### Task 1: Extract Shared TaskSpec Runner

**Files:**

- Create: `ClaudePlugin/task-core/src/task/service/task-spec-runner.ts`
- Modify: `ClaudePlugin/mcp/src/mcp/tools/task-tools.ts`
- Test: `ClaudePlugin/mcp/src/tests/mcp/task-tools.regression.test.ts`

- [ ] **Step 1: Add a failing parity test for MCP task tools**

Add this test near the existing preview and execute tests in `ClaudePlugin/mcp/src/tests/mcp/task-tools.regression.test.ts`:

```ts
test('task tools call shared TaskSpec runner without changing Bridge command order', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const tools = registerWithBridge(async (command, payload): Promise<BridgeResponse> => {
    calls.push({ command, payload });
    if (command === 'preview_task_plan') {
      return {
        request_id: 'preview',
        success: true,
        result: {
          status: 'dry_run',
          data: {
            schema: 'BlueprintHelper.TaskRuntimeResult.v1',
            dry_run: { can_execute: true, warnings: [], conflicts: [], errors: [] },
            steps: []
          }
        }
      };
    }
    return {
      request_id: 'execute',
      success: true,
      result: {
        status: 'applied',
        data: {
          schema: 'BlueprintHelper.TaskRuntimeResult.v1',
          task_run_id: 'task_shared_runner_001',
          steps: []
        }
      }
    };
  });

  const executeTool = tools.get('blueprinthelper_execute_task');
  assert.ok(executeTool);

  const result = await invokeTool(executeTool, { task_spec: makeTaskSpec() });

  assert.equal(result.isError, false);
  assert.deepEqual(calls.map((call) => call.command), ['preview_task_plan', 'execute_task_plan']);
  assert.equal(
    ((result.structuredContent?.data as Record<string, unknown>).task as Record<string, unknown>).task_run_id,
    'task_shared_runner_001'
  );
});
```

- [ ] **Step 2: Run the failing test**

Run:

```powershell
cd G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\ClaudePlugin\mcp
npm run build
node scripts/run-node-tests.mjs
```

Expected: current tests pass before the refactor. If this new test fails because helper wiring has not been extracted yet, keep the failure as the refactor guide.

- [ ] **Step 3: Create the shared runner module**

Create `ClaudePlugin/task-core/src/task/service/task-spec-runner.ts` with this public API:

```ts
import type { BridgeResponse } from '../../bridge/bridge-client.js';
import type { PythonTaskCompilerResult } from '../compiler/task-python-orchestrator.js';
import type { TaskIssue, TaskPlan, TaskSpec } from '../schema/task-schemas.js';
import type { ToolResultBase } from '../../mcp/result/tool-result.js';

export type TaskRunnerBridge = {
  sendCommand(command: string, payload?: Record<string, unknown>): Promise<BridgeResponse>;
};

export type TaskCompiler = (taskSpec: TaskSpec, dryRun: boolean) => Promise<PythonTaskCompilerResult>;

export interface TaskPreviewOutcome {
  previewId: string;
  taskPlan: TaskPlan;
  passed: boolean;
  issues: TaskIssue[];
  toolResult: ToolResultBase;
}

export interface TaskSpecRunner {
  readTaskContext(input: Record<string, unknown>): Promise<ToolResultBase>;
  readReferenceContext(input: Record<string, unknown>): Promise<ToolResultBase>;
  previewTask(taskSpec: TaskSpec): Promise<TaskPreviewOutcome>;
  executeTask(taskSpec: TaskSpec): Promise<ToolResultBase>;
  getTaskResult(taskRunId: string): Promise<ToolResultBase>;
}

export function createTaskSpecRunner(input: {
  bridge: TaskRunnerBridge;
  taskCompiler: TaskCompiler;
}): TaskSpecRunner;
```

Move the existing task behavior from `task-tools.ts` into this module without changing command names:

```text
blueprinthelper_read_task_context -> buildTaskContextPack
blueprinthelper_read_reference_context -> read_reference_context
blueprinthelper_preview_task -> Python compiler -> preview_task_plan
blueprinthelper_execute_task -> preview_task_plan -> execute_task_plan
blueprinthelper_get_task_result -> get_task_run_journal with local fallback
```

- [ ] **Step 4: Rewire MCP task tools to the shared runner**

In `ClaudePlugin/mcp/src/mcp/tools/task-tools.ts`, keep zod input schemas and `toMcpResult` wrapping in the MCP layer. Replace inline logic with runner calls:

```ts
const runner = createTaskSpecRunner({
  bridge,
  taskCompiler: config.taskCompiler ?? compileTaskSpecWithPython,
});
```

Each MCP handler should parse input through the existing schema, call the runner, and return:

```ts
return toMcpResult(toolResult);
```

- [ ] **Step 5: Verify MCP parity**

Run:

```powershell
cd G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\ClaudePlugin\mcp
npm run build
npm run test:node
```

Expected: all node tests pass, and no Bridge command order changes.

- [ ] **Step 6: Commit**

```powershell
git add ClaudePlugin/task-core/src/task/service/task-spec-runner.ts ClaudePlugin/mcp/src/mcp/tools/task-tools.ts ClaudePlugin/mcp/src/tests/mcp/task-tools.regression.test.ts
git commit -m "refactor: share TaskSpec runner between MCP and CLI"
```

---

### Task 2: Add CLI Command Dispatcher

**Files:**

- Create: `ClaudePlugin/cli/src/cli/index.ts`
- Create: `ClaudePlugin/cli/src/cli/run.ts`
- Modify: `ClaudePlugin/cli/package.json`
- Test: `ClaudePlugin/cli/src/tests/cli/cli-task-runner.test.ts`

- [ ] **Step 1: Write a failing CLI dispatcher test**

Create `ClaudePlugin/cli/src/tests/cli/cli-task-runner.test.ts`:

```ts
import { strict as assert } from 'node:assert';
import test from 'node:test';
import { runCli } from '../../cli/run.js';
import type { TaskSpecRunner } from '../../task/service/task-spec-runner.js';

test('task preview reads TaskSpec file and prints compact summary JSON', async () => {
  const writes: string[] = [];
  const runner = {
    previewTask: async () => ({
      previewId: 'preview_cli_001',
      taskPlan: {
        schema: 'BlueprintHelper.TaskPlan.v1',
        task_name: 'CLI Preview',
        task_type: 'edit_blueprint_graph',
        context_id: 'ctx_001',
        target_assets: ['/Game/BP_Player'],
        execution_policy: { dry_run_mode: 'full', should_compile: true, should_save: false },
        steps: []
      },
      passed: true,
      issues: [],
      toolResult: {
        ok: true,
        schema: 'BlueprintHelper.McpToolResult.v1',
        operation: 'preview_task',
        trace_id: 'trace_cli',
        status: 'dry_run',
        modified: false,
        data: {
          schema: 'BlueprintHelper.TaskPreview.v1',
          preview_id: 'preview_cli_001',
          passed: true,
          blocked: false,
          issues: []
        }
      }
    }),
    executeTask: async () => { throw new Error('not used'); },
    getTaskResult: async () => { throw new Error('not used'); },
    readTaskContext: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); }
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['task', 'preview', '--file', 'fixtures/task-spec.json', '--artifact-dir', 'artifacts'],
    cwd: 'G:/UnrealPractise/MrStone/Plugins/BlueprintHelper/ClaudePlugin/mcp',
    runner,
    stdout: (line) => writes.push(line),
    stderr: () => {}
  });

  assert.equal(exitCode, 0);
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  assert.equal(output.schema, 'BlueprintHelper.CliResult.v1');
  assert.equal(output.operation, 'task.preview');
  assert.equal(output.status, 'preview_passed');
});
```

Add a small fixture file under `ClaudePlugin/cli/src/tests/fixtures/task-spec.json` if no reusable TaskSpec fixture exists. The fixture must use `BlueprintHelper.TaskSpec.v1` and a non-destructive `edit_blueprint_graph` dry run target.

- [ ] **Step 2: Implement `runCli` with injectable dependencies**

Create `ClaudePlugin/cli/src/cli/run.ts`:

```ts
import * as fs from 'node:fs';
import * as path from 'node:path';
import { BridgeClient } from '../bridge/bridge-client.js';
import { compileTaskSpecWithPython } from '../task/compiler/task-python-orchestrator.js';
import { createTaskSpecRunner, type TaskSpecRunner } from '../task/service/task-spec-runner.js';
import { writeCliResult } from './output.js';

export interface CliRuntime {
  argv: string[];
  cwd: string;
  runner?: TaskSpecRunner;
  stdout: (text: string) => void;
  stderr: (text: string) => void;
}

export async function runCli(runtime: CliRuntime): Promise<number> {
  const command = parseArgs(runtime.argv);
  const runner = runtime.runner ?? createDefaultRunner();

  if (command.kind === 'task.preview') {
    const taskSpec = readJsonFile(path.resolve(runtime.cwd, command.file));
    const preview = await runner.previewTask(taskSpec);
    writeCliResult(runtime, command, preview.toolResult, {
      previewId: preview.previewId,
      taskPlan: preview.taskPlan,
      passed: preview.passed,
      issues: preview.issues
    });
    return preview.passed ? 0 : 2;
  }

  if (command.kind === 'task.execute') {
    const taskSpec = readJsonFile(path.resolve(runtime.cwd, command.file));
    const toolResult = await runner.executeTask(taskSpec);
    writeCliResult(runtime, command, toolResult);
    return toolResult.ok ? 0 : 2;
  }

  if (command.kind === 'task.result') {
    const toolResult = await runner.getTaskResult(command.taskRunId);
    writeCliResult(runtime, command, toolResult);
    return toolResult.ok ? 0 : 2;
  }

  runtime.stderr(`Unsupported BlueprintHelper CLI command: ${runtime.argv.join(' ')}\n`);
  return 64;
}
```

The implemented `parseArgs` must support:

```text
task preview --file <path>
task execute --file <path>
task result --id <task_run_id>
--format summary|json|full
--artifact-dir <path>
--max-bytes <number>
```

It must reject unknown flags with exit code `64`.

- [ ] **Step 3: Add the CLI entry file**

Create `ClaudePlugin/cli/src/cli/index.ts`:

```ts
#!/usr/bin/env node
import { runCli } from './run.js';

const exitCode = await runCli({
  argv: process.argv.slice(2),
  cwd: process.cwd(),
  stdout: (text) => process.stdout.write(text),
  stderr: (text) => process.stderr.write(text)
});

process.exit(exitCode);
```

- [ ] **Step 4: Register the CLI in package metadata**

Modify `ClaudePlugin/cli/package.json`:

```json
{
  "bin": {
    "blueprinthelper-cli": "build/cli/index.js",
    "bh": "build/cli/index.js"
  },
  "scripts": {
    "cli": "node build/cli/index.js"
  }
}
```

Keep all existing scripts.

- [ ] **Step 5: Verify CLI dispatcher**

Run:

```powershell
cd G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\ClaudePlugin\mcp
npm run build
npm run test:node
node build/cli/index.js --help
```

Expected:

```text
BlueprintHelper CLI
```

The help command exits `0`. Unknown commands exit `64`.

- [ ] **Step 6: Commit**

```powershell
git add ClaudePlugin/cli/src/cli/index.ts ClaudePlugin/cli/src/cli/run.ts ClaudePlugin/cli/src/tests/cli/cli-task-runner.test.ts ClaudePlugin/cli/package.json
git commit -m "feat: add TaskSpec CLI dispatcher"
```

---

### Task 3: Implement Compact Output And Artifacts

**Files:**

- Create: `ClaudePlugin/cli/src/cli/output.ts`
- Create: `ClaudePlugin/cli/src/cli/artifacts.ts`
- Test: `ClaudePlugin/cli/src/tests/cli/cli-output.test.ts`

- [ ] **Step 1: Write failing output tests**

Create `ClaudePlugin/cli/src/tests/cli/cli-output.test.ts`:

```ts
import { strict as assert } from 'node:assert';
import test from 'node:test';
import { buildCliSummary } from '../../cli/output.js';

test('summary output omits full task_plan and points to artifacts', () => {
  const result = buildCliSummary({
    command: { kind: 'task.preview', format: 'summary', artifactDir: 'artifacts', maxBytes: 4096 },
    toolResult: {
      ok: true,
      schema: 'BlueprintHelper.McpToolResult.v1',
      operation: 'preview_task',
      trace_id: 'trace_001',
      status: 'dry_run',
      modified: false,
      target: { target_type: 'blueprint', asset_path: '/Game/BP_Player' },
      data: {
        schema: 'BlueprintHelper.TaskPreview.v1',
        preview_id: 'preview_001',
        passed: true,
        blocked: false,
        task_plan: { steps: [{ step_id: 'step_1' }] },
        issues: []
      }
    },
    artifactRefs: {
      full_result: 'artifacts/preview_001/result.json',
      task_plan: 'artifacts/preview_001/task_plan.json'
    }
  });

  assert.equal(result.schema, 'BlueprintHelper.CliResult.v1');
  assert.equal(result.operation, 'task.preview');
  assert.equal(result.status, 'preview_passed');
  assert.equal(JSON.stringify(result).includes('step_1'), false);
  assert.equal((result.artifacts as Record<string, unknown>).task_plan, 'artifacts/preview_001/task_plan.json');
});
```

- [ ] **Step 2: Implement artifact writing**

Create `ClaudePlugin/cli/src/cli/artifacts.ts`:

```ts
import * as fs from 'node:fs';
import * as path from 'node:path';

export function resolveArtifactRoot(input: { cwd: string; cliDir?: string }): string {
  return input.cliDir
    ?? process.env['BPH_CLI_ARTIFACT_DIR']
    ?? path.resolve(input.cwd, 'Saved', 'BlueprintHelper', 'Cli');
}

export function writeJsonArtifact(input: {
  root: string;
  runId: string;
  name: string;
  value: unknown;
}): string {
  const dir = path.resolve(input.root, input.runId);
  fs.mkdirSync(dir, { recursive: true });
  const filePath = path.resolve(dir, `${input.name}.json`);
  fs.writeFileSync(filePath, `${JSON.stringify(input.value, null, 2)}\n`, 'utf8');
  return filePath;
}
```

- [ ] **Step 3: Implement summary building**

Create `ClaudePlugin/cli/src/cli/output.ts` with exported helpers:

```ts
export const CLI_RESULT_SCHEMA = 'BlueprintHelper.CliResult.v1';

export function buildCliSummary(input: {
  command: CliCommand;
  toolResult: ToolResultBase;
  artifactRefs: Record<string, string>;
}): Record<string, unknown>;

export function writeCliResult(
  runtime: CliRuntime,
  command: CliCommand,
  toolResult: ToolResultBase,
  extra?: Record<string, unknown>
): void;
```

Summary status mapping:

```text
task.preview + passed true -> preview_passed
task.preview + passed false -> preview_blocked
task.execute + ok true -> executed
task.execute + ok false -> execute_failed
task.result + ok true -> result_found
task.result + ok false -> result_missing
```

The summary must include counts only:

```text
planned_steps
warnings
errors
target_assets
task_type
modified
```

It must not include full `task_plan`, full `bridge_result`, full `steps`, or raw LogicJson unless `--format json` or `--format full` is explicitly selected.

- [ ] **Step 4: Enforce stdout byte budget**

In `writeCliResult`, calculate the UTF-8 byte length before writing. If the output exceeds `--max-bytes`, write a compact error JSON instead:

```json
{
  "ok": false,
  "schema": "BlueprintHelper.CliResult.v1",
  "operation": "output",
  "status": "output_too_large",
  "message": "CLI stdout exceeds --max-bytes. Read the artifact path instead.",
  "artifacts": {
    "full_result": "Saved/BlueprintHelper/Cli/preview_123/result.json"
  }
}
```

Exit code remains `3` for output budget failure.

- [ ] **Step 5: Verify output behavior**

Run:

```powershell
cd G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\ClaudePlugin\mcp
npm run build
npm run test:node
```

Expected: CLI output tests pass and summary output does not include full `task_plan`.

- [ ] **Step 6: Commit**

```powershell
git add ClaudePlugin/cli/src/cli/output.ts ClaudePlugin/cli/src/cli/artifacts.ts ClaudePlugin/cli/src/tests/cli/cli-output.test.ts
git commit -m "feat: add compact CLI output artifacts"
```

---

### Task 4: Add Bridge Utility Commands Without Bypassing TaskSpec Writes

**Files:**

- Modify: `ClaudePlugin/cli/src/cli/run.ts`
- Test: `ClaudePlugin/cli/src/tests/cli/cli-task-runner.test.ts`

- [ ] **Step 1: Add tests for read-only bridge utility commands**

Add tests for:

```text
bridge ping
bridge call --command get_runtime_profile
context read --file context-request.json
```

The test must assert:

```text
bridge call execute_task_plan exits 64
bridge call preview_task_plan exits 64
bridge call import_agent_graph exits 64
```

Reason: write and preview commands must go through TaskSpec runner commands, not raw Bridge call.

- [ ] **Step 2: Implement allowed raw Bridge command list**

In `run.ts`, add:

```ts
const READ_ONLY_BRIDGE_COMMANDS = new Set([
  'get_editor_context',
  'get_runtime_profile',
  'diagnostics_runtime',
  'read_reference_context',
  'get_debug_case',
  'get_task_run_journal'
]);
```

The `bridge call` command must reject anything outside this set.

- [ ] **Step 3: Implement context read**

`context read --file <path>` reads a JSON request and calls `runner.readTaskContext`. Output uses the same `summary` artifact contract.

- [ ] **Step 4: Verify utility commands**

Run:

```powershell
cd G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\ClaudePlugin\mcp
npm run build
npm run test:node
```

Expected: raw write attempts are blocked at CLI parse or policy layer.

- [ ] **Step 5: Commit**

```powershell
git add ClaudePlugin/cli/src/cli/run.ts ClaudePlugin/cli/src/tests/cli/cli-task-runner.test.ts
git commit -m "feat: add safe BlueprintHelper CLI utilities"
```

---

### Task 5: Documentation And Migration Guidance

**Files:**

- Replace: `BlueprintHelper/Docs/Install_CLI_QuickStart.md`
- Replace: `BlueprintHelper/Docs/CLI_Tools_API_Reference.md`
- Create: `BlueprintHelper/Docs/TaskSpec_CLI_QuickStart.md`

- [ ] **Step 1: Add CLI quickstart**

Create `BlueprintHelper/Docs/TaskSpec_CLI_QuickStart.md` with these sections:

~~~markdown
# BlueprintHelper TaskSpec CLI QuickStart

## Purpose

Use the CLI when an Agent can run shell commands and should avoid large MCP escaped JSON output. The CLI preserves TaskSpec-first orchestration and Python compilation.

## Build

```powershell
cd <PLUGIN_ROOT>\ClaudePlugin\task-core
npm install
npm run build

cd <PLUGIN_ROOT>\ClaudePlugin\cli
npm install
npm run build
```

## Preview

```powershell
node <PLUGIN_ROOT>\ClaudePlugin\cli\build\cli\index.js task preview --file .\task_spec.json --format summary
```

## Execute

```powershell
node <PLUGIN_ROOT>\ClaudePlugin\cli\build\cli\index.js task execute --file .\task_spec.json --format summary
```

## Read Full Result

Use the artifact paths returned by summary output. Use `--format json` only when the Agent needs the full JSON in context.
~~~

- [ ] **Step 2: Update MCP quickstart without demoting MCP**

In `BlueprintHelper/Docs/Install_CLI_QuickStart.md`, document CLI as the only supported Agent entry:

```markdown
## Optional TaskSpec CLI Entry

The CLI is an alternate Agent entry for shell-capable environments. It does not replace the TaskSpec flow. It calls the same Python compiler, Bridge preview, and UE Task Runtime execution path as the MCP task tools.
```

- [ ] **Step 3: Update API reference boundaries**

In `BlueprintHelper/Docs/CLI_Tools_API_Reference.md`, add:

```markdown
CLI parity rule: any CLI write command must be expressible as `BlueprintHelper.TaskSpec.v1` and must pass through preview before execute. Raw Bridge write commands are not an Agent-facing CLI surface.
```

- [ ] **Step 4: Verify docs**

Run:

```powershell
cd G:\UnrealPractise\MrStone\Plugins\BlueprintHelper
rg -n "TaskSpec CLI|CLI parity rule|Optional TaskSpec CLI" BlueprintHelper\Docs Plan\transfer
```

Expected: all three documentation locations are found.

- [ ] **Step 5: Commit**

```powershell
git add BlueprintHelper/Docs/TaskSpec_CLI_QuickStart.md BlueprintHelper/Docs/Install_CLI_QuickStart.md BlueprintHelper/Docs/CLI_Tools_API_Reference.md
git commit -m "docs: document TaskSpec CLI migration path"
```

---

### Task 6: End-To-End Verification

**Files:**

- Modify only if tests reveal defects in previous task files.

- [ ] **Step 1: Full MCP package verification**

Run:

```powershell
cd G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\ClaudePlugin\mcp
npm test
```

Expected:

```text
npm run build
npm run test:python
npm run test:node
```

All three phases pass.

- [ ] **Step 2: CLI no-Bridge failure check**

Run without Unreal Editor:

```powershell
cd G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\ClaudePlugin\cli
node build/cli/index.js bridge ping --format summary
```

Expected: exit code is non-zero and stdout is compact JSON with `ok=false`, `status=bridge_unavailable`, and no stack trace.

- [ ] **Step 3: CLI preview with running Bridge**

With Unreal Editor and BlueprintHelper Bridge running:

```powershell
cd G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\ClaudePlugin\cli
node build/cli/index.js task preview --file G:\UnrealPractise\MrStone\Saved\BlueprintHelper\CliSmoke\task_spec.json --format summary --max-bytes 4096
```

Expected:

```json
{
  "ok": true,
  "schema": "BlueprintHelper.CliResult.v1",
  "operation": "task.preview",
  "status": "preview_passed"
}
```

The real output may contain additional summary and artifact fields, but must stay under 4096 bytes.

- [ ] **Step 4: CLI execute with running Bridge**

Run:

```powershell
node build/cli/index.js task execute --file G:\UnrealPractise\MrStone\Saved\BlueprintHelper\CliSmoke\task_spec.json --format summary --max-bytes 4096
```

Expected: preview runs first, execute runs second, summary returns `task_run_id`, and full journal is available only through artifact path unless `--format json` is requested.

- [ ] **Step 5: Commit verification-only fixes**

If verification required code changes:

```powershell
git add ClaudePlugin/mcp BlueprintHelper/Docs
git commit -m "test: verify TaskSpec CLI path"
```

If no changes were needed, do not create an empty commit.

---

## Migration Acceptance Criteria

- Deprecated MCP wrappers and CLI task commands share one TaskSpec runner.
- Python `blueprinthelper_task` remains the compiler and orchestration layer for TaskSpec-to-TaskPlan.
- CLI writes always run preview before execute.
- Raw CLI Bridge calls cannot execute write commands.
- Default CLI stdout is compact and artifact-backed.
- `--format json` and `--format full` are explicit opt-in modes.
- Deprecated MCP behavior remains backward compatible.
- `npm test` passes in `ClaudePlugin/mcp`.
- Documentation clearly says CLI is the only supported Agent entry and preserves TaskSpec.

## Rollback Plan

If CLI causes regressions, keep the shared runner only if MCP tests pass. Remove `bin` and CLI scripts from `ClaudePlugin/cli/package.json`, leave MCP server as the supported Agent entry, and keep the CLI docs unpublished until the failing task is fixed.

## Self-Review Notes

- No plan task removes the deprecated internal transport package.
- No plan task lets Agent-authored TaskPlan become public surface.
- No plan task bypasses Python compilation.
- No plan task bypasses `preview_task_plan`.
- Large output token savings come from summary output plus artifact paths, not from CLI transport alone.
