import assert from 'node:assert/strict';
import { mkdir, mkdtemp, readFile, rm } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import type { MetricsEvent } from './metrics-types.js';
import { createMetricsStore } from './metrics-store.js';

test('record appends jsonl events and readEvents filters by 1d 7d 30d and all windows', async (t) => {
  const root = await createTempDir(t);
  const store = createMetricsStore({
    root,
    now: () => new Date('2026-06-03T12:00:00.000Z'),
  });

  await store.record(createEvent({
    timestamp: '2026-04-01T00:00:00.000Z',
    tool_name: 'older_than_30d',
  }));
  await store.record(createEvent({
    timestamp: '2026-05-10T00:00:00.000Z',
    tool_name: 'within_30d',
  }));
  await store.record(createEvent({
    timestamp: '2026-05-30T12:00:00.000Z',
    tool_name: 'within_7d',
  }));
  await store.record(createEvent({
    timestamp: '2026-06-03T11:00:00.000Z',
    tool_name: 'within_1d',
  }));

  const allEvents = await store.readEvents('all');
  const oneDayEvents = await store.readEvents('1d');
  const sevenDayEvents = await store.readEvents('7d');
  const thirtyDayEvents = await store.readEvents('30d');
  const jsonl = await readFile(path.join(root, 'events', '2026-06-03.jsonl'), 'utf8');

  assert.deepEqual(allEvents.map((event) => event.tool_name), [
    'older_than_30d',
    'within_30d',
    'within_7d',
    'within_1d',
  ]);
  assert.deepEqual(oneDayEvents.map((event) => event.tool_name), ['within_1d']);
  assert.deepEqual(sevenDayEvents.map((event) => event.tool_name), ['within_7d', 'within_1d']);
  assert.deepEqual(thirtyDayEvents.map((event) => event.tool_name), [
    'within_30d',
    'within_7d',
    'within_1d',
  ]);
  assert.equal(jsonl.trim().split('\n').length, 1);
  assert.equal(JSON.parse(jsonl.trim()).schema, 'BlueprintHelper.MetricsEvent.v1');
});

test('episode closes after preview failures then execute validation and readback success', async (t) => {
  const root = await createTempDir(t);
  const store = createMetricsStore({
    root,
    now: () => new Date('2026-06-03T12:00:00.000Z'),
  });

  await store.upsertEpisodeAttempt(createEvent({
    timestamp: '2026-06-03T10:00:00.000Z',
    event_type: 'taskspec_preview_completed',
    status: 'failed',
    error_category: 'capability_boundary',
    error_code: 'unsupported_scope_policy',
  }));
  await store.upsertEpisodeAttempt(createEvent({
    timestamp: '2026-06-03T10:05:00.000Z',
    event_type: 'taskspec_preview_completed',
    status: 'failed',
    error_category: 'parameter_error',
    error_code: 'missing_required_field',
  }));
  await store.upsertEpisodeAttempt(createEvent({
    timestamp: '2026-06-03T10:10:00.000Z',
    event_type: 'taskspec_execute_completed',
    status: 'success',
  }));
  await store.upsertEpisodeAttempt(createEvent({
    timestamp: '2026-06-03T10:11:00.000Z',
    event_type: 'validation_completed',
    status: 'success',
  }));
  await store.upsertEpisodeAttempt(createEvent({
    timestamp: '2026-06-03T10:12:00.000Z',
    event_type: 'readback_completed',
    status: 'success',
  }));

  const openEpisodes = await store.readOpenEpisodes();
  const closedEpisodes = await store.readClosedEpisodes('all');

  assert.equal(openEpisodes.length, 0);
  assert.equal(closedEpisodes.length, 1);
  assert.equal(closedEpisodes[0]?.preview_attempts, 2);
  assert.equal(closedEpisodes[0]?.execute_attempts, 1);
  assert.equal(closedEpisodes[0]?.failed_attempts, 2);
  assert.equal(closedEpisodes[0]?.success_attempts, 1);
  assert.equal(closedEpisodes[0]?.attempts_to_success, 3);
  assert.equal(closedEpisodes[0]?.close_reason, 'success');
  assert.equal(closedEpisodes[0]?.closed_at, '2026-06-03T10:12:00.000Z');
});

test('concurrent record calls serialize jsonl writes', async (t) => {
  const root = await createTempDir(t);
  const store = createMetricsStore({
    root,
    now: () => new Date('2026-06-03T12:00:00.000Z'),
  });

  await Promise.all(Array.from({ length: 20 }, (_, index) => store.record(createEvent({
    timestamp: '2026-06-03T12:00:00.000Z',
    tool_name: `tool_${index}`,
  }))));

  const jsonl = await readFile(path.join(root, 'events', '2026-06-03.jsonl'), 'utf8');
  const lines = jsonl.trim().split('\n');

  assert.equal(lines.length, 20);
  for (const line of lines) {
    assert.equal(JSON.parse(line).schema, 'BlueprintHelper.MetricsEvent.v1');
  }
});

test('multiple store instances serialize open episode updates through a shared lock', async (t) => {
  const root = await createTempDir(t);
  const stores = Array.from({ length: 16 }, () => createMetricsStore({
    root,
    now: () => new Date('2026-06-03T12:00:00.000Z'),
  }));

  await Promise.all(stores.map((store, index) => store.upsertEpisodeAttempt(createEvent({
    timestamp: `2026-06-03T12:00:${index.toString().padStart(2, '0')}.000Z`,
    event_type: 'taskspec_preview_completed',
    status: 'failed',
    task_key: {
      task_type: 'create_blueprint_feature',
      feature_name: `Feature_${index}`,
      target_type: 'blueprint',
      target_ref_hash: `sha256:${index.toString(16).padStart(64, '0')}`,
      target_ref_label: `/Game/.../WBP_${index}`,
    },
    task_spec_hash: `sha256:${(index + 100).toString(16).padStart(64, '0')}`,
  }))));

  const openEpisodes = await stores[0]!.readOpenEpisodes();

  assert.equal(openEpisodes.length, 16);
  assert.deepEqual(
    new Set(openEpisodes.map((episode) => episode.task_key.feature_name)),
    new Set(Array.from({ length: 16 }, (_, index) => `Feature_${index}`)),
  );
});

test('success close keeps an episode open when closed jsonl append fails', async (t) => {
  const root = await createTempDir(t);
  const store = createMetricsStore({
    root,
    now: () => new Date('2026-06-03T12:00:00.000Z'),
  });

  await store.upsertEpisodeAttempt(createEvent({
    timestamp: '2026-06-03T10:00:00.000Z',
    event_type: 'taskspec_preview_completed',
    status: 'failed',
  }));
  await mkdir(path.join(root, 'episodes', 'closed-2026-06.jsonl'), { recursive: true });

  await store.upsertEpisodeAttempt(createEvent({
    timestamp: '2026-06-03T10:10:00.000Z',
    event_type: 'taskspec_execute_completed',
    status: 'success',
  }));
  await store.upsertEpisodeAttempt(createEvent({
    timestamp: '2026-06-03T10:11:00.000Z',
    event_type: 'validation_completed',
    status: 'success',
  }));
  await assert.rejects(() => store.upsertEpisodeAttempt(createEvent({
    timestamp: '2026-06-03T10:12:00.000Z',
    event_type: 'readback_completed',
    status: 'success',
  })));

  await rm(path.join(root, 'episodes', 'closed-2026-06.jsonl'), { recursive: true, force: true });
  const openEpisodes = await store.readOpenEpisodes();

  assert.equal(openEpisodes.length, 1);
  assert.equal(openEpisodes[0]?.execute_succeeded, true);
  assert.equal(openEpisodes[0]?.validation_succeeded, true);
  assert.equal(openEpisodes[0]?.readback_succeeded, true);
  assert.equal(openEpisodes[0]?.close_reason, undefined);
});

async function createTempDir(t: { after(callback: () => void | Promise<void>): void }): Promise<string> {
  const root = await mkdtemp(path.join(os.tmpdir(), 'blueprinthelper-metrics-store-'));
  t.after(() => rm(root, { recursive: true, force: true }));
  return root;
}

function createEvent(overrides: Partial<MetricsEvent> = {}): MetricsEvent {
  return {
    schema: 'BlueprintHelper.MetricsEvent.v1',
    timestamp: '2026-06-03T12:00:00.000Z',
    event_type: 'tool_completed',
    tool_name: 'blueprinthelper_preview_task',
    task_key: {
      task_type: 'create_blueprint_feature',
      feature_name: 'ReviewPanel_UI',
      target_type: 'blueprint',
      target_ref_hash: 'sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
      target_ref_label: '/Game/UI/WBP_ReviewPanel',
    },
    task_spec_hash: 'sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb',
    status: 'success',
    correctness_basis: 'not_applicable',
    ...overrides,
  };
}
