import { spawn, spawnSync } from 'node:child_process';
import * as fs from 'node:fs';
import * as path from 'node:path';
import { fileURLToPath } from 'node:url';
import type {
  TaskIssue,
  TaskPlan,
  TaskSpec,
} from '../schema/task-schemas.js';
import { TaskPlanSchema } from '../schema/task-schemas.js';
import { TaskSpecCompileError } from './task-compiler.js';

export interface PythonTaskCompilerResult {
  schema: 'BlueprintHelper.TaskCompilerResult.v1';
  task_plan: TaskPlan;
  bridge_payload: Record<string, unknown>;
  task_plan_summary: Record<string, unknown>;
}

interface PythonCompilerEnvelope {
  ok: boolean;
  result?: PythonTaskCompilerResult;
  error?: {
    code?: string;
    message?: string;
    issues?: TaskIssue[];
  };
}

export class PythonTaskOrchestratorError extends TaskSpecCompileError {
  constructor(code: string, message: string, issues: TaskIssue[]) {
    super(code, message, issues);
    this.name = 'PythonTaskOrchestratorError';
  }
}

export async function compileGraphWriteAppendWithPython(
  taskSpec: TaskSpec,
  dryRun: boolean,
): Promise<PythonTaskCompilerResult> {
  return compileTaskSpecWithPython(taskSpec, dryRun);
}

export async function compileTaskSpecWithPython(
  taskSpec: TaskSpec,
  dryRun: boolean,
): Promise<PythonTaskCompilerResult> {
  const envelope = await runPythonTaskCompiler({
    task_spec: taskSpec,
    dry_run: dryRun,
  });

  if (!envelope.ok) {
    const error = envelope.error ?? {};
    throw new PythonTaskOrchestratorError(
      error.code ?? 'python_task_compile_failed',
      error.message ?? 'Python task compiler failed.',
      Array.isArray(error.issues) ? error.issues : [],
    );
  }

  if (!envelope.result) {
    throw new PythonTaskOrchestratorError(
      'python_task_invalid_response',
      'Python task compiler returned ok=true without a result.',
      [],
    );
  }

  return validateCompilerResult(envelope.result);
}

async function runPythonTaskCompiler(input: Record<string, unknown>): Promise<PythonCompilerEnvelope> {
  const pythonExe = process.env['BPH_TASK_PYTHON'] ?? process.env['PYTHON'] ?? 'python';
  const pythonPath = resolvePythonPath();
  const pythonArgs = ['-m', 'blueprinthelper_task', 'compile-task-spec'];
  const cwd = path.resolve(pythonPath, '..');
  const env = {
    ...process.env,
    PYTHONPATH: process.env['PYTHONPATH']
      ? `${pythonPath}${path.delimiter}${process.env['PYTHONPATH']}`
      : pythonPath,
  };
  const timeoutMs = Number(process.env['BPH_TASK_PYTHON_TIMEOUT_MS'] ?? 10_000);
  let child: ReturnType<typeof spawn>;
  try {
    child = spawn(
      pythonExe,
      pythonArgs,
      {
        cwd,
        env,
        stdio: ['pipe', 'pipe', 'pipe'],
      },
    );
  } catch (err) {
    return runPythonTaskCompilerSync(input, pythonExe, pythonArgs, cwd, env, timeoutMs, err);
  }

  const stdoutChunks: Buffer[] = [];
  const stderrChunks: Buffer[] = [];
  child.stdout?.on('data', (chunk: Buffer) => stdoutChunks.push(chunk));
  child.stderr?.on('data', (chunk: Buffer) => stderrChunks.push(chunk));

  const exitCodePromise = new Promise<number>((resolve, reject) => {
    const timeout = setTimeout(() => {
      child.kill();
      reject(new PythonTaskOrchestratorError(
        'python_task_timeout',
        `Python task compiler timed out after ${timeoutMs}ms.`,
        [],
      ));
    }, timeoutMs);

    child.on('error', (err) => {
      clearTimeout(timeout);
      reject(new PythonTaskOrchestratorError(
        'python_task_launch_failed',
        err.message,
        [],
      ));
    });
    child.on('close', (code) => {
      clearTimeout(timeout);
      resolve(code ?? 0);
    });
  });
  if (!child.stdin) {
    throw new PythonTaskOrchestratorError(
      'python_task_launch_failed',
      'Python task compiler stdin pipe was not available.',
      [],
    );
  }
  child.stdin.end(JSON.stringify(input));
  const exitCode = await exitCodePromise;

  const stdout = Buffer.concat(stdoutChunks).toString('utf8').trim();
  const stderr = Buffer.concat(stderrChunks).toString('utf8').trim();
  if (exitCode !== 0) {
    throw new PythonTaskOrchestratorError(
      'python_task_process_failed',
      stderr || stdout || `Python task compiler exited with code ${exitCode}.`,
      [],
    );
  }

  try {
    return JSON.parse(stdout) as PythonCompilerEnvelope;
  } catch (err) {
    const message = err instanceof Error ? err.message : String(err);
    throw new PythonTaskOrchestratorError(
      'python_task_invalid_json',
      `Python task compiler returned invalid JSON: ${message}`,
      [],
    );
  }
}

function runPythonTaskCompilerSync(
  input: Record<string, unknown>,
  pythonExe: string,
  pythonArgs: string[],
  cwd: string,
  env: NodeJS.ProcessEnv,
  timeoutMs: number,
  spawnError: unknown,
): PythonCompilerEnvelope {
  const result = spawnSync(pythonExe, pythonArgs, {
    cwd,
    env,
    input: JSON.stringify(input),
    encoding: 'utf8',
    timeout: timeoutMs,
  });

  if (result.error) {
    const code = (result.error as NodeJS.ErrnoException).code === 'ETIMEDOUT'
      ? 'python_task_timeout'
      : 'python_task_launch_failed';
    throw new PythonTaskOrchestratorError(
      code,
      result.error.message,
      [],
    );
  }

  if (result.status !== 0) {
    throw new PythonTaskOrchestratorError(
      'python_task_process_failed',
      result.stderr?.trim() || result.stdout?.trim() || `Python task compiler exited with code ${result.status}.`,
      [],
    );
  }

  try {
    return JSON.parse(result.stdout.trim()) as PythonCompilerEnvelope;
  } catch (err) {
    const message = err instanceof Error ? err.message : String(err);
    const launchMessage = spawnError instanceof Error ? ` Initial async spawn failure: ${spawnError.message}` : '';
    throw new PythonTaskOrchestratorError(
      'python_task_invalid_json',
      `Python task compiler returned invalid JSON: ${message}.${launchMessage}`,
      [],
    );
  }
}

function validateCompilerResult(result: PythonTaskCompilerResult): PythonTaskCompilerResult {
  TaskPlanSchema.parse(result.task_plan);
  if (result.schema !== 'BlueprintHelper.TaskCompilerResult.v1') {
    throw new PythonTaskOrchestratorError(
      'python_task_invalid_schema',
      `Unexpected Python task compiler schema: ${String(result.schema)}`,
      [],
    );
  }
  if (!isRecord(result.bridge_payload)) {
    throw new PythonTaskOrchestratorError(
      'python_task_invalid_bridge_payload',
      'Python task compiler did not return a Bridge payload.',
      [],
    );
  }
  return result;
}

function resolvePythonPath(): string {
  const here = path.dirname(fileURLToPath(import.meta.url));
  const candidates = [
    path.resolve(here, '..', '..', '..', 'python'),
    path.resolve(here, '..', 'python'),
    path.resolve(process.cwd(), 'python'),
  ];
  return candidates.find((candidate) => fs.existsSync(candidate)) ?? candidates[0];
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}
