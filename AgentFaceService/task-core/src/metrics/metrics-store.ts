import { randomUUID } from 'node:crypto';
import { appendFile, mkdir, readFile, readdir, rename, rm, writeFile } from 'node:fs/promises';
import path from 'node:path';

import type { MetricsEvent, MetricsTaskKey } from './metrics-types.js';
import { hashStableJson } from './stable-hash.js';

export type MetricsWindow = '1d' | '7d' | '30d' | 'all';
export type MetricsEpisodeCloseReason = 'success' | 'stale_open';

export interface MetricsEpisode {
  schema: 'BlueprintHelper.MetricsEpisode.v1';
  episode_id: string;
  task_key: MetricsTaskKey;
  task_spec_hash?: string;
  first_attempt_at: string;
  last_attempt_at: string;
  preview_attempts: number;
  execute_attempts: number;
  failed_attempts: number;
  success_attempts: number;
  attempts_to_success?: number;
  closed_at?: string;
  close_reason?: MetricsEpisodeCloseReason;
  execute_succeeded?: boolean;
  validation_succeeded?: boolean;
  readback_succeeded?: boolean;
}

export interface CreateMetricsStoreOptions {
  root: string;
  now?: Date | (() => Date);
}

export interface CloseStaleEpisodesOptions {
  staleAfterMs?: number;
}

export interface MetricsStore {
  record(event: MetricsEvent): Promise<void>;
  readEvents(window?: MetricsWindow): Promise<MetricsEvent[]>;
  readOpenEpisodes(): Promise<MetricsEpisode[]>;
  readClosedEpisodes(window?: MetricsWindow): Promise<MetricsEpisode[]>;
  upsertEpisodeAttempt(event: MetricsEvent): Promise<MetricsEpisode | undefined>;
  closeStaleEpisodes(options?: CloseStaleEpisodesOptions): Promise<MetricsEpisode[]>;
}

interface OpenEpisodesFile {
  schema: 'BlueprintHelper.MetricsOpenEpisodes.v1';
  episodes: MetricsEpisode[];
}

const DEFAULT_STALE_AFTER_MS = 24 * 60 * 60 * 1000;
const OPEN_EPISODE_LOCK_RETRY_MS = 10;
const OPEN_EPISODE_LOCK_TIMEOUT_MS = 5_000;

export function createMetricsStore(options: CreateMetricsStoreOptions): MetricsStore {
  const root = options.root;
  const now = createNowProvider(options.now);
  let queue: Promise<unknown> = Promise.resolve();

  return {
    record(event) {
      return enqueue(async () => {
        await appendEvent(root, event);
      });
    },

    readEvents(window = '7d') {
      return enqueue(async () => readEventsUnsafe(root, window, now()));
    },

    readOpenEpisodes() {
      return enqueue(async () => readOpenEpisodesUnsafe(root));
    },

    readClosedEpisodes(window = '7d') {
      return enqueue(async () => readClosedEpisodesUnsafe(root, window, now()));
    },

    upsertEpisodeAttempt(event) {
      return enqueue(async () => withOpenEpisodesLock(root, () => upsertEpisodeAttemptUnsafe(root, event)));
    },

    closeStaleEpisodes(closeOptions = {}) {
      return enqueue(async () => withOpenEpisodesLock(root, () => closeStaleEpisodesUnsafe(
        root,
        now().toISOString(),
        closeOptions.staleAfterMs ?? DEFAULT_STALE_AFTER_MS,
      )));
    },
  };

  function enqueue<T>(operation: () => Promise<T>): Promise<T> {
    const next = queue.then(operation, operation);
    queue = next.catch(() => undefined);
    return next;
  }
}

async function appendEvent(root: string, event: MetricsEvent): Promise<void> {
  const eventsDir = path.join(root, 'events');
  await mkdir(eventsDir, { recursive: true });
  await appendFile(
    path.join(eventsDir, `${datePart(event.timestamp)}.jsonl`),
    `${JSON.stringify(event)}\n`,
    'utf8',
  );
}

async function readEventsUnsafe(root: string, window: MetricsWindow, now: Date): Promise<MetricsEvent[]> {
  const eventsDir = path.join(root, 'events');
  const files = await readJsonlFileNames(eventsDir);
  const events = (await Promise.all(
    files.map((file) => readJsonl<MetricsEvent>(path.join(eventsDir, file))),
  )).flat();
  const cutoff = cutoffTime(window, now);

  return events
    .filter((event) => cutoff === undefined || Date.parse(event.timestamp) >= cutoff)
    .sort((left, right) => compareText(left.timestamp, right.timestamp));
}

async function upsertEpisodeAttemptUnsafe(root: string, event: MetricsEvent): Promise<MetricsEpisode | undefined> {
  if (event.task_key === undefined || !isEpisodeEvent(event)) {
    return undefined;
  }

  const openEpisodes = await readOpenEpisodesUnsafe(root);
  const episodeId = hashStableJson(event.task_key);
  let episode = openEpisodes.find((entry) => entry.episode_id === episodeId);

  if (episode === undefined) {
    if (!isAttemptEvent(event)) {
      return undefined;
    }
    episode = createEpisode(episodeId, event);
    openEpisodes.push(episode);
  }

  applyEpisodeEvent(episode, event);

  if (shouldCloseAsSuccess(episode, event)) {
    const closedEpisode: MetricsEpisode = {
      ...episode,
      closed_at: event.timestamp,
      close_reason: 'success',
      attempts_to_success: episode.preview_attempts + episode.execute_attempts,
    };
    try {
      await appendClosedEpisode(root, closedEpisode);
    } catch (error) {
      await writeOpenEpisodesUnsafe(root, openEpisodes);
      throw error;
    }
    await writeOpenEpisodesUnsafe(root, openEpisodes.filter((entry) => entry.episode_id !== episodeId));
    return closedEpisode;
  }

  await writeOpenEpisodesUnsafe(root, openEpisodes);
  return episode;
}

async function closeStaleEpisodesUnsafe(
  root: string,
  closedAt: string,
  staleAfterMs: number,
): Promise<MetricsEpisode[]> {
  const openEpisodes = await readOpenEpisodesUnsafe(root);
  const cutoff = Date.parse(closedAt) - staleAfterMs;
  const staleEpisodes: MetricsEpisode[] = [];
  const remainingEpisodes: MetricsEpisode[] = [];

  for (const episode of openEpisodes) {
    if (Date.parse(episode.last_attempt_at) <= cutoff) {
      const closedEpisode: MetricsEpisode = {
        ...episode,
        closed_at: closedAt,
        close_reason: 'stale_open',
      };
      staleEpisodes.push(closedEpisode);
    } else {
      remainingEpisodes.push(episode);
    }
  }

  if (staleEpisodes.length > 0) {
    for (const episode of staleEpisodes) {
      await appendClosedEpisode(root, episode);
    }
    await writeOpenEpisodesUnsafe(root, remainingEpisodes);
  }

  return staleEpisodes;
}

function createEpisode(episodeId: string, event: MetricsEvent): MetricsEpisode {
  return {
    schema: 'BlueprintHelper.MetricsEpisode.v1',
    episode_id: episodeId,
    task_key: event.task_key!,
    ...(event.task_spec_hash ? { task_spec_hash: event.task_spec_hash } : {}),
    first_attempt_at: event.timestamp,
    last_attempt_at: event.timestamp,
    preview_attempts: 0,
    execute_attempts: 0,
    failed_attempts: 0,
    success_attempts: 0,
  };
}

function applyEpisodeEvent(episode: MetricsEpisode, event: MetricsEvent): void {
  if (event.task_spec_hash !== undefined && episode.task_spec_hash === undefined) {
    episode.task_spec_hash = event.task_spec_hash;
  }

  if (isAttemptEvent(event)) {
    episode.last_attempt_at = event.timestamp;

    if (event.event_type === 'taskspec_preview_completed') {
      episode.preview_attempts += 1;
    } else {
      episode.execute_attempts += 1;
      if (event.status === 'success') {
        episode.execute_succeeded = true;
      }
    }

    if (event.status === 'success') {
      episode.success_attempts += 1;
    } else {
      episode.failed_attempts += 1;
    }
  }

  if (event.event_type === 'validation_completed' && event.status === 'success') {
    episode.validation_succeeded = true;
  }

  if (event.event_type === 'readback_completed' && event.status === 'success') {
    episode.readback_succeeded = true;
  }
}

function shouldCloseAsSuccess(episode: MetricsEpisode, event: MetricsEvent): boolean {
  if (event.event_type === 'taskspec_execute_completed' && event.correctness_basis === 'validation_readback') {
    return event.status === 'success';
  }

  return Boolean(episode.execute_succeeded && episode.validation_succeeded && episode.readback_succeeded);
}

function isEpisodeEvent(event: MetricsEvent): boolean {
  return isAttemptEvent(event)
    || event.event_type === 'validation_completed'
    || event.event_type === 'readback_completed';
}

function isAttemptEvent(event: MetricsEvent): boolean {
  return event.event_type === 'taskspec_preview_completed'
    || event.event_type === 'taskspec_execute_completed';
}

async function readOpenEpisodesUnsafe(root: string): Promise<MetricsEpisode[]> {
  const file = path.join(root, 'episodes', 'open.json');
  try {
    const parsed = JSON.parse(await readFile(file, 'utf8')) as Partial<OpenEpisodesFile>;
    return Array.isArray(parsed.episodes) ? parsed.episodes : [];
  } catch (error) {
    if (isNotFoundError(error)) {
      return [];
    }
    throw error;
  }
}

async function writeOpenEpisodesUnsafe(root: string, episodes: MetricsEpisode[]): Promise<void> {
  const episodesDir = path.join(root, 'episodes');
  await mkdir(episodesDir, { recursive: true });
  const file = path.join(episodesDir, 'open.json');
  const tempFile = path.join(episodesDir, `open.${process.pid}.${Date.now()}.${randomUUID()}.tmp`);
  const payload: OpenEpisodesFile = {
    schema: 'BlueprintHelper.MetricsOpenEpisodes.v1',
    episodes,
  };
  await writeFile(tempFile, `${JSON.stringify(payload, null, 2)}\n`, 'utf8');
  await rename(tempFile, file);
}

async function appendClosedEpisode(root: string, episode: MetricsEpisode): Promise<void> {
  const episodesDir = path.join(root, 'episodes');
  await mkdir(episodesDir, { recursive: true });
  await appendFile(
    path.join(episodesDir, `closed-${monthPart(episode.closed_at ?? episode.last_attempt_at)}.jsonl`),
    `${JSON.stringify(episode)}\n`,
    'utf8',
  );
}

async function readClosedEpisodesUnsafe(root: string, window: MetricsWindow, now: Date): Promise<MetricsEpisode[]> {
  const episodesDir = path.join(root, 'episodes');
  const files = (await readJsonlFileNames(episodesDir)).filter((file) => /^closed-\d{4}-\d{2}\.jsonl$/.test(file));
  const episodes = (await Promise.all(
    files.map((file) => readJsonl<MetricsEpisode>(path.join(episodesDir, file))),
  )).flat();
  const cutoff = cutoffTime(window, now);

  return episodes
    .filter((episode) => {
      const timestamp = episode.closed_at ?? episode.last_attempt_at;
      return cutoff === undefined || Date.parse(timestamp) >= cutoff;
    })
    .sort((left, right) => compareText(left.closed_at ?? left.last_attempt_at, right.closed_at ?? right.last_attempt_at));
}

async function withOpenEpisodesLock<T>(root: string, operation: () => Promise<T>): Promise<T> {
  const episodesDir = path.join(root, 'episodes');
  const lockDir = path.join(episodesDir, 'open.lock');
  await mkdir(episodesDir, { recursive: true });
  await acquireDirectoryLock(lockDir);
  try {
    return await operation();
  } finally {
    await rm(lockDir, { recursive: true, force: true });
  }
}

async function acquireDirectoryLock(lockDir: string): Promise<void> {
  const startedAt = Date.now();

  while (true) {
    try {
      await mkdir(lockDir);
      return;
    } catch (error) {
      if (!isAlreadyExistsError(error) || Date.now() - startedAt > OPEN_EPISODE_LOCK_TIMEOUT_MS) {
        throw error;
      }
      await sleep(OPEN_EPISODE_LOCK_RETRY_MS);
    }
  }
}

function sleep(durationMs: number): Promise<void> {
  return new Promise((resolve) => {
    setTimeout(resolve, durationMs);
  });
}

async function readJsonlFileNames(dir: string): Promise<string[]> {
  try {
    return (await readdir(dir, { withFileTypes: true }))
      .filter((entry) => entry.isFile() && entry.name.endsWith('.jsonl'))
      .map((entry) => entry.name)
      .sort();
  } catch (error) {
    if (isNotFoundError(error)) {
      return [];
    }
    throw error;
  }
}

async function readJsonl<T>(file: string): Promise<T[]> {
  const text = await readFile(file, 'utf8');
  return text
    .split(/\r?\n/)
    .filter((line) => line.trim().length > 0)
    .map((line) => JSON.parse(line) as T);
}

function cutoffTime(window: MetricsWindow, now: Date): number | undefined {
  if (window === 'all') {
    return undefined;
  }
  const days = window === '1d' ? 1 : window === '7d' ? 7 : 30;
  return now.getTime() - days * 24 * 60 * 60 * 1000;
}

function datePart(timestamp: string): string {
  const direct = /^(\d{4}-\d{2}-\d{2})/.exec(timestamp)?.[1];
  return direct ?? new Date(timestamp).toISOString().slice(0, 10);
}

function monthPart(timestamp: string): string {
  return datePart(timestamp).slice(0, 7);
}

function createNowProvider(now: Date | (() => Date) | undefined): () => Date {
  if (typeof now === 'function') {
    return () => new Date(now().getTime());
  }
  if (now instanceof Date) {
    return () => new Date(now.getTime());
  }
  return () => new Date();
}

function compareText(left: string, right: string): number {
  return left < right ? -1 : left > right ? 1 : 0;
}

function isNotFoundError(error: unknown): boolean {
  return error instanceof Error && 'code' in error && error.code === 'ENOENT';
}

function isAlreadyExistsError(error: unknown): boolean {
  return error instanceof Error && 'code' in error && error.code === 'EEXIST';
}
