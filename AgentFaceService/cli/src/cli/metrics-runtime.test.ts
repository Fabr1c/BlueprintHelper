import assert from 'node:assert/strict';
import { access, mkdtemp, readFile, readdir, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import { createCliMetricsService } from './metrics-runtime.js';
import { runCli } from './run.js';

test('createCliMetricsService returns disabled no-op without creating metrics root', async (t) => {
  const workspace = await createTempDir(t, 'blueprinthelper-cli-metrics-disabled-');
  const metricsRoot = path.join(workspace, 'custom-metrics');
  const service = createCliMetricsService({
    cwd: workspace,
    env: {
      ...process.env,
      BPH_METRICS_DIR: metricsRoot,
      BPH_METRICS_DISABLED: '1',
    },
    now: () => new Date('2026-06-03T12:00:00.000Z'),
  });

  await service.collector.recordToolCompleted({
    tool_name: 'blueprinthelper_diagnostics',
    status: 'success',
    duration_ms: 5,
    capability: 'blueprinthelper_diagnostics',
    semantic_operation: 'blueprinthelper_diagnostics',
  });

  assert.equal(service.enabled, false);
  assert.equal(await exists(metricsRoot), false);
});

test('createCliMetricsService records jsonl under env metrics root when enabled', async (t) => {
  const workspace = await createTempDir(t, 'blueprinthelper-cli-metrics-enabled-');
  const metricsRoot = path.join(workspace, 'custom-metrics');
  const service = createCliMetricsService({
    cwd: workspace,
    env: {
      ...process.env,
      BPH_METRICS_DIR: metricsRoot,
      BPH_METRICS_EPISODE_TTL_HOURS: '12',
    },
    now: () => new Date('2026-06-03T12:00:00.000Z'),
  });

  await service.collector.recordToolCompleted({
    tool_name: 'blueprinthelper_diagnostics',
    status: 'success',
    duration_ms: 7,
    capability: 'blueprinthelper_diagnostics',
    semantic_operation: 'blueprinthelper_diagnostics',
  });

  const events = await readEvents(metricsRoot);

  assert.equal(service.enabled, true);
  assert.equal(events.length, 1);
  assert.equal(events[0]?.event_type, 'tool_completed');
  assert.equal(events[0]?.tool_name, 'blueprinthelper_diagnostics');
  assert.equal(events[0]?.duration_ms, 7);
});

test('createCliMetricsService uses default 24h stale episode TTL when env is unset', async (t) => {
  const workspace = await createTempDir(t, 'blueprinthelper-cli-metrics-ttl-default-');
  const metricsRoot = path.join(workspace, 'metrics');
  const firstService = createCliMetricsService({
    cwd: workspace,
    env: {
      ...process.env,
      BPH_METRICS_DIR: metricsRoot,
    },
    now: () => new Date('2026-06-01T00:00:00.000Z'),
  });
  await firstService.collector.recordTaskPreviewCompleted({
    taskSpec: createTaskSpec(),
    passed: false,
    capability: 'graph_write',
    semantic_operation: 'append_new_owned_graph',
  });

  const secondService = createCliMetricsService({
    cwd: workspace,
    env: {
      ...process.env,
      BPH_METRICS_DIR: metricsRoot,
    },
    now: () => new Date('2026-06-02T01:00:00.000Z'),
  });
  await secondService.collector.recordToolCompleted({
    tool_name: 'blueprinthelper_diagnostics',
    status: 'success',
    duration_ms: 1,
    capability: 'blueprinthelper_diagnostics',
    semantic_operation: 'blueprinthelper_diagnostics',
  });

  const openEpisodes = await secondService.store?.readOpenEpisodes();
  const closedEpisodes = await secondService.store?.readClosedEpisodes('all');

  assert.equal(openEpisodes?.length, 0);
  assert.equal(closedEpisodes?.length, 1);
  assert.equal(closedEpisodes?.[0]?.close_reason, 'stale_open');
});

test('runCli direct read_context records tool_completed with extracted semantic operation', async (t) => {
  const workspace = await createTempDir(t, 'blueprinthelper-cli-read-context-');
  const metricsRoot = path.join(workspace, 'metrics');
  const readSpecPath = path.join(workspace, 'read-spec.json');
  await writeFile(readSpecPath, JSON.stringify({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP/BP_Metrics',
      target_type: 'blueprint',
    },
  }, null, 2));

  const stdout: string[] = [];
  const stderr: string[] = [];
  const exitCode = await withEnv({
    BPH_METRICS_DIR: metricsRoot,
  }, () => runCli({
    argv: ['context', 'read', '--file', 'read-spec.json', '--format', 'json'],
    cwd: workspace,
    bridge: {
      async sendCommand() {
        return {
          success: true,
          request_id: 'read_context_metrics_request',
          result: {
            data: {
              schema: 'BlueprintHelper.LogicFlow.v1',
              nodes: [],
              edges: [],
            },
          },
        };
      },
    },
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  }));

  const events = await readEvents(metricsRoot);
  const output = JSON.parse(stdout.join(''));

  assert.equal(exitCode, 0);
  assert.deepEqual(stderr, []);
  assert.equal(output.ok, true);
  assert.equal(output.operation, 'context.read');
  assert.equal(events.length, 1);
  assert.equal(events[0]?.event_type, 'tool_completed');
  assert.equal(events[0]?.tool_name, 'blueprinthelper_read_context');
  assert.equal(events[0]?.capability, 'read_context');
  assert.equal(events[0]?.semantic_operation, 'blueprint_logic.logic_flow');
  assert.equal(events[0]?.status, 'success');
});

test('runCli direct tool success records metrics without changing stdout payload', async (t) => {
  const workspace = await createTempDir(t, 'blueprinthelper-cli-direct-tool-');
  const metricsRoot = path.join(workspace, 'metrics');
  const stdout: string[] = [];
  const stderr: string[] = [];

  const exitCode = await withEnv({
    BPH_METRICS_DIR: metricsRoot,
  }, () => runCli({
    argv: ['blueprinthelper_diagnostics', '--json', '{}', '--format', 'json'],
    cwd: workspace,
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  }));

  const events = await readEvents(metricsRoot);
  const output = JSON.parse(stdout.join(''));

  assert.equal(exitCode, 0);
  assert.deepEqual(stderr, []);
  assert.equal(output.ok, true);
  assert.equal(output.operation, 'tool.invoke');
  assert.equal(output.tool_name, 'blueprinthelper_diagnostics');
  assert.equal(events.length, 1);
  assert.equal(events[0]?.tool_name, 'blueprinthelper_diagnostics');
  assert.equal(events[0]?.semantic_operation, 'blueprinthelper_diagnostics');
  assert.equal(events[0]?.status, 'success');
});

test('runCli direct tool malformed --json records failed metrics event with input guidance', async (t) => {
  const workspace = await createTempDir(t, 'blueprinthelper-cli-direct-tool-bad-json-');
  const metricsRoot = path.join(workspace, 'metrics');
  const stdout: string[] = [];
  const stderr: string[] = [];

  const exitCode = await withEnv({
    BPH_METRICS_DIR: metricsRoot,
  }, () => runCli({
    argv: ['blueprinthelper_diagnostics', '--json', '{bad', '--format', 'json'],
    cwd: workspace,
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  }));

  const events = await readEvents(metricsRoot);
  const output = JSON.parse(stdout.join(''));

  assert.equal(exitCode, 1);
  assert.deepEqual(stderr, []);
  assert.equal(output.ok, false);
  assert.equal(output.status, 'cli_error');
  assert.match(output.message, /pipe JSON through --stdin/);
  assert.equal(events.length, 1);
  assert.equal(events[0]?.event_type, 'tool_completed');
  assert.equal(events[0]?.tool_name, 'blueprinthelper_diagnostics');
  assert.equal(events[0]?.status, 'failed');
  assert.equal(events[0]?.error_category, 'parameter_error');
  assert.equal(events[0]?.error_code, 'malformed_json');
});

test('runCli direct tools do not write metrics when BPH_METRICS_DISABLED=1', async (t) => {
  const workspace = await createTempDir(t, 'blueprinthelper-cli-metrics-off-');
  const metricsRoot = path.join(workspace, 'metrics');
  const stdout: string[] = [];
  const stderr: string[] = [];

  const exitCode = await withEnv({
    BPH_METRICS_DIR: metricsRoot,
    BPH_METRICS_DISABLED: '1',
  }, () => runCli({
    argv: ['blueprinthelper_diagnostics', '--json', '{}', '--format', 'json'],
    cwd: workspace,
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  }));

  assert.equal(exitCode, 0);
  assert.deepEqual(stderr, []);
  assert.equal(JSON.parse(stdout.join('')).ok, true);
  assert.equal(await exists(metricsRoot), false);
});

test('runCli task preview records taskspec preview metrics through the default runner', async (t) => {
  const workspace = await createTempDir(t, 'blueprinthelper-cli-task-preview-');
  const metricsRoot = path.join(workspace, 'metrics');
  await writeFile(path.join(workspace, 'task-spec.json'), JSON.stringify(createTaskSpec(), null, 2));

  const stdout: string[] = [];
  const stderr: string[] = [];
  const exitCode = await withEnv({
    BPH_METRICS_DIR: metricsRoot,
  }, () => runCli({
    argv: ['task', 'preview', '--file', 'task-spec.json', '--format', 'json'],
    cwd: workspace,
    bridge: {
      async sendCommand(command) {
        assert.equal(command, 'preview_task_plan');
        return {
          success: true,
          request_id: 'preview_metrics_request',
          result: {
            ok: true,
            schema: 'BlueprintHelper.ToolResult.v1',
            operation: 'preview_task_plan',
            trace_id: 'trace_preview_metrics_request',
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
      },
    },
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  }));

  const events = await readEvents(metricsRoot);
  const output = JSON.parse(stdout.join(''));

  assert.equal(exitCode, 0);
  assert.deepEqual(stderr, []);
  assert.equal(output.ok, true);
  assert.equal(output.operation, 'task.preview');
  assert.equal(events.length, 1);
  assert.equal(events[0]?.event_type, 'taskspec_preview_completed');
  assert.equal(events[0]?.tool_name, 'blueprinthelper_preview_task');
  assert.equal(events[0]?.status, 'success');
});

test('runCli task execute records taskspec execute metrics through the default runner', async (t) => {
  const workspace = await createTempDir(t, 'blueprinthelper-cli-task-execute-');
  const metricsRoot = path.join(workspace, 'metrics');
  await writeFile(path.join(workspace, 'task-spec.json'), JSON.stringify(createTaskSpec(), null, 2));

  const stdout: string[] = [];
  const stderr: string[] = [];
  const bridgeCommands: string[] = [];
  const exitCode = await withEnv({
    BPH_METRICS_DIR: metricsRoot,
  }, () => runCli({
    argv: ['task', 'execute', '--file', 'task-spec.json', '--format', 'json'],
    cwd: workspace,
    bridge: {
      async sendCommand(command) {
        bridgeCommands.push(command);
        if (command === 'preview_task_plan') {
          return {
            success: true,
            request_id: 'execute_metrics_preview_request',
            result: {
              ok: true,
              schema: 'BlueprintHelper.ToolResult.v1',
              operation: 'preview_task_plan',
              trace_id: 'trace_execute_metrics_preview_request',
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
        assert.equal(command, 'execute_task_plan');
        return {
          success: true,
          request_id: 'execute_metrics_execute_request',
          result: {
            ok: true,
            schema: 'BlueprintHelper.ToolResult.v1',
            operation: 'execute_task_plan',
            trace_id: 'trace_execute_metrics_execute_request',
            status: 'completed',
            modified: true,
            data: {
              task_run_id: 'task_cli_metrics_execute',
              target_assets: ['/Game/BP/BP_CliMetrics'],
              steps: [{ step_id: 'step_001', modified: true }],
            },
          },
        };
      },
    },
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  }));

  const events = await readEvents(metricsRoot);
  const output = JSON.parse(stdout.join(''));

  assert.equal(exitCode, 0);
  assert.deepEqual(stderr, []);
  assert.deepEqual(bridgeCommands, ['preview_task_plan', 'execute_task_plan']);
  assert.equal(output.ok, true);
  assert.equal(output.operation, 'task.execute');
  assert.equal(events.length, 1);
  assert.equal(events[0]?.event_type, 'taskspec_execute_completed');
  assert.equal(events[0]?.tool_name, 'blueprinthelper_execute_task');
  assert.equal(events[0]?.status, 'success');
  assert.equal(events[0]?.correctness_basis, 'pending_confirmation');
});

async function createTempDir(
  t: { after(callback: () => void | Promise<void>): void },
  prefix: string,
): Promise<string> {
  const root = await mkdtemp(path.join(os.tmpdir(), prefix));
  t.after(() => rm(root, { recursive: true, force: true }));
  return root;
}

async function exists(target: string): Promise<boolean> {
  try {
    await access(target);
    return true;
  } catch {
    return false;
  }
}

async function readEvents(root: string): Promise<Array<Record<string, unknown>>> {
  const eventsDir = path.join(root, 'events');
  const files = (await readdir(eventsDir)).filter((file) => file.endsWith('.jsonl'));
  const chunks = await Promise.all(files.map(async (file) => readFile(path.join(eventsDir, file), 'utf8')));
  return chunks
    .flatMap((chunk) => chunk.trim().split('\n'))
    .filter((line) => line.length > 0)
    .map((line) => JSON.parse(line) as Record<string, unknown>);
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

function createTaskSpec(): Record<string, unknown> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_cli_metrics_preview',
    task_type: 'edit_blueprint_graph',
    feature_name: 'CliMetricsPreview',
    target: {
      asset_path: '/Game/BP/BP_CliMetrics',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EG_CliMetricsPreview',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [{
        entry_type: 'custom_event',
        name: 'ApplyCliMetrics',
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements: [{
            kind: 'create',
            create_operation: 'make_array',
            pin_type: { category: 'int' },
            args: {
              item: { kind: 'literal', value_type: 'number', value: 42 },
            },
          }],
        },
      }],
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
  };
}
