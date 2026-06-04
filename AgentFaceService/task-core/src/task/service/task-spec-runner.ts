import type { BridgeClient, BridgeResponse, BridgeSendCommandOptions } from '../../bridge/bridge-client.js';
import {
  createMetricsCollector,
  type MetricsCollector,
} from '../../metrics/metrics-collector.js';
import { classifyMetricsError } from '../../metrics/error-classifier.js';
import type {
  MetricsEventSink,
  MetricsOperationIdentity,
} from '../../metrics/metrics-types.js';
import { extractTaskPlanMetricOperations } from '../../metrics/operation-extractor.js';
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

const TASK_ISSUE_DETAIL_KEYS = [
  'signature_differences',
] as const;

export type TaskSpecRunnerMetrics =
  | MetricsEventSink
  | MetricsCollector
  | { collector: MetricsCollector };

export type TaskRunnerBridge = {
  sendCommand(
    command: string,
    payload?: Record<string, unknown>,
    options?: BridgeSendCommandOptions,
  ): Promise<BridgeResponse>;
};

export interface TaskPreviewOutcome {
  previewId: string;
  taskPlan?: TaskPlan;
  previewToken?: TaskPreviewToken;
  passed: boolean;
  issues: TaskIssue[];
  toolResult: ToolResultBase;
}

export interface TaskExecuteOptions {
  previewToken?: TaskPreviewToken;
}

export interface TaskSpecRunner {
  readReferenceContext(input: Record<string, unknown>): Promise<ToolResultBase>;
  previewTask(taskSpec: TaskSpec, timing?: TaskTimingTrace): Promise<TaskPreviewOutcome>;
  executeTask(taskSpec: TaskSpec, timing?: TaskTimingTrace, options?: TaskExecuteOptions): Promise<ToolResultBase>;
  getTaskResult(taskRunId: string): Promise<ToolResultBase>;
}

export function createTaskSpecRunner(input: {
  bridge: TaskRunnerBridge;
  taskCompiler?: TaskCompiler;
  metrics?: TaskSpecRunnerMetrics;
}): TaskSpecRunner {
  const bridge = input.bridge;
  const taskCompiler = input.taskCompiler ?? createTaskSpecCompiler();
  const metrics = createTaskSpecRunnerMetrics(input.metrics);

  return {
    async readReferenceContext(rawInput) {
      try {
        const response = await bridge.sendCommand('read_reference_context', rawInput);
        return referenceContextToolResult(response, String(rawInput['asset_path'] ?? ''));
      } catch (err) {
        return taskFailure('read_reference_context', 'bridge_error', 'bridge_error', err);
      }
    },

    async previewTask(taskSpec, timing) {
      const startedAt = Date.now();
      const outcome = await runPreviewTask(bridge, taskSpec, taskCompiler, timing);
      outcome.toolResult = attachTaskTiming(outcome.toolResult, timing);
      await recordPreviewMetrics(metrics, taskSpec, outcome, startedAt);
      return outcome;
    },

    async executeTask(taskSpec, timing, options) {
      const startedAt = Date.now();
      const recordAndReturn = async (
        result: ToolResultBase,
        taskPlan?: TaskPlan,
        alreadyAttached = false,
      ): Promise<ToolResultBase> => {
        const finalResult = alreadyAttached ? result : attachTaskTiming(result, timing);
        await recordExecuteMetrics(metrics, taskSpec, finalResult, startedAt, taskPlan);
        return finalResult;
      };

      try {
        if (options?.previewToken) {
          return await recordAndReturn(
            await executeTaskWithPreviewToken(bridge, taskSpec, options.previewToken, timing),
            undefined,
            true,
          );
        }

        const preview = await runPreviewTaskForExecute(bridge, taskSpec, taskCompiler, timing);
        if (!preview.passed) {
          return await recordAndReturn(taskFailure(
            'execute_task',
            previewBlockedErrorCode(preview.issues),
            'preview_error',
            'Task preview was blocked; execute_task did not write assets.',
            preview.issues,
          ), preview.taskPlan);
        }
        if (!preview.taskPlan) {
          return await recordAndReturn(taskFailure(
            'execute_task',
            'task_plan_missing_after_preview',
            'internal_error',
            'Task preview passed without a compiled TaskPlan.',
          ));
        }
        const taskPlan = preview.taskPlan;

        const writeResponse = await measureTaskTimingAsync(timing, 'bridge.execute_task_plan', () => bridge.sendCommand('execute_task_plan', {
          task_plan: taskPlan,
          ...(hasTaskTiming(timing) ? { include_timing: true } : {}),
        }, {
          timing,
          timingPrefix: 'bridge.execute_task_plan.transport',
        }));
        addNestedTaskTiming(timing, 'bridge.execute_task_plan', extractBridgeTransportTiming(writeResponse));
        addNestedTaskTiming(timing, 'ue.execute_task_plan', extractBridgeTiming(writeResponse.result));
        if (!writeResponse.success) {
          return await recordAndReturn(taskFailureFromBridgeResponse(
            'execute_task',
            writeResponse,
            'bridge_error',
            'Bridge write failed.',
            'bridge.execute_task_plan',
          ), taskPlan);
        }
        if (isFailedBridgeToolResult(writeResponse)) {
          return await recordAndReturn(taskFailureFromBridgeResponse(
            'execute_task',
            writeResponse,
            'execution_failed',
            'Bridge execute returned a failed ToolResult.',
            'bridge.execute_task_plan',
          ), taskPlan);
        }

        const result = measureTaskTiming(timing, 'result_wrap', () => {
          const taskRunId = extractUeTaskRunId(writeResponse) ?? nextTaskRunId();
          const bridgeResult = asRecord(writeResponse.result);
          const modified = isBridgeResultModified(bridgeResult);
          storeTaskResult({
            taskRunId,
            taskPlan,
            status: 'completed',
            bridgeResult,
          });

          const toolResult = successRead(
            'execute_task',
            { target_type: 'blueprint', asset_path: taskPlan.target_assets[0] },
            {
              task_run_id: taskRunId,
              task: {
                feature_name: taskPlan.task_name,
                applied_steps: taskPlan.steps.length,
                modified_assets: taskPlan.target_assets.length,
              },
              ...extractDevelopExecuteDiagnostics(writeResponse, timing),
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
        return await recordAndReturn(result, taskPlan);
      } catch (err) {
        return await recordAndReturn(taskErrorFromUnknown('execute_task', err));
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

function createTaskSpecRunnerMetrics(metrics: TaskSpecRunnerMetrics | undefined): MetricsCollector | undefined {
  if (metrics === undefined) {
    return undefined;
  }

  const metricsRecord = asRecord(metrics);
  const serviceCollector = metricsRecord?.['collector'];
  if (isMetricsCollector(serviceCollector)) {
    return serviceCollector;
  }

  if (isMetricsCollector(metrics)) {
    return metrics;
  }

  return createMetricsCollector({ sink: metrics as MetricsEventSink });
}

function isMetricsCollector(value: unknown): value is MetricsCollector {
  const raw = asRecord(value);
  return raw !== undefined
    && typeof raw['recordTaskPreviewCompleted'] === 'function'
    && typeof raw['recordTaskExecuteCompleted'] === 'function';
}

async function recordPreviewMetrics(
  metrics: MetricsCollector | undefined,
  taskSpec: TaskSpec,
  outcome: TaskPreviewOutcome,
  startedAt: number,
): Promise<void> {
  if (metrics === undefined) {
    return;
  }

  try {
    await metrics.recordTaskPreviewCompleted({
      taskSpec,
      passed: outcome.passed,
      toolResult: createMetricsToolResultSource(outcome.toolResult),
      duration_ms: elapsedMs(startedAt),
      ...selectPrimaryMetricOperation(outcome.taskPlan),
    });
  } catch {
    return;
  }
}

async function recordExecuteMetrics(
  metrics: MetricsCollector | undefined,
  taskSpec: TaskSpec,
  toolResult: ToolResultBase,
  startedAt: number,
  taskPlan?: TaskPlan,
): Promise<void> {
  if (metrics === undefined) {
    return;
  }

  try {
    await metrics.recordTaskExecuteCompleted({
      taskSpec,
      passed: toolResult.ok === true,
      toolResult: createMetricsToolResultSource(toolResult),
      duration_ms: elapsedMs(startedAt),
      ...selectPrimaryMetricOperation(taskPlan),
      ...extractExecuteEvidence(toolResult),
    });
  } catch {
    return;
  }
}

function selectPrimaryMetricOperation(taskPlan: TaskPlan | undefined): MetricsOperationIdentity {
  const operations = extractTaskPlanMetricOperations(taskPlan);
  return operations.find((operation) => operation.capability !== 'blueprint_signature')
    ?? operations[0]
    ?? {};
}

function createMetricsToolResultSource(toolResult: ToolResultBase): Record<string, unknown> {
  const classification = classifyMetricsError(toolResult);
  const issue = readPrimaryMetricsIssue(toolResult);

  return removeUndefined({
    error_category: classification.category,
    error_code: classification.code ?? issue?.code,
    issue,
  });
}

function readPrimaryMetricsIssue(toolResult: ToolResultBase): Record<string, unknown> | undefined {
  const error = asRecord(toolResult.error);
  const issue = arrayOfRecords(error?.['issues'])[0] ?? asRecord(toolResult.data?.['issue']);
  if (issue === undefined) {
    return undefined;
  }

  return removeUndefined({
    code: readString(issue['code']),
    path: readString(issue['path']) ?? readString(issue['field']) ?? readString(issue['target']),
    message: readString(issue['message']),
  });
}

function extractExecuteEvidence(toolResult: ToolResultBase): {
  validationPassed?: boolean;
  readbackPassed?: boolean;
} {
  const bridgeResult = asRecord(toolResult.debug?.['bridge_result']);
  const sources = [
    toolResult,
    asRecord(toolResult.data),
    bridgeResult,
    asRecord(bridgeResult?.['data']),
  ].filter((source): source is Record<string, unknown> => source !== undefined);

  return removeUndefined({
    validationPassed: readSuccessEvidence(sources, ['validation_success', 'validation_passed'], ['validation', 'validation_result']),
    readbackPassed: readSuccessEvidence(sources, ['readback_success', 'readback_passed'], ['readback', 'readback_result']),
  });
}

function readSuccessEvidence(
  sources: Array<Record<string, unknown>>,
  booleanKeys: string[],
  nestedKeys: string[],
): boolean | undefined {
  for (const source of sources) {
    for (const key of booleanKeys) {
      if (source[key] === true) {
        return true;
      }
    }

    for (const key of nestedKeys) {
      const nested = asRecord(source[key]);
      if (nested && isSuccessEvidenceRecord(nested)) {
        return true;
      }
    }
  }

  return undefined;
}

function isSuccessEvidenceRecord(value: Record<string, unknown>): boolean {
  for (const key of ['success', 'passed', 'ok']) {
    if (value[key] === true) {
      return true;
    }
  }

  const status = readString(value['status']) ?? readString(value['result']);
  return status === 'success' || status === 'passed';
}

function elapsedMs(startedAt: number): number {
  return Math.max(0, Date.now() - startedAt);
}

function removeUndefined<T extends object>(value: T): T {
  return Object.fromEntries(Object.entries(value).filter(([, entry]) => entry !== undefined)) as T;
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
  let compiled: CompiledTaskPlan;
  try {
    compiled = await compileTaskSpecForRunner(taskSpec, taskCompiler, timing);
  } catch (err) {
    if (err instanceof TaskSpecCompileError) {
      return compileFailurePreviewOutcome(taskSpec, err, timing);
    }
    throw err;
  }
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

function compileFailurePreviewOutcome(
  taskSpec: TaskSpec,
  err: TaskSpecCompileError,
  timing?: TaskTimingTrace,
): TaskPreviewOutcome {
  const previewId = measureTaskTiming(timing, 'preview_token.allocate_preview_id', () => nextPreviewId());
  const targetAsset = readTaskSpecAssetPath(taskSpec);
  const toolResult = taskFailure(
    'preview_task',
    err.code,
    'semantic_error',
    err.message,
    err.issues,
    {
      stage: 'preflight',
      retryable: true,
    },
  );
  toolResult.target = { target_type: 'blueprint', ...(targetAsset ? { asset_path: targetAsset } : {}) };
  toolResult.data = {
    schema: TASK_PREVIEW_SCHEMA,
    preview_id: previewId,
    passed: false,
    blocked: true,
    task_type: readString((taskSpec as Record<string, unknown>)['task_type']),
    ...(readTaskSpecFeatureName(taskSpec) ? { feature_name: readTaskSpecFeatureName(taskSpec) } : {}),
    ...(targetAsset ? { target_assets: [targetAsset] } : {}),
    issues: err.issues,
  };

  return {
    previewId,
    passed: false,
    issues: err.issues,
    toolResult,
  };
}

function recordTaskCompileTiming(timing: TaskTimingTrace | undefined, compiled: CompiledTaskPlan): void {
  addTaskTimingMarker(timing, 'taskspec_compile.strategy', {
    strategy: compiled.strategyId,
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

  for (const key of ['dry_run', 'call_function_resolution_cache', 'runtime_facts', 'cache_diagnostics', 'graph_write_execution_stats']) {
    if (data && Object.hasOwn(data, key)) {
      diagnostics[key] = data[key];
    }
  }

  return diagnostics;
}

function extractDevelopExecuteDiagnostics(
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
    ue_execute_result: result,
  };

  if (data && Object.hasOwn(data, 'graph_write_execution_stats')) {
    diagnostics['graph_write_execution_stats'] = data['graph_write_execution_stats'];
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
  if (isFailedBridgeToolResult(writeResponse)) {
    return attachTaskTiming(taskFailureFromBridgeResponse(
      'execute_task',
      writeResponse,
      'execution_failed',
      'Bridge execute returned a failed ToolResult.',
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
        ...extractDevelopExecuteDiagnostics(writeResponse, timing),
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
  const errorDetailRecord = errorDetails as Partial<ToolResultError> & { agent_action?: string };
  const toolError = {
    code,
    category,
    stage: errorDetails.stage ?? (category === 'bridge_error' ? 'bridge' : 'parse_input'),
    message,
    retryable: errorDetails.retryable ?? category !== 'internal_error',
    rollback_result: errorDetails.rollback_result ?? 'not_needed',
    agent_action: errorDetailRecord.agent_action
      ?? (category === 'semantic_error' ? 'fix_taskspec_and_retry' : 'stop_and_report'),
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
  error: Partial<ToolResultError> & { agent_action?: string };
} {
  const result = asRecord(response.result);
  const topLevelResponse = response as unknown as Record<string, unknown>;
  const responseError = asRecord(topLevelResponse['error']);
  const resultError = asRecord(result?.['error']);
  const data = asRecord(result?.['data']);
  const errorRecord = responseError ?? resultError;
  const rawCode = readString(errorRecord?.['code']) ?? response.error_code ?? fallbackCode;
  const agentAction = readString(errorRecord?.['agent_action']) ?? readString(data?.['agent_action']);
  const explicitIssues = collectErrorRecordIssues(errorRecord);
  const dryRunIssues = extractDryRun(response).issues;
  const issues = explicitIssues.length > 0
    ? explicitIssues
    : dryRunIssues.length > 0
      ? dryRunIssues
      : [];
  const primaryIssue = issues.length === 1 ? issues[0] : undefined;
  const bPromotePrimaryIssue =
    primaryIssue?.code !== undefined &&
    (rawCode === fallbackCode || rawCode === 'execution_failed' || rawCode === 'bridge_error');
  const code = bPromotePrimaryIssue && primaryIssue?.code ? primaryIssue.code : rawCode;
  const responseMessage = readNonEmptyString(response.message);
  const message =
    readNonEmptyString(errorRecord?.['message']) ??
    responseMessage ??
    primaryIssue?.message ??
    fallbackMessage;
  const field =
    bPromotePrimaryIssue && primaryIssue?.path
      ? primaryIssue.path
      : readString(errorRecord?.['field']) ?? primaryIssue?.path ?? fallbackIssuePath;
  const retryable = code === 'context_stale'
    ? true
    : typeof errorRecord?.['retryable'] === 'boolean'
      ? errorRecord['retryable']
      : false;
  const normalizedIssuesBase = issues.length > 0
    ? issues
    : [{
      code,
      path: field,
      message,
    }];
  const issueDetailFields = collectTaskIssueDetailFields(data);
  const normalizedIssues = normalizedIssuesBase.length === 1
    ? [mergeTaskIssueDetailFields(normalizedIssuesBase[0], issueDetailFields)]
    : normalizedIssuesBase;

  return {
    code,
    message,
    issues: normalizedIssues,
    error: {
      stage: readToolStage(errorRecord?.['stage']) ?? 'bridge',
      retryable,
      rollback_result: readRollbackResult(errorRecord?.['rollback_result']) ?? 'not_needed',
      ...(agentAction ? { agent_action: agentAction } : {}),
      ...(field ? { field } : {}),
      ...(readString(errorRecord?.['expected']) ? { expected: readString(errorRecord?.['expected']) } : {}),
      ...(readString(errorRecord?.['actual']) ? { actual: readString(errorRecord?.['actual']) } : {}),
    },
  };
}

function collectErrorRecordIssues(errorRecord: Record<string, unknown> | undefined): TaskIssue[] {
  return arrayOfRecords(errorRecord?.['issues']).map((issue, index) => ({
    code: readString(issue['code']) ?? 'bridge_issue',
    path: readString(issue['path']) ?? readString(issue['field']) ?? `bridge.error.issues[${index}]`,
    message: readNonEmptyString(issue['message']) ?? JSON.stringify(issue),
  }));
}

function extractUeTaskRunId(writeResponse: BridgeResponse): string | undefined {
  const result = asRecord(writeResponse.result);
  const data = asRecord(result?.['data']);
  const taskRunId = data?.['task_run_id'];
  return typeof taskRunId === 'string' && taskRunId.length > 0
    ? taskRunId
    : undefined;
}

function isFailedBridgeToolResult(response: BridgeResponse): boolean {
  const result = asRecord(response.result);
  return result?.['ok'] === false;
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
  const failedStepIssues = collectFailedPreviewStepIssues(result, data, dryRun);
  const blockedByStatus =
    result?.['status'] === 'failed' ||
    dryRun?.['result'] === 'blocked' ||
    canExecute === false ||
    failedStepIssues.length > 0;
  const dryRunIssues = collectIssues(dryRun);
  const issues = dedupeTaskIssues([
    ...dryRunIssues,
    ...failedStepIssues,
  ]);
  return {
    canExecute: typeof canExecute === 'boolean' ? canExecute && !blockedByStatus : !blockedByStatus,
    issues: issues.length > 0 || !blockedByStatus
      ? issues
      : collectBlockedPreviewIssues(result, dryRun),
  };
}

function collectFailedPreviewStepIssues(
  result: Record<string, unknown> | undefined,
  data: Record<string, unknown> | undefined,
  dryRun: Record<string, unknown> | undefined,
): TaskIssue[] {
  const steps = [
    ...arrayOfRecords(data?.['steps']),
    ...arrayOfRecords(dryRun?.['steps']),
    ...arrayOfRecords(result?.['steps']),
  ];

  const issues: TaskIssue[] = [];
  const seenIssueKeys = new Set<string>();

  steps.forEach((step, index) => {
    const stepResult = asRecord(step['result']);
    const error = asRecord(stepResult?.['error']) ?? asRecord(step['error']);
    const failed =
      step['status'] === 'failed' ||
      stepResult?.['status'] === 'failed' ||
      stepResult?.['ok'] === false;
    if (!failed) {
      return;
    }

    const stepId = readNonEmptyString(step['step_id']) ?? `steps[${index}]`;
    const code =
      readString(error?.['code']) ??
      readString(stepResult?.['error_code']) ??
      'preview_step_failed';
    const path =
      readString(error?.['field']) ??
      readString(error?.['path']) ??
      `preview.steps.${stepId}`;
    const message =
      readNonEmptyString(error?.['message']) ??
      readNonEmptyString(stepResult?.['message']) ??
      readNonEmptyString(step['message']) ??
      `Preview step ${stepId} failed.`;
    const detailFields = collectFailedPreviewStepIssueDetailFields(stepResult);

    const issueKey = `${code}\u0000${path}\u0000${message}`;
    if (!seenIssueKeys.has(issueKey)) {
      seenIssueKeys.add(issueKey);
      issues.push({
        code,
        path,
        message,
        ...detailFields,
      } as TaskIssue);
    }
  });

  return issues;
}

function collectFailedPreviewStepIssueDetailFields(
  stepResult: Record<string, unknown> | undefined,
): Record<string, unknown> {
  const data = asRecord(stepResult?.['data']);
  if (!data) {
    return {};
  }

  return collectTaskIssueDetailFields(data);
}

function collectTaskIssueDetailFields(data: Record<string, unknown> | undefined): Record<string, unknown> {
  if (!data) {
    return {};
  }

  const details: Record<string, unknown> = {};
  for (const key of TASK_ISSUE_DETAIL_KEYS) {
    if (Object.hasOwn(data, key)) {
      details[key] = data[key];
    }
  }
  return details;
}

function mergeTaskIssueDetailFields(
  issue: TaskIssue,
  detailFields: Record<string, unknown>,
): TaskIssue {
  const detailEntries = Object.entries(detailFields);
  if (detailEntries.length === 0) {
    return issue;
  }

  const merged = { ...(issue as Record<string, unknown>) };
  for (const [key, value] of detailEntries) {
    if (merged[key] === undefined) {
      merged[key] = value;
    }
  }
  return merged as TaskIssue;
}

function dedupeTaskIssues(issues: TaskIssue[]): TaskIssue[] {
  const mergedIssues: TaskIssue[] = [];
  const issueByKey = new Map<string, TaskIssue>();

  for (const issue of issues) {
    const issueKey = `${issue.code}\u0000${issue.path}\u0000${issue.message}`;
    const existingIssue = issueByKey.get(issueKey);
    if (!existingIssue) {
      const copiedIssue = { ...(issue as Record<string, unknown>) } as TaskIssue;
      issueByKey.set(issueKey, copiedIssue);
      mergedIssues.push(copiedIssue);
      continue;
    }

    const existingRecord = existingIssue as Record<string, unknown>;
    for (const [key, value] of Object.entries(issue as Record<string, unknown>)) {
      if (key === 'code' || key === 'path' || key === 'message') {
        continue;
      }
      if (existingRecord[key] === undefined) {
        existingRecord[key] = value;
      }
    }
  }

  return mergedIssues;
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

function previewBlockedErrorCode(issues: TaskIssue[]): string {
  const issueCodes = new Set(
    issues
      .map((issue) => issue.code)
      .filter((code): code is string => typeof code === 'string' && code.length > 0),
  );
  if (issueCodes.size === 1) {
    return [...issueCodes][0] ?? 'task_preview_blocked';
  }
  return 'task_preview_blocked';
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
