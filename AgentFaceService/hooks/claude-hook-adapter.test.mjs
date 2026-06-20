import assert from 'node:assert/strict';
import { mkdir, mkdtemp, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import { runClaudeHook } from './claude-hook-adapter.cjs';

async function makeProject() {
  const projectDir = await mkdtemp(path.join(os.tmpdir(), 'bh-claude-hook-'));
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
        task_package_id: 'pkg-claude',
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
    receipt_id: 'receipt-claude',
    cli_run_id: 'cli-claude',
    preview_id: 'preview-claude',
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
  const receipt = makeReceipt({ status: 'applied', task_run_id: 'run-claude' });
  return JSON.stringify({ ok: true, status: 'execute_succeeded', task_run_id: 'run-claude', receipt });
}

function parseStdoutJson(result) {
  assert.equal(result.exitCode, 0);
  assert.equal(result.stderr, '');
  return JSON.parse(result.stdout);
}

test('runClaudeHook denies PreToolUse through Claude hookSpecificOutput', async () => {
  const projectDir = await makeProject();
  try {
    const taskSpecPath = await writeTaskSpec(projectDir, { task_package_id: 'pkg-blocked' });
    const result = await runClaudeHook({
      cwd: projectDir,
      payload: {
        hook_event_name: 'PreToolUse',
        tool_name: 'Bash',
        tool_input: { command: `bh task execute --file "${taskSpecPath}" --preview-token tok` },
      },
    });

    const output = parseStdoutJson(result);
    assert.deepEqual(output, {
      hookSpecificOutput: {
        hookEventName: 'PreToolUse',
        permissionDecision: 'deny',
        permissionDecisionReason: 'BlueprintHelper execute requires a passed preview in the hook ledger.',
      },
    });
  } finally {
    await rm(projectDir, { recursive: true, force: true });
  }
});

test('runClaudeHook adds PostToolUse readback reminder as additional context', async () => {
  const projectDir = await makeProject();
  try {
    const taskSpecPath = await writeTaskSpec(projectDir);
    await runClaudeHook({
      cwd: projectDir,
      payload: {
        hook_event_name: 'PostToolUse',
        tool_name: 'Bash',
        tool_input: { command: `bh task preview --file "${taskSpecPath}"` },
        tool_response: { stdout: previewStdout(), exit_code: 0 },
      },
    });

    const result = await runClaudeHook({
      cwd: projectDir,
      payload: {
        hook_event_name: 'PostToolUse',
        tool_name: 'Bash',
        tool_input: { command: `bh task execute --file "${taskSpecPath}" --preview-token tok --receipt-id receipt-claude` },
        tool_response: { stdout: executeStdout(), exit_code: 0 },
      },
    });

    const output = parseStdoutJson(result);
    assert.deepEqual(output, {
      hookSpecificOutput: {
        hookEventName: 'PostToolUse',
        additionalContext: 'BlueprintHelper execute succeeded. Run the required readback with bh context read before reporting success.',
      },
    });
  } finally {
    await rm(projectDir, { recursive: true, force: true });
  }
});

test('runClaudeHook blocks Stop through Claude decision control when readback is missing', async () => {
  const projectDir = await makeProject();
  try {
    const taskSpecPath = await writeTaskSpec(projectDir);
    await runClaudeHook({
      cwd: projectDir,
      payload: {
        hook_event_name: 'PostToolUse',
        tool_name: 'Bash',
        tool_input: { command: `bh task preview --file "${taskSpecPath}"` },
        tool_response: { stdout: previewStdout(), exit_code: 0 },
      },
    });
    await runClaudeHook({
      cwd: projectDir,
      payload: {
        hook_event_name: 'PostToolUse',
        tool_name: 'Bash',
        tool_input: { command: `bh task execute --file "${taskSpecPath}" --preview-token tok --receipt-id receipt-claude` },
        tool_response: { stdout: executeStdout(), exit_code: 0 },
      },
    });

    const result = await runClaudeHook({
      cwd: projectDir,
      payload: {
        hook_event_name: 'Stop',
      },
    });

    const output = parseStdoutJson(result);
    assert.equal(output.decision, 'block');
    assert.match(output.reason, /readback is missing/i);
  } finally {
    await rm(projectDir, { recursive: true, force: true });
  }
});

test('runClaudeHook stays quiet for unrelated tool payloads', async () => {
  const result = await runClaudeHook({
    cwd: os.tmpdir(),
    payload: {
      hook_event_name: 'PreToolUse',
      tool_name: 'Bash',
      tool_input: { command: 'node --version' },
    },
  });

  assert.deepEqual(result, { exitCode: 0, stdout: '', stderr: '' });
});
