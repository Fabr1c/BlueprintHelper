import assert from 'node:assert/strict';
import { mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import { runCodexHook } from './codex-hook-adapter.cjs';

async function makeProject() {
  const projectDir = await mkdtemp(path.join(os.tmpdir(), 'bh-codex-hook-'));
  await writeFile(path.join(projectDir, 'Demo.uproject'), '{}\n', 'utf8');
  await mkdir(path.join(projectDir, '.blueprinthelper'), { recursive: true });
  await writeFile(path.join(projectDir, '.blueprinthelper', 'project-profile.json'), '{}\n', 'utf8');
  return projectDir;
}

async function writeTaskSpec(projectDir, value = {}) {
  const taskSpecPath = path.join(projectDir, 'task.json');
  await writeFile(
    taskSpecPath,
    JSON.stringify(
      {
        task_package_id: 'pkg-123',
        project_dir: projectDir,
        prewrite_gates: {
          source_control: { status: 'passed' },
          write_session: { status: 'approved' },
        },
        readback_required: true,
        ...value,
      },
      null,
      2,
    ),
    'utf8',
  );
  return taskSpecPath;
}

const HASH_A = 'a'.repeat(64);
const HASH_B = 'b'.repeat(64);
const HASH_C = 'c'.repeat(64);

function makeReceipt(value = {}) {
  return {
    schema: 'BlueprintHelper.ExecutionReceipt.v1',
    receipt_id: 'receipt-123',
    cli_run_id: 'cli-123',
    preview_id: 'preview-123',
    task_spec_hash: HASH_A,
    task_plan_hash: HASH_B,
    policy_hash: HASH_C,
    status: 'previewed',
    created_at: '2026-06-20T00:00:00.000Z',
    updated_at: '2026-06-20T00:00:00.000Z',
    ...value,
  };
}

function previewStdout() {
  const receipt = makeReceipt();
  return JSON.stringify({ ok: true, status: 'preview_passed', preview_token: 'tok', receipt });
}

function executeStdout() {
  const receipt = makeReceipt({ status: 'applied', task_run_id: 'run-123' });
  return JSON.stringify({ ok: true, status: 'execute_succeeded', task_run_id: 'run-123', receipt });
}

test('runCodexHook parses Codex snake_case payloads and emits machine-readable blocks', async () => {
  const projectDir = await makeProject();
  try {
    const taskSpecPath = await writeTaskSpec(projectDir, { task_package_id: 'pkg-blocked' });
    const result = await runCodexHook({
      event: 'PreToolUse',
      cwd: projectDir,
      payload: {
        tool_name: 'Bash',
        tool_input: { command: `bh task execute --file "${taskSpecPath}" --preview-token tok` },
        tool_response: { stdout: '{"status":"completed"}', stderr: '' },
      },
    });

    assert.equal(result.exitCode, 2);
    assert.match(result.stdout, /"action":"block"/);
    assert.match(result.stdout, /"reason":"preview_required_before_execute"/);
    assert.match(result.stderr, /preview/i);
  } finally {
    await rm(projectDir, { recursive: true, force: true });
  }
});

test('runCodexHook parses Codex camelCase payloads and emits readback reminders after execute', async () => {
  const projectDir = await makeProject();
  try {
    const taskSpecPath = await writeTaskSpec(projectDir);
    await runCodexHook({
      event: 'PostToolUse',
      cwd: projectDir,
      payload: {
        toolName: 'functions.shell_command',
        toolInput: { command: `bh task preview --file "${taskSpecPath}"` },
        toolOutput: { stdout: previewStdout(), exit_code: 0 },
      },
    });

    const result = await runCodexHook({
      event: 'PostToolUse',
      cwd: projectDir,
      payload: {
        toolName: 'functions.shell_command',
        toolInput: { command: `bh task execute --file "${taskSpecPath}" --preview-token tok --receipt-id receipt-123` },
        toolOutput: { stdout: executeStdout(), exit_code: 0 },
      },
    });

    assert.equal(result.exitCode, 0);
    assert.equal(result.stdout, '');
    assert.match(result.stderr, /readback/i);
  } finally {
    await rm(projectDir, { recursive: true, force: true });
  }
});

test('runCodexHook stays quiet for unrelated commands and malformed empty payloads', async () => {
  const unrelated = await runCodexHook({
    event: 'PreToolUse',
    cwd: os.tmpdir(),
    payload: {
      toolName: 'functions.shell_command',
      toolInput: { command: 'node --version' },
    },
  });
  assert.deepEqual(unrelated, { exitCode: 0, stdout: '', stderr: '' });

  const empty = await runCodexHook({ event: 'PreToolUse', cwd: os.tmpdir(), payload: undefined });
  assert.deepEqual(empty, { exitCode: 0, stdout: '', stderr: '' });
});
