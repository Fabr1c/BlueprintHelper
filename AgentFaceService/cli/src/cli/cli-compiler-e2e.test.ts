import assert from 'node:assert/strict';
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import type { BridgeResponse } from '@blueprinthelper/task-core/bridge/bridge-client';
import { graphWriteAppendTaskSpecFixture } from '@blueprinthelper/task-core/task/fixtures/task-protocol.fixtures';
import { createDescriptorFixtureRuntimeCapabilityState } from '@blueprinthelper/task-core/tool-surface/tool-registry';
import { runCli } from './run.js';

const ACTIVE_RUNTIME_ARGS = [
  '--runtime-adapters',
  createDescriptorFixtureRuntimeCapabilityState().registered_runtime_adapter_ids.join(','),
] as const;

test('CLI task preview normalizes bare TaskSpec and compiles before preview_task_plan', async (t) => {
  const workspace = await createTempDir(t, 'bph-cli-e2e-preview-bare-');
  await writeFile(path.join(workspace, 'task-spec.json'), `\uFEFF${JSON.stringify(graphWriteAppendTaskSpecFixture, null, 2)}`);
  const commands: string[] = [];
  const payloads: Array<Record<string, unknown>> = [];
  const stdout: string[] = [];

  const exitCode = await withEnv({
    BPH_METRICS_DIR: path.join(workspace, 'metrics'),
  }, () => runCli({
    argv: ['task', 'preview', '--file', 'task-spec.json', ...ACTIVE_RUNTIME_ARGS, '--format', 'json'],
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

test('CLI removed direct preview command reports grouped-command replacement', async (t) => {
  const workspace = await createTempDir(t, 'bph-cli-e2e-preview-direct-removed-');
  await writeFile(path.join(workspace, 'preview.json'), JSON.stringify({
    task_spec: graphWriteAppendTaskSpecFixture,
  }, null, 2));
  const commands: string[] = [];
  const payloads: Array<Record<string, unknown>> = [];
  const stdout: string[] = [];
  const stderr: string[] = [];

  const exitCode = await withEnv({
    BPH_METRICS_DIR: path.join(workspace, 'metrics'),
  }, () => runCli({
    argv: ['blueprinthelper_preview_task', '--file', 'preview.json', '--format', 'json'],
    cwd: workspace,
    bridge: createRecordingBridge(commands, payloads),
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  }));

  assert.equal(exitCode, 64);
  assert.deepEqual(commands, []);
  assert.equal(stdout.join(''), '');
  assert.match(stderr.join(''), /blueprinthelper_preview_task direct CLI command was removed/);
  assert.match(stderr.join(''), /bh task preview --file <task-spec\.json>/);
});

test('CLI removed lifecycle commands report global MCP replacement without contacting Bridge', async (t) => {
  const workspace = await createTempDir(t, 'bph-cli-e2e-lifecycle-direct-removed-');
  const cases = [
    {
      command: 'open_editor',
      mcpTool: 'mcp__blueprint_helper__blueprint_open_editor',
    },
    {
      command: 'close_editor',
      mcpTool: 'mcp__blueprint_helper__blueprint_close_editor',
    },
    {
      command: 'dismiss_editor_dialogs',
      mcpTool: 'mcp__blueprint_helper__blueprint_dismiss_editor_dialogs',
    },
    {
      command: 'close_editor_dialogs',
      mcpTool: 'mcp__blueprint_helper__blueprint_close_editor_dialogs',
    },
  ] as const;

  for (const entry of cases) {
    const commands: string[] = [];
    const payloads: Array<Record<string, unknown>> = [];
    const stdout: string[] = [];
    const stderr: string[] = [];

    const exitCode = await withEnv({
      BPH_METRICS_DIR: path.join(workspace, 'metrics'),
    }, () => runCli({
      argv: [entry.command, '--format', 'json'],
      cwd: workspace,
      bridge: createRecordingBridge(commands, payloads),
      stdout: (text) => stdout.push(text),
      stderr: (text) => stderr.push(text),
    }));

    assert.equal(exitCode, 64);
    assert.deepEqual(commands, []);
    assert.equal(stdout.join(''), '');
    assert.match(stderr.join(''), new RegExp(`${entry.command} direct CLI command was removed`));
    assert.match(stderr.join(''), /Editor lifecycle is not available through the BlueprintHelper CLI/);
    assert.match(stderr.join(''), new RegExp(entry.mcpTool));
  }
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
    argv: ['task', 'execute', '--file', 'task-spec.json', ...ACTIVE_RUNTIME_ARGS, '--format', 'json'],
    cwd: workspace,
    bridge: createRecordingBridge(commands, payloads),
    stdout: (text) => stdout.push(text),
    stderr: () => {},
  }));
  const output = JSON.parse(stdout.join('')) as Record<string, unknown>;

  assert.equal(exitCode, 0);
  assert.deepEqual(commands, ['preview_task_plan', 'source_control_status', 'execute_task_plan']);
  assertTaskPlanPayload(payloads[0]);
  assertSourceControlStatusPayload(payloads[1]);
  assertTaskPlanPayload(payloads[2]);
  assert.equal(output.ok, true);
  assert.equal(output.operation, 'task.execute');
  assert.equal(JSON.stringify(output).includes('bridge_result'), false);
  assert.ok((output.artifacts as Record<string, unknown>).full_result);
});

test('CLI graph body adapter loop previews, executes, then reads context', async (t) => {
  const workspace = await createTempDir(t, 'bph-cli-node-graph-body-loop-');
  await writeFile(path.join(workspace, 'task-spec.json'), JSON.stringify(graphWriteMacroBodyTaskSpecFixture(), null, 2));
  await writeFile(path.join(workspace, 'read-spec.json'), JSON.stringify(graphWriteMacroBodyReadSpecFixture(), null, 2));
  const commands: string[] = [];
  const payloads: Array<Record<string, unknown>> = [];
  const stdout: string[] = [];

  const exitExecute = await withEnv({
    BPH_METRICS_DIR: path.join(workspace, 'metrics'),
  }, () => runCli({
    argv: ['task', 'execute', '--file', 'task-spec.json', ...ACTIVE_RUNTIME_ARGS, '--format', 'json'],
    cwd: workspace,
    bridge: createRecordingBridge(commands, payloads),
    stdout: (text) => stdout.push(text),
    stderr: () => {},
  }));
  assert.equal(exitExecute, 0);

  const exitRead = await withEnv({
    BPH_METRICS_DIR: path.join(workspace, 'metrics'),
  }, () => runCli({
    argv: ['context', 'read', '--file', 'read-spec.json', '--format', 'json'],
    cwd: workspace,
    bridge: createRecordingBridge(commands, payloads),
    stdout: (text) => stdout.push(text),
    stderr: () => {},
  }));
  assert.equal(exitRead, 0);

  assert.deepEqual(commands, ['preview_task_plan', 'source_control_status', 'execute_task_plan', 'read_blueprint_logic_json']);
  assertGraphBodyReplacePayload(payloads[0], 'macro_body');
  assertSourceControlStatusPayload(payloads[1]);
  assertGraphBodyReplacePayload(payloads[2], 'macro_body');
  assert.equal(payloads[3]?.['target_type'], 'graph');
  assert.equal(payloads[3]?.['graph'], 'ClampScoreMacro');
  const serializedOutput = stdout.join('');
  assert.match(serializedOutput, /LogicFlow\.v1/);
  assert.match(serializedOutput, /k2\.macro_body/);
  assert.match(serializedOutput, /Macro In/);
});

test('CLI context read supports stdin as the grouped ReadSpec entry', async (t) => {
  const workspace = await createTempDir(t, 'bph-cli-e2e-context-read-stdin-');
  const commands: string[] = [];
  const payloads: Array<Record<string, unknown>> = [];
  const stdout: string[] = [];

  const exitRead = await withEnv({
    BPH_METRICS_DIR: path.join(workspace, 'metrics'),
  }, () => runCli({
    argv: ['context', 'read', '--stdin', '--format', 'json'],
    cwd: workspace,
    bridge: createRecordingBridge(commands, payloads),
    readStdin: () => `\uFEFF${JSON.stringify(graphWriteMacroBodyReadSpecFixture())}`,
    stdout: (text) => stdout.push(text),
    stderr: () => {},
  }));

  assert.equal(exitRead, 0);
  assert.deepEqual(commands, ['read_blueprint_logic_json']);
  assert.equal(payloads[0]?.['target_type'], 'graph');
  assert.equal(payloads[0]?.['graph'], 'ClampScoreMacro');
  assert.match(stdout.join(''), /LogicFlow\.v1/);
});

test('CLI tools templates composer output is descriptor-backed and does not contact Bridge', async (t) => {
  const workspace = await createTempDir(t, 'bph-cli-e2e-templates-');
  const stdout: string[] = [];
  const outputPath = path.join(workspace, 'graph-append.taskspec.json');

  const exitCode = await runCli({
    argv: [
      'tools',
      'templates',
      'compose',
      '--family',
      'graph_write',
      '--write-mode',
      'graph.append',
      '--templates',
      'generic_ops.let.default(generic_ops.expression.literal)',
      '--out',
      outputPath,
      '--format',
      'json',
    ],
    cwd: workspace,
    bridge: createFailingBridge(),
    stdout: (text) => stdout.push(text),
    stderr: () => {},
  });
  const output = JSON.parse(stdout.join('')) as Record<string, unknown>;

  assert.equal(exitCode, 0);
  assert.equal(output.schema, 'BlueprintHelper.TaskSpecTemplateComposition.v1');
  assert.equal(output.status, 'ok');
  assert.deepEqual(
    (output.required_placeholders as Array<Record<string, unknown>>).map((item) => item.placeholder),
    [
      '__REQUIRED_FEATURE_NAME__',
      '__REQUIRED_BLUEPRINT_ASSET_PATH__',
      '__REQUIRED_CUSTOM_EVENT_NAME__',
      '__REQUIRED_SYMBOL_NAME__',
      '__REQUIRED_LITERAL_VALUE_TYPE__',
      '__REQUIRED_VALUE__',
    ],
  );
  assert.equal('inserted_slots' in output, false);
  const taskSpec = JSON.parse(await readFile(outputPath, 'utf8')) as {
    behavior: { entries: Array<{ body: { statements: Array<{ value: { kind: string } }> } }> };
  };
  assert.equal(taskSpec.behavior.entries[0]?.body.statements[0]?.value.kind, 'literal');
});

test('CLI read-template composer output is descriptor-backed and does not contact Bridge', async (t) => {
  const workspace = await createTempDir(t, 'bph-cli-e2e-read-templates-');
  const stdout: string[] = [];
  const outputPath = path.join(workspace, 'function-flow.readspec.json');

  const exitCode = await runCli({
    argv: [
      'tools',
      'read-templates',
      'compose',
      '--template',
      'blueprint.logic.function.flow',
      '--out',
      outputPath,
      '--format',
      'json',
    ],
    cwd: workspace,
    bridge: createFailingBridge(),
    stdout: (text) => stdout.push(text),
    stderr: () => {},
  });
  const output = JSON.parse(stdout.join('')) as Record<string, unknown>;
  const readSpec = JSON.parse(await readFile(outputPath, 'utf8')) as Record<string, unknown>;

  assert.equal(exitCode, 0);
  assert.equal(output.schema, 'BlueprintHelper.ReadContextTemplateComposition.v1');
  assert.equal(output.status, 'ok');
  assert.equal(readSpec.schema, 'BlueprintHelper.ReadSpec.v1');
  assert.equal(readSpec.read_type, 'blueprint_logic');
});

test('CLI help output is manifest-backed and does not contact Bridge', async (t) => {
  const workspace = await createTempDir(t, 'bph-cli-e2e-help-');
  const stdout: string[] = [];

  const exitCode = await runCli({
    argv: ['task', 'preview', '--help'],
    cwd: workspace,
    bridge: createFailingBridge(),
    stdout: (text) => stdout.push(text),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const help = stdout.join('');
  assert.match(help, /blueprint\.plan\.taskspec\.preview/);
  assert.match(help, /task preview/);
  assert.match(help, /tools templates families --workflow preview_execute/);
  assert.doesNotMatch(help, new RegExp(['tools templates', '<tool_id>'].join(' ')));
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
      if (command === 'source_control_status') {
        return sourceControlEditableResponse();
      }
      if (command === 'execute_task_plan') {
        return executeResponse();
      }
      if (command === 'read_blueprint_logic_json') {
        return readContextMacroLogicJsonResponse();
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

function assertSourceControlStatusPayload(payload: Record<string, unknown> | undefined): asserts payload is Record<string, unknown> {
  assert.ok(payload);
  assert.ok(Array.isArray(payload['asset_paths']));
  assert.notEqual((payload['asset_paths'] as unknown[]).length, 0);
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

function sourceControlEditableResponse(): BridgeResponse {
  return {
    success: true,
    request_id: 'p7_source_control_request',
    result: {
      source_control: {
        status: 'editable',
        files: [
          {
            asset_path: '/Game/Blueprints/BP_StoneGate',
            status: 'editable',
            editable: true,
          },
        ],
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

function readContextMacroLogicJsonResponse(): BridgeResponse {
  return {
    success: true,
    request_id: 'p7_read_context_request',
    result: {
      schema: 'LogicJson.v1',
      adapter_boundary: {
        runtime_adapter_id: 'k2.macro_body',
        body_kind: 'k2.macro_body',
        entry_boundaries: [{ node_ref: 'TunnelEntry', display_name: 'Macro In' }],
        exit_boundaries: [{ node_ref: 'TunnelExit', display_name: 'Macro Out' }],
        visible_boundary_node_refs: ['TunnelEntry', 'TunnelExit'],
      },
      logic: {
        nodes: [
          { node_ref: 'TunnelEntry', name: 'Tunnel Entry', kind: 'macro_entry' },
          { node_ref: 'PrintString', name: 'PrintString', kind: 'call' },
          { node_ref: 'TunnelExit', name: 'Tunnel Exit', kind: 'macro_exit' },
        ],
        links: [
          { type: 'exec', from_node: 'TunnelEntry', from_pin: 'then', to_node: 'PrintString', to_pin: 'execute' },
          { type: 'exec', from_node: 'PrintString', from_pin: 'then', to_node: 'TunnelExit', to_pin: 'execute' },
        ],
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

function assertGraphBodyReplacePayload(payload: Record<string, unknown> | undefined, expectedScope: string): void {
  assert.ok(payload, 'Expected Bridge payload.');
  const taskPlan = payload['task_plan'] as Record<string, unknown> | undefined;
  assert.equal(taskPlan?.schema, 'BlueprintHelper.TaskPlan.v1');
  assert.equal(taskPlan?.task_type, 'edit_blueprint_graph');
  const steps = taskPlan?.steps as Array<Record<string, unknown>> | undefined;
  assert.ok(Array.isArray(steps), 'Compiled TaskPlan must include steps.');
  const graphWriteStep = steps.find((step) => step.capability === 'graph_write');
  assert.ok(graphWriteStep, 'Compiled TaskPlan must include a graph_write step.');
  const write = graphWriteStep['write'] as Record<string, unknown> | undefined;
  const ops = write?.ops as Array<Record<string, unknown>> | undefined;
  assert.equal(ops?.some((op) => op.op === 'replace_body' && op.replace_scope === expectedScope), true);
}

function graphWriteMacroBodyTaskSpecFixture(): Record<string, unknown> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_cli_node_graph_body_macro_replace',
    task_type: 'edit_blueprint_graph',
    feature_name: 'CliNodeGraphBodyMacroReplace',
    target: {
      asset_path: '/Game/BlueprintHelper/NodeGraphBody/BP_NodeGraphBodyAdapter',
      target_type: 'blueprint',
    },
    behavior: {
      graph_strategy: 'replace_owned_graph',
      replace: {
        scope: 'macro_body',
        selector: {
          kind: 'macro',
          name: 'ClampScoreMacro',
        },
        body: {
          schema: 'BlueprintLogicSpec.v2',
          statements: [
            {
              kind: 'call',
              target: 'PrintString',
              args: {
                InString: {
                  kind: 'literal',
                  value_type: 'string',
                  value: 'NodeGraphBody macro replacement',
                },
              },
            },
          ],
        },
      },
    },
  };
}

function graphWriteMacroBodyReadSpecFixture(): Record<string, unknown> {
  return {
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BlueprintHelper/NodeGraphBody/BP_NodeGraphBodyAdapter',
      target_type: 'graph',
      target_name: 'ClampScoreMacro',
    },
    view: {
      format: 'logic_flow',
    },
  };
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
