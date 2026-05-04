import { spawn } from 'node:child_process';
import * as path from 'node:path';
import { fileURLToPath } from 'node:url';
import type {
  AppendBridgePayload,
  TaskIssue,
  TaskPlan,
  TaskSpec,
} from './task-schemas.js';
import { TaskPlanSchema } from './task-schemas.js';
import { TaskSpecCompileError } from './task-compiler.js';

export interface PythonTaskCompilerResult {
  schema: 'BlueprintHelper.TaskCompilerResult.v1';
  task_plan: TaskPlan;
  bridge_payload: AppendBridgePayload;
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
  const child = spawn(
    pythonExe,
    ['-m', 'blueprinthelper_task', 'compile-graph-write-append'],
    {
      cwd: path.resolve(pythonPath, '..'),
      env: {
        ...process.env,
        PYTHONPATH: process.env['PYTHONPATH']
          ? `${pythonPath}${path.delimiter}${process.env['PYTHONPATH']}`
          : pythonPath,
      },
      stdio: ['pipe', 'pipe', 'pipe'],
    },
  );

  const stdoutChunks: Buffer[] = [];
  const stderrChunks: Buffer[] = [];
  child.stdout.on('data', (chunk: Buffer) => stdoutChunks.push(chunk));
  child.stderr.on('data', (chunk: Buffer) => stderrChunks.push(chunk));

  const timeoutMs = Number(process.env['BPH_TASK_PYTHON_TIMEOUT_MS'] ?? 10_000);
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
  return path.resolve(here, '..', 'python');
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}
