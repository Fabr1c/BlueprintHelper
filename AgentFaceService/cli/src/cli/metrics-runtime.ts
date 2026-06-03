import path from 'node:path';

import {
  createMetricsService,
  type MetricsService,
} from '@blueprinthelper/task-core/metrics/metrics-service';
import { extractReadToolOperation } from '@blueprinthelper/task-core/metrics/operation-extractor';
import type { MetricsOperationIdentity } from '@blueprinthelper/task-core/metrics/metrics-types';
import {
  failureResult,
  type ToolResultBase,
} from '@blueprinthelper/task-core/result/tool-result';
import type { CliCommand } from './output.js';

export interface CreateCliMetricsServiceOptions {
  cwd: string;
  env?: NodeJS.ProcessEnv;
  now?: Date | (() => Date);
}

export interface RecordCliToolCompletionOptions {
  metrics: MetricsService;
  command: Pick<CliCommand, 'toolName'>;
  toolResult: ToolResultBase;
  durationMs: number;
  rawParams?: Record<string, unknown>;
  parsedParams?: Record<string, unknown>;
}

export interface RecordCliToolThrownErrorOptions {
  metrics: MetricsService;
  command: Pick<CliCommand, 'toolName'>;
  error: unknown;
  durationMs: number;
}

const HOUR_IN_MS = 60 * 60 * 1000;
const DEFAULT_EPISODE_TTL_HOURS = 24;

export function createCliMetricsService(options: CreateCliMetricsServiceOptions): MetricsService {
  const env = options.env ?? process.env;

  return createMetricsService({
    root: resolveCliMetricsRoot(options.cwd, env),
    disabled: env['BPH_METRICS_DISABLED'] === '1',
    now: options.now,
    staleAfterMs: resolveCliMetricsEpisodeTtlMs(env),
  });
}

export function resolveCliMetricsRoot(cwd: string, env: NodeJS.ProcessEnv = process.env): string {
  const overrideRoot = readNonEmptyString(env['BPH_METRICS_DIR']);
  return overrideRoot
    ? path.resolve(overrideRoot)
    : path.resolve(cwd, 'Saved', 'BlueprintHelper', 'Metrics');
}

export async function recordCliToolCompletion(options: RecordCliToolCompletionOptions): Promise<void> {
  const toolName = readNonEmptyString(options.command.toolName);
  if (!toolName) {
    return;
  }

  try {
    await options.metrics.collector.recordToolCompleted({
      tool_name: toolName,
      status: options.toolResult.ok ? 'success' : 'failed',
      toolResult: options.toolResult,
      duration_ms: options.durationMs,
      ...resolveCliToolOperation(toolName, options.parsedParams ?? options.rawParams),
    });
  } catch {
    return;
  }
}

export async function recordCliToolThrownError(options: RecordCliToolThrownErrorOptions): Promise<void> {
  const toolName = readNonEmptyString(options.command.toolName);
  if (!toolName) {
    return;
  }

  try {
    await recordCliToolCompletion({
      metrics: options.metrics,
      command: options.command,
      durationMs: options.durationMs,
      toolResult: failureResult(toolName, {
        code: classifyCliToolErrorCode(options.error),
        stage: 'parse_input',
        message: options.error instanceof Error ? options.error.message : String(options.error),
        retryable: false,
        rollback_result: 'not_needed',
      }),
    });
  } catch {
    return;
  }
}

function resolveCliMetricsEpisodeTtlMs(env: NodeJS.ProcessEnv): number | undefined {
  const hours = readPositiveInteger(env['BPH_METRICS_EPISODE_TTL_HOURS']) ?? DEFAULT_EPISODE_TTL_HOURS;
  return hours * HOUR_IN_MS;
}

function resolveCliToolOperation(toolName: string, input: unknown): MetricsOperationIdentity {
  if (toolName === 'blueprinthelper_read_context') {
    return extractReadToolOperation(input);
  }

  return {
    capability: toolName,
    semantic_operation: toolName,
  };
}

function classifyCliToolErrorCode(error: unknown): string {
  const message = error instanceof Error ? error.message : String(error);
  if (/parse .*json/i.test(message)) {
    return 'malformed_json';
  }
  if (/json object/i.test(message)) {
    return 'invalid_type';
  }
  return 'cli_input_error';
}

function readPositiveInteger(value: string | undefined): number | undefined {
  if (!value) {
    return undefined;
  }

  const parsed = Number(value);
  return Number.isInteger(parsed) && parsed > 0 ? parsed : undefined;
}

function readNonEmptyString(value: string | undefined): string | undefined {
  return typeof value === 'string' && value.trim().length > 0 ? value.trim() : undefined;
}
