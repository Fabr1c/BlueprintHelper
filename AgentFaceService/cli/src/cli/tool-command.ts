import {
  failureResult,
  type ToolResultBase,
} from '@blueprinthelper/task-core/result/tool-result';
import type { BridgeClient } from '@blueprinthelper/task-core/bridge/bridge-client';
import type { TaskSpecRunner } from '@blueprinthelper/task-core/task/service/task-spec-runner';
import type { TaskTimingTrace } from '@blueprinthelper/task-core/task/service/task-timing';
import type { LocalProcessResult } from '@blueprinthelper/task-core/tool-surface/types';
import type { TaskSpecRunnerMetrics } from '@blueprinthelper/task-core/task/service/task-spec-runner';
import type { MetricsIoSummary } from '@blueprinthelper/task-core/metrics/metrics-types';
import {
  getBlueprintHelperTool,
} from '@blueprinthelper/task-core/tool-surface/tool-registry';
import { readCliInputObjectWithStats } from './input.js';
import type { CliCommand } from './output.js';

export interface CliToolInvocationResult {
  toolResult: ToolResultBase;
  rawParams?: Record<string, unknown>;
  parsedParams?: Record<string, unknown>;
  inputIo?: MetricsIoSummary;
}

export async function invokeCliTool(input: {
  command: CliCommand;
  cwd: string;
  bridge: BridgeClient;
  taskRunner: TaskSpecRunner;
  metrics?: TaskSpecRunnerMetrics;
  timing?: TaskTimingTrace;
  readStdin?: () => Promise<string> | string;
  runLocalProcess?: (command: string, args: string[], options?: {
    timeoutMs?: number;
    detached?: boolean;
    env?: NodeJS.ProcessEnv;
  }) => Promise<LocalProcessResult>;
  sleep?: (ms: number) => Promise<void>;
}): Promise<CliToolInvocationResult> {
  const toolName = input.command.toolName ?? '';
  const tool = getBlueprintHelperTool(toolName);
  if (!tool) {
    return {
      toolResult: failureResult('tool.invoke', {
        code: 'unknown_tool',
        stage: 'parse_input',
        message: `Unknown BlueprintHelper tool: ${toolName}`,
        retryable: false,
        rollback_result: 'not_needed',
      }),
    };
  }
  if (tool.requiresExpert && !input.command.expert) {
    return {
      toolResult: failureResult(toolName, {
        code: 'expert_flag_required',
        stage: 'parse_input',
        message: `${toolName} requires --expert because it is ${tool.risk} risk.`,
        retryable: false,
        rollback_result: 'not_needed',
      }),
    };
  }

  const inputObject = input.command.params
    ? { value: input.command.params, io: undefined }
    : await readCliInputObjectWithStats({
      cwd: input.cwd,
      file: input.command.file,
      json: input.command.json,
      stdin: input.command.stdin,
      readStdin: input.readStdin,
    });
  const rawParams = inputObject.value;
  const params = applyDevelopFlag(toolName, rawParams, input.command.develop === true);
  const parsed = tool.inputSchema.parse(params) as Record<string, unknown>;
  return {
    rawParams,
    parsedParams: parsed,
    inputIo: inputObject.io,
    toolResult: await tool.execute(parsed, {
      cwd: input.cwd,
      bridge: input.bridge,
      taskRunner: input.taskRunner,
      metrics: input.metrics,
      timing: input.timing,
      expert: input.command.expert === true,
      runLocalProcess: input.runLocalProcess,
      sleep: input.sleep,
    }),
  };
}

function applyDevelopFlag(toolName: string, params: unknown, develop: boolean): unknown {
  if (!develop || !isTaskSpecExecutionTool(toolName) || !isRecord(params)) {
    return params;
  }

  if ('task_spec' in params) {
    return { ...params, develop: true };
  }

  return { task_spec: params, develop: true };
}

function isTaskSpecExecutionTool(toolName: string): boolean {
  return toolName === 'blueprinthelper_preview_task' || toolName === 'blueprinthelper_execute_task';
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}
