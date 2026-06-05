import * as fs from 'node:fs';
import * as path from 'node:path';
import { BridgeClient, type BridgeResponse, type BridgeSendCommandOptions } from '@blueprinthelper/task-core/bridge/bridge-client';
import { getBlueprintHelperTool } from '@blueprinthelper/task-core/tool-surface/tool-registry';
import {
  createTaskSpecRunner,
  type TaskRunnerBridge,
  type TaskSpecRunner,
} from '@blueprinthelper/task-core/task/service/task-spec-runner';
import {
  attachTaskTiming,
  measureTaskTiming,
  measureTaskTimingAsync,
  startTaskTiming,
  type TaskTimingTrace,
} from '@blueprinthelper/task-core/task/service/task-timing';
import {
  TaskSpecSchema,
} from '@blueprinthelper/task-core/task/schema/task-schemas';
import {
  buildCliError,
  shapeCliOutput,
  type CliWriteOutcome,
  writeCliResult,
  type CliCommand,
  type CliFormat,
} from './output.js';
import { createInputIoSummary, createOutputIoSummary } from './io-stats.js';
import { buildHelpText } from './help.js';
import { runMetricsCommand, type MetricsCliCommand } from './metrics-command.js';
import { parseToolAudience, parseToolRisk, runToolsCommand } from './tools-command.js';
import {
  createCliMetricsService,
  recordCliIoCompleted,
  recordCliToolCompletion,
  recordCliToolThrownError,
  resolveCliMetricsRoot,
  type CreateCliMetricsServiceOptions,
} from './metrics-runtime.js';
import { invokeCliTool, type CliToolInvocationResult } from './tool-command.js';
import {
  TOOL_RESULT_SCHEMA,
  failureResult,
  normalizeToolResult,
  successRead,
  type ToolResultBase,
} from '@blueprinthelper/task-core/result/tool-result';
import type { LocalProcessResult } from '@blueprinthelper/task-core/tool-surface/types';
import type { MetricsIoSummary } from '@blueprinthelper/task-core/metrics/metrics-types';

export interface CliRuntime {
  argv: string[];
  cwd: string;
  runner?: TaskSpecRunner;
  metrics?: ReturnType<typeof createCliMetricsService>;
  bridge?: TaskRunnerBridge;
  readStdin?: () => Promise<string> | string;
  runLocalProcess?: (command: string, args: string[], options?: {
    timeoutMs?: number;
    detached?: boolean;
    env?: NodeJS.ProcessEnv;
  }) => Promise<LocalProcessResult>;
  sleep?: (ms: number) => Promise<void>;
  stdout: (text: string) => void;
  stderr: (text: string) => void;
}

type ParseResult =
  | { ok: true; command: CliCommand }
  | { ok: true; help: true; helpTarget: string[] }
  | { ok: false; message: string };

type CliBridge = TaskRunnerBridge & {
  ping(): Promise<boolean>;
  setWriteSessionId(sessionId: string): void;
  clearWriteSessionId(): void;
  close(): void | Promise<void>;
};

const DEFAULT_CLI_BRIDGE_REQUEST_TIMEOUT_MS = 10 * 60 * 1000;
const DEFAULT_WAIT_HINT_INITIAL_MS = 30000;
const DEFAULT_WAIT_HINT_INTERVAL_MS = 30000;

const runtimeBridgeCache = new WeakMap<CliRuntime, CliBridge>();
const runtimeRunnerCache = new WeakMap<CliRuntime, TaskSpecRunner>();
const runtimeMetricsCache = new WeakMap<CliRuntime, ReturnType<typeof createCliMetricsService>>();

const READ_ONLY_BRIDGE_COMMANDS = new Set([
  'get_editor_context',
  'get_runtime_profile',
  'diagnostics_runtime',
  'read_reference_context',
  'get_debug_case',
  'list_debug_cases',
  'export_debug_bundle',
  'get_task_run_journal',
]);

export async function runCli(runtime: CliRuntime): Promise<number> {
  const cliTiming = startTaskTiming(runtime.argv.includes('--develop'), 'cli_command', 'agentface_cli');
  let parsed: ParseResult;
  try {
    parsed = measureTaskTiming(cliTiming, 'cli.parse_args', () => parseArgs(runtime.argv));
  } catch (err) {
    runtime.stderr(`${err instanceof Error ? err.message : String(err)}\n`);
    return 64;
  }
  if (!parsed.ok) {
    runtime.stderr(`${parsed.message}\n`);
    return 64;
  }
  if ('help' in parsed) {
    runtime.stdout(`${buildHelpText(parsed.helpTarget)}\n`);
    return 0;
  }

  const command = parsed.command;
  const timing = command.develop === true ? cliTiming : undefined;

  try {
    if (command.kind === 'tool.invoke') {
      const result = await runDirectCliTool(runtime, command, timing, 'cli.invoke_tool');
      const outcome = writeTimedCliResult(runtime, command, result.toolResult, timing);
      await recordCliIo(runtime, command, outcome, result.inputIo, result.parsedParams ?? result.rawParams);
      return outcome.outputTooLarge ? 3 : result.toolResult.ok ? 0 : 2;
    }

    if (command.kind === 'tools.domains' || command.kind === 'tools.list' || command.kind === 'tools.templates') {
      const output = shapeCliOutput(runToolsCommand(command), command.fields, command.omitFields);
      runtime.stdout(`${JSON.stringify(output)}\n`);
      return 0;
    }

    if (command.kind === 'metrics.report') {
      const metricsCommand = resolveMetricsCommand(command, runtime.cwd);
      const toolResult = await measureTaskTimingAsync(timing, 'cli.metrics_report', () => runMetricsCommand({
        command: metricsCommand,
      }));
      const outcome = writeTimedCliResult(runtime, metricsCommand, toolResult, timing);
      return outcome.outputTooLarge ? 3 : toolResult.ok ? 0 : 2;
    }

    if (command.kind === 'task.preview') {
      const taskSpecInput = measureTaskTiming(timing, 'taskspec_file_read_parse', () => readTaskSpecInput(
        path.resolve(runtime.cwd, required(command.file)),
      ));
      const taskSpec = TaskSpecSchema.parse(taskSpecInput.value);
      const preview = await getRunner(runtime).previewTask(taskSpec, timing);
      const outcome = writeTimedCliResult(runtime, command, preview.toolResult, timing, {
        previewId: preview.previewId,
        previewToken: preview.previewToken,
        taskPlan: preview.taskPlan,
        passed: preview.passed,
        issues: preview.issues,
      });
      await recordCliIo(runtime, command, outcome, taskSpecInput.io, taskSpec);
      return outcome.outputTooLarge ? 3 : preview.passed ? 0 : 2;
    }

    if (command.kind === 'task.execute') {
      const taskSpecInput = measureTaskTiming(timing, 'taskspec_file_read_parse', () => readTaskSpecInput(
        path.resolve(runtime.cwd, required(command.file)),
      ));
      const taskSpec = TaskSpecSchema.parse(taskSpecInput.value);
      const toolResult = await getRunner(runtime).executeTask(taskSpec, timing, { previewToken: command.previewToken });
      const outcome = writeTimedCliResult(runtime, command, toolResult, timing);
      await recordCliIo(runtime, command, outcome, taskSpecInput.io, taskSpec);
      return outcome.outputTooLarge ? 3 : toolResult.ok ? 0 : 2;
    }

    if (command.kind === 'task.result') {
      const toolResult = await measureTaskTimingAsync(timing, 'cli.get_task_result', () => getRunner(runtime).getTaskResult(required(command.taskRunId)));
      const outcome = writeTimedCliResult(runtime, command, toolResult, timing);
      await recordCliIo(runtime, command, outcome, undefined, { task_run_id: command.taskRunId });
      return outcome.outputTooLarge ? 3 : toolResult.ok ? 0 : 2;
    }

    if (command.kind === 'context.read') {
      const result = await runDirectCliTool(runtime, command, timing, 'cli.read_context');
      const outcome = writeTimedCliResult(runtime, command, result.toolResult, timing);
      await recordCliIo(runtime, command, outcome, result.inputIo, result.parsedParams ?? result.rawParams);
      return outcome.outputTooLarge ? 3 : result.toolResult.ok ? 0 : 2;
    }

    if (command.kind === 'bridge.ping') {
      const bridge = getBridge(runtime);
      const response = await measureTaskTimingAsync(timing, 'bridge.ping', () => bridge.sendCommand('ping', {}));
      const toolResult = response.success
        ? successRead('bridge_ping', { target_type: 'asset' }, normalizeBridgeData(response))
        : bridgeFailureResult('bridge_ping', response);
      const outcome = writeTimedCliResult(runtime, command, toolResult, timing);
      return outcome.outputTooLarge ? 3 : toolResult.ok ? 0 : 2;
    }

    if (command.kind === 'bridge.call') {
      const bridgeCommand = required(command.bridgeCommand);
      if (!READ_ONLY_BRIDGE_COMMANDS.has(bridgeCommand)) {
        runtime.stderr(`Bridge command is not allowed through CLI: ${bridgeCommand}\n`);
        return 64;
      }
      const response = await measureTaskTimingAsync(timing, 'bridge.call', () => getBridge(runtime).sendCommand(bridgeCommand, {}));
      const toolResult = response.success
        ? normalizeToolResult(response, `bridge.${bridgeCommand}`)
        : bridgeFailureResult(`bridge.${bridgeCommand}`, response);
      const outcome = writeTimedCliResult(runtime, command, toolResult, timing);
      return outcome.outputTooLarge ? 3 : toolResult.ok ? 0 : 2;
    }
  } catch (err) {
    const status = isBridgeUnavailable(err) ? 'bridge_unavailable' : 'cli_error';
    const errorText = `${JSON.stringify(buildCliError({
      operation: command.kind,
      status,
      message: err instanceof Error ? err.message : String(err),
      fields: command.fields,
      omitFields: command.omitFields,
    }))}\n`;
    runtime.stdout(errorText);
    await recordCliIoCompleted({
      metrics: getMetrics(runtime),
      command,
      inputIo: readCommandInputIoBestEffort(runtime, command),
      outputIo: createOutputIoSummary(errorText),
      operationInput: undefined,
    });
    return status === 'bridge_unavailable' ? 2 : 1;
  } finally {
    await closeCliOwnedBridge(runtime);
  }

  runtime.stderr(`Unsupported BlueprintHelper CLI command: ${runtime.argv.join(' ')}\n`);
  return 64;
}

function writeTimedCliResult(
  runtime: CliRuntime,
  command: CliCommand,
  toolResult: ToolResultBase,
  timing: TaskTimingTrace | undefined,
  extra: Record<string, unknown> = {},
) {
  const timedResult = attachCliTiming(toolResult, timing);
  return writeCliResult(runtime, command, timedResult, extra);
}

function attachCliTiming(
  toolResult: ToolResultBase,
  timing: TaskTimingTrace | undefined,
): ToolResultBase {
  if (!timing) {
    return toolResult;
  }

  measureTaskTiming(timing, 'cli.result_return', () => undefined);
  return attachTaskTiming(toolResult, timing);
}

async function runDirectCliTool(
  runtime: CliRuntime,
  command: CliCommand,
  timing: TaskTimingTrace | undefined,
  stageName: string,
): Promise<CliToolInvocationResult> {
  const startedAt = Date.now();
  const metrics = getMetrics(runtime);

  try {
    const result = await measureTaskTimingAsync(timing, stageName, () => invokeCliTool({
      command,
      cwd: runtime.cwd,
      bridge: getBridge(runtime) as BridgeClient,
      taskRunner: getRunner(runtime),
      metrics,
      timing,
      readStdin: runtime.readStdin ?? readProcessStdin,
      runLocalProcess: runtime.runLocalProcess,
      sleep: runtime.sleep,
    }));
    await recordCliToolCompletion({
      metrics,
      command,
      toolResult: result.toolResult,
      durationMs: elapsedMs(startedAt),
      rawParams: result.rawParams,
      parsedParams: result.parsedParams,
    });
    return result;
  } catch (err) {
    await recordCliToolThrownError({
      metrics,
      command,
      error: err,
      durationMs: elapsedMs(startedAt),
    });
    throw err;
  }
}

async function recordCliIo(
  runtime: CliRuntime,
  command: CliCommand,
  outcome: CliWriteOutcome,
  inputIo: MetricsIoSummary | undefined,
  operationInput: unknown,
): Promise<void> {
  await recordCliIoCompleted({
    metrics: getMetrics(runtime),
    command,
    inputIo,
    outputIo: {
      output_chars: outcome.outputChars,
      output_utf8_bytes: outcome.outputUtf8Bytes,
      estimated_output_tokens: outcome.estimatedOutputTokens,
    },
    operationInput,
  });
}

function parseArgs(argv: string[]): ParseResult {
  if (argv.length === 0 || argv.includes('--help') || argv.includes('-h')) {
    return { ok: true, help: true, helpTarget: parseHelpTarget(argv) };
  }

  const positionals: string[] = [];
  const options: {
    file?: string;
    id?: string;
    command?: string;
    json?: string;
    stdin?: boolean;
    develop?: boolean;
    expert?: boolean;
    previewToken?: string;
    format?: CliFormat;
    window?: '1d' | '7d' | '30d' | 'all';
    limit?: number;
    artifactDir?: string;
    maxBytes?: number;
    fields?: string[];
    omitFields?: string[];
    includeReserved?: boolean;
    audience?: 'default' | 'compat' | 'expert';
    requiresBridge?: boolean;
    risks?: string[];
  } = {};

  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === '--file') {
      options.file = readOptionValue(argv, ++index, arg);
    } else if (arg === '--id') {
      options.id = readOptionValue(argv, ++index, arg);
    } else if (arg === '--command') {
      options.command = readOptionValue(argv, ++index, arg);
    } else if (arg === '--json') {
      options.json = readOptionValue(argv, ++index, arg);
    } else if (arg === '--stdin') {
      options.stdin = true;
    } else if (arg === '--develop') {
      options.develop = true;
    } else if (arg === '--expert') {
      options.expert = true;
    } else if (arg === '--include-reserved') {
      options.includeReserved = true;
    } else if (arg === '--audience') {
      try {
        options.audience = parseToolAudience(readOptionValue(argv, ++index, arg));
      } catch (err) {
        return { ok: false, message: err instanceof Error ? err.message : String(err) };
      }
    } else if (arg === '--requires-bridge') {
      const raw = readOptionValue(argv, ++index, arg);
      if (raw !== 'true' && raw !== 'false') {
        return { ok: false, message: `--requires-bridge must be true or false: ${raw}` };
      }
      options.requiresBridge = raw === 'true';
    } else if (arg === '--risk') {
      const risks = readOptionValue(argv, ++index, arg)
        .split(',')
        .map((entry) => entry.trim())
        .filter((entry) => entry.length > 0);
      for (const risk of risks) {
        try {
          parseToolRisk(risk);
        } catch (err) {
          return { ok: false, message: err instanceof Error ? err.message : String(err) };
        }
      }
      options.risks = risks;
    } else if (arg === '--preview-token') {
      options.previewToken = readOptionValue(argv, ++index, arg);
    } else if (arg === '--format') {
      const format = readOptionValue(argv, ++index, arg);
      if (!['summary', 'json', 'full', 'markdown'].includes(format)) {
        return { ok: false, message: `Unsupported --format value: ${format}` };
      }
      options.format = format as CliFormat;
    } else if (arg === '--window') {
      const window = readOptionValue(argv, ++index, arg);
      if (!['1d', '7d', '30d', 'all'].includes(window)) {
        return { ok: false, message: `Unsupported --window value: ${window}` };
      }
      options.window = window as '1d' | '7d' | '30d' | 'all';
    } else if (arg === '--limit') {
      const rawLimit = readOptionValue(argv, ++index, arg);
      const limit = Number(rawLimit);
      if (!Number.isInteger(limit) || limit <= 0) {
        return { ok: false, message: `--limit must be a positive integer: ${rawLimit}` };
      }
      options.limit = limit;
    } else if (arg === '--artifact-dir') {
      options.artifactDir = readOptionValue(argv, ++index, arg);
    } else if (arg === '--max-bytes') {
      const rawMaxBytes = readOptionValue(argv, ++index, arg);
      const maxBytes = Number(rawMaxBytes);
      if (!Number.isInteger(maxBytes) || maxBytes <= 0) {
        return { ok: false, message: `--max-bytes must be a positive integer: ${rawMaxBytes}` };
      }
      options.maxBytes = maxBytes;
    } else if (arg === '--fields' || arg === '--select') {
      const rawFields = readOptionValue(argv, ++index, arg);
      const fields = parseFieldList(rawFields);
      if (fields.length === 0) {
        return { ok: false, message: `${arg} must include at least one field path.` };
      }
      options.fields = fields;
    } else if (arg === '--omit' || arg === '--exclude') {
      const rawFields = readOptionValue(argv, ++index, arg);
      const fields = parseFieldList(rawFields);
      if (fields.length === 0) {
        return { ok: false, message: `${arg} must include at least one field path.` };
      }
      options.omitFields = fields;
    } else if (arg.startsWith('--')) {
      return { ok: false, message: `Unknown option: ${arg}` };
    } else {
      positionals.push(arg);
    }
  }

  const base = {
    format: options.format ?? 'summary',
    artifactDir: options.artifactDir,
    maxBytes: options.maxBytes,
    fields: options.fields,
    omitFields: options.omitFields,
    develop: options.develop,
    expert: options.expert,
  };
  const [group, action] = positionals;
  const metricsOnlyOptionError = group === 'metrics' ? undefined : readMetricsOnlyOptionError(options);
  if (metricsOnlyOptionError) {
    return { ok: false, message: metricsOnlyOptionError };
  }

  if (group === 'tools') {
    if (action === 'domains' && positionals.length === 2) {
      return {
        ok: true,
        command: {
          ...base,
          kind: 'tools.domains',
          includeReserved: options.includeReserved,
          audience: options.audience ?? 'default',
        },
      };
    }
    if (action === 'list' && positionals.length === 4) {
      return {
        ok: true,
        command: {
          ...base,
          kind: 'tools.list',
          toolDomain: positionals[2],
          toolCatalogKind: positionals[3],
          audience: options.audience ?? 'default',
          requiresBridge: options.requiresBridge,
          risks: options.risks,
          expert: options.expert,
        },
      };
    }
    if (action === 'templates' && positionals.length === 3) {
      return {
        ok: true,
        command: {
          ...base,
          kind: 'tools.templates',
          toolId: positionals[2],
        },
      };
    }
    return { ok: false, message: `Unsupported BlueprintHelper CLI tools command: ${action ?? ''}` };
  }

  if (positionals.length === 1 && (group === 'open_editor' || group === 'close_editor')) {
    const toolName = group === 'open_editor' ? 'blueprint_open_editor' : 'blueprint_close_editor';
    const hasInputSource = options.file !== undefined || options.json !== undefined || options.stdin === true;
    return {
      ok: true,
      command: {
        ...base,
        kind: 'tool.invoke',
        toolName,
        params: hasInputSource ? undefined : {},
        file: options.file,
        json: options.json,
        stdin: options.stdin,
        expert: options.expert,
      },
    };
  }

  if (positionals.length === 1 && group === 'blueprinthelper_get_task_result' && options.id) {
    return {
      ok: true,
      command: {
        ...base,
        kind: 'tool.invoke',
        toolName: group,
        params: { task_run_id: options.id },
      },
    };
  }

  if (positionals.length === 1 && getBlueprintHelperTool(group)) {
    return {
      ok: true,
      command: {
        ...base,
        kind: 'tool.invoke',
        toolName: group,
        file: options.file,
        json: options.json,
        stdin: options.stdin,
        expert: options.expert,
      },
    };
  }

  if (group === 'task' && action === 'preview' && options.file) {
    return { ok: true, command: { ...base, kind: 'task.preview', file: options.file } };
  }
  if (group === 'task' && action === 'execute' && options.file) {
    return { ok: true, command: { ...base, kind: 'task.execute', file: options.file, previewToken: options.previewToken } };
  }
  if (group === 'task' && action === 'result' && options.id) {
    return { ok: true, command: { ...base, kind: 'task.result', taskRunId: options.id } };
  }
  if (group === 'bridge' && action === 'ping') {
    return { ok: true, command: { ...base, kind: 'bridge.ping' } };
  }
  if (group === 'bridge' && action === 'call' && options.command) {
    return { ok: true, command: { ...base, kind: 'bridge.call', bridgeCommand: options.command } };
  }
  if (group === 'context' && action === 'read' && options.file) {
    return { ok: true, command: { ...base, kind: 'context.read', toolName: 'blueprinthelper_read_context', file: options.file } };
  }
  if (group === 'metrics' && isMetricsAction(action)) {
    if (options.format !== undefined && options.format !== 'json' && options.format !== 'markdown') {
      return { ok: false, message: `Unsupported --format value for bh metrics ${action}: ${options.format}` };
    }
    return {
      ok: true,
      command: {
        kind: 'metrics.report',
        format: options.format ?? 'json',
        metricsKind: action,
        window: options.window ?? '7d',
        limit: options.limit ?? 20,
        artifactDir: options.artifactDir,
        maxBytes: options.maxBytes,
        fields: options.fields,
        omitFields: options.omitFields,
        develop: options.develop,
      },
    };
  }

  return { ok: false, message: `Unsupported BlueprintHelper CLI command: ${argv.join(' ')}` };
}

function readOptionValue(argv: string[], index: number, flag: string): string {
  const value = argv[index];
  if (!value || value.startsWith('--')) {
    throw new Error(`Missing value for ${flag}`);
  }
  return value;
}

function readProcessStdin(): Promise<string> {
  return new Promise((resolve, reject) => {
    const chunks: Buffer[] = [];
    process.stdin.on('data', (chunk: Buffer | string) => {
      chunks.push(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk));
    });
    process.stdin.on('end', () => resolve(Buffer.concat(chunks).toString('utf8')));
    process.stdin.on('error', reject);
  });
}

function parseFieldList(rawFields: string): string[] {
  return rawFields
    .split(',')
    .map((field) => field.trim())
    .filter((field) => field.length > 0)
    .map((field) => {
      if (!/^[A-Za-z0-9_.-]+$/.test(field)) {
        throw new Error(`Invalid field path for --fields: ${field}`);
      }
      return field;
    });
}

function getRunner(runtime: CliRuntime): TaskSpecRunner {
  if (runtime.runner) {
    return runtime.runner;
  }
  const cached = runtimeRunnerCache.get(runtime);
  if (cached) {
    return cached;
  }
  const bridge = getBridge(runtime);
  const runner = createTaskSpecRunner({
    bridge,
    metrics: getMetrics(runtime),
  });
  runtimeRunnerCache.set(runtime, runner);
  return runner;
}

function parseHelpTarget(argv: string[]): string[] {
  const optionsWithValues = new Set([
    '--artifact-dir',
    '--command',
    '--exclude',
    '--fields',
    '--file',
    '--format',
    '--id',
    '--json',
    '--max-bytes',
    '--omit',
    '--preview-token',
    '--audience',
    '--requires-bridge',
    '--risk',
    '--select',
    '--limit',
    '--window',
  ]);
  const target: string[] = [];
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === '--help' || arg === '-h') {
      continue;
    }
    if (optionsWithValues.has(arg)) {
      index += 1;
      continue;
    }
    if (arg.startsWith('--')) {
      continue;
    }
    target.push(arg);
  }
  return target;
}

function getBridge(runtime: CliRuntime): CliBridge {
  const cached = runtimeBridgeCache.get(runtime);
  if (cached) {
    return cached;
  }

  const baseBridge = runtime.bridge ?? new BridgeClient({
    host: process.env['BRIDGE_HOST'] ?? '127.0.0.1',
    port: Number(process.env['BRIDGE_PORT'] ?? 54321),
    requestTimeoutMs: readPositiveEnvInt(
      'BPH_CLI_BRIDGE_REQUEST_TIMEOUT_MS',
      readPositiveEnvInt('BRIDGE_REQUEST_TIMEOUT_MS', DEFAULT_CLI_BRIDGE_REQUEST_TIMEOUT_MS),
    ),
  });
  const bridge = createWaitHintBridge(baseBridge, runtime);
  runtimeBridgeCache.set(runtime, bridge);
  return bridge;
}

function getMetrics(runtime: CliRuntime): ReturnType<typeof createCliMetricsService> {
  if (runtime.metrics) {
    return runtime.metrics;
  }

  const cached = runtimeMetricsCache.get(runtime);
  if (cached) {
    return cached;
  }

  const options: CreateCliMetricsServiceOptions = {
    cwd: runtime.cwd,
  };
  const metrics = createCliMetricsService(options);
  runtimeMetricsCache.set(runtime, metrics);
  return metrics;
}

async function closeCliOwnedBridge(runtime: CliRuntime): Promise<void> {
  if (runtime.bridge) {
    return;
  }

  const bridge = runtimeBridgeCache.get(runtime);
  if (!bridge) {
    return;
  }

  runtimeBridgeCache.delete(runtime);
  await bridge.close();
}

function createWaitHintBridge(baseBridge: TaskRunnerBridge, runtime: CliRuntime): CliBridge {
  const optionalBridge = baseBridge as TaskRunnerBridge & Partial<CliBridge>;
  let bridge: CliBridge;
  bridge = {
    sendCommand(command, payload, options?: BridgeSendCommandOptions) {
      return runWithWaitHints(runtime, command, () => baseBridge.sendCommand(command, payload, options));
    },
    ping() {
      if (typeof optionalBridge.ping === 'function') {
        return runWithWaitHints(runtime, 'ping', () => optionalBridge.ping!());
      }
      return Promise.resolve(false);
    },
    setWriteSessionId(sessionId) {
      optionalBridge.setWriteSessionId?.(sessionId);
    },
    clearWriteSessionId() {
      optionalBridge.clearWriteSessionId?.();
    },
    close() {
      return optionalBridge.close?.();
    },
  };
  return bridge;
}

async function runWithWaitHints<T>(
  runtime: CliRuntime,
  command: string,
  operation: () => Promise<T>,
): Promise<T> {
  if (isWaitHintDisabled()) {
    return await operation();
  }

  const initialMs = readPositiveEnvInt('BPH_CLI_WAIT_HINT_INITIAL_MS', DEFAULT_WAIT_HINT_INITIAL_MS);
  const intervalMs = readPositiveEnvInt('BPH_CLI_WAIT_HINT_INTERVAL_MS', DEFAULT_WAIT_HINT_INTERVAL_MS);
  const startedAt = Date.now();
  let interval: ReturnType<typeof setInterval> | undefined;
  const emitHint = () => {
    const elapsedMs = Date.now() - startedAt;
    runtime.stderr(
      `[BlueprintHelper CLI] waiting for UE Bridge response: command=${command} elapsed_ms=${elapsedMs}. `
      + 'UE-bound requests are serialized on the editor side; keep waiting unless the CLI exits.\n',
    );
  };
  const initial = setTimeout(() => {
    emitHint();
    interval = setInterval(emitHint, intervalMs);
    interval.unref?.();
  }, initialMs);
  initial.unref?.();

  try {
    return await operation();
  } finally {
    clearTimeout(initial);
    if (interval) {
      clearInterval(interval);
    }
  }
}

function isWaitHintDisabled(): boolean {
  const raw = process.env['BPH_CLI_WAIT_HINTS'];
  return raw === '0' || raw?.toLowerCase() === 'false';
}

function readPositiveEnvInt(name: string, fallback: number): number {
  const raw = process.env[name];
  if (!raw) {
    return fallback;
  }
  const value = Number(raw);
  return Number.isInteger(value) && value > 0 ? value : fallback;
}

function readTaskSpecInput(filePath: string): { value: unknown; io: MetricsIoSummary } {
  const text = fs.readFileSync(filePath, 'utf8');
  return {
    value: JSON.parse(text),
    io: createInputIoSummary('task_file', text),
  };
}

function readCommandInputIoBestEffort(runtime: CliRuntime, command: CliCommand): MetricsIoSummary | undefined {
  if (command.json !== undefined) {
    return createInputIoSummary('json', command.json);
  }
  if (!command.file) {
    return undefined;
  }

  try {
    const text = fs.readFileSync(path.resolve(runtime.cwd, command.file), 'utf8');
    return createInputIoSummary(isTaskFileCommand(command) ? 'task_file' : 'file', text);
  } catch {
    return undefined;
  }
}

function isTaskFileCommand(command: CliCommand): boolean {
  return command.kind === 'task.preview' || command.kind === 'task.execute';
}

function required(value: string | undefined): string {
  if (!value) {
    throw new Error('Missing required CLI argument.');
  }
  return value;
}

function bridgeFailureResult(operation: string, response: BridgeResponse): ToolResultBase {
  return failureResult(operation, {
    code: response.error_code ?? 'bridge_error',
    stage: 'bridge',
    message: response.message ?? 'Bridge request failed.',
    retryable: false,
    rollback_result: 'not_needed',
  });
}

function normalizeBridgeData(response: BridgeResponse): Record<string, unknown> {
  return response.result ?? { schema: TOOL_RESULT_SCHEMA };
}

function isBridgeUnavailable(err: unknown): boolean {
  if (!(err instanceof Error)) {
    return false;
  }
  return /Bridge connection|ECONNREFUSED|timed out|closed|ended/i.test(err.message);
}

function isMetricsAction(value: string | undefined): value is MetricsCliCommand['metricsKind'] {
  return value === 'report'
    || value === 'top-errors'
    || value === 'tool-usage'
    || value === 'task-health';
}

function readMetricsOnlyOptionError(options: {
  format?: CliFormat;
  window?: '1d' | '7d' | '30d' | 'all';
  limit?: number;
}): string | undefined {
  if (options.window !== undefined) {
    return '--window is only supported for bh metrics ...';
  }
  if (options.limit !== undefined) {
    return '--limit is only supported for bh metrics ...';
  }
  if (options.format === 'markdown') {
    return '--format markdown is only supported for bh metrics ...';
  }
  return undefined;
}

function resolveMetricsCommand(command: CliCommand, cwd: string): MetricsCliCommand {
  return {
    kind: 'metrics.report',
    format: command.format === 'markdown' ? 'markdown' : 'json',
    metricsKind: command.metricsKind ?? 'report',
    metricsRoot: command.metricsRoot ?? resolveMetricsRoot(cwd),
    window: command.window ?? '7d',
    limit: command.limit ?? 20,
  };
}

function resolveMetricsRoot(cwd: string): string {
  return resolveCliMetricsRoot(cwd);
}

function elapsedMs(startedAt: number): number {
  return Math.max(0, Date.now() - startedAt);
}
