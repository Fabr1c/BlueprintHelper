import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import type { BridgeClient, BridgeResponse } from './bridge-client.js';
import { buildTaskContextPack } from './task-context.js';
import {
  TaskSpecCompileError,
  summarizeTaskPlan,
} from './task-compiler.js';
import { compileGraphWriteAppendWithPython } from './task-python-orchestrator.js';
import {
  ExecuteTaskInputSchema,
  GetTaskResultInputSchema,
  PreviewTaskInputSchema,
  ReadTaskContextInputSchema,
  TASK_EXECUTION_SCHEMA,
  TASK_PREVIEW_SCHEMA,
  type TaskIssue,
  type TaskPlan,
  type TaskSpec,
} from './task-schemas.js';
import {
  TOOL_RESULT_SCHEMA,
  failureResult,
  successRead,
  toMcpResult,
  type ToolResultBase,
  type ToolResultError,
} from './tool-result.js';
import {
  getTaskResult,
  nextPreviewId,
  nextTaskRunId,
  storeTaskRunJournal,
  storeTaskResult,
} from './task-result-store.js';

export interface TaskToolsConfig {
  ueEngineDir: string;
  ueProjectFile: string;
}

export function registerTaskTools(server: McpServer, bridge: BridgeClient, _config: TaskToolsConfig): void {
  server.registerTool(
    'blueprinthelper_read_task_context',
    {
      description: 'Read a compact BlueprintHelper.TaskContextPack.v1 for building a TaskSpec.',
      inputSchema: ReadTaskContextInputSchema,
    },
    async (input) => {
      try {
        const contextPack = await buildTaskContextPack(bridge, input);
        return toMcpResult(successRead(
          'read_task_context',
          { target_type: 'blueprint', asset_path: input.target.asset_path },
          contextPack,
        ));
      } catch (err) {
        return toMcpResult(taskFailure('read_task_context', 'task_context_read_failed', 'context_error', err));
      }
    },
  );

  server.registerTool(
    'blueprinthelper_preview_task',
    {
      description: 'Validate BlueprintHelper.TaskSpec.v1, compile a TaskPlan, and dry-run the first GraphWrite Append slice.',
      inputSchema: PreviewTaskInputSchema,
    },
    async ({ task_spec }) => {
      try {
        const preview = await previewTask(bridge, task_spec);
        return toMcpResult(preview.toolResult);
      } catch (err) {
        return toMcpResult(taskErrorFromUnknown('preview_task', err));
      }
    },
  );

  server.registerTool(
    'blueprinthelper_execute_task',
    {
      description: 'Preview and execute the first BlueprintHelper TaskSpec GraphWrite Append slice.',
      inputSchema: ExecuteTaskInputSchema,
    },
    async ({ task_spec }) => {
      try {
        const preview = await previewTask(bridge, task_spec);
        if (!preview.passed) {
          return toMcpResult(taskFailure(
            'execute_task',
            'task_preview_blocked',
            'preview_error',
            'Task preview was blocked; execute_task did not write assets.',
            preview.issues,
          ));
        }

        const writeResponse = await bridge.sendCommand('execute_task_plan', {
          task_plan: preview.taskPlan,
        });
        if (!writeResponse.success) {
          return toMcpResult(taskFailure(
            'execute_task',
            writeResponse.error_code ?? 'bridge_error',
            'bridge_error',
            writeResponse.message ?? 'Bridge write failed.',
          ));
        }

        const taskRunId = extractUeTaskRunId(writeResponse) ?? nextTaskRunId();
        const bridgeResult = asRecord(writeResponse.result);
        storeTaskResult({
          taskRunId,
          previewId: preview.previewId,
          taskPlan: preview.taskPlan,
          status: 'completed',
          bridgeResult,
        });

        return toMcpResult(successRead(
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
        ) as ToolResultBase);
      } catch (err) {
        return toMcpResult(taskErrorFromUnknown('execute_task', err));
      }
    },
  );

  server.registerTool(
    'blueprinthelper_get_task_result',
    {
      description: 'Read the in-process BlueprintHelper.TaskRunJournal.v1 summary for a task_run_id.',
      inputSchema: GetTaskResultInputSchema,
    },
    async ({ task_run_id }) => {
      const taskResult = getTaskResult(task_run_id);
      const bridgeTaskResult = taskResult ?? await getBridgeTaskRunJournal(bridge, task_run_id);
      if (!bridgeTaskResult) {
        return toMcpResult(taskFailure(
          'get_task_result',
          'task_result_not_found',
          'not_found',
          `Task result not found for task_run_id=${task_run_id}.`,
        ));
      }

      return toMcpResult(successRead(
        'get_task_result',
        { target_type: 'asset' },
        bridgeTaskResult,
      ));
    },
  );
}

async function getBridgeTaskRunJournal(
  bridge: BridgeClient,
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

async function previewTask(bridge: BridgeClient, taskSpec: TaskSpec): Promise<{
  previewId: string;
  taskPlan: TaskPlan;
  passed: boolean;
  issues: TaskIssue[];
  toolResult: ToolResultBase;
}> {
  const compiled = await compileGraphWriteAppendWithPython(taskSpec, true);
  const taskPlan = compiled.task_plan;
  const previewId = nextPreviewId();
  const previewResponse = await bridge.sendCommand('preview_task_plan', {
    task_plan: taskPlan,
  });

  if (!previewResponse.success) {
    return {
      previewId,
      taskPlan,
      passed: false,
      issues: [{
        code: previewResponse.error_code ?? 'bridge_error',
        path: 'bridge.append_blueprint_graph',
        message: previewResponse.message ?? 'Bridge dry-run failed.',
      }],
      toolResult: taskFailure(
        'preview_task',
        previewResponse.error_code ?? 'bridge_error',
        'bridge_error',
        previewResponse.message ?? 'Bridge dry-run failed.',
      ),
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
    toolResult: {
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
    },
  };
}

function taskErrorFromUnknown(operation: string, err: unknown): ToolResultBase {
  if (err instanceof TaskSpecCompileError) {
    return taskFailure(operation, err.code, 'semantic_error', err.message, err.issues);
  }
  return taskFailure(operation, 'task_internal_error', 'internal_error', err);
}

function taskFailure(
  operation: string,
  code: string,
  category: string,
  error: unknown,
  issues: TaskIssue[] = [],
): ToolResultBase {
  const message = error instanceof Error ? error.message : String(error);
  const toolError = {
    code,
    category,
    stage: category === 'bridge_error' ? 'bridge' : 'parse_input',
    message,
    retryable: category !== 'internal_error',
    rollback_result: 'not_needed',
    agent_action: category === 'semantic_error' ? 'fix_taskspec_and_retry' : 'stop_and_report',
    issues,
  } as ToolResultError;

  return failureResult(operation, toolError);
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
  return taskRunId === requestedTaskRunId ? journal : undefined;
}

function extractDryRun(resp: BridgeResponse): { canExecute: boolean; issues: TaskIssue[] } {
  const result = asRecord(resp.result);
  const data = asRecord(result?.['data']);
  const dryRun = asRecord(data?.['dry_run']) ?? asRecord(result?.['dry_run']);
  const canExecute = dryRun?.['can_execute'];
  const blockedByStatus = result?.['status'] === 'failed' || dryRun?.['result'] === 'blocked';
  const issues = collectIssues(dryRun);
  return {
    canExecute: typeof canExecute === 'boolean' ? canExecute : !blockedByStatus,
    issues,
  };
}

function collectIssues(dryRun: Record<string, unknown> | undefined): TaskIssue[] {
  if (!dryRun) return [];
  const rawIssues = [
    ...arrayOfRecords(dryRun['errors']),
    ...arrayOfRecords(dryRun['conflicts']),
    ...arrayOfRecords(dryRun['warnings']),
  ];
  return rawIssues.map((issue, index) => ({
    code: typeof issue['code'] === 'string' ? issue['code'] : 'dry_run_issue',
    path: typeof issue['target'] === 'string' ? issue['target'] : `dry_run.issues[${index}]`,
    message: typeof issue['message'] === 'string' ? issue['message'] : JSON.stringify(issue),
  }));
}

function arrayOfRecords(value: unknown): Array<Record<string, unknown>> {
  return Array.isArray(value)
    ? value.filter((item): item is Record<string, unknown> => item !== null && typeof item === 'object' && !Array.isArray(item))
    : [];
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return value !== null && typeof value === 'object' && !Array.isArray(value)
    ? value as Record<string, unknown>
    : undefined;
}
