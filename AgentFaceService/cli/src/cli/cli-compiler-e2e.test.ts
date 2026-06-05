import assert from 'node:assert/strict';
import { mkdtemp, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import type { BridgeResponse } from '@blueprinthelper/task-core/bridge/bridge-client';
import { graphWriteAppendTaskSpecFixture } from '@blueprinthelper/task-core/task/fixtures/task-protocol.fixtures';
import { runCli } from './run.js';

test('CLI task preview normalizes bare TaskSpec and compiles before preview_task_plan', async (t) => {
  const workspace = await createTempDir(t, 'bph-cli-e2e-preview-bare-');
  await writeFile(path.join(workspace, 'task-spec.json'), JSON.stringify(graphWriteAppendTaskSpecFixture, null, 2));
  const commands: string[] = [];
  const payloads: Array<Record<string, unknown>> = [];
  const stdout: string[] = [];

  const exitCode = await withEnv({
    BPH_METRICS_DIR: path.join(workspace, 'metrics'),
  }, () => runCli({
    argv: ['task', 'preview', '--file', 'task-spec.json', '--format', 'json'],
    cwd: workspace,
    bridge: createRecordingBridge(commands, payloads),
    stdout: (text) => stdout.push(text),
    stderr: () => {},
  }));
  const output = JSON.parse(stdout.join('')) as Record<string, unknown>;

  assert.equal(exitCode, 0);
  assert.deepEqual(commands, ['preview_task_plan']);
  assertTaskPlanPayload(payloads[0]);
  assert.equal(output.ok, true);
  assert.equal(output.operation, 'task.preview');
  assert.equal(JSON.stringify(output).includes('bridge_result'), false);
  assert.ok((output.artifacts as Record<string, unknown>).full_result);
});

test('CLI blueprinthelper_preview_task normalizes wrapped TaskSpec and compiles before preview_task_plan', async (t) => {
  const workspace = await createTempDir(t, 'bph-cli-e2e-preview-wrapped-');
  await writeFile(path.join(workspace, 'preview.json'), JSON.stringify({
    task_spec: graphWriteAppendTaskSpecFixture,
  }, null, 2));
  const commands: string[] = [];
  const payloads: Array<Record<string, unknown>> = [];
  const stdout: string[] = [];

  const exitCode = await withEnv({
    BPH_METRICS_DIR: path.join(workspace, 'metrics'),
  }, () => runCli({
    argv: ['blueprinthelper_preview_task', '--file', 'preview.json', '--format', 'json'],
    cwd: workspace,
    bridge: createRecordingBridge(commands, payloads),
    stdout: (text) => stdout.push(text),
    stderr: () => {},
  }));
  const output = JSON.parse(stdout.join('')) as Record<string, unknown>;

  assert.equal(exitCode, 0);
  assert.deepEqual(commands, ['preview_task_plan']);
  assertTaskPlanPayload(payloads[0]);
  assert.equal(output.ok, true);
  assert.equal(output.operation, 'tool.invoke');
  assert.equal(JSON.stringify(output).includes('bridge_result'), false);
  assert.ok((output.artifacts as Record<string, unknown>).full_result);
});

test('CLI task execute previews then sends execute_task_plan to mocked Bridge', async (t) => {
  const workspace = await createTempDir(t, 'bph-cli-e2e-execute-');
  await writeFile(path.join(workspace, 'task-spec.json'), JSON.stringify(graphWriteAppendTaskSpecFixture, null, 2));
  const commands: string[] = [];
  const payloads: Array<Record<string, unknown>> = [];
  const stdout: string[] = [];

  const exitCode = await withEnv({
    BPH_METRICS_DIR: path.join(workspace, 'metrics'),
  }, () => runCli({
    argv: ['task', 'execute', '--file', 'task-spec.json', '--format', 'json'],
    cwd: workspace,
    bridge: createRecordingBridge(commands, payloads),
    stdout: (text) => stdout.push(text),
    stderr: () => {},
  }));
  const output = JSON.parse(stdout.join('')) as Record<string, unknown>;

  assert.equal(exitCode, 0);
  assert.deepEqual(commands, ['preview_task_plan', 'execute_task_plan']);
  assertTaskPlanPayload(payloads[0]);
  assertTaskPlanPayload(payloads[1]);
  assert.equal(output.ok, true);
  assert.equal(output.operation, 'task.execute');
  assert.equal(JSON.stringify(output).includes('bridge_result'), false);
  assert.ok((output.artifacts as Record<string, unknown>).full_result);
});

test('CLI tools templates route output is descriptor-backed and does not contact Bridge', async (t) => {
  const workspace = await createTempDir(t, 'bph-cli-e2e-templates-');
  const stdout: string[] = [];

  const exitCode = await runCli({
    argv: ['tools', 'templates', 'blueprinthelper_preview_task', '--route', 'graph.replace.function_body', '--slot', '--format', 'json'],
    cwd: workspace,
    bridge: createFailingBridge(),
    stdout: (text) => stdout.push(text),
    stderr: () => {},
  });
  const output = JSON.parse(stdout.join('')) as Record<string, unknown>;
  const selectedRoute = output.selected_route as Record<string, unknown>;
  const slotTemplates = output.slot_templates as Array<Record<string, unknown>>;

  assert.equal(exitCode, 0);
  assert.equal(selectedRoute.route_id, 'graph.replace.function_body');
  assert.equal(selectedRoute.route_kind, 'graph_write');
  assert.equal((selectedRoute.template_paths as string[]).some((entry) => entry.endsWith('graph_replace_owned_template.json')), true);
  assert.equal((selectedRoute.required_fields as string[]).includes('behavior.replace.scope=function_body'), true);
  assert.equal(output.input_shape, '{ "task_spec": BlueprintHelper.TaskSpec.v1 }');
  assert.equal((output.cli_invocation_templates as Array<Record<string, unknown>>)
    .some((template) => template.input_shape === 'BlueprintHelper.TaskSpec.v1'), true);
  assert.equal(Array.isArray(slotTemplates), true);
  assert.equal(slotTemplates.some((slot) => slot.slot_id === 'graph.expression.get.function_param'), true);
});

test('CLI help output is manifest-backed and does not contact Bridge', async (t) => {
  const workspace = await createTempDir(t, 'bph-cli-e2e-help-');
  const stdout: string[] = [];

  const exitCode = await runCli({
    argv: ['blueprinthelper_preview_task', '--help'],
    cwd: workspace,
    bridge: createFailingBridge(),
    stdout: (text) => stdout.push(text),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const help = stdout.join('');
  assert.match(help, /blueprint\.plan\.taskspec\.preview/);
  assert.match(help, /blueprinthelper_preview_task/);
  assert.match(help, /tools templates blueprint\.plan\.taskspec\.preview/);
  assert.doesNotMatch(help, /helpEntries/);
});

function createRecordingBridge(
  commands: string[],
  payloads: Array<Record<string, unknown>>,
) {
  return {
    async sendCommand(command: string, payload?: unknown): Promise<BridgeResponse> {
      commands.push(command);
      payloads.push((payload ?? {}) as Record<string, unknown>);
      if (command === 'preview_task_plan') {
        return previewResponse();
      }
      if (command === 'execute_task_plan') {
        return executeResponse();
      }
      throw new Error(`Unexpected bridge command: ${command}`);
    },
  };
}

function createFailingBridge() {
  return {
    async sendCommand(command: string): Promise<BridgeResponse> {
      throw new Error(`Bridge should not be contacted for ${command}.`);
    },
  };
}

function previewResponse(): BridgeResponse {
  return {
    success: true,
    request_id: 'p7_preview_request',
    result: {
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'preview_task_plan',
      trace_id: 'trace_p7_preview',
      status: 'dry_run',
      modified: false,
      data: {
        preview_token: '0123456789abcdef0123456789abcdef',
        dry_run: {
          can_execute: true,
          result: 'passed',
          errors: [],
          conflicts: [],
          warnings: [],
        },
      },
    },
  };
}

function executeResponse(): BridgeResponse {
  return {
    success: true,
    request_id: 'p7_execute_request',
    result: {
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'execute_task_plan',
      trace_id: 'trace_p7_execute',
      status: 'completed',
      modified: true,
      data: {
        task_run_id: 'task_p7_execute',
        target_assets: ['/Game/Blueprints/BP_StoneGate'],
        steps: [{ step_id: 'step_001', modified: true }],
      },
    },
  };
}

function assertTaskPlanPayload(payload: Record<string, unknown> | undefined): void {
  assert.ok(payload, 'Expected Bridge payload.');
  const taskPlan = payload['task_plan'] as Record<string, unknown> | undefined;
  assert.equal(taskPlan?.schema, 'BlueprintHelper.TaskPlan.v1');
  assert.equal(taskPlan?.task_type, 'edit_blueprint_graph');
  assert.equal('task_spec' in payload, false);
  assert.equal(taskPlan?.task_name, 'StoneGateActivation');
  assert.deepEqual(taskPlan?.target_assets, ['/Game/Blueprints/BP_StoneGate']);
  const steps = taskPlan?.steps as Array<Record<string, unknown>> | undefined;
  assert.ok(Array.isArray(steps), 'Compiled TaskPlan must include steps.');
  assert.equal(steps.length, 2);
  assertSignatureStep(steps[0]);
  assertGraphWriteStep(steps[1]);
}

function assertSignatureStep(step: Record<string, unknown> | undefined): void {
  assert.equal(step?.capability, 'blueprint_signature');
  const write = step?.write as Record<string, unknown> | undefined;
  assert.equal(write?.strategy, 'custom_event_signature');
  const ops = write?.ops as Array<Record<string, unknown>> | undefined;
  assert.equal(ops?.[0]?.op, 'ensure_custom_event');
  assert.equal(ops?.[0]?.event_name, 'InitializeStoneGate');
}

function assertGraphWriteStep(step: Record<string, unknown> | undefined): void {
  assert.equal(step?.capability, 'graph_write');
  assert.deepEqual(step?.depends_on, ['step_001']);
  const target = step?.target as Record<string, unknown> | undefined;
  assert.equal(target?.graph, 'BH_StoneGateActivation');
  const write = step?.write as Record<string, unknown> | undefined;
  assert.equal(write?.strategy, 'owned_graph_edit');
  const ops = write?.ops as Array<Record<string, unknown>> | undefined;
  const op = ops?.[0] as Record<string, unknown> | undefined;
  assert.equal(op?.op, 'ensure_entry');
  assert.equal(op?.entry_type, 'custom_event');
  assert.equal(op?.name, 'InitializeStoneGate');
  const body = op?.body as Record<string, unknown> | undefined;
  assert.equal(body?.schema, 'BlueprintLogicSpec.v2');
  const statements = body?.statements as Array<Record<string, unknown>> | undefined;
  assert.equal(statements?.some((statement) => statement.kind === 'field' && statement.field_operation === 'set'), true);
}

async function createTempDir(
  t: { after(callback: () => void | Promise<void>): void },
  prefix: string,
): Promise<string> {
  const root = await mkdtemp(path.join(os.tmpdir(), prefix));
  t.after(() => rm(root, { recursive: true, force: true }));
  return root;
}

async function withEnv<T>(
  overrides: Record<string, string | undefined>,
  action: () => Promise<T>,
): Promise<T> {
  const previous = new Map<string, string | undefined>();
  for (const [key, value] of Object.entries(overrides)) {
    previous.set(key, process.env[key]);
    if (value === undefined) {
      delete process.env[key];
    } else {
      process.env[key] = value;
    }
  }

  try {
    return await action();
  } finally {
    for (const [key, value] of previous.entries()) {
      if (value === undefined) {
        delete process.env[key];
      } else {
        process.env[key] = value;
      }
    }
  }
}
