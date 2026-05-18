import * as fs from 'node:fs';
import * as path from 'node:path';
import { BridgeClient, type BridgeResponse } from '@blueprinthelper/task-core/bridge/bridge-client';
import { getBlueprintHelperTool } from '@blueprinthelper/task-core/tool-surface/tool-registry';
import { compileTaskSpecWithPython } from '@blueprinthelper/task-core/task/compiler/task-python-orchestrator';
import {
  createTaskSpecRunner,
  type TaskRunnerBridge,
  type TaskSpecRunner,
} from '@blueprinthelper/task-core/task/service/task-spec-runner';
import {
  ReadTaskContextInputSchema,
  TaskSpecSchema,
} from '@blueprinthelper/task-core/task/schema/task-schemas';
import {
  buildCliError,
  writeCliResult,
  type CliCommand,
  type CliFormat,
} from './output.js';
import { invokeCliTool } from './tool-command.js';
import {
  TOOL_RESULT_SCHEMA,
  failureResult,
  normalizeToolResult,
  successRead,
  type ToolResultBase,
} from '@blueprinthelper/task-core/result/tool-result';
import type { LocalProcessResult } from '@blueprinthelper/task-core/tool-surface/types';

export interface CliRuntime {
  argv: string[];
  cwd: string;
  runner?: TaskSpecRunner;
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
  | { ok: true; help: true }
  | { ok: false; message: string };

type CliBridge = TaskRunnerBridge & {
  ping(): Promise<boolean>;
  setWriteSessionId(sessionId: string): void;
  clearWriteSessionId(): void;
  close(): void;
};

const DEFAULT_CLI_BRIDGE_REQUEST_TIMEOUT_MS = 10 * 60 * 1000;
const DEFAULT_WAIT_HINT_INITIAL_MS = 30000;
const DEFAULT_WAIT_HINT_INTERVAL_MS = 30000;

const runtimeBridgeCache = new WeakMap<CliRuntime, CliBridge>();

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
  let parsed: ParseResult;
  try {
    parsed = parseArgs(runtime.argv);
  } catch (err) {
    runtime.stderr(`${err instanceof Error ? err.message : String(err)}\n`);
    return 64;
  }
  if (!parsed.ok) {
    runtime.stderr(`${parsed.message}\n`);
    return 64;
  }
  if ('help' in parsed) {
    runtime.stdout(`${helpText()}\n`);
    return 0;
  }

  const command = parsed.command;

  try {
    if (command.kind === 'tool.invoke') {
      const toolResult = await invokeCliTool({
        command,
        cwd: runtime.cwd,
        bridge: getBridge(runtime) as BridgeClient,
        taskRunner: getRunner(runtime),
        readStdin: runtime.readStdin ?? readProcessStdin,
        runLocalProcess: runtime.runLocalProcess,
        sleep: runtime.sleep,
      });
      const outcome = writeCliResult(runtime, command, toolResult);
      return outcome.outputTooLarge ? 3 : toolResult.ok ? 0 : 2;
    }

    if (command.kind === 'task.preview') {
      const taskSpec = TaskSpecSchema.parse(readJsonFile(path.resolve(runtime.cwd, required(command.file))));
      const preview = await getRunner(runtime).previewTask(taskSpec);
      const outcome = writeCliResult(runtime, command, preview.toolResult, {
        previewId: preview.previewId,
        taskPlan: preview.taskPlan,
        passed: preview.passed,
        issues: preview.issues,
      });
      return outcome.outputTooLarge ? 3 : preview.passed ? 0 : 2;
    }

    if (command.kind === 'task.execute') {
      const taskSpec = TaskSpecSchema.parse(readJsonFile(path.resolve(runtime.cwd, required(command.file))));
      const toolResult = await getRunner(runtime).executeTask(taskSpec);
      const outcome = writeCliResult(runtime, command, toolResult);
      return outcome.outputTooLarge ? 3 : toolResult.ok ? 0 : 2;
    }

    if (command.kind === 'task.result') {
      const toolResult = await getRunner(runtime).getTaskResult(required(command.taskRunId));
      const outcome = writeCliResult(runtime, command, toolResult);
      return outcome.outputTooLarge ? 3 : toolResult.ok ? 0 : 2;
    }

    if (command.kind === 'context.read') {
      const input = ReadTaskContextInputSchema.parse(readJsonFile(path.resolve(runtime.cwd, required(command.file))));
      const toolResult = await getRunner(runtime).readTaskContext(input);
      const outcome = writeCliResult(runtime, command, toolResult);
      return outcome.outputTooLarge ? 3 : toolResult.ok ? 0 : 2;
    }

    if (command.kind === 'bridge.ping') {
      const bridge = getBridge(runtime);
      const response = await bridge.sendCommand('ping', {});
      const toolResult = response.success
        ? successRead('bridge_ping', { target_type: 'asset' }, normalizeBridgeData(response))
        : bridgeFailureResult('bridge_ping', response);
      const outcome = writeCliResult(runtime, command, toolResult);
      return outcome.outputTooLarge ? 3 : toolResult.ok ? 0 : 2;
    }

    if (command.kind === 'bridge.call') {
      const bridgeCommand = required(command.bridgeCommand);
      if (!READ_ONLY_BRIDGE_COMMANDS.has(bridgeCommand)) {
        runtime.stderr(`Bridge command is not allowed through CLI: ${bridgeCommand}\n`);
        return 64;
      }
      const response = await getBridge(runtime).sendCommand(bridgeCommand, {});
      const toolResult = response.success
        ? normalizeToolResult(response, `bridge.${bridgeCommand}`)
        : bridgeFailureResult(`bridge.${bridgeCommand}`, response);
      const outcome = writeCliResult(runtime, command, toolResult);
      return outcome.outputTooLarge ? 3 : toolResult.ok ? 0 : 2;
    }
  } catch (err) {
    const status = isBridgeUnavailable(err) ? 'bridge_unavailable' : 'cli_error';
    runtime.stdout(`${JSON.stringify(buildCliError({
      operation: command.kind,
      status,
      message: err instanceof Error ? err.message : String(err),
      fields: command.fields,
      omitFields: command.omitFields,
    }))}\n`);
    return status === 'bridge_unavailable' ? 2 : 1;
  }

  runtime.stderr(`Unsupported BlueprintHelper CLI command: ${runtime.argv.join(' ')}\n`);
  return 64;
}

function parseArgs(argv: string[]): ParseResult {
  if (argv.length === 0 || argv.includes('--help') || argv.includes('-h')) {
    return { ok: true, help: true };
  }

  const positionals: string[] = [];
  const options: {
    file?: string;
    id?: string;
    command?: string;
    json?: string;
    stdin?: boolean;
    expert?: boolean;
    format?: CliFormat;
    artifactDir?: string;
    maxBytes?: number;
    fields?: string[];
    omitFields?: string[];
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
    } else if (arg === '--expert') {
      options.expert = true;
    } else if (arg === '--format') {
      const format = readOptionValue(argv, ++index, arg);
      if (!['summary', 'json', 'full'].includes(format)) {
        return { ok: false, message: `Unsupported --format value: ${format}` };
      }
      options.format = format as CliFormat;
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
    expert: options.expert,
  };
  const [group, action] = positionals;

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
    return { ok: true, command: { ...base, kind: 'task.execute', file: options.file } };
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
    return { ok: true, command: { ...base, kind: 'context.read', file: options.file } };
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
  const bridge = getBridge(runtime);
  return createTaskSpecRunner({
    bridge,
    taskCompiler: compileTaskSpecWithPython,
  });
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

function createWaitHintBridge(baseBridge: TaskRunnerBridge, runtime: CliRuntime): CliBridge {
  const optionalBridge = baseBridge as TaskRunnerBridge & Partial<CliBridge>;
  let bridge: CliBridge;
  bridge = {
    sendCommand(command, payload) {
      return runWithWaitHints(runtime, command, () => baseBridge.sendCommand(command, payload));
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
      optionalBridge.close?.();
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

function readJsonFile(filePath: string): unknown {
  return JSON.parse(fs.readFileSync(filePath, 'utf8'));
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

function helpText(): string {
  return [
    'BlueprintHelper CLI',
    '',
    'Usage:',
    '  blueprinthelper-cli <tool_name> [--file params.json | --json json | --stdin] [--fields path[,path...]] [--omit path[,path...]]',
    '  blueprinthelper-cli open_editor [--file params.json | --json json | --stdin] [--fields path[,path...]] [--omit path[,path...]]',
    '  blueprinthelper-cli close_editor [--file params.json | --json json | --stdin] [--fields path[,path...]] [--omit path[,path...]]',
    '  blueprinthelper-cli task preview --file <task-spec.json> [--format summary|json|full] [--fields path[,path...]] [--omit path[,path...]]',
    '  blueprinthelper-cli task execute --file <task-spec.json> [--format summary|json|full] [--fields path[,path...]] [--omit path[,path...]]',
    '  blueprinthelper-cli task result --id <task_run_id> [--fields path[,path...]] [--omit path[,path...]]',
    '  blueprinthelper-cli context read --file <context-request.json> [--fields path[,path...]] [--omit path[,path...]]',
    '  blueprinthelper-cli bridge ping [--fields path[,path...]] [--omit path[,path...]]',
    '  blueprinthelper-cli bridge call --command <read_only_command> [--fields path[,path...]] [--omit path[,path...]]',
    '',
    'Notes:',
    '  Long UE Bridge waits emit progress hints to stderr. Stdout remains final JSON.',
    '  In PowerShell, prefer --file or --stdin for generated JSON; inline --json may lose quotes.',
  ].join('\n');
}
