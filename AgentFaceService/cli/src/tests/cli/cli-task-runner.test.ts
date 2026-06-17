import { strict as assert } from 'node:assert';
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
import test from 'node:test';
import { runCli } from '../../cli/run.js';
import type { BridgeResponse } from '@blueprinthelper/task-core/bridge/bridge-client';
import type { TaskSpecRunner } from '@blueprinthelper/task-core/task/service/task-spec-runner';
import { createDescriptorFixtureRuntimeCapabilityState } from '@blueprinthelper/task-core/tool-surface/tool-registry';

const fixturesDir = path.resolve(import.meta.dirname, '..', '..', '..', 'src', 'tests', 'fixtures');
const ACTIVE_RUNTIME_ARGS = [
  '--runtime-adapters',
  createDescriptorFixtureRuntimeCapabilityState().registered_runtime_adapter_ids.join(','),
] as const;

function makeTempDir() {
  return fs.mkdtempSync(path.join(os.tmpdir(), 'bph-cli-test-'));
}

function assertNoDefaultReturnPolicyFields(value: unknown) {
  const serialized = JSON.stringify(value);
  assert.doesNotMatch(serialized, /"schema":"BlueprintHelper\.Cli(Result|FullResult)\.v1"/);
  assert.doesNotMatch(serialized, /"execution_policy"/);
  assert.doesNotMatch(serialized, /"scope_policy"/);
  assert.doesNotMatch(serialized, /"should_compile"/);
  assert.doesNotMatch(serialized, /"should_save"/);
}

function readJsonFile(filePath: unknown): Record<string, unknown> {
  if (typeof filePath !== 'string') {
    assert.fail(`Expected artifact path string, got ${typeof filePath}`);
  }
  return JSON.parse(fs.readFileSync(filePath, 'utf8')) as Record<string, unknown>;
}

test('task preview reads TaskSpec file and prints compact summary JSON', async () => {
  const writes: string[] = [];
  const artifactDir = makeTempDir();
  const runner = {
    previewTask: async () => ({
      previewId: 'preview_cli_001',
      previewToken: '0123456789abcdef0123456789abcdef',
      taskPlan: {
        schema: 'BlueprintHelper.TaskPlan.v1',
        task_name: 'CLI Preview',
        task_type: 'edit_blueprint_graph',
        context_id: 'ctx_001',
        target_assets: ['/Game/BP_Player'],
        execution_policy: { dry_run_mode: 'full', should_compile: true, should_save: false, review_baseline_dirty_asset_policy: 'block' },
        steps: [],
      },
      passed: true,
      issues: [],
      toolResult: {
        ok: true,
        schema: 'BlueprintHelper.ToolResult.v1',
        operation: 'preview_task',
        trace_id: 'trace_cli',
        status: 'dry_run',
        modified: false,
        data: {
          schema: 'BlueprintHelper.TaskPreview.v1',
          preview_id: 'preview_cli_001',
          passed: true,
          blocked: false,
          issues: [],
        },
      },
    }),
    executeTask: async () => { throw new Error('not used'); },
    getTaskResult: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['task', 'preview', '--file', 'task-spec.json', ...ACTIVE_RUNTIME_ARGS, '--artifact-dir', artifactDir],
    cwd: fixturesDir,
    runner,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  assert.equal('schema' in output, false);
  assert.equal(output.operation, 'task.preview');
  assert.equal(output.status, 'preview_passed');
  assert.equal(JSON.stringify(output).includes('TaskPlan.v1'), false);
  assertNoDefaultReturnPolicyFields(output);
  const artifacts = output.artifacts as Record<string, unknown>;
  const fullResult = readJsonFile(artifacts.full_result);
  assertNoDefaultReturnPolicyFields(fullResult);
  const taskPlan = readJsonFile(artifacts.task_plan);
  assertNoDefaultReturnPolicyFields(taskPlan);
});

test('task execute calls the TaskSpec runner and returns executed summary', async () => {
  const writes: string[] = [];
  const artifactDir = makeTempDir();
  let executeCalled = false;
  const runner = {
    previewTask: async () => { throw new Error('not used'); },
    executeTask: async () => {
      executeCalled = true;
      return {
        ok: true,
        schema: 'BlueprintHelper.ToolResult.v1',
        operation: 'execute_task',
        trace_id: 'trace_execute',
        status: 'completed',
        modified: true,
        target: { target_type: 'blueprint', asset_path: '/Game/BP_Player' },
        data: {
          schema: 'BlueprintHelper.TaskExecution.v1',
          task_run_id: 'task_cli_001',
          preview_id: 'preview_cli_001',
          task: {
            task_run_id: 'task_cli_001',
            target_assets: ['/Game/BP_Player'],
            applied_steps: 1,
          },
          bridge_result: {
            ok: true,
            schema: 'BlueprintHelper.ToolResult.v1',
            operation: 'execute_task_plan',
            trace_id: 'trace_bridge',
            status: 'applied',
            modified: true,
          },
        },
        debug: {
          bridge_result: {
            ok: true,
            schema: 'BlueprintHelper.ToolResult.v1',
            operation: 'execute_task_plan',
            trace_id: 'trace_bridge',
            status: 'applied',
            modified: true,
          },
        },
      };
    },
    getTaskResult: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['task', 'execute', '--file', 'task-spec.json', ...ACTIVE_RUNTIME_ARGS, '--artifact-dir', artifactDir],
    cwd: fixturesDir,
    runner,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  assert.equal(executeCalled, true);
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  assert.equal(output.operation, 'task.execute');
  assert.equal(output.status, 'executed');
  assert.equal(output.task_run_id, 'task_cli_001');
  assert.equal('preview_id' in output, false);
  const artifacts = output.artifacts as Record<string, unknown>;
  assert.equal('debug_result' in artifacts, false);
  const fullResultPath = String(artifacts.full_result);
  const fullResult = JSON.parse(fs.readFileSync(fullResultPath, 'utf8')) as Record<string, unknown>;
  assert.equal('schema' in fullResult, false);
  assertNoDefaultReturnPolicyFields(output);
  assertNoDefaultReturnPolicyFields(fullResult);
  const toolResult = fullResult.toolResult as Record<string, unknown>;
  assert.equal('schema' in toolResult, false);
  assert.equal('trace_id' in toolResult, false);
  const data = toolResult.data as Record<string, unknown>;
  const task = data.task as Record<string, unknown>;
  assert.equal('preview_id' in data, false);
  assert.equal('schema' in data, false);
  assert.equal('bridge_result' in data, false);
  assert.equal(data.task_run_id, 'task_cli_001');
  assert.equal('task_run_id' in task, false);
  assert.equal('target_assets' in task, false);
  assert.deepEqual(toolResult.target, { target_type: 'blueprint', asset_path: '/Game/BP_Player' });
});

test('task execute exposes raw bridge and trace data only through expert debug artifact', async () => {
  const writes: string[] = [];
  const artifactDir = makeTempDir();
  const runner = {
    previewTask: async () => { throw new Error('not used'); },
    executeTask: async () => ({
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'execute_task',
      trace_id: 'trace_execute_debug',
      status: 'completed',
      modified: true,
      target: { target_type: 'blueprint', asset_path: '/Game/BP_Player' },
      data: {
        schema: 'BlueprintHelper.TaskExecution.v1',
        task_run_id: 'task_cli_debug',
        task: {
          task_run_id: 'task_cli_debug',
          target_assets: ['/Game/BP_Player'],
          applied_steps: 1,
        },
      },
      debug: {
        bridge_result: {
          ok: true,
          schema: 'BlueprintHelper.ToolResult.v1',
          operation: 'execute_task_plan',
          trace_id: 'trace_bridge_debug',
          status: 'applied',
          modified: true,
        },
      },
    }),
    getTaskResult: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['task', 'execute', '--file', 'task-spec.json', ...ACTIVE_RUNTIME_ARGS, '--artifact-dir', artifactDir, '--expert'],
    cwd: fixturesDir,
    runner,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  const artifacts = output.artifacts as Record<string, unknown>;
  assert.equal(typeof artifacts.debug_result, 'string');

  const fullResult = JSON.parse(fs.readFileSync(String(artifacts.full_result), 'utf8')) as Record<string, unknown>;
  assert.equal(JSON.stringify(fullResult).includes('trace_bridge_debug'), false);
  assert.equal(JSON.stringify(fullResult).includes('BlueprintHelper.ToolResult.v1'), false);

  const debugResult = JSON.parse(fs.readFileSync(String(artifacts.debug_result), 'utf8')) as Record<string, unknown>;
  assert.equal(debugResult.schema, 'BlueprintHelper.CliDebugResult.v1');
  assert.equal((debugResult.tool_result as Record<string, unknown>).trace_id, 'trace_execute_debug');
  assert.equal((debugResult.bridge_result as Record<string, unknown>).trace_id, 'trace_bridge_debug');
});

test('task execute can project stdout to selected fields only', async () => {
  const writes: string[] = [];
  const runner = {
    previewTask: async () => { throw new Error('not used'); },
    executeTask: async () => ({
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'execute_task',
      trace_id: 'trace_execute',
      status: 'completed',
      modified: true,
      data: {
        schema: 'BlueprintHelper.TaskExecution.v1',
        task_run_id: 'task_cli_002',
        task: {
          task_run_id: 'task_cli_002',
          target_assets: ['/Game/BP_Player'],
          applied_steps: 1,
        },
      },
    }),
    getTaskResult: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['task', 'execute', '--file', 'task-spec.json', ...ACTIVE_RUNTIME_ARGS, '--fields', 'status,task_run_id,artifacts.full_result'],
    cwd: fixturesDir,
    runner,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  assert.deepEqual(Object.keys(output).sort(), ['artifacts', 'status', 'task_run_id']);
  assert.equal(output.status, 'executed');
  assert.equal(output.task_run_id, 'task_cli_002');
  assert.equal(typeof (output.artifacts as Record<string, unknown>).full_result, 'string');
});

test('removed direct preview command reports grouped replacement without dispatch', async () => {
  const writes: string[] = [];
  const errors: string[] = [];
  const runner = {
    readReferenceContext: async () => { throw new Error('not used'); },
    previewTask: async () => { throw new Error('removed direct preview must not dispatch'); },
    executeTask: async () => { throw new Error('not used'); },
    getTaskResult: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['blueprinthelper_preview_task', '--file', 'task-spec.json', '--select', 'status,preview_id,artifacts.full_result'],
    cwd: fixturesDir,
    runner,
    stdout: (line) => writes.push(line),
    stderr: (line) => errors.push(line),
  });

  assert.equal(exitCode, 64);
  assert.equal(writes.join(''), '');
  assert.match(errors.join(''), /blueprinthelper_preview_task direct CLI command was removed/);
  assert.match(errors.join(''), /bh task preview --file <task-spec\.json>/);
});

test('default task preview artifacts omit internal policy fields', async () => {
  const writes: string[] = [];
  const artifactDir = makeTempDir();
  const taskPlan = {
    schema: 'BlueprintHelper.TaskPlan.v1',
    task_name: 'Policy Reduction Preview',
    task_type: 'edit_blueprint_graph',
    context_id: 'ctx_policy',
    target_assets: ['/Game/BP_Player'],
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
      should_compile: false,
      should_save: false,
      review_baseline_dirty_asset_policy: 'block',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
    steps: [],
  };
  const runner = {
    readReferenceContext: async () => { throw new Error('not used'); },
    previewTask: async () => ({
      previewId: 'preview_policy_001',
      previewToken: '11111111111111111111111111111111',
      taskPlan,
      passed: true,
      issues: [],
      toolResult: {
        ok: true,
        schema: 'BlueprintHelper.ToolResult.v1',
        operation: 'preview_task',
        trace_id: 'trace_policy_preview',
        status: 'dry_run',
        modified: false,
        data: {
          schema: 'BlueprintHelper.TaskPreview.v1',
          preview_id: 'preview_policy_001',
          passed: true,
          blocked: false,
          task_plan: taskPlan,
          validation: {
            should_compile: false,
            should_save: false,
          },
          issues: [],
        },
      },
    }),
    executeTask: async () => { throw new Error('not used'); },
    getTaskResult: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['task', 'preview', '--file', 'task-spec.json', ...ACTIVE_RUNTIME_ARGS, '--artifact-dir', artifactDir],
    cwd: fixturesDir,
    runner,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  assertNoDefaultReturnPolicyFields(output);
  const artifacts = output.artifacts as Record<string, unknown>;
  assertNoDefaultReturnPolicyFields(readJsonFile(artifacts.full_result));
  assertNoDefaultReturnPolicyFields(readJsonFile(artifacts.task_plan));
});

test('expert task preview debug artifact keeps raw policy fields', async () => {
  const writes: string[] = [];
  const artifactDir = makeTempDir();
  const taskPlan = {
    schema: 'BlueprintHelper.TaskPlan.v1',
    task_name: 'Expert Policy Preview',
    task_type: 'edit_blueprint_graph',
    context_id: 'ctx_expert_policy',
    target_assets: ['/Game/BP_Player'],
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
      should_compile: false,
      should_save: false,
      review_baseline_dirty_asset_policy: 'block',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
    steps: [],
  };
  const runner = {
    readReferenceContext: async () => { throw new Error('not used'); },
    previewTask: async () => ({
      previewId: 'preview_expert_policy_001',
      previewToken: '22222222222222222222222222222222',
      taskPlan,
      passed: true,
      issues: [],
      toolResult: {
        ok: true,
        schema: 'BlueprintHelper.ToolResult.v1',
        operation: 'preview_task',
        trace_id: 'trace_expert_policy_preview',
        status: 'dry_run',
        modified: false,
        data: {
          schema: 'BlueprintHelper.TaskPreview.v1',
          preview_id: 'preview_expert_policy_001',
          passed: true,
          blocked: false,
          task_plan: taskPlan,
          validation: {
            should_compile: false,
            should_save: false,
          },
          issues: [],
        },
      },
    }),
    executeTask: async () => { throw new Error('not used'); },
    getTaskResult: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['task', 'preview', '--file', 'task-spec.json', ...ACTIVE_RUNTIME_ARGS, '--artifact-dir', artifactDir, '--expert'],
    cwd: fixturesDir,
    runner,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  assertNoDefaultReturnPolicyFields(output);
  const artifacts = output.artifacts as Record<string, unknown>;
  assertNoDefaultReturnPolicyFields(readJsonFile(artifacts.full_result));
  assertNoDefaultReturnPolicyFields(readJsonFile(artifacts.task_plan));
  const debugResult = readJsonFile(artifacts.debug_result);
  const debugText = JSON.stringify(debugResult);
  assert.match(debugText, /"schema":"BlueprintHelper\.CliDebugResult\.v1"/);
  assert.match(debugText, /"execution_policy"/);
  assert.match(debugText, /"scope_policy"/);
  assert.match(debugText, /"should_compile"/);
  assert.match(debugText, /"should_save"/);
});

test('select is an alias for fields', async () => {
  const writes: string[] = [];
  const bridge = {
    sendCommand: async (): Promise<BridgeResponse> => ({
      request_id: 'bridge_ping',
      success: true,
      result: { status: 'completed' },
    }),
  };

  const exitCode = await runCli({
    argv: ['bridge', 'ping', '--select', 'status'],
    cwd: fixturesDir,
    bridge,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  assert.deepEqual(JSON.parse(writes.join('')), { status: 'bridge_available' });
});

test('develop timing applies to direct CLI tool invocation', async () => {
  const writes: string[] = [];
  const exitCode = await runCli({
    argv: ['blueprinthelper_read_context_capabilities', '--json', '{}', '--develop', '--format', 'json'],
    cwd: fixturesDir,
    bridge: {
      sendCommand: async () => { throw new Error('not used'); },
    },
    runner: {
      previewTask: async () => { throw new Error('not used'); },
      executeTask: async () => { throw new Error('not used'); },
      getTaskResult: async () => { throw new Error('not used'); },
      readReferenceContext: async () => { throw new Error('not used'); },
    } as TaskSpecRunner,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const output = JSON.parse(writes.join('')) as Record<string, any>;
  const timing = output.tool_result.data.timing as Record<string, any>;
  assert.equal(timing.source, 'agentface_cli');
  assert.equal(timing.operation, 'cli_command');
  assert.ok((timing.stages as Array<Record<string, unknown>>).some((stage) => stage.name === 'cli.invoke_tool'));
});

test('develop timing records read_context logic_flow stages and UE nested timing', async () => {
  const writes: string[] = [];
  const calls: Array<{ command: string; payload: Record<string, unknown> }> = [];
  const exitCode = await runCli({
    argv: ['context', 'read', '--json', JSON.stringify({
      schema: 'BlueprintHelper.ReadSpec.v1',
      read_type: 'blueprint_logic',
      target: {
        asset_path: '/Game/BP_Player',
        target_type: 'event',
        target_name: 'BeginPlay',
      },
      view: {
        format: 'logic_flow',
      },
    }), '--develop', '--format', 'json'],
    cwd: fixturesDir,
    bridge: {
      sendCommand: async (command: string, payload: Record<string, unknown>): Promise<BridgeResponse> => {
        calls.push({ command, payload });
        return {
          request_id: 'read_context_logic_flow_timing',
          success: true,
          result: {
            data: {
              schema: 'LogicJson.v1',
              logic: {
                nodes: [
                  { node_ref: 'nodes[0]', kind: 'event', name: 'BeginPlay' },
                  { node_ref: 'nodes[1]', name: 'InitGame' },
                ],
                links: [
                  { type: 'exec', from_node: 'nodes[0]', from_pin: 'then', to_node: 'nodes[1]', to_pin: 'execute' },
                ],
              },
              stats: {
                nodes: 2,
                exec_links: 1,
                data_links: 0,
              },
              timing: {
                schema: 'BlueprintHelper.TimingTrace.v1',
                source: 'ue_bridge_router',
                operation: 'read_blueprint_logic_json',
                timing_id: 'ue_timing_001',
                total_ms: 4.2,
                stages: [
                  { name: 'route_execute', started_at_ms: 0, duration_ms: 4.2 },
                ],
              },
            },
          },
        };
      },
    },
    runner: {
      previewTask: async () => { throw new Error('not used'); },
      executeTask: async () => { throw new Error('not used'); },
      getTaskResult: async () => { throw new Error('not used'); },
      readReferenceContext: async () => { throw new Error('not used'); },
    } as TaskSpecRunner,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  assert.equal(calls.length, 1);
  assert.equal(calls[0].command, 'read_blueprint_logic_json');
  assert.equal(calls[0].payload['include_timing'], true);
  assert.equal(calls[0].payload['target_type'], 'event');
  assert.equal(calls[0].payload['target_name'], 'BeginPlay');
  assert.equal(calls[0].payload['scope'], 'target_event');

  const output = JSON.parse(writes.join('')) as Record<string, any>;
  const toolResult = output.tool_result as Record<string, any>;
  const payload = toolResult.data.payload as Record<string, unknown>;
  assert.equal(payload['schema'], 'LogicFlow.v1');
  assert.equal(payload['mode'], 'execflow');
  assert.equal(Object.hasOwn(payload, 'timing'), false);

  const timing = toolResult.data.timing as Record<string, any>;
  const stageNames = (timing.stages as Array<Record<string, unknown>>).map((stage) => stage.name);
  for (const expected of [
    'cli.parse_args',
    'cli.read_context',
    'read_context.parse_input',
    'read_context.resolve_bridge_request',
    'read_context.build_bridge_payload',
    'read_context.bridge_send_receive',
    'read_context.bridge_payload_extract',
    'read_context.ue_timing_extract',
    'read_context.logic_project_payload',
    'read_context.result_wrap',
    'cli.result_return',
  ]) {
    assert.ok(stageNames.includes(expected), expected);
  }

  const nested = timing.nested as Array<Record<string, unknown>>;
  assert.ok(nested.some((entry) => (
    entry['name'] === 'ue.read_blueprint_logic_json'
    && entry['source'] === 'ue_bridge_router'
    && entry['operation'] === 'read_blueprint_logic_json'
  )));
});

test('omit removes selected fields from compact output', async () => {
  const writes: string[] = [];
  const bridge = {
    sendCommand: async (): Promise<BridgeResponse> => ({
      request_id: 'bridge_ping',
      success: true,
      result: { status: 'completed' },
    }),
  };

  const exitCode = await runCli({
    argv: ['bridge', 'ping', '--omit', 'operation,status'],
    cwd: fixturesDir,
    bridge,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  assert.equal('operation' in output, false);
  assert.equal('status' in output, false);
  assert.equal(output.ok, true);
});

test('invalid field path exits 64', async () => {
  const stderr: string[] = [];
  const exitCode = await runCli({
    argv: ['bridge', 'ping', '--fields', 'status,$schema'],
    cwd: fixturesDir,
    bridge: {
      sendCommand: async () => { throw new Error('not used'); },
    },
    stdout: () => {},
    stderr: (line) => stderr.push(line),
  });

  assert.equal(exitCode, 64);
  assert.match(stderr.join(''), /Invalid field path/);
});

test('task result reads by id and prints compact summary', async () => {
  const writes: string[] = [];
  const runner = {
    previewTask: async () => { throw new Error('not used'); },
    executeTask: async () => { throw new Error('not used'); },
    getTaskResult: async (taskRunId: string) => ({
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'get_task_result',
      trace_id: 'trace_result',
      status: 'completed',
      modified: false,
      data: {
        schema: 'BlueprintHelper.TaskRunJournal.v1',
        task_run_id: taskRunId,
        task_type: 'edit_blueprint_graph',
        target_assets: ['/Game/BP_Player'],
        steps: [{ step_id: 'step_1' }],
      },
    }),
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['task', 'result', '--id', 'task_cli_001'],
    cwd: fixturesDir,
    runner,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  assert.equal(output.operation, 'task.result');
  assert.equal(output.status, 'result_found');
  assert.equal(output.task_run_id, 'task_cli_001');
  assert.equal(JSON.stringify(output).includes('step_1'), false);
});

test('removed direct get task result command reports grouped replacement', async () => {
  const writes: string[] = [];
  const errors: string[] = [];
  const runner = {
    previewTask: async () => { throw new Error('not used'); },
    executeTask: async () => { throw new Error('not used'); },
    getTaskResult: async () => { throw new Error('removed direct task result must not dispatch'); },
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['blueprinthelper_get_task_result', '--id', 'task_cli_002', '--select', 'operation,status,task_run_id'],
    cwd: fixturesDir,
    runner,
    stdout: (line) => writes.push(line),
    stderr: (line) => errors.push(line),
  });

  assert.equal(exitCode, 64);
  assert.equal(writes.join(''), '');
  assert.match(errors.join(''), /blueprinthelper_get_task_result direct CLI command was removed/);
  assert.match(errors.join(''), /bh task result --id <task_run_id>/);
});

test('removed direct get task result rejects before parsing escaped JSON quotes', async () => {
  const writes: string[] = [];
  const errors: string[] = [];
  const runner = {
    previewTask: async () => { throw new Error('not used'); },
    executeTask: async () => { throw new Error('not used'); },
    getTaskResult: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['blueprinthelper_get_task_result', '--json', '{\\"task_run_id\\":\\"task_cli_003\\"}'],
    cwd: fixturesDir,
    runner,
    stdout: (line) => writes.push(line),
    stderr: (line) => errors.push(line),
  });

  assert.equal(exitCode, 64);
  assert.equal(writes.join(''), '');
  assert.match(errors.join(''), /blueprinthelper_get_task_result direct CLI command was removed/);
});

test('unknown commands exit 64', async () => {
  const stderr: string[] = [];
  const exitCode = await runCli({
    argv: ['task', 'unknown'],
    cwd: fixturesDir,
    runner: {} as TaskSpecRunner,
    stdout: () => {},
    stderr: (line) => stderr.push(line),
  });

  assert.equal(exitCode, 64);
  assert.match(stderr.join(''), /Unsupported BlueprintHelper CLI command/);
});

test('bridge call allows read-only commands and rejects raw write commands', async () => {
  const writes: string[] = [];
  const calls: string[] = [];
  const bridge = {
    sendCommand: async (command: string): Promise<BridgeResponse> => {
      calls.push(command);
      return {
        request_id: 'bridge_call',
        success: true,
        result: { status: 'completed', data: { schema: 'RuntimeProfile.v1' } },
      };
    },
  };

  const okExit = await runCli({
    argv: ['bridge', 'call', '--command', 'get_runtime_profile', '--expert'],
    cwd: fixturesDir,
    bridge,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(okExit, 0);
  assert.deepEqual(calls, ['get_runtime_profile']);

  for (const command of ['execute_task_plan', 'preview_task_plan', 'import_agent_graph']) {
    const exitCode = await runCli({
      argv: ['bridge', 'call', '--command', command, '--expert'],
      cwd: fixturesDir,
      bridge,
      stdout: () => {},
      stderr: () => {},
    });
    assert.equal(exitCode, 64, command);
  }
});

test('bridge ping reports bridge availability through compact output', async () => {
  const writes: string[] = [];
  const bridge = {
    sendCommand: async (command: string): Promise<BridgeResponse> => {
      assert.equal(command, 'ping');
      return {
        request_id: 'bridge_ping',
        success: true,
        result: { status: 'completed', data: { schema: 'BridgePing.v1' } },
      };
    },
  };

  const exitCode = await runCli({
    argv: ['bridge', 'ping'],
    cwd: fixturesDir,
    bridge,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  assert.equal(output.operation, 'bridge.ping');
  assert.equal(output.status, 'bridge_available');
});

test('missing option values exit 64 without throwing', async () => {
  const stderr: string[] = [];
  const exitCode = await runCli({
    argv: ['task', 'preview', '--file'],
    cwd: fixturesDir,
    runner: {} as TaskSpecRunner,
    stdout: () => {},
    stderr: (line) => stderr.push(line),
  });

  assert.equal(exitCode, 64);
  assert.match(stderr.join(''), /Missing value for --file/);
});

test('context read uses ReadContext Bridge route', async () => {
  const writes: string[] = [];
  const calls: Array<{ command: string; payload?: Record<string, unknown> }> = [];
  const runner = {
    previewTask: async () => { throw new Error('not used'); },
    executeTask: async () => { throw new Error('not used'); },
    getTaskResult: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['context', 'read', '--file', 'context-request.json'],
    cwd: fixturesDir,
    runner,
    bridge: {
      sendCommand: async (command: string, payload?: Record<string, unknown>): Promise<BridgeResponse> => {
        calls.push({ command, payload });
        return {
          request_id: 'context_read_alias',
          success: true,
          result: {
            schema: 'LogicJson.v1',
            scope: 'target_graph',
            logic: { graph: 'EventGraph', nodes: [] },
            stats: { nodes: 0, exec_links: 0, data_links: 0, orphan_nodes: 0 },
          },
        };
      },
    },
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  assert.equal(calls[0].command, 'read_blueprint_logic_json');
  assert.equal(calls[0].payload?.['target_type'], 'graph');
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  assert.equal(output.operation, 'context.read');
  assert.equal(output.status, 'completed');
});

test('develop timing applies to context read command', async () => {
  const writes: string[] = [];
  const bridge = {
    sendCommand: async (): Promise<BridgeResponse> => ({
      request_id: 'context_read_develop',
      success: true,
      result: {
        schema: 'LogicJson.v1',
        scope: 'target_graph',
        logic: { graph: 'EventGraph', nodes: [] },
        stats: { nodes: 0, exec_links: 0, data_links: 0, orphan_nodes: 0 },
      },
    }),
  };
  const runner = {
    previewTask: async () => { throw new Error('not used'); },
    executeTask: async () => { throw new Error('not used'); },
    getTaskResult: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['context', 'read', '--file', 'context-request.json', '--develop', '--format', 'json'],
    cwd: fixturesDir,
    runner,
    bridge,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const output = JSON.parse(writes.join('')) as Record<string, any>;
  const timing = output.tool_result.data.timing as Record<string, any>;
  assert.equal(timing.source, 'agentface_cli');
  assert.ok((timing.stages as Array<Record<string, unknown>>).some((stage) => stage.name === 'cli.read_context'));
  assert.ok((timing.stages as Array<Record<string, unknown>>).some((stage) => stage.name === 'read_context.parse_input'));
});

test('context read artifact escapes localized node names as ascii-safe JSON', async () => {
  const writes: string[] = [];
  const artifactDir = makeTempDir();
  const runner = {
    previewTask: async () => { throw new Error('not used'); },
    executeTask: async () => { throw new Error('not used'); },
    getTaskResult: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['context', 'read', '--file', 'context-request.json', '--artifact-dir', artifactDir],
    cwd: fixturesDir,
    runner,
    bridge: {
      sendCommand: async (): Promise<BridgeResponse> => ({
        request_id: 'context_read_localized',
        success: true,
        result: {
          schema: 'LogicJson.v1',
          scope: 'target_graph',
          logic: {
            graph: 'EventGraph',
            nodes: [
              { kind: 'call_function', name: '\u6253\u5370\u5b57\u7b26\u4e32' },
              { kind: 'branch', name: '\u5206\u652f' },
            ],
          },
          stats: { nodes: 2, exec_links: 0, data_links: 0, orphan_nodes: 0 },
        },
      }),
    },
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  const artifacts = output.artifacts as Record<string, unknown>;
  const fullResult = String(artifacts.full_result);
  const rawUtf8 = fs.readFileSync(fullResult, 'utf8');
  assert.match(rawUtf8, /\\u6253\\u5370\\u5b57\\u7b26\\u4e32/);
  assert.equal(rawUtf8.includes(String.fromCharCode(0x95f9)), false);
  JSON.parse(rawUtf8);
  JSON.parse(fs.readFileSync(fullResult, 'latin1'));
});

