import assert from 'node:assert/strict';
import { access, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import type { MetricsEvent } from './metrics-types.js';
import { createMetricsService } from './metrics-service.js';

test('createMetricsService returns disabled no-op without creating files', async (t) => {
  const tempRoot = await createTempDir(t);
  const root = path.join(tempRoot, 'disabled-metrics');
  const service = createMetricsService({
    root,
    disabled: true,
    now: () => new Date('2026-06-03T12:00:00.000Z'),
  });

  await service.record(createEvent());
  await service.collector.recordTaskPreviewCompleted({
    taskSpec: createTaskSpec(),
    passed: true,
  });

  assert.equal(service.enabled, false);
  assert.equal(await exists(root), false);
});

test('createMetricsService records events and episode attempts when enabled', async (t) => {
  const tempRoot = await createTempDir(t);
  const root = path.join(tempRoot, 'enabled-metrics');
  const service = createMetricsService({
    root,
    now: () => new Date('2026-06-03T12:00:00.000Z'),
  });

  await service.record(createEvent({
    event_type: 'taskspec_preview_completed',
    status: 'failed',
    error_code: 'unsupported_scope_policy',
    error_category: 'capability_boundary',
  }));

  const jsonl = await readFile(path.join(root, 'events', '2026-06-03.jsonl'), 'utf8');
  const openEpisodes = JSON.parse(await readFile(path.join(root, 'episodes', 'open.json'), 'utf8')) as {
    episodes: Array<Record<string, unknown>>;
  };

  assert.equal(service.enabled, true);
  assert.equal(JSON.parse(jsonl.trim()).event_type, 'taskspec_preview_completed');
  assert.equal(openEpisodes.episodes.length, 1);
  assert.equal(openEpisodes.episodes[0]?.['preview_attempts'], 1);
});

test('createMetricsService closes stale episodes with the default 24h TTL', async (t) => {
  const tempRoot = await createTempDir(t);
  const root = path.join(tempRoot, 'stale-metrics');
  let now = new Date('2026-06-01T00:00:00.000Z');
  const service = createMetricsService({
    root,
    now: () => now,
  });

  await service.record(createEvent({
    timestamp: '2026-06-01T00:00:00.000Z',
    event_type: 'taskspec_preview_completed',
    status: 'failed',
    error_code: 'taskspec_semantic_invalid',
    error_category: 'parameter_error',
  }));

  now = new Date('2026-06-02T01:00:00.000Z');
  await service.record(createEvent({
    timestamp: '2026-06-02T01:00:00.000Z',
    event_type: 'tool_completed',
    status: 'success',
    task_key: undefined,
    task_spec_hash: undefined,
  }));

  const openEpisodes = await service.store?.readOpenEpisodes();
  const closedEpisodes = await service.store?.readClosedEpisodes('all');

  assert.equal(openEpisodes?.length, 0);
  assert.equal(closedEpisodes?.length, 1);
  assert.equal(closedEpisodes?.[0]?.close_reason, 'stale_open');
});

test('createMetricsService swallows store write failures', async (t) => {
  const tempRoot = await createTempDir(t);
  const rootAsFile = path.join(tempRoot, 'metrics-root-is-file');
  await writeFile(rootAsFile, 'not a directory', 'utf8');
  const service = createMetricsService({ root: rootAsFile });

  await assert.doesNotReject(async () => {
    await service.record(createEvent());
  });
});

async function createTempDir(t: { after(callback: () => Promise<void>): void }): Promise<string> {
  const root = await mkdtemp(path.join(os.tmpdir(), 'blueprinthelper-metrics-service-'));
  t.after(async () => {
    await rm(root, { recursive: true, force: true });
  });
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

function createTaskSpec(): never {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
    feature_name: 'StoneGateActivation',
    target: {
      target_type: 'blueprint',
      asset_path: '/Game/Blueprints/BP_StoneGate',
    },
  } as never;
}

function createEvent(overrides: Partial<MetricsEvent> = {}): MetricsEvent {
  return {
    schema: 'BlueprintHelper.MetricsEvent.v1',
    timestamp: '2026-06-03T12:00:00.000Z',
    event_type: 'tool_completed',
    tool_name: 'blueprinthelper_preview_task',
    task_key: {
      task_type: 'edit_blueprint_graph',
      feature_name: 'StoneGateActivation',
      target_type: 'blueprint',
      target_ref_hash: 'sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
      target_ref_label: '/Game/Blueprints/BP_StoneGate',
    },
    task_spec_hash: 'sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb',
    status: 'success',
    correctness_basis: 'not_applicable',
    ...overrides,
  };
}
