import assert from 'node:assert/strict';
import test from 'node:test';

import type { MetricsEvent } from './metrics-types.js';
import type { MetricsEpisode, MetricsStore } from './metrics-store.js';
import type { MetricsReportKind } from './metrics-reporter.js';
import { buildMetricsReport, renderMetricsMarkdown } from './metrics-reporter.js';

test('buildMetricsReport summarizes tool usage errors health and visible unknown rankings', async () => {
  const taskKey = {
    task_type: 'create_blueprint_feature',
    feature_name: 'ReviewPanel_UI',
    target_type: 'blueprint',
    target_ref_hash: 'sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
    target_ref_label: '/Game/UI/WBP_ReviewPanel',
  };
  const store: Pick<MetricsStore, 'readEvents' | 'readOpenEpisodes' | 'readClosedEpisodes'> = {
    async readEvents() {
      return [
        createEvent({ tool_name: 'blueprinthelper_preview_task', status: 'success' }),
        createEvent({
          tool_name: 'blueprinthelper_preview_task',
          status: 'failed',
          error_category: 'unknown',
          error_code: 'mystery_preview_failure',
        }),
        createEvent({
          event_type: 'taskspec_preview_completed',
          tool_name: 'blueprinthelper_preview_task',
          task_key: taskKey,
          capability: 'graph_write',
          semantic_operation: 'call',
          status: 'failed',
          error_category: 'unknown',
          error_code: 'mystery_preview_failure',
        }),
        createEvent({
          tool_name: 'blueprinthelper_execute_task',
          status: 'failed',
          error_category: 'parameter_error',
          error_code: 'missing_required_field',
        }),
      ];
    },
    async readOpenEpisodes() {
      return [];
    },
    async readClosedEpisodes() {
      return [
        {
          schema: 'BlueprintHelper.MetricsEpisode.v1',
          episode_id: 'sha256:episode',
          task_key: taskKey,
          first_attempt_at: '2026-06-03T10:00:00.000Z',
          last_attempt_at: '2026-06-03T10:10:00.000Z',
          preview_attempts: 2,
          execute_attempts: 1,
          failed_attempts: 2,
          success_attempts: 1,
          attempts_to_success: 3,
          closed_at: '2026-06-03T10:12:00.000Z',
          close_reason: 'success',
        } satisfies MetricsEpisode,
      ];
    },
  };

  const report = await buildMetricsReport({
    store,
    window: '7d',
    limit: 10,
    kind: 'report',
  });
  const markdown = renderMetricsMarkdown(report);

  assert.equal(report.schema, 'BlueprintHelper.MetricsReport.v1');
  assert.equal(report.tool_usage.find((entry) => entry.tool_name === 'blueprinthelper_preview_task')?.total, 3);
  assert.equal(report.top_errors.find((entry) => entry.error_category === 'unknown')?.error_code, 'mystery_preview_failure');
  assert.equal(report.error_category_distribution.find((entry) => entry.error_category === 'unknown')?.count, 2);
  assert.equal(report.operation_usage.find((entry) => entry.capability === 'graph_write')?.semantic_operation, 'call');
  assert.equal(report.task_health[0]?.attempts_to_success, 3);
  assert.equal(report.summary.unknown_errors, 2);
  assert.match(markdown, /## Tool Usage/);
  assert.match(markdown, /mystery_preview_failure/);
  assert.match(markdown, /attempts_to_success/);
});

test('top-errors report kind keeps error sections and omits unrelated large sections from markdown', async () => {
  const report = await buildMetricsReport({
    store: createReportStore(),
    window: '7d',
    limit: 10,
    kind: 'top-errors',
  });
  const markdown = renderMetricsMarkdown(report);

  assert.ok(report.top_errors.length > 0);
  assert.ok(report.unknown_errors.length > 0);
  assert.ok(report.error_category_distribution.length > 0);
  assert.deepEqual(report.tool_usage, []);
  assert.deepEqual(report.operation_usage, []);
  assert.deepEqual(report.task_health, []);
  assert.match(markdown, /## Top Errors/);
  assert.match(markdown, /## Unknown Errors/);
  assert.match(markdown, /## Error Category Distribution/);
  assert.doesNotMatch(markdown, /## Tool Usage/);
  assert.doesNotMatch(markdown, /## Operation Usage/);
  assert.doesNotMatch(markdown, /## Task Health/);
});

test('tool-usage report kind keeps usage sections and omits unrelated large sections from markdown', async () => {
  const report = await buildMetricsReport({
    store: createReportStore(),
    window: '7d',
    limit: 10,
    kind: 'tool-usage',
  });
  const markdown = renderMetricsMarkdown(report);

  assert.ok(report.tool_usage.length > 0);
  assert.ok(report.operation_usage.length > 0);
  assert.deepEqual(report.top_errors, []);
  assert.deepEqual(report.unknown_errors, []);
  assert.deepEqual(report.error_category_distribution, []);
  assert.deepEqual(report.task_health, []);
  assert.match(markdown, /## Tool Usage/);
  assert.match(markdown, /## Operation Usage/);
  assert.doesNotMatch(markdown, /## Top Errors/);
  assert.doesNotMatch(markdown, /## Task Health/);
});

test('task-health report kind counts all stale_open episodes before limiting the episode list', async () => {
  const report = await buildMetricsReport({
    store: createReportStore({
      closedEpisodes: [
        createEpisode({ episode_id: 'success', attempts_to_success: 1, close_reason: 'success' }),
        createEpisode({ episode_id: 'stale-1', close_reason: 'stale_open', last_attempt_at: '2026-06-03T10:00:00.000Z' }),
        createEpisode({ episode_id: 'stale-2', close_reason: 'stale_open', last_attempt_at: '2026-06-03T09:00:00.000Z' }),
        createEpisode({ episode_id: 'stale-3', close_reason: 'stale_open', last_attempt_at: '2026-06-03T08:00:00.000Z' }),
      ],
    }),
    window: '7d',
    limit: 2,
    kind: 'task-health',
  });
  const markdown = renderMetricsMarkdown(report);

  assert.equal(report.stale_open.count, 3);
  assert.equal(report.summary.stale_open_episodes, 3);
  assert.equal(report.stale_open.episodes.length, 2);
  assert.ok(report.task_health.length <= 2);
  assert.deepEqual(report.tool_usage, []);
  assert.deepEqual(report.operation_usage, []);
  assert.deepEqual(report.top_errors, []);
  assert.match(markdown, /## Task Health/);
  assert.match(markdown, /## Stale Open/);
  assert.doesNotMatch(markdown, /## Tool Usage/);
  assert.doesNotMatch(markdown, /## Top Errors/);
});

test('report kind keeps the full report section set', async () => {
  const report = await buildMetricsReport({
    store: createReportStore(),
    window: '7d',
    limit: 10,
    kind: 'report',
  });
  const markdown = renderMetricsMarkdown(report);

  assertReportKind(report.kind, 'report');
  assert.ok(report.tool_usage.length > 0);
  assert.ok(report.operation_usage.length > 0);
  assert.ok(report.top_errors.length > 0);
  assert.ok(report.task_health.length > 0);
  assert.match(markdown, /## Tool Usage/);
  assert.match(markdown, /## Top Errors/);
  assert.match(markdown, /## Task Health/);
  assert.match(markdown, /## Operation Usage/);
});

test('top error ordering uses fixed code point comparison for tied rows', async () => {
  const report = await buildMetricsReport({
    store: createReportStore({
      events: [
        createEvent({ status: 'failed', error_category: 'unknown', error_code: 'zeta_error' }),
        createEvent({ status: 'failed', error_category: 'unknown', error_code: 'Alpha_error' }),
        createEvent({ status: 'failed', error_category: 'unknown', error_code: 'ä_error' }),
      ],
    }),
    window: '7d',
    limit: 10,
    kind: 'top-errors',
  });

  assert.deepEqual(report.top_errors.map((entry) => entry.error_code), [
    'Alpha_error',
    'zeta_error',
    'ä_error',
  ]);
});

function createEvent(overrides: Partial<MetricsEvent> = {}): MetricsEvent {
  return {
    schema: 'BlueprintHelper.MetricsEvent.v1',
    timestamp: '2026-06-03T12:00:00.000Z',
    event_type: 'tool_completed',
    tool_name: 'blueprinthelper_preview_task',
    status: 'success',
    ...overrides,
  };
}

function createReportStore(overrides: {
  events?: MetricsEvent[];
  openEpisodes?: MetricsEpisode[];
  closedEpisodes?: MetricsEpisode[];
} = {}): Pick<MetricsStore, 'readEvents' | 'readOpenEpisodes' | 'readClosedEpisodes'> {
  const events = overrides.events ?? [
    createEvent({ tool_name: 'blueprinthelper_preview_task', status: 'success' }),
    createEvent({
      tool_name: 'blueprinthelper_preview_task',
      status: 'failed',
      error_category: 'unknown',
      error_code: 'mystery_preview_failure',
    }),
    createEvent({
      event_type: 'taskspec_preview_completed',
      tool_name: 'blueprinthelper_preview_task',
      task_key: createTaskKey(),
      capability: 'graph_write',
      semantic_operation: 'call',
      status: 'failed',
      error_category: 'unknown',
      error_code: 'mystery_preview_failure',
    }),
    createEvent({
      tool_name: 'blueprinthelper_execute_task',
      capability: 'graph_write',
      semantic_operation: 'set',
      status: 'failed',
      error_category: 'parameter_error',
      error_code: 'missing_required_field',
    }),
  ];
  const closedEpisodes = overrides.closedEpisodes ?? [
    createEpisode({
      episode_id: 'success',
      attempts_to_success: 3,
      close_reason: 'success',
    }),
  ];

  return {
    async readEvents() {
      return events;
    },
    async readOpenEpisodes() {
      return overrides.openEpisodes ?? [];
    },
    async readClosedEpisodes() {
      return closedEpisodes;
    },
  };
}

function createTaskKey(): MetricsEvent['task_key'] {
  return {
    task_type: 'create_blueprint_feature',
    feature_name: 'ReviewPanel_UI',
    target_type: 'blueprint',
    target_ref_hash: 'sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
    target_ref_label: '/Game/.../WBP_ReviewPanel',
  };
}

function createEpisode(overrides: Partial<MetricsEpisode> = {}): MetricsEpisode {
  return {
    schema: 'BlueprintHelper.MetricsEpisode.v1',
    episode_id: 'sha256:episode',
    task_key: createTaskKey()!,
    first_attempt_at: '2026-06-03T10:00:00.000Z',
    last_attempt_at: '2026-06-03T10:10:00.000Z',
    preview_attempts: 2,
    execute_attempts: 1,
    failed_attempts: 2,
    success_attempts: 1,
    closed_at: '2026-06-03T10:12:00.000Z',
    ...overrides,
  };
}

function assertReportKind(actual: MetricsReportKind, expected: MetricsReportKind): void {
  assert.equal(actual, expected);
}
