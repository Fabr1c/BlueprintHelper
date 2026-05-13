import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import type { BridgeClient } from '@blueprinthelper/task-core/bridge/bridge-client';
import { TaskSpecCompileError } from '@blueprinthelper/task-core/task/compiler/task-compiler';
import {
  compileTaskSpecWithPython,
} from '@blueprinthelper/task-core/task/compiler/task-python-orchestrator';
import {
  createTaskSpecRunner,
  type TaskCompiler,
} from '@blueprinthelper/task-core/task/service/task-spec-runner';
import {
  ExecuteTaskInputSchema,
  GetTaskResultInputSchema,
  PreviewTaskInputSchema,
  ReadTaskContextInputSchema,
  type TaskIssue,
} from '@blueprinthelper/task-core/task/schema/task-schemas';
import {
  failureResult,
  toMcpResult,
  type ToolResultBase,
  type ToolResultError,
} from '../result/tool-result.js';

export interface TaskToolsConfig {
  ueEngineDir: string;
  taskCompiler?: TaskCompiler;
}

export type { TaskCompiler } from '@blueprinthelper/task-core/task/service/task-spec-runner';

const ReadReferenceContextInputSchema = z.object({
  asset_path: z.string(),
  target_type: z.enum([
    'asset',
    'blueprint',
    'graph',
    'function',
    'event',
    'custom_event',
    'member_variable',
    'block',
    'widget',
    'data_table_row',
    'interface',
  ]).optional().default('asset'),
  target_name: z.string().optional(),
  graph_name: z.string().optional(),
  block_id: z.string().optional(),
  widget_name: z.string().optional(),
  row_name: z.string().optional(),
  interface_path: z.string().optional(),
  scope: z.enum([
    'safety_context',
    'dependencies',
    'referencers',
    'external_dependents',
    'all',
  ]).optional().default('safety_context'),
  max_results: z.number().int().positive().max(500).optional().default(50),
  include_samples: z.boolean().optional().default(true),
});

export function registerTaskTools(server: McpServer, bridge: BridgeClient, config: TaskToolsConfig): void {
  const runner = createTaskSpecRunner({
    bridge,
    taskCompiler: config.taskCompiler ?? compileTaskSpecWithPython,
  });

  server.registerTool(
    'blueprinthelper_read_task_context',
    {
      description: 'Read a compact BlueprintHelper.TaskContextPack.v1 for building a TaskSpec.',
      inputSchema: ReadTaskContextInputSchema,
    },
    async (input) => {
      return toMcpResult(await runner.readTaskContext(input));
    },
  );

  server.registerTool(
    'blueprinthelper_read_reference_context',
    {
      description: 'Read a compact BlueprintHelper.ReferenceContextPack.v1 for explicit reference questions, preview-blocked explanations, or high-risk remove/replace/rename impact checks.',
      inputSchema: ReadReferenceContextInputSchema,
    },
    async (input) => {
      return toMcpResult(await runner.readReferenceContext(input));
    },
  );

  server.registerTool(
    'blueprinthelper_preview_task',
    {
      description: 'Validate BlueprintHelper.TaskSpec.v1, compile a TaskPlan, and dry-run supported TaskSpec-first slices including composite Blueprint features.',
      inputSchema: PreviewTaskInputSchema,
    },
    async ({ task_spec }) => {
      try {
        const preview = await runner.previewTask(task_spec);
        return toMcpResult(preview.toolResult);
      } catch (err) {
        return toMcpResult(taskErrorFromUnknown('preview_task', err));
      }
    },
  );

  server.registerTool(
    'blueprinthelper_execute_task',
    {
      description: 'Preview and execute supported BlueprintHelper TaskSpec-first slices including composite Blueprint features.',
      inputSchema: ExecuteTaskInputSchema,
    },
    async ({ task_spec }) => {
      try {
        return toMcpResult(await runner.executeTask(task_spec));
      } catch (err) {
        return toMcpResult(taskErrorFromUnknown('execute_task', err));
      }
    },
  );

  server.registerTool(
    'blueprinthelper_get_task_result',
    {
      description: 'Read BlueprintHelper.TaskRunJournal.v1 for a task_run_id, preferring the UE Task Runtime journal with an in-process fallback.',
      inputSchema: GetTaskResultInputSchema,
    },
    async ({ task_run_id }) => {
      return toMcpResult(await runner.getTaskResult(task_run_id));
    },
  );
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
