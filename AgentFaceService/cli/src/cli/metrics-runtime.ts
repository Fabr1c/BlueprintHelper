import { readdirSync } from 'node:fs';
import path from 'node:path';

import {
  createMetricsService,
  type MetricsService,
} from '@blueprinthelper/task-core/metrics/metrics-service';
import { extractTaskPlanMetricOperations } from '@blueprinthelper/task-core/metrics/operation-extractor';
import {
  buildReadonlyToolCommandManifestRegistry,
  getToolCapabilityDescriptor,
} from '@blueprinthelper/task-core/tool-surface/tool-registry';
import type {
  MetricsIoSummary,
  MetricsOperationIdentity,
} from '@blueprinthelper/task-core/metrics/metrics-types';
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

export interface RecordCliIoCompletedOptions {
  metrics: MetricsService;
  command: Pick<CliCommand, 'metricsToolName' | 'toolName'>;
  inputIo?: MetricsIoSummary;
  outputIo?: MetricsIoSummary;
  operationInput?: unknown;
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
  if (overrideRoot) {
    return path.resolve(overrideRoot);
  }

  const projectRoot = findNearestUnrealProjectRoot(cwd);
  if (!projectRoot) {
    throw new Error(
      `Unable to resolve BlueprintHelper Metrics root from cwd "${path.resolve(cwd)}". ` +
      'Run the CLI from inside an Unreal project or set BPH_METRICS_DIR.',
    );
  }

  return path.resolve(projectRoot, 'Saved', 'BlueprintHelper', 'Metrics');
}

function findNearestUnrealProjectRoot(cwd: string): string | undefined {
  let dir = path.resolve(cwd);

  while (true) {
    const projectFiles = readUprojectFiles(dir);
    if (projectFiles.length === 1) {
      return dir;
    }
    if (projectFiles.length > 1) {
      return undefined;
    }

    const parent = path.dirname(dir);
    if (parent === dir) {
      return undefined;
    }
    dir = parent;
  }
}

function readUprojectFiles(dir: string): string[] {
  try {
    return readdirSync(dir, { withFileTypes: true })
      .filter((entry) => entry.isFile() && entry.name.toLowerCase().endsWith('.uproject'))
      .map((entry) => entry.name);
  } catch {
    return [];
  }
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

export async function recordCliIoCompleted(options: RecordCliIoCompletedOptions): Promise<void> {
  const toolName = resolveCliIoToolName(options.command);
  if (!toolName || (options.inputIo === undefined && options.outputIo === undefined)) {
    return;
  }

  try {
    await options.metrics.collector.recordCliIoCompleted({
      tool_name: toolName,
      status: 'success',
      io: {
        ...options.inputIo,
        ...options.outputIo,
      },
      ...resolveCliToolOperation(toolName, options.operationInput),
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
  const manifest = buildReadonlyToolCommandManifestRegistry().get(toolName);
  if (manifest?.metrics_identity) {
    return manifest.metrics_identity;
  }
  if (manifest) {
    const descriptorIdentity = getToolCapabilityDescriptor(manifest.tool_id)?.metrics_identity;
    if (descriptorIdentity) {
      return descriptorIdentity;
    }
  }

  const taskPlanOperation = resolveTaskPlanOperationIdentity(input);
  if (taskPlanOperation) {
    return taskPlanOperation;
  }

  if (manifest) {
    return {
      capability: `${manifest.domain}.${manifest.kind}`,
      semantic_operation: manifest.tool_id,
      fallback: true,
    };
  }

  return {
    capability: toolName,
    semantic_operation: toolName,
    fallback: true,
  };
}

function resolveTaskPlanOperationIdentity(input: unknown): MetricsOperationIdentity | undefined {
  const taskPlan = readTaskPlanLike(input);
  if (!taskPlan) {
    return undefined;
  }
  return extractTaskPlanMetricOperations(taskPlan as never)[0];
}

function readTaskPlanLike(input: unknown): Record<string, unknown> | undefined {
  if (!isRecord(input)) {
    return undefined;
  }
  const directSchema = typeof input['schema'] === 'string' ? input['schema'] : undefined;
  if (directSchema?.includes('TaskPlan')) {
    return input;
  }
  const taskPlan = input['task_plan'] ?? input['taskPlan'];
  return isRecord(taskPlan) ? taskPlan : undefined;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function resolveCliIoToolName(command: Pick<CliCommand, 'metricsToolName' | 'toolName'>): string | undefined {
  if (command.metricsToolName) {
    return command.metricsToolName;
  }
  if (command.toolName) {
    return command.toolName;
  }
  return undefined;
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
