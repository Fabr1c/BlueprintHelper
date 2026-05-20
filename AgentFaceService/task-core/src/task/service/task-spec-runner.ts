import type { BridgeClient, BridgeResponse, BridgeSendCommandOptions } from '../../bridge/bridge-client.js';
import { buildTaskContextPack } from '../context/task-context.js';
import {
  TaskSpecCompileError,
  type CompiledTaskPlan,
  summarizeTaskPlan,
} from '../compiler/task-compiler.js';
import {
  createTaskSpecCompiler,
  type TaskCompiler,
} from '../compiler/task-compiler-service.js';
import {
  TASK_PREVIEW_SCHEMA,
  type ReadTaskContextInput,
  type TaskIssue,
  type TaskPlan,
  type TaskPreviewToken,
  type TaskSpec,
} from '../schema/task-schemas.js';
import {
  TOOL_RESULT_SCHEMA,
  failureResult,
  sanitizeAgentFacingToolResult,
  successRead,
  type ToolResultBase,
  type ToolResultError,
} from '../../result/tool-result.js';
import {
  getTaskResult,
  nextPreviewId,
  nextTaskRunId,
  storeTaskRunJournal,
  storeTaskResult,
} from '../runtime/task-result-store.js';
import {
  TaskTimingTrace,
  addTaskTimingMarker,
  addNestedTaskTiming,
  attachTaskTiming,
  extractBridgeTiming,
  extractBridgeTransportTiming,
  hasTaskTiming,
  measureTaskTiming,
  measureTaskTimingAsync,
} from './task-timing.js';
import {
  createExecutionPolicyHash,
  createTaskPlanHash,
  createTaskSpecHash,
} from './task-plan-hash.js';

export type { TaskPreviewToken } from '../schema/task-schemas.js';
export type { TaskCompiler } from '../compiler/task-compiler-service.js';

export type TaskRunnerBridge = {
  sendCommand(
    command: string,
    payload?: Record<string, unknown>,
    options?: BridgeSendCommandOptions,
  ): Promise<BridgeResponse>;
};

export interface TaskPreviewOutcome {
  previewId: string;
  taskPlan: TaskPlan;
  previewToken?: TaskPreviewToken;
  passed: boolean;
  issues: TaskIssue[];
  toolResult: ToolResultBase;
}

export interface TaskExecuteOptions {
  previewToken?: TaskPreviewToken;
}

export interface TaskSpecRunner {
  readTaskContext(input: Record<string, unknown>): Promise<ToolResultBase>;
  readReferenceContext(input: Record<string, unknown>): Promise<ToolResultBase>;
  previewTask(taskSpec: TaskSpec, timing?: TaskTimingTrace): Promise<TaskPreviewOutcome>;
  executeTask(taskSpec: TaskSpec, timing?: TaskTimingTrace, options?: TaskExecuteOptions): Promise<ToolResultBase>;
  getTaskResult(taskRunId: string): Promise<ToolResultBase>;
}

export function createTaskSpecRunner(input: {
  bridge: TaskRunnerBridge;
  taskCompiler?: TaskCompiler;
}): TaskSpecRunner {
  const bridge = input.bridge;
  const taskCompiler = input.taskCompiler ?? createTaskSpecCompiler();

  return {
    async readTaskContext(rawInput) {
      const taskInput = rawInput as ReadTaskContextInput;
      try {
        const contextPack = await buildTaskContextPack(bridge as unknown as BridgeClient, taskInput);
        return successRead(
          'read_task_context',
          { target_type: 'blueprint', asset_path: taskInput.target.asset_path },
          contextPack,
        );
      } catch (err) {
        return taskFailure('read_task_context', 'task_context_read_failed', 'context_error', err);
      }
    },

    async readReferenceContext(rawInput) {
      try {
        const response = await bridge.sendCommand('read_reference_context', rawInput);
        return referenceContextToolResult(response, String(rawInput['asset_path'] ?? ''));
      } catch (err) {
        return taskFailure('read_reference_context', 'bridge_error', 'bridge_error', err);
      }
    },

    async previewTask(taskSpec, timing) {
      const outcome = await runPreviewTask(bridge, taskSpec, taskCompiler, timing);
      outcome.toolResult = attachTaskTiming(outcome.toolResult, timing);
      return outcome;
    },

    async executeTask(taskSpec, timing, options) {
      try {
        if (options?.previewToken) {
          return await executeTaskWithPreviewToken(bridge, taskSpec, options.previewToken, timing);
        }

        const preview = await runPreviewTaskForExecute(bridge, taskSpec, taskCompiler, timing);
        if (!preview.passed) {
          return attachTaskTiming(taskFailure(
            'execute_task',
            'task_preview_blocked',
            'preview_error',
            'Task preview was blocked; execute_task did not write assets.',
            preview.issues,
          ), timing);
        }

        const writeResponse = await measureTaskTimingAsync(timing, 'bridge.execute_task_plan', () => bridge.sendCommand('execute_task_plan', {
          task_plan: preview.taskPlan,
          ...(hasTaskTiming(timing) ? { include_timing: true } : {}),
        }, {
          timing,
          timingPrefix: 'bridge.execute_task_plan.transport',
        }));
        addNestedTaskTiming(timing, 'bridge.execute_task_plan', extractBridgeTransportTiming(writeResponse));
        addNestedTaskTiming(timing, 'ue.execute_task_plan', extractBridgeTiming(writeResponse.result));
        if (!writeResponse.success) {
          return attachTaskTiming(taskFailureFromBridgeResponse(
            'execute_task',
            writeResponse,
            'bridge_error',
            'Bridge write failed.',
            'bridge.execute_task_plan',
          ), timing);
        }

        const result = measureTaskTiming(timing, 'result_wrap', () => {
          const taskRunId = extractUeTaskRunId(writeResponse) ?? nextTaskRunId();
          const bridgeResult = asRecord(writeResponse.result);
          const modified = isBridgeResultModified(bridgeResult);
          storeTaskResult({
            taskRunId,
            taskPlan: preview.taskPlan,
            status: 'completed',
            bridgeResult,
          });

          const toolResult = successRead(
            'execute_task',
            { target_type: 'blueprint', asset_path: preview.taskPlan.target_assets[0] },
            {
              task_run_id: taskRunId,
              task: {
                feature_name: preview.taskPlan.task_name,
                applied_steps: preview.taskPlan.steps.length,
                modified_assets: preview.taskPlan.target_assets.length,
              },
            },
          ) as ToolResultBase;
          Object.defineProperty(toolResult, 'debug', {
            value: { bridge_result: bridgeResult },
            enumerable: false,
            configurable: true,
          });
          toolResult.modified = modified;
          return toolResult;
        });
        return attachTaskTiming(result, timing);
      } catch (err) {
        return attachTaskTiming(taskErrorFromUnknown('execute_task', err), timing);
      }
    },

    async getTaskResult(taskRunId) {
      const taskResult = getTaskResult(taskRunId);
      const bridgeTaskResult = await getBridgeTaskRunJournal(bridge, taskRunId);
      const resolvedTaskResult = bridgeTaskResult ?? taskResult;
      if (!resolvedTaskResult) {
        return taskFailure(
          'get_task_result',
          'task_result_not_found',
          'not_found',
          `Task result not found for task_run_id=${taskRunId}.`,
        );
      }

      return successRead(
        'get_task_result',
        { target_type: 'asset' },
        resolvedTaskResult,
      );
    },
  };
}

async function getBridgeTaskRunJournal(
  bridge: TaskRunnerBridge,
  taskRunId: string,
): Promise<Record<string, unknown> | undefined> {
  try {
    const response = await bridge.sendCommand('get_task_run_journal', {
      task_run_id: taskRunId,
    });
    if (!response.success) {
      return undefined;
    }

    const journal = extractBridgeTaskRunJournal(response, taskRunId);
    return journal ? storeTaskRunJournal(taskRunId, journal) : undefined;
  } catch {
    return undefined;
  }
}

async function runPreviewTask(
  bridge: TaskRunnerBridge,
  taskSpec: TaskSpec,
  taskCompiler: TaskCompiler,
  timing?: TaskTimingTrace,
): Promise<TaskPreviewOutcome> {
  const compiled = await compileTaskSpecForRunner(taskSpec, taskCompiler, timing);
  return await runPreviewTaskFromPlan(bridge, taskSpec, compiled.taskPlan, timing);
}

async function runPreviewTaskForExecute(
  bridge: TaskRunnerBridge,
  taskSpec: TaskSpec,
  taskCompiler: TaskCompiler,
  timing?: TaskTimingTrace,
): Promise<TaskPreviewOutcome> {
  const compiled = await compileTaskSpecForRunner(taskSpec, taskCompiler, timing);
  const taskPlan = compiled.taskPlan;
  if (taskPlan.execution_policy.dry_run_mode === 'none') {
    throw new DryRunModeNoneRequiresPreviewTokenError();
  }
  return await runPreviewTaskFromPlan(bridge, taskSpec, taskPlan, timing);
}

async function compileTaskSpecForRunner(
  taskSpec: TaskSpec,
  taskCompiler: TaskCompiler,
  timing?: TaskTimingTrace,
): Promise<CompiledTaskPlan> {
  const compiled = await measureTaskTimingAsync(timing, 'taskspec_compile', () => taskCompiler(taskSpec, {
    dryRun: true,
    diagnostics: hasTaskTiming(timing),
  }));
  recordTaskCompileTiming(timing, compiled);
  return compiled;
}

function recordTaskCompileTiming(timing: TaskTimingTrace | undefined, compiled: CompiledTaskPlan): void {
  addTaskTimingMarker(timing, 'taskspec_compile.strategy', {
    strategy: compiled.strategyId,
    ...(compiled.diagnostics?.parityStatus ? { parity_status: compiled.diagnostics.parityStatus } : {}),
    ...(compiled.diagnostics?.parityReason ? { parity_reason: compiled.diagnostics.parityReason } : {}),
    ...(compiled.diagnostics?.fallbackReason ? { fallback_reason: compiled.diagnostics.fallbackReason } : {}),
    ...(typeof compiled.diagnostics?.compilerOutputBytes === 'number'
      ? { output_bytes: compiled.diagnostics.compilerOutputBytes }
      : {}),
  });
}

async function runPreviewTaskFromPlan(
  bridge: TaskRunnerBridge,
  taskSpec: TaskSpec,
  taskPlan: TaskPlan,
  timing?: TaskTimingTrace,
): Promise<TaskPreviewOutcome> {
  const previewId = measureTaskTiming(timing, 'preview_token.allocate_preview_id', () => nextPreviewId());
  const previewTokenRequest = measureTaskTiming(timing, 'preview_token.prepare_request', () => ({
    task_spec_hash: createTaskSpecHash(taskSpec),
    task_plan_hash: createTaskPlanHash(taskPlan),
    execution_policy_hash: createExecutionPolicyHash(taskPlan.execution_policy),
  }));
  const previewResponse = await measureTaskTimingAsync(timing, 'bridge.preview_task_plan', () => bridge.sendCommand('preview_task_plan', {
    task_plan: taskPlan,
    preview_token_request: previewTokenRequest,
    ...(hasTaskTiming(timing) ? { include_timing: true } : {}),
  }, {
    timing,
    timingPrefix: 'bridge.preview_task_plan.transport',
  }));
  addNestedTaskTiming(timing, 'bridge.preview_task_plan', extractBridgeTransportTiming(previewResponse));
  addNestedTaskTiming(timing, 'ue.preview_task_plan', extractBridgeTiming(previewResponse.result));

  if (!previewResponse.success) {
    const failure = bridgeFailureFromResponse(
      previewResponse,
      'bridge_error',
      'Bridge dry-run failed.',
      'bridge.preview_task_plan',
    );
    const toolResult = taskFailure(
      'preview_task',
      failure.code,
      'bridge_error',
      failure.message,
      failure.issues,
      failure.error,
    );
    const previewToken = extractPreviewToken(previewResponse);
    toolResult.target = { target_type: 'blueprint', asset_path: taskPlan.target_assets[0] };
    toolResult.data = {
      schema: TASK_PREVIEW_SCHEMA,
      preview_id: previewId,
      ...(previewToken ? { preview_token: previewToken } : {}),
      passed: false,
      blocked: true,
      task_plan: summarizeTaskPlan(taskPlan),
      issues: failure.issues,
      ...extractDevelopPreviewDiagnostics(previewResponse, timing),
    };

    return {
      previewId,
      taskPlan,
      previewToken,
      passed: false,
      issues: failure.issues,
      toolResult,
    };
  }

  const dryRun = extractDryRun(previewResponse);
  const passed = dryRun.canExecute;
  const issues = dryRun.issues;
  const previewToken = extractPreviewToken(previewResponse);

  return {
    previewId,
    taskPlan,
    previewToken,
    passed,
    issues,
    toolResult: sanitizeAgentFacingToolResult({
      ok: true,
      schema: TOOL_RESULT_SCHEMA,
      operation: 'preview_task',
      trace_id: `trace_${Date.now()}_${previewId}`,
      status: 'dry_run',
      modified: false,
      target: { target_type: 'blueprint', asset_path: taskPlan.target_assets[0] },
      data: {
        schema: TASK_PREVIEW_SCHEMA,
        preview_id: previewId,
        ...(previewToken ? { preview_token: previewToken } : {}),
        passed,
        blocked: !passed,
        task_plan: summarizeTaskPlan(taskPlan),
        issues,
        ...extractDevelopPreviewDiagnostics(previewResponse, timing),
      },
    }),
  };
}

function extractDevelopPreviewDiagnostics(
  resp: BridgeResponse,
  timing?: TaskTimingTrace,
): Record<string, unknown> {
  if (!hasTaskTiming(timing)) {
    return {};
  }

  const result = asRecord(resp.result);
  const data = asRecord(result?.['data']);
  if (!result) {
    return {};
  }

  const diagnostics: Record<string, unknown> = {
    ue_preview_result: result,
  };

  for (const key of ['dry_run', 'call_function_resolution_cache', 'runtime_facts', 'cache_diagnostics']) {
    if (data && Object.hasOwn(data, key)) {
      diagnostics[key] = data[key];
    }
  }

  return diagnostics;
}

async function executeTaskWithPreviewToken(
  bridge: TaskRunnerBridge,
  taskSpec: TaskSpec,
  previewToken: TaskPreviewToken,
  timing?: TaskTimingTrace,
): Promise<ToolResultBase> {
  const taskSpecHash = measureTaskTiming(timing, 'preview_token.validate', () => {
    if (!isPreviewTokenFormat(previewToken)) {
      throw new PreviewTokenValidationError({
        code: 'preview_token_mismatch',
        message: 'preview_token must be a 32-character hex string.',
        field: 'preview_token',
        actual: String(previewToken),
      });
    }
    return createTaskSpecHash(taskSpec);
  });

  const writeResponse = await measureTaskTimingAsync(timing, 'bridge.execute_task_plan', () => bridge.sendCommand('execute_task_plan', {
    preview_token: previewToken,
    task_spec_hash: taskSpecHash,
    ...(hasTaskTiming(timing) ? { include_timing: true } : {}),
  }, {
    timing,
    timingPrefix: 'bridge.execute_task_plan.transport',
  }));
  addNestedTaskTiming(timing, 'bridge.execute_task_plan', extractBridgeTransportTiming(writeResponse));
  addNestedTaskTiming(timing, 'ue.execute_task_plan', extractBridgeTiming(writeResponse.result));

  if (!writeResponse.success) {
    return attachTaskTiming(taskFailureFromBridgeResponse(
      'execute_task',
      writeResponse,
      'bridge_error',
      'Bridge write failed.',
      'bridge.execute_task_plan',
    ), timing);
  }

  const result = measureTaskTiming(timing, 'result_wrap', () => {
    const taskRunId = extractUeTaskRunId(writeResponse) ?? nextTaskRunId();
    const bridgeResult = asRecord(writeResponse.result);
    const data = asRecord(bridgeResult?.['data']);
    const targetAssets = arrayOfStrings(data?.['target_assets']);
    const steps = arrayOfRecords(data?.['steps']);
    const modified = isBridgeResultModified(bridgeResult);
    const targetAsset = targetAssets[0] ?? readTaskSpecAssetPath(taskSpec);

    const toolResult = successRead(
      'execute_task',
      { target_type: 'blueprint', asset_path: targetAsset },
      {
        task_run_id: taskRunId,
        task: {
          feature_name: readTaskSpecFeatureName(taskSpec),
          applied_steps: steps.length,
          modified_assets: targetAssets.length,
          target_assets: targetAssets,
        },
      },
    ) as ToolResultBase;
    Object.defineProperty(toolResult, 'debug', {
      value: { bridge_result: bridgeResult },
      enumerable: false,
      configurable: true,
    });
    toolResult.modified = modified;
    return toolResult;
  });
  return attachTaskTiming(result, timing);
}

class PreviewTokenValidationError extends Error {
  constructor(
    readonly validation: {
      code: 'preview_token_missing' | 'preview_token_expired' | 'preview_token_mismatch';
      message: string;
      field?: string;
      expected?: string;
      actual?: string;
    },
  ) {
    super(validation.message);
  }
}

class DryRunModeNoneRequiresPreviewTokenError extends Error {
  readonly code = 'dry_run_mode_none_requires_preview_token';
  readonly field = 'execution_policy.dry_run_mode';

  constructor() {
    super('TaskSpec execution_policy.dry_run_mode=none requires a valid preview_token from a prior preview_task call.');
  }
}

function referenceContextToolResult(response: BridgeResponse, assetPath: string): ToolResultBase {
  const raw = asRecord(response.result);
  if (raw && typeof raw['ok'] === 'boolean' && raw['schema'] === TOOL_RESULT_SCHEMA) {
    return sanitizeAgentFacingToolResult(raw as unknown as ToolResultBase);
  }

  if (!response.success) {
    return taskFailure(
      'read_reference_context',
      response.error_code ?? 'bridge_error',
      'bridge_error',
      response.message ?? 'Bridge read_reference_context failed.',
    );
  }

  return successRead(
    'read_reference_context',
    { target_type: 'asset', asset_path: assetPath },
    raw ?? {},
  ) as ToolResultBase;
}

function taskErrorFromUnknown(operation: string, err: unknown): ToolResultBase {
  if (err instanceof DryRunModeNoneRequiresPreviewTokenError) {
    return taskFailure(
      operation,
      err.code,
      'semantic_error',
      err.message,
      [{
        code: err.code,
        path: err.field,
        message: 'Run preview_task first and pass its preview_token to execute_task, or use dry_run_mode quick/full.',
      }],
      {
        stage: 'preflight',
        retryable: true,
        field: err.field,
      },
    );
  }
  if (err instanceof PreviewTokenValidationError) {
    return taskFailure(
      operation,
      err.validation.code,
      'semantic_error',
      err.message,
      [{
        code: err.validation.code,
        path: err.validation.field ?? 'preview_token',
        message: err.message,
      }],
      {
        stage: 'preflight',
        retryable: true,
        ...(err.validation.field ? { field: err.validation.field } : {}),
        ...(err.validation.expected ? { expected: err.validation.expected } : {}),
        ...(err.validation.actual ? { actual: err.validation.actual } : {}),
      },
    );
  }
  if (err instanceof TaskSpecCompileError) {
    return taskFailure(operation, err.code, 'semantic_error', err.message, err.issues);
  }
  return taskFailure(operation, 'task_internal_error', 'internal_error', err);
}

function taskFailureFromBridgeResponse(
  operation: string,
  response: BridgeResponse,
  fallbackCode: string,
  fallbackMessage: string,
  fallbackIssuePath: string,
): ToolResultBase {
  const failure = bridgeFailureFromResponse(response, fallbackCode, fallbackMessage, fallbackIssuePath);
  return taskFailure(
    operation,
    failure.code,
    'bridge_error',
    failure.message,
    failure.issues,
    failure.error,
  );
}

function taskFailure(
  operation: string,
  code: string,
  category: string,
  error: unknown,
  issues: TaskIssue[] = [],
  errorDetails: Partial<ToolResultError> = {},
): ToolResultBase {
  const message = error instanceof Error ? error.message : String(error);
  const toolError = {
    code,
    category,
    stage: errorDetails.stage ?? (category === 'bridge_error' ? 'bridge' : 'parse_input'),
    message,
    retryable: errorDetails.retryable ?? category !== 'internal_error',
    rollback_result: errorDetails.rollback_result ?? 'not_needed',
    agent_action: category === 'semantic_error' ? 'fix_taskspec_and_retry' : 'stop_and_report',
    issues,
    ...(errorDetails.field ? { field: errorDetails.field } : {}),
    ...(errorDetails.expected ? { expected: errorDetails.expected } : {}),
    ...(errorDetails.actual ? { actual: errorDetails.actual } : {}),
  } as ToolResultError;

  return failureResult(operation, toolError);
}

function bridgeFailureFromResponse(
  response: BridgeResponse,
  fallbackCode: string,
  fallbackMessage: string,
  fallbackIssuePath: string,
): {
  code: string;
  message: string;
  issues: TaskIssue[];
  error: Partial<ToolResultError>;
} {
  const result = asRecord(response.result);
  const nestedError = asRecord(result?.['error']);
  const code = readString(nestedError?.['code']) ?? response.error_code ?? fallbackCode;
  const responseMessage = readNonEmptyString(response.message);
  const message = readNonEmptyString(nestedError?.['message']) ?? responseMessage ?? fallbackMessage;
  const field = readString(nestedError?.['field']);
  const dryRunIssues = extractDryRun(response).issues;
  const issues = dryRunIssues.length > 0
    ? dryRunIssues
    : [{
      code,
      path: field ?? fallbackIssuePath,
      message,
    }];

  return {
    code,
    message,
    issues,
    error: {
      stage: readToolStage(nestedError?.['stage']) ?? 'bridge',
      retryable: typeof nestedError?.['retryable'] === 'boolean' ? nestedError['retryable'] : false,
      rollback_result: readRollbackResult(nestedError?.['rollback_result']) ?? 'not_needed',
      ...(field ? { field } : {}),
      ...(readString(nestedError?.['expected']) ? { expected: readString(nestedError?.['expected']) } : {}),
      ...(readString(nestedError?.['actual']) ? { actual: readString(nestedError?.['actual']) } : {}),
    },
  };
}

function extractUeTaskRunId(writeResponse: BridgeResponse): string | undefined {
  const result = asRecord(writeResponse.result);
  const data = asRecord(result?.['data']);
  const taskRunId = data?.['task_run_id'];
  return typeof taskRunId === 'string' && taskRunId.length > 0
    ? taskRunId
    : undefined;
}

function extractPreviewToken(response: BridgeResponse): TaskPreviewToken | undefined {
  const result = asRecord(response.result);
  const data = asRecord(result?.['data']);
  const token = readString(data?.['preview_token']);
  return token && isPreviewTokenFormat(token) ? token : undefined;
}

function extractBridgeTaskRunJournal(
  response: BridgeResponse,
  requestedTaskRunId: string,
): Record<string, unknown> | undefined {
  const result = asRecord(response.result);
  const data = asRecord(result?.['data']);
  const journal = data ?? result;
  const taskRunId = journal?.['task_run_id'];
  const schema = journal?.['schema'];
  return taskRunId === requestedTaskRunId && schema === 'BlueprintHelper.TaskRunJournal.v1'
    ? journal
    : undefined;
}

function isBridgeResultModified(value: unknown): boolean {
  const result = asRecord(value);
  if (result?.['modified'] === true) {
    return true;
  }

  const data = asRecord(result?.['data']);
  const steps = arrayOfRecords(data?.['steps']);
  return steps.some((step) => {
    if (step['modified'] === true) {
      return true;
    }
    const stepResult = asRecord(step['result']);
    return stepResult?.['modified'] === true;
  });
}

function extractDryRun(resp: BridgeResponse): { canExecute: boolean; issues: TaskIssue[] } {
  const result = asRecord(resp.result);
  const data = asRecord(result?.['data']);
  const dryRun = asRecord(data?.['dry_run']) ?? asRecord(result?.['dry_run']);
  const canExecute = dryRun?.['can_execute'];
  const blockedByStatus =
    result?.['status'] === 'failed' ||
    dryRun?.['result'] === 'blocked' ||
    canExecute === false;
  const dryRunIssues = collectIssues(dryRun);
  const issues = dryRunIssues.length > 0 || !blockedByStatus
    ? dryRunIssues
    : collectBlockedPreviewIssues(result, dryRun);
  return {
    canExecute: typeof canExecute === 'boolean' ? canExecute : !blockedByStatus,
    issues,
  };
}

function collectBlockedPreviewIssues(
  result: Record<string, unknown> | undefined,
  dryRun: Record<string, unknown> | undefined,
): TaskIssue[] {
  const error = asRecord(dryRun?.['error']) ?? asRecord(result?.['error']);
  const code =
    readString(error?.['code']) ??
    readString(result?.['error_code']) ??
    'task_preview_blocked';
  const path =
    readString(error?.['target']) ??
    readString(error?.['field']) ??
    readString(error?.['path']) ??
    'bridge.preview_task_plan';
  const message =
    readNonEmptyString(error?.['message']) ??
    readNonEmptyString(dryRun?.['message']) ??
    readNonEmptyString(result?.['message']) ??
    'Task preview was blocked.';

  return [{ code, path, message }];
}

function collectIssues(dryRun: Record<string, unknown> | undefined): TaskIssue[] {
  if (!dryRun) return [];
  const rawIssues = [
    ...arrayOfRecords(dryRun['errors']),
    ...arrayOfRecords(dryRun['conflicts']),
    ...arrayOfRecords(dryRun['warnings']),
  ];
  return rawIssues.map((issue, index) => {
    const normalized: Record<string, unknown> = {
      code: typeof issue['code'] === 'string' ? issue['code'] : 'dry_run_issue',
      path: typeof issue['target'] === 'string' ? issue['target'] : `dry_run.issues[${index}]`,
      message: typeof issue['message'] === 'string' ? issue['message'] : JSON.stringify(issue),
    };
    for (const [key, value] of Object.entries(issue)) {
      if (key === 'code' || key === 'target' || key === 'message') continue;
      normalized[key] = value;
    }
    return normalized as TaskIssue;
  });
}

function arrayOfRecords(value: unknown): Array<Record<string, unknown>> {
  return Array.isArray(value)
    ? value.filter((item): item is Record<string, unknown> => item !== null && typeof item === 'object' && !Array.isArray(item))
    : [];
}

function arrayOfStrings(value: unknown): string[] {
  return Array.isArray(value)
    ? value.filter((item): item is string => typeof item === 'string')
    : [];
}

function readString(value: unknown): string | undefined {
  return typeof value === 'string' ? value : undefined;
}

function readNonEmptyString(value: unknown): string | undefined {
  return typeof value === 'string' && value.trim().length > 0 ? value : undefined;
}

function readToolStage(value: unknown): ToolResultError['stage'] | undefined {
  if (typeof value !== 'string') return undefined;
  return [
    'parse_input',
    'auth',
    'runtime_profile',
    'resolve_target',
    'preflight',
    'dry_run',
    'execute',
    'validate',
    'review',
    'rollback',
    'bridge',
    'mcp_wrap',
  ].includes(value)
    ? value as ToolResultError['stage']
    : undefined;
}

function readRollbackResult(value: unknown): ToolResultError['rollback_result'] | undefined {
  if (typeof value !== 'string') return undefined;
  return [
    'not_needed',
    'rolled_back',
    'rollback_failed',
    'unavailable',
  ].includes(value)
    ? value as ToolResultError['rollback_result']
    : undefined;
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return value !== null && typeof value === 'object' && !Array.isArray(value)
    ? value as Record<string, unknown>
    : undefined;
}

function isPreviewTokenFormat(value: unknown): value is TaskPreviewToken {
  return typeof value === 'string' && /^[0-9a-fA-F]{32}$/.test(value);
}

function readTaskSpecAssetPath(taskSpec: TaskSpec): string | undefined {
  const target = asRecord((taskSpec as Record<string, unknown>)['target']);
  return readString(target?.['asset_path']);
}

function readTaskSpecFeatureName(taskSpec: TaskSpec): string | undefined {
  return readString((taskSpec as Record<string, unknown>)['feature_name']);
}
