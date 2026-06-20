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

async function writeProjectSetting(projectDir, value) {
  const settingPath = path.join(projectDir, '.blueprinthelper', 'setting.json');
  await writeFile(settingPath, JSON.stringify(value, null, 2), 'utf8');
  return settingPath;
}

const HASH_A = 'a'.repeat(64);
const HASH_B = 'b'.repeat(64);
const HASH_C = 'c'.repeat(64);
const RECEIPT_TIME = '2026-06-20T00:00:00.000Z';

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
    created_at: RECEIPT_TIME,
    updated_at: RECEIPT_TIME,
    ...value,
  };
}

function previewStdout(value = {}) {
  const previewToken = value.preview_token ?? 'tok';
  const receipt = makeReceipt({ status: 'previewed', ...value.receipt });
  return JSON.stringify({
    ok: true,
    status: 'preview_passed',
    preview_token: previewToken,
    receipt,
    tool_result: {
      data: {
        preview_token: previewToken,
        receipt,
      },
    },
  });
}

function executeStdout(value = {}) {
  const taskRunId = value.task_run_id ?? 'run-123';
  const receipt = makeReceipt({
    status: 'applied',
    task_run_id: taskRunId,
    ...value.receipt,
  });
  return JSON.stringify({
    ok: true,
    status: 'execute_succeeded',
    task_run_id: taskRunId,
    receipt,
    tool_result: {
      data: {
        task_run_id: taskRunId,
        receipt,
      },
    },
  });
}

function readbackStdout(value = {}) {
  const taskRunId = value.task_run_id ?? 'run-123';
  const receipt = makeReceipt({
    status: 'readback_verified',
    task_run_id: taskRunId,
    readback_ref: 'readback://run-123',
    ...value.receipt,
  });
  return JSON.stringify({
    ok: true,
    status: 'completed',
    task_run_id: taskRunId,
    receipt,
  });
}

test('classifyCommand recognizes BlueprintHelper task/read commands and ignores unrelated commands', () => {
  assert.deepEqual(classifyCommand('bh task preview --file task-spec.json'), {
    kind: 'preview',
    file: 'task-spec.json',
  });
  assert.deepEqual(classifyCommand('bh task execute --file task-spec.json --preview-token tok --receipt-id receipt-123'), {
    kind: 'execute',
    file: 'task-spec.json',
    previewToken: 'tok',
    receiptId: 'receipt-123',
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
        toolResult: { exit_code: 0, stdout: previewStdout() },
      });
      assert.equal(result.action, 'allow');
    }

    assert.equal(
      (
        await runWorkflowHook({
          event: 'PreToolUse',
          command: `bh task execute --file "${taskSpecPath}" --preview-token tok --receipt-id receipt-123`,
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
      toolResult: { exit_code: 0, stdout: previewStdout({ receipt: { receipt_id: 'receipt-exec' } }) },
    });
    const executeResult = await runWorkflowHook({
      event: 'PostToolUse',
      command: `bh task execute --file "${taskSpecWithBudget}" --preview-token tok --receipt-id receipt-exec`,
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: executeStdout({ receipt: { receipt_id: 'receipt-exec' } }) },
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

    const resultTaskRunMismatch = await runWorkflowHook({
      event: 'PostToolUse',
      command: 'bh task result --id run-123',
      cwd: projectDir,
      toolResult: {
        exit_code: 0,
        stdout: executeStdout({
          task_run_id: 'run-wrong',
          receipt: { receipt_id: 'receipt-exec', task_run_id: 'run-wrong' },
        }),
      },
    });
    assert.equal(resultTaskRunMismatch.action, 'block');
    assert.equal(resultTaskRunMismatch.reason, 'receipt_task_run_mismatch');

    await runWorkflowHook({
      event: 'PostToolUse',
      command: 'bh task result --id run-123',
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: executeStdout({ receipt: { receipt_id: 'receipt-exec' } }) },
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

test('PostToolUse applies task-worker max attempts from project setting to hook retry budget', async () => {
  const projectDir = await makeProject();
  try {
    await writeProjectSetting(projectDir, {
      agent: {
        task_worker: {
          max_attempts: 2,
        },
      },
    });
    const taskSpecPath = await writeTaskSpec(projectDir, { task_package_id: 'pkg-setting-budget' });

    for (let index = 0; index < 2; index += 1) {
      const result = await runWorkflowHook({
        event: 'PostToolUse',
        command: `bh task preview --file "${taskSpecPath}"`,
        cwd: projectDir,
        toolResult: { exit_code: 0, stdout: previewStdout({ receipt: { receipt_id: 'receipt-setting-budget' } }) },
      });
      assert.equal(result.action, 'allow');
    }

    const ledger = await readLedger({
      ledgerRoot: path.join(projectDir, 'Saved', 'BlueprintHelper', 'HookLedger'),
      taskPackageId: 'pkg-setting-budget',
    });
    assert.equal(ledger.retry_budget.max_attempts, 2);

    assert.equal(
      (
        await runWorkflowHook({
          event: 'PreToolUse',
          command: `bh task execute --file "${taskSpecPath}" --preview-token tok --receipt-id receipt-setting-budget`,
          cwd: projectDir,
        })
      ).reason,
      'retry_budget_exceeded',
    );
  } finally {
    await rm(projectDir, { recursive: true, force: true });
  }
});

test('PostToolUse resolves retry budget priority from package, user setting, project setting, then fallback', async () => {
  const projectDir = await makeProject();
  try {
    await writeProjectSetting(projectDir, {
      agent: {
        task_worker: {
          max_attempts: 2,
        },
      },
    });
    await mkdir(path.join(projectDir, 'Saved', 'BlueprintHelper'), { recursive: true });
    await writeFile(
      path.join(projectDir, 'Saved', 'BlueprintHelper', 'setting.user.json'),
      JSON.stringify({ agent: { task_worker: { max_attempts: 4 } } }, null, 2),
      'utf8',
    );

    const packageOverrideSpec = await writeTaskSpec(projectDir, {
      task_package_id: 'pkg-package-budget',
      retry_budget: { max_attempts: 5 },
    });
    await runWorkflowHook({
      event: 'PostToolUse',
      command: `bh task preview --file "${packageOverrideSpec}"`,
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: previewStdout({ receipt: { receipt_id: 'receipt-package-budget' } }) },
    });
    assert.equal(
      (
        await readLedger({
          ledgerRoot: path.join(projectDir, 'Saved', 'BlueprintHelper', 'HookLedger'),
          taskPackageId: 'pkg-package-budget',
        })
      ).retry_budget.max_attempts,
      5,
    );

    const userOverrideSpec = await writeTaskSpec(projectDir, { task_package_id: 'pkg-user-budget' });
    await runWorkflowHook({
      event: 'PostToolUse',
      command: `bh task preview --file "${userOverrideSpec}"`,
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: previewStdout({ receipt: { receipt_id: 'receipt-user-budget' } }) },
    });
    assert.equal(
      (
        await readLedger({
          ledgerRoot: path.join(projectDir, 'Saved', 'BlueprintHelper', 'HookLedger'),
          taskPackageId: 'pkg-user-budget',
        })
      ).retry_budget.max_attempts,
      4,
    );

    await writeFile(
      path.join(projectDir, 'Saved', 'BlueprintHelper', 'setting.user.json'),
      JSON.stringify({ agent: { task_worker: { max_attempts: 0 } } }, null, 2),
      'utf8',
    );
    await writeProjectSetting(projectDir, { agent: { task_worker: { max_attempts: 11 } } });
    const fallbackSpec = await writeTaskSpec(projectDir, { task_package_id: 'pkg-fallback-budget' });
    await runWorkflowHook({
      event: 'PostToolUse',
      command: `bh task preview --file "${fallbackSpec}"`,
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: previewStdout({ receipt: { receipt_id: 'receipt-fallback-budget' } }) },
    });
    assert.equal(
      (
        await readLedger({
          ledgerRoot: path.join(projectDir, 'Saved', 'BlueprintHelper', 'HookLedger'),
          taskPackageId: 'pkg-fallback-budget',
        })
      ).retry_budget.max_attempts,
      3,
    );
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
      toolResult: { exit_code: 0, stdout: previewStdout() },
    });
    await runWorkflowHook({
      event: 'PostToolUse',
      command: `bh task execute --file "${taskSpecPath}" --preview-token tok --receipt-id receipt-123`,
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: executeStdout() },
    });

    const stopBeforeReadback = await runWorkflowHook({ event: 'Stop', cwd: projectDir });
    assert.equal(stopBeforeReadback.action, 'block');
    assert.equal(stopBeforeReadback.reason, 'readback_missing_before_success');

    const subagentStopBeforeReadback = await runWorkflowHook({ event: 'SubagentStop', cwd: projectDir });
    assert.equal(subagentStopBeforeReadback.action, 'block');
    assert.equal(subagentStopBeforeReadback.reason, 'readback_missing_before_success');

    await runWorkflowHook({
      event: 'PostToolUse',
      command: 'bh context read --file read-spec.json --receipt-id receipt-123 --task-run-id run-123',
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: readbackStdout() },
      metadata: { task_package_id: 'pkg-123', receipt_id: 'receipt-123', task_run_id: 'run-123' },
    });

    const stopAfterReadback = await runWorkflowHook({ event: 'Stop', cwd: projectDir });
    assert.equal(stopAfterReadback.action, 'allow');
  } finally {
    await rm(projectDir, { recursive: true, force: true });
  }
});

test('Hook ledger records preview receipt identity and writes no temp leftovers', async () => {
  const projectDir = await makeProject();
  try {
    const taskSpecPath = await writeTaskSpec(projectDir, { task_package_id: 'pkg-receipt-preview' });
    await runWorkflowHook({
      event: 'PostToolUse',
      command: `bh task preview --file "${taskSpecPath}"`,
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: previewStdout({ receipt: { receipt_id: 'receipt-preview-ledger' } }) },
    });

    const ledgerRoot = path.join(projectDir, 'Saved', 'BlueprintHelper', 'HookLedger');
    const ledger = await readLedger({ ledgerRoot, taskPackageId: 'pkg-receipt-preview' });
    assert.equal(ledger.receipt.receipt_id, 'receipt-preview-ledger');
    assert.equal(ledger.last_preview.receipt_id, 'receipt-preview-ledger');
    assert.equal(ledger.last_preview.task_spec_hash, HASH_A);

    const entries = await readFile(path.join(ledgerRoot, 'pkg-receipt-preview.json'), 'utf8');
    assert.match(entries, /"receipt_id": "receipt-preview-ledger"/);
  } finally {
    await rm(projectDir, { recursive: true, force: true });
  }
});

test('Hook blocks execute when command or result receipt mismatches preview receipt', async () => {
  const projectDir = await makeProject();
  try {
    const taskSpecPath = await writeTaskSpec(projectDir, { task_package_id: 'pkg-receipt-mismatch' });
    await runWorkflowHook({
      event: 'PostToolUse',
      command: `bh task preview --file "${taskSpecPath}"`,
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: previewStdout({ receipt: { receipt_id: 'receipt-preview-match' } }) },
    });

    const preResult = await runWorkflowHook({
      event: 'PreToolUse',
      command: `bh task execute --file "${taskSpecPath}" --preview-token tok --receipt-id receipt-wrong`,
      cwd: projectDir,
    });
    assert.equal(preResult.action, 'block');
    assert.equal(preResult.reason, 'receipt_hash_mismatch');

    const postResult = await runWorkflowHook({
      event: 'PostToolUse',
      command: `bh task execute --file "${taskSpecPath}" --preview-token tok --receipt-id receipt-preview-match`,
      cwd: projectDir,
      toolResult: {
        exit_code: 0,
        stdout: executeStdout({
          receipt: {
            receipt_id: 'receipt-preview-match',
            task_spec_hash: 'd'.repeat(64),
          },
        }),
      },
    });
    assert.equal(postResult.action, 'block');
    assert.equal(postResult.reason, 'receipt_hash_mismatch');
  } finally {
    await rm(projectDir, { recursive: true, force: true });
  }
});

test('Hook blocks missing status and missing receipt instead of defaulting to success', async () => {
  const projectDir = await makeProject();
  try {
    const taskSpecPath = await writeTaskSpec(projectDir, { task_package_id: 'pkg-fail-closed' });
    const missingStatus = await runWorkflowHook({
      event: 'PostToolUse',
      command: `bh task preview --file "${taskSpecPath}"`,
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: JSON.stringify({ ok: true, receipt: makeReceipt() }) },
    });
    assert.equal(missingStatus.action, 'block');
    assert.equal(missingStatus.reason, 'tool_status_missing');

    const missingReceipt = await runWorkflowHook({
      event: 'PostToolUse',
      command: `bh task preview --file "${taskSpecPath}"`,
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: '{"ok":true,"status":"preview_passed","preview_token":"tok"}' },
    });
    assert.equal(missingReceipt.action, 'block');
    assert.equal(missingReceipt.reason, 'receipt_missing');
  } finally {
    await rm(projectDir, { recursive: true, force: true });
  }
});

test('Hook blocks readback without matching receipt identity and does not guess latest active ledger', async () => {
  const projectDir = await makeProject();
  try {
    const taskSpecPath = await writeTaskSpec(projectDir, { task_package_id: 'pkg-readback-identity' });
    await runWorkflowHook({
      event: 'PostToolUse',
      command: `bh task preview --file "${taskSpecPath}"`,
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: previewStdout({ receipt: { receipt_id: 'receipt-readback' } }) },
    });
    await runWorkflowHook({
      event: 'PostToolUse',
      command: `bh task execute --file "${taskSpecPath}" --preview-token tok --receipt-id receipt-readback`,
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: executeStdout({ receipt: { receipt_id: 'receipt-readback' } }) },
    });

    const missingIdentity = await runWorkflowHook({
      event: 'PostToolUse',
      command: 'bh context read --file read-spec.json',
      cwd: projectDir,
      toolResult: { exit_code: 0, stdout: '{"ok":true,"status":"completed"}' },
    });
    assert.equal(missingIdentity.action, 'block');
    assert.equal(missingIdentity.reason, 'receipt_missing');

    const mismatch = await runWorkflowHook({
      event: 'PostToolUse',
      command: 'bh context read --file read-spec.json --receipt-id receipt-readback --task-run-id run-wrong',
      cwd: projectDir,
      metadata: { task_package_id: 'pkg-readback-identity', receipt_id: 'receipt-readback', task_run_id: 'run-wrong' },
      toolResult: {
        exit_code: 0,
        stdout: readbackStdout({
          task_run_id: 'run-wrong',
          receipt: { receipt_id: 'receipt-readback', task_run_id: 'run-wrong' },
        }),
      },
    });
    assert.equal(mismatch.action, 'block');
    assert.equal(mismatch.reason, 'receipt_readback_mismatch');

    const hashMismatch = await runWorkflowHook({
      event: 'PostToolUse',
      command: 'bh context read --file read-spec.json --receipt-id receipt-readback --task-run-id run-123',
      cwd: projectDir,
      metadata: { task_package_id: 'pkg-readback-identity', receipt_id: 'receipt-readback', task_run_id: 'run-123' },
      toolResult: {
        exit_code: 0,
        stdout: readbackStdout({
          receipt: {
            receipt_id: 'receipt-readback',
            task_run_id: 'run-123',
            task_spec_hash: 'd'.repeat(64),
          },
        }),
      },
    });
    assert.equal(hashMismatch.action, 'block');
    assert.equal(hashMismatch.reason, 'receipt_readback_mismatch');
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
