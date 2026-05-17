import type { BridgeClient, BridgeResponse } from '../../bridge/bridge-client.js';
import { buildTaskContextPack } from '../context/task-context.js';
import {
  TaskSpecCompileError,
  summarizeTaskPlan,
} from '../compiler/task-compiler.js';
import type { PythonTaskCompilerResult } from '../compiler/task-python-orchestrator.js';
import {
  TASK_EXECUTION_SCHEMA,
  TASK_PREVIEW_SCHEMA,
  type ReadTaskContextInput,
  type TaskIssue,
  type TaskPlan,
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

export type TaskRunnerBridge = {
  sendCommand(command: string, payload?: Record<string, unknown>): Promise<BridgeResponse>;
};

export type TaskCompiler = (taskSpec: TaskSpec, dryRun: boolean) => Promise<PythonTaskCompilerResult>;

export interface TaskPreviewOutcome {
  previewId: string;
  taskPlan: TaskPlan;
  passed: boolean;
  issues: TaskIssue[];
  toolResult: ToolResultBase;
}

export interface TaskSpecRunner {
  readTaskContext(input: Record<string, unknown>): Promise<ToolResultBase>;
  readReferenceContext(input: Record<string, unknown>): Promise<ToolResultBase>;
  previewTask(taskSpec: TaskSpec): Promise<TaskPreviewOutcome>;
  executeTask(taskSpec: TaskSpec): Promise<ToolResultBase>;
  getTaskResult(taskRunId: string): Promise<ToolResultBase>;
}

export function createTaskSpecRunner(input: {
  bridge: TaskRunnerBridge;
  taskCompiler: TaskCompiler;
}): TaskSpecRunner {
  const bridge = input.bridge;
  const taskCompiler = input.taskCompiler;

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

    previewTask(taskSpec) {
      return previewTask(bridge, taskSpec, taskCompiler);
    },

    async executeTask(taskSpec) {
      try {
        const preview = await previewTask(bridge, taskSpec, taskCompiler);
        if (!preview.passed) {
          return taskFailure(
            'execute_task',
            'task_preview_blocked',
            'preview_error',
            'Task preview was blocked; execute_task did not write assets.',
            preview.issues,
          );
        }

        const writeResponse = await bridge.sendCommand('execute_task_plan', {
          task_plan: preview.taskPlan,
        });
        if (!writeResponse.success) {
          return taskFailureFromBridgeResponse(
            'execute_task',
            writeResponse,
            'bridge_error',
            'Bridge write failed.',
            'bridge.execute_task_plan',
          );
        }

        const taskRunId = extractUeTaskRunId(writeResponse) ?? nextTaskRunId();
        const bridgeResult = asRecord(writeResponse.result);
        const modified = isBridgeResultModified(bridgeResult);
        storeTaskResult({
          taskRunId,
          previewId: preview.previewId,
          taskPlan: preview.taskPlan,
          status: 'completed',
          bridgeResult,
        });

        const result = successRead(
          'execute_task',
          { target_type: 'blueprint', asset_path: preview.taskPlan.target_assets[0] },
          {
            schema: TASK_EXECUTION_SCHEMA,
            task_run_id: taskRunId,
            preview_id: preview.previewId,
            task: {
              task_run_id: taskRunId,
              feature_name: preview.taskPlan.task_name,
              target_assets: preview.taskPlan.target_assets,
              applied_steps: preview.taskPlan.steps.length,
              modified_assets: preview.taskPlan.target_assets.length,
            },
            bridge_result: bridgeResult,
          },
        ) as ToolResultBase;
        result.modified = modified;
        return result;
      } catch (err) {
        return taskErrorFromUnknown('execute_task', err);
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

async function previewTask(
  bridge: TaskRunnerBridge,
  taskSpec: TaskSpec,
  taskCompiler: TaskCompiler,
): Promise<TaskPreviewOutcome> {
  const compiled = await taskCompiler(taskSpec, true);
  const taskPlan = compiled.task_plan;
  const previewId = nextPreviewId();
  const previewResponse = await bridge.sendCommand('preview_task_plan', {
    task_plan: taskPlan,
  });

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
    toolResult.target = { target_type: 'blueprint', asset_path: taskPlan.target_assets[0] };
    toolResult.data = {
      schema: TASK_PREVIEW_SCHEMA,
      preview_id: previewId,
      passed: false,
      blocked: true,
      task_plan: summarizeTaskPlan(taskPlan),
      issues: failure.issues,
    };

    return {
      previewId,
      taskPlan,
      passed: false,
      issues: failure.issues,
      toolResult,
    };
  }

  const dryRun = extractDryRun(previewResponse);
  const passed = dryRun.canExecute;
  const issues = dryRun.issues;

  return {
    previewId,
    taskPlan,
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
        passed,
        blocked: !passed,
        task_plan: summarizeTaskPlan(taskPlan),
        issues,
      },
    }),
  };
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
