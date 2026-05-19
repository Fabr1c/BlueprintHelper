import { strict as assert } from 'node:assert';
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
import test from 'node:test';
import { runCli } from '../../cli/run.js';
import type { BridgeResponse } from '@blueprinthelper/task-core/bridge/bridge-client';
import type { TaskSpecRunner } from '@blueprinthelper/task-core/task/service/task-spec-runner';

const fixturesDir = path.resolve(import.meta.dirname, '..', '..', '..', 'src', 'tests', 'fixtures');

function makeTempDir() {
  return fs.mkdtempSync(path.join(os.tmpdir(), 'bph-cli-test-'));
}

test('task preview reads TaskSpec file and prints compact summary JSON', async () => {
  const writes: string[] = [];
  const artifactDir = makeTempDir();
  const runner = {
    previewTask: async () => ({
      previewId: 'preview_cli_001',
      previewToken: {
        preview_id: 'preview_cli_001',
        task_plan_hash: 'task_plan_hash_cli_001',
        task_spec_hash: 'task_spec_hash_cli_001',
        execution_policy_hash: 'execution_policy_hash_cli_001',
        created_at: '2026-05-19T00:00:00.000Z',
      },
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
    readTaskContext: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['task', 'preview', '--file', 'task-spec.json', '--artifact-dir', artifactDir],
    cwd: fixturesDir,
    runner,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  assert.equal(output.schema, 'BlueprintHelper.CliResult.v1');
  assert.equal(output.operation, 'task.preview');
  assert.equal(output.status, 'preview_passed');
  assert.equal(JSON.stringify(output).includes('TaskPlan.v1'), false);
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
    readTaskContext: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['task', 'execute', '--file', 'task-spec.json', '--artifact-dir', artifactDir],
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
  assert.equal(fullResult.schema, 'BlueprintHelper.CliFullResult.v1');
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
    readTaskContext: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['task', 'execute', '--file', 'task-spec.json', '--artifact-dir', artifactDir, '--expert'],
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
    readTaskContext: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['task', 'execute', '--file', 'task-spec.json', '--fields', 'status,task_run_id,artifacts.full_result'],
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

test('direct CLI tool name dispatches blueprinthelper_preview_task through TaskSpec runner', async () => {
  const writes: string[] = [];
  const runner = {
    readTaskContext: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
    previewTask: async () => ({
      previewId: 'preview_direct_001',
      previewToken: {
        preview_id: 'preview_direct_001',
        task_plan_hash: 'task_plan_hash_direct_001',
        task_spec_hash: 'task_spec_hash_direct_001',
        execution_policy_hash: 'execution_policy_hash_direct_001',
        created_at: '2026-05-19T00:00:00.000Z',
      },
      taskPlan: {
        schema: 'BlueprintHelper.TaskPlan.v1',
        task_name: 'Direct Preview',
        task_type: 'edit_blueprint_graph',
        context_id: 'ctx_direct',
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
        trace_id: 'trace_direct',
        status: 'dry_run',
        modified: false,
        data: {
          schema: 'BlueprintHelper.TaskPreview.v1',
          preview_id: 'preview_direct_001',
          passed: true,
          blocked: false,
          issues: [],
        },
      },
    }),
    executeTask: async () => { throw new Error('not used'); },
    getTaskResult: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['blueprinthelper_preview_task', '--file', 'task-spec.json', '--select', 'status,preview_id,artifacts.full_result'],
    cwd: fixturesDir,
    runner,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  assert.equal(output.status, 'preview_passed');
  assert.equal(output.preview_id, 'preview_direct_001');
  assert.equal(typeof (output.artifacts as Record<string, unknown>).full_result, 'string');
  assert.equal('schema' in output, false);
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
      readTaskContext: async () => { throw new Error('not used'); },
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
    argv: ['blueprinthelper_read_context', '--json', JSON.stringify({
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
      readTaskContext: async () => { throw new Error('not used'); },
      readReferenceContext: async () => { throw new Error('not used'); },
    } as TaskSpecRunner,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  assert.equal(calls.length, 1);
  assert.equal(calls[0].command, 'read_blueprint_logic_json');
  assert.equal(calls[0].payload['include_timing'], true);

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
    'cli.invoke_tool',
    'read_context.parse_input',
    'read_context.resolve_format',
    'read_context.resolve_bridge_request',
    'read_context.build_bridge_payload',
    'read_context.bridge.read_blueprint_logic_json',
    'read_context.extract_bridge_payload',
    'read_context.strip_bridge_timing',
    'read_context.logic_flow_build_payload',
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
    readTaskContext: async () => { throw new Error('not used'); },
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

test('direct get task result accepts id alias', async () => {
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
        steps: [],
      },
    }),
    readTaskContext: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['blueprinthelper_get_task_result', '--id', 'task_cli_002', '--select', 'operation,status,task_run_id'],
    cwd: fixturesDir,
    runner,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  assert.deepEqual(JSON.parse(writes.join('')), {
    operation: 'tool.invoke',
    status: 'result_found',
    task_run_id: 'task_cli_002',
  });
});

test('direct get task result rejects escaped JSON quotes', async () => {
  const writes: string[] = [];
  const runner = {
    previewTask: async () => { throw new Error('not used'); },
    executeTask: async () => { throw new Error('not used'); },
    getTaskResult: async () => { throw new Error('not used'); },
    readTaskContext: async () => { throw new Error('not used'); },
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['blueprinthelper_get_task_result', '--json', '{\\"task_run_id\\":\\"task_cli_003\\"}'],
    cwd: fixturesDir,
    runner,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 1);
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  assert.equal(output.status, 'cli_error');
  assert.match(String(output.message), /Expected property name/);
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
    argv: ['bridge', 'call', '--command', 'get_runtime_profile'],
    cwd: fixturesDir,
    bridge,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(okExit, 0);
  assert.deepEqual(calls, ['get_runtime_profile']);

  for (const command of ['execute_task_plan', 'preview_task_plan', 'import_agent_graph']) {
    const exitCode = await runCli({
      argv: ['bridge', 'call', '--command', command],
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

test('context read uses TaskSpec runner readTaskContext', async () => {
  const writes: string[] = [];
  const runner = {
    previewTask: async () => { throw new Error('not used'); },
    executeTask: async () => { throw new Error('not used'); },
    getTaskResult: async () => { throw new Error('not used'); },
    readTaskContext: async () => ({
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'read_task_context',
      trace_id: 'trace_context',
      status: 'completed',
      modified: false,
      data: {
        schema: 'BlueprintHelper.TaskContextPack.v1',
        target: { asset_path: '/Game/BP_Player' },
      },
    }),
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['context', 'read', '--file', 'context-request.json'],
    cwd: fixturesDir,
    runner,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  assert.equal(output.operation, 'context.read');
  assert.equal(output.status, 'completed');
});

test('develop timing applies to context read command', async () => {
  const writes: string[] = [];
  const runner = {
    previewTask: async () => { throw new Error('not used'); },
    executeTask: async () => { throw new Error('not used'); },
    getTaskResult: async () => { throw new Error('not used'); },
    readTaskContext: async () => ({
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'read_task_context',
      trace_id: 'trace_context_develop',
      status: 'completed',
      modified: false,
      data: {
        schema: 'BlueprintHelper.TaskContextPack.v1',
        target: { asset_path: '/Game/BP_Player' },
      },
    }),
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['context', 'read', '--file', 'context-request.json', '--develop', '--format', 'json'],
    cwd: fixturesDir,
    runner,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const output = JSON.parse(writes.join('')) as Record<string, any>;
  const timing = output.tool_result.data.timing as Record<string, any>;
  assert.equal(timing.source, 'agentface_cli');
  assert.ok((timing.stages as Array<Record<string, unknown>>).some((stage) => stage.name === 'context_file_read_parse'));
  assert.ok((timing.stages as Array<Record<string, unknown>>).some((stage) => stage.name === 'cli.read_task_context'));
});

test('context read artifact escapes localized node names as ascii-safe JSON', async () => {
  const writes: string[] = [];
  const artifactDir = makeTempDir();
  const runner = {
    previewTask: async () => { throw new Error('not used'); },
    executeTask: async () => { throw new Error('not used'); },
    getTaskResult: async () => { throw new Error('not used'); },
    readTaskContext: async () => ({
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'read_task_context',
      trace_id: 'trace_context_localized',
      status: 'completed',
      modified: false,
      data: {
        schema: 'BlueprintHelper.TaskContextPack.v1',
        target: { asset_path: '/Game/BP_Player' },
        payload: {
          nodes: [
            { kind: 'call_function', name: '打印字符串' },
            { kind: 'branch', name: '分支' },
          ],
        },
      },
    }),
    readReferenceContext: async () => { throw new Error('not used'); },
  } as TaskSpecRunner;

  const exitCode = await runCli({
    argv: ['context', 'read', '--file', 'context-request.json', '--artifact-dir', artifactDir],
    cwd: fixturesDir,
    runner,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  const artifacts = output.artifacts as Record<string, unknown>;
  const fullResult = String(artifacts.full_result);
  const rawUtf8 = fs.readFileSync(fullResult, 'utf8');
  assert.match(rawUtf8, /\\u6253\\u5370\\u5b57\\u7b26\\u4e32/);
  assert.doesNotMatch(rawUtf8, /打印字符串/);
  JSON.parse(rawUtf8);
  JSON.parse(fs.readFileSync(fullResult, 'latin1'));
});

