import assert from 'node:assert/strict';
import { mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import {
  classifyCommand,
  readLedger,
  resolveLedgerRoot,
  runWorkflowHook,
} from './workflow-hook-core.cjs';

async function makeProject() {
  const projectDir = await mkdtemp(path.join(os.tmpdir(), 'bh-hook-core-'));
  await writeFile(path.join(projectDir, 'Demo.uproject'), '{"FileVersion":3}\n', 'utf8');
  await mkdir(path.join(projectDir, '.blueprinthelper'), { recursive: true });
  await writeFile(
    path.join(projectDir, '.blueprinthelper', 'project-profile.json'),
    JSON.stringify({ schema: 'BlueprintHelper.ProjectProfile.v1' }, null, 2),
    'utf8',
  );
  return projectDir;
}

async function writeTaskSpec(projectDir, value = {}) {
  const taskSpecPath = path.join(projectDir, 'task-spec.json');
  await writeFile(
    taskSpecPath,
    JSON.stringify(
      {
        schema: 'BlueprintHelper.TaskSpec.v1',
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

test('classifyCommand recognizes BlueprintHelper task/read commands and ignores unrelated commands', () => {
  assert.deepEqual(classifyCommand('bh task preview --file task-spec.json'), {
    kind: 'preview',
    file: 'task-spec.json',
  });
  assert.deepEqual(classifyCommand('bh task execute --file task-spec.json --preview-token tok'), {
    kind: 'execute',
    file: 'task-spec.json',
    previewToken: 'tok',
  });
  assert.deepEqual(classifyCommand('bh task result --id task-run-id'), {
    kind: 'result',
    id: 'task-run-id',
  });
  assert.deepEqual(classifyCommand('bh context read --file read-spec.json'), {
    kind: 'context_read',
    file: 'read-spec.json',
  });
  assert.deepEqual(classifyCommand('bh blueprinthelper_read_reference_context --file ref.json'), {
    kind: 'context_read',
    file: 'ref.json',
  });
  assert.equal(classifyCommand('node script.js'), undefined);
});

test('resolveLedgerRoot uses explicit project dir, project profile, package data, and rejects plugin checkout fallback', async () => {
  const projectDir = await makeProject();
  try {
    assert.equal(
      resolveLedgerRoot({ projectDir, cwd: path.join(projectDir, 'Plugins', 'BlueprintHelper') }),
      path.join(projectDir, 'Saved', 'BlueprintHelper', 'HookLedger'),
    );
    assert.equal(
      resolveLedgerRoot({ cwd: path.join(projectDir, '.blueprinthelper') }),
      path.join(projectDir, 'Saved', 'BlueprintHelper', 'HookLedger'),
    );
    assert.equal(
      resolveLedgerRoot({ packageData: { project_dir: projectDir }, cwd: os.tmpdir() }),
      path.join(projectDir, 'Saved', 'BlueprintHelper', 'HookLedger'),
    );
    assert.throws(
      () => resolveLedgerRoot({ cwd: os.tmpdir() }),
      /hook_ledger_project_path_unresolved/,
    );
  } finally {
    await rm(projectDir, { recursive: true, force: true });
  }
});

test('PreToolUse blocks execute without package id, prewrite gates, or passed preview', async () => {
  const projectDir = await makeProject();
  try {
    const missingPackage = await writeTaskSpec(projectDir, { task_package_id: '' });
    assert.equal(
      (
        await runWorkflowHook({
          event: 'PreToolUse',
          command: `bh task execute --file "${missingPackage}" --preview-token tok`,
          cwd: projectDir,
        })
      ).reason,
      'task_package_id_missing',
    );

    const missingGate = await writeTaskSpec(projectDir, {
      task_package_id: 'pkg-missing-gate',
      prewrite_gates: { source_control: { status: 'passed' } },
    });
    assert.equal(
      (
        await runWorkflowHook({
          event: 'PreToolUse',
          command: `bh task execute --file "${missingGate}" --preview-token tok`,
          cwd: projectDir,
        })
      ).reason,
      'write_session_gate_missing_or_failed',
    );

    const failedSourceControl = await writeTaskSpec(projectDir, {
      task_package_id: 'pkg-failed-source-control',
      prewrite_gates: {
        source_control: { status: 'failed' },
        write_session: { status: 'approved' },
      },
    });
    assert.equal(
      (
        await runWorkflowHook({
          event: 'PreToolUse',
          command: `bh task execute --file "${failedSourceControl}" --preview-token tok`,
          cwd: projectDir,
        })
      ).reason,
      'source_control_gate_missing_or_failed',
    );

    const noPreview = await writeTaskSpec(projectDir, { task_package_id: 'pkg-no-preview' });
    assert.equal(
      (
        await runWorkflowHook({
          event: 'PreToolUse',
          command: `bh task execute --file "${noPreview}" --preview-token tok`,
          cwd: projectDir,
        })
      ).reason,
      'preview_required_before_execute',
    );
  } finally {
    await rm(projectDir, { recursive: true, force: true });
  }
});

test('PostToolUse records preview, blocks retry budget after 3 attempts, and reminds after execute success', async () => {
  const projectDir = await makeProject();
  try {
    const taskSpecPath = await writeTaskSpec(projectDir);
    for (let index = 0; index < 3; index += 1) {
      const result = await runWorkflowHook({
        event: 'PostToolUse',
        command: `bh task preview --file "${taskSpecPath}"`,
        cwd: projectDir,
        toolResult: { exit_code: 0, stdout: '{"ok":true,"status":"preview_passed","tool_result":{"data":{"preview_token":"tok"}}}' },
      });
      assert.equal(result.action, 'allow');
    }

    assert.equal(
      (
        await runWorkflowHook({
          event: 'PreToolUse',
          command: `bh task execute --file "${taskSpecPath}" --preview-token tok`,
          cwd: projectDir,
        })
      ).reason,
      'retry_budget_exceeded',
    );

    const taskSpecWithBudget = await writeTaskSpec(projectDir, { task_package_id: 'pkg-exec' });
    await runWorkflowHook({
      event: 'PostToolUse',
      command: `bh task preview --file "${taskSpecWithBudget}"`,
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: '{"ok":true,"status":"preview_passed","preview_token":"tok"}' },
    });
    const executeResult = await runWorkflowHook({
      event: 'PostToolUse',
      command: `bh task execute --file "${taskSpecWithBudget}" --preview-token tok`,
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: '{"ok":true,"status":"execute_succeeded","task_run_id":"run-123"}' },
    });
    assert.equal(executeResult.action, 'remind');
    assert.equal(executeResult.reason, 'readback_required_after_execute');
    assert.match(executeResult.message, /readback/i);

    const ledger = await readLedger({
      ledgerRoot: path.join(projectDir, 'Saved', 'BlueprintHelper', 'HookLedger'),
      taskPackageId: 'pkg-exec',
    });
    assert.equal(ledger.last_execute.task_run_id, 'run-123');
    assert.equal(ledger.last_preview.preview_token, 'tok');
    assert.equal(ledger.readback.completed, false);

    await runWorkflowHook({
      event: 'PostToolUse',
      command: 'bh task result --id run-123',
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: '{"status":"success"}' },
    });
    const ledgerAfterResult = await readLedger({
      ledgerRoot: path.join(projectDir, 'Saved', 'BlueprintHelper', 'HookLedger'),
      taskPackageId: 'pkg-exec',
    });
    assert.equal(ledgerAfterResult.last_result.task_run_id, 'run-123');
  } finally {
    await rm(projectDir, { recursive: true, force: true });
  }
});

test('PostToolUse marks readback complete and Stop blocks active ledgers missing readback', async () => {
  const projectDir = await makeProject();
  try {
    const taskSpecPath = await writeTaskSpec(projectDir);
    await runWorkflowHook({
      event: 'PostToolUse',
      command: `bh task preview --file "${taskSpecPath}"`,
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: '{"ok":true,"status":"preview_passed","preview_token":"tok"}' },
    });
    await runWorkflowHook({
      event: 'PostToolUse',
      command: `bh task execute --file "${taskSpecPath}" --preview-token tok`,
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: '{"ok":true,"status":"execute_succeeded","task_run_id":"run-123"}' },
    });

    const stopBeforeReadback = await runWorkflowHook({ event: 'Stop', cwd: projectDir });
    assert.equal(stopBeforeReadback.action, 'block');
    assert.equal(stopBeforeReadback.reason, 'readback_missing_before_success');

    const subagentStopBeforeReadback = await runWorkflowHook({ event: 'SubagentStop', cwd: projectDir });
    assert.equal(subagentStopBeforeReadback.action, 'block');
    assert.equal(subagentStopBeforeReadback.reason, 'readback_missing_before_success');

    await runWorkflowHook({
      event: 'PostToolUse',
      command: 'bh context read --file read-spec.json',
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: '{"status":"success"}' },
      metadata: { task_package_id: 'pkg-123' },
    });

    const stopAfterReadback = await runWorkflowHook({ event: 'Stop', cwd: projectDir });
    assert.equal(stopAfterReadback.action, 'allow');
  } finally {
    await rm(projectDir, { recursive: true, force: true });
  }
});

test('Stop and SubagentStop stay quiet when there is no active ledger', async () => {
  const projectDir = await makeProject();
  try {
    assert.deepEqual(await runWorkflowHook({ event: 'Stop', cwd: projectDir }), { action: 'allow' });
    assert.deepEqual(await runWorkflowHook({ event: 'SubagentStop', cwd: projectDir }), { action: 'allow' });
  } finally {
    await rm(projectDir, { recursive: true, force: true });
  }
});
