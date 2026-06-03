import assert from 'node:assert/strict';
import { mkdtemp, readFile, readdir, rm } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import { createMetricsStore } from '@blueprinthelper/task-core/metrics/metrics-store';
import type { MetricsEvent } from '@blueprinthelper/task-core/metrics/metrics-types';
import type { ToolResultBase } from '@blueprinthelper/task-core/result/tool-result';
import { buildHelpText } from './help.js';
import { runMetricsCommand, type MetricsCliCommand } from './metrics-command.js';
import { writeCliResult, type CliCommand } from './output.js';
import { runCli } from './run.js';

test('runMetricsCommand report returns BlueprintHelper.MetricsReport.v1 data', async (t) => {
  const root = await createTempDir(t, 'blueprinthelper-cli-metrics-root-');
  const store = createMetricsStore({
    root,
    now: () => new Date('2026-06-03T12:00:00.000Z'),
  });
  await store.record(createEvent({
    timestamp: '2026-06-03T11:00:00.000Z',
    tool_name: 'blueprinthelper_preview_task',
    status: 'failed',
    error_category: 'unknown',
    error_code: 'mystery_preview_failure',
  }));

  const command: MetricsCliCommand = {
    kind: 'metrics.report',
    format: 'json',
    metricsKind: 'report',
    metricsRoot: root,
    window: 'all',
    limit: 20,
  };

  const result = await runMetricsCommand({ command });

  assert.equal(result.ok, true);
  assert.equal(result.operation, 'metrics.report');
  assert.equal(result.data?.schema, 'BlueprintHelper.MetricsReport.v1');
  assert.equal(result.data?.kind, 'report');
  assert.equal(asRecord(result.data?.summary)?.total_events, 1);
});

test('runMetricsCommand markdown writes reports markdown under metrics root', async (t) => {
  const root = await createTempDir(t, 'blueprinthelper-cli-metrics-markdown-');
  const store = createMetricsStore({
    root,
    now: () => new Date('2026-06-03T12:00:00.000Z'),
  });
  await store.record(createEvent({
    timestamp: '2026-06-03T11:00:00.000Z',
    tool_name: 'blueprinthelper_execute_task',
    status: 'failed',
    error_category: 'parameter_error',
    error_code: 'missing_required_field',
  }));

  const command: MetricsCliCommand = {
    kind: 'metrics.report',
    format: 'markdown',
    metricsKind: 'top-errors',
    metricsRoot: root,
    window: '7d',
    limit: 5,
  };

  const result = await runMetricsCommand({ command });
  const reportsDir = path.join(root, 'reports');
  const reportFiles = await readdir(reportsDir);
  const reportPath = result.data?.markdown_report_path;

  assert.equal(result.ok, true);
  assert.equal(result.operation, 'metrics.report');
  assert.equal(result.data?.schema, 'BlueprintHelper.MetricsReport.v1');
  assert.equal(result.data?.kind, 'top-errors');
  assert.equal(typeof reportPath, 'string');
  assert.equal(path.dirname(reportPath as string), reportsDir);
  assert.equal(reportFiles.length, 1);
  assert.match(reportFiles[0] ?? '', /\.md$/);
  assert.match(await readFile(reportPath as string, 'utf8'), /# BlueprintHelper Metrics Report/);
});

test('runCli parses metrics report json command and writes report data to stdout', async (t) => {
  const workspace = await createTempDir(t, 'blueprinthelper-cli-workspace-');
  const metricsRoot = path.join(workspace, 'Saved', 'BlueprintHelper', 'Metrics');
  const store = createMetricsStore({
    root: metricsRoot,
    now: () => new Date('2026-06-03T12:00:00.000Z'),
  });
  await store.record(createEvent({
    timestamp: '2026-06-03T11:00:00.000Z',
    tool_name: 'blueprinthelper_preview_task',
    status: 'success',
  }));

  const stdout: string[] = [];
  const stderr: string[] = [];
  const exitCode = await runCli({
    argv: ['metrics', 'report', '--window', 'all', '--format', 'json'],
    cwd: workspace,
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  });

  assert.equal(exitCode, 0);
  assert.deepEqual(stderr, []);
  const output = JSON.parse(stdout.join(''));
  assert.equal(output.ok, true);
  assert.equal(output.operation, 'metrics.report');
  assert.equal(output.data.schema, 'BlueprintHelper.MetricsReport.v1');
  assert.equal(output.data.summary.total_events, 1);
});

test('runCli parses metrics tool-usage and task-health json commands', async (t) => {
  const workspace = await createTempDir(t, 'blueprinthelper-cli-workspace-metrics-kinds-');
  const metricsRoot = path.join(workspace, 'Saved', 'BlueprintHelper', 'Metrics');
  const store = createMetricsStore({
    root: metricsRoot,
    now: () => new Date('2026-06-03T12:00:00.000Z'),
  });
  await store.record(createEvent({
    timestamp: '2026-06-03T11:00:00.000Z',
    tool_name: 'blueprinthelper_read_context',
    status: 'success',
  }));
  const previewEvent = createEvent({
    event_type: 'taskspec_preview_completed',
    timestamp: '2026-06-03T11:30:00.000Z',
    tool_name: 'blueprinthelper_preview_task',
    status: 'failed',
    error_category: 'parameter_error',
    error_code: 'taskspec_semantic_invalid',
    task_key: {
      task_type: 'edit_blueprint_graph',
      feature_name: 'MetricsKinds',
      target_type: 'blueprint',
      target_ref_hash: 'sha256:1111111111111111111111111111111111111111111111111111111111111111',
      target_ref_label: '/Game/.../BP_MetricsKinds',
    },
    task_spec_hash: 'sha256:2222222222222222222222222222222222222222222222222222222222222222',
  });
  await store.record(previewEvent);
  await store.upsertEpisodeAttempt(previewEvent);

  const toolUsage = await runCliJson(workspace, ['metrics', 'tool-usage', '--window', 'all', '--limit', '5', '--format', 'json']);
  const taskHealth = await runCliJson(workspace, ['metrics', 'task-health', '--window', 'all', '--limit', '5', '--format', 'json']);

  assert.equal(toolUsage.output.ok, true);
  assert.equal(toolUsage.output.data.kind, 'tool-usage');
  assert.equal(toolUsage.output.data.tool_usage.length, 2);
  assert.equal(toolUsage.output.data.top_errors.length, 0);
  assert.equal(taskHealth.output.ok, true);
  assert.equal(taskHealth.output.data.kind, 'task-health');
  assert.equal(taskHealth.output.data.task_health.length, 1);
  assert.equal(taskHealth.output.data.tool_usage.length, 0);
});

test('metrics help includes grouped metrics commands', () => {
  const globalHelp = buildHelpText();
  const metricsHelp = buildHelpText(['metrics', 'report']);

  assert.match(globalHelp, /bh metrics report --window 7d --format json/);
  assert.match(globalHelp, /bh metrics top-errors --window 7d --format markdown/);
  assert.match(metricsHelp, /BlueprintHelper CLI help: metrics report/);
  assert.match(metricsHelp, /--window 1d\|7d\|30d\|all/);
});

test('runCli parses metrics help target without treating metrics options as help target text', async () => {
  const stdout: string[] = [];
  const stderr: string[] = [];

  const exitCode = await runCli({
    argv: ['metrics', 'report', '--window', 'all', '--limit', '5', '--help'],
    cwd: path.join('D:', 'UEProjects', 'Template', 'Plugins', 'BlueprintHelper'),
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  });

  assert.equal(exitCode, 0);
  assert.deepEqual(stderr, []);
  const helpText = stdout.join('');
  assert.match(helpText, /BlueprintHelper CLI help: metrics report/);
  assert.doesNotMatch(helpText, /No tool-specific help is registered/);
});

test('normal CLI json output does not gain metrics payload shape', () => {
  const stdout: string[] = [];
  const command: CliCommand = {
    kind: 'tool.invoke',
    format: 'json',
    toolName: 'blueprinthelper_diagnostics',
  };
  const toolResult: ToolResultBase = {
    ok: true,
    schema: 'BlueprintHelper.ToolResult.v1',
    operation: 'blueprinthelper_diagnostics',
    trace_id: 'trace_metrics_output_isolation',
    status: 'completed',
    modified: false,
    data: {
      schema: 'BlueprintHelper.Diagnostics.v1',
      status: 'ok',
    },
  };

  writeCliResult({
    cwd: path.join('D:', 'UEProjects', 'Template', 'Plugins', 'BlueprintHelper', 'AgentFaceService', 'cli'),
    stdout: (text) => stdout.push(text),
  }, command, toolResult);

  const output = JSON.parse(stdout.join(''));
  assert.equal(output.ok, true);
  assert.equal(output.operation, 'tool.invoke');
  assert.equal('data' in output, false);
  assert.equal(output.tool_name, 'blueprinthelper_diagnostics');
  assert.equal(output.tool_result.status, 'completed');
  assert.equal(output.tool_result.data.status, 'ok');
});

function createEvent(overrides: Partial<MetricsEvent> = {}): MetricsEvent {
  return {
    schema: 'BlueprintHelper.MetricsEvent.v1',
    timestamp: '2026-06-03T12:00:00.000Z',
    event_type: 'tool_completed',
    tool_name: 'blueprinthelper_preview_task',
    status: 'success',
    correctness_basis: 'not_applicable',
    ...overrides,
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

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return value !== null && typeof value === 'object' && !Array.isArray(value)
    ? value as Record<string, unknown>
    : undefined;
}

async function runCliJson(
  cwd: string,
  argv: string[],
): Promise<{ output: Record<string, any>; stderr: string[] }> {
  const stdout: string[] = [];
  const stderr: string[] = [];
  const exitCode = await runCli({
    argv,
    cwd,
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  });

  assert.equal(exitCode, 0);
  assert.deepEqual(stderr, []);
  return {
    output: JSON.parse(stdout.join('')) as Record<string, any>,
    stderr,
  };
}
