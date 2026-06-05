import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { executeTask, previewTask } from './task-execution-handlers.js';

const normalizedTaskSpec = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  task_type: 'edit_blueprint_variables',
  feature_name: 'P2_Handler_Normalized',
  context_id: 'ctx-p2-handler',
  target: { asset_path: '/Game/BP_P2_Handler', target_type: 'blueprint' },
  execution_policy: { dry_run_mode: 'quick' },
  validation: { should_compile: false, should_save: false },
  behavior: {
    variable_strategy: 'member_variables',
    changes: [{
      kind: 'ensure_member_variable',
      name: 'HandlerValue',
      variable_type: { category: 'bool' },
    }],
  },
};

function makeContext() {
  const calls: Array<{ method: string; taskSpec: unknown; previewToken?: string }> = [];
  return {
    calls,
    context: {
      taskRunner: {
        async previewTask(taskSpec: unknown) {
          calls.push({ method: 'previewTask', taskSpec });
          return { toolResult: { ok: true, operation: 'preview_task' } };
        },
        async executeTask(taskSpec: unknown, _timing: unknown, options: { previewToken?: string }) {
          calls.push({ method: 'executeTask', taskSpec, previewToken: options.previewToken });
          return { ok: true, operation: 'execute_task' };
        },
      },
    },
  };
}

test('task execution handlers consume already normalized preview input', async () => {
  const { calls, context } = makeContext();

  await previewTask({ task_spec: normalizedTaskSpec }, context as never);

  assert.equal(calls.length, 1);
  assert.equal(calls[0]?.method, 'previewTask');
  assert.deepEqual(calls[0]?.taskSpec, normalizedTaskSpec);
});

test('task execution handlers consume already normalized execute input', async () => {
  const { calls, context } = makeContext();

  await executeTask({
    task_spec: normalizedTaskSpec,
    preview_token: '0123456789abcdef0123456789abcdef',
  }, context as never);

  assert.equal(calls.length, 1);
  assert.equal(calls[0]?.method, 'executeTask');
  assert.deepEqual(calls[0]?.taskSpec, normalizedTaskSpec);
  assert.equal(calls[0]?.previewToken, '0123456789abcdef0123456789abcdef');
});

test('task execution handlers no longer branch on wrapper or bare input shape', () => {
  const source = fs.readFileSync(
    sourcePath('task-execution-handlers.ts'),
    'utf8',
  );

  assert.doesNotMatch(source, /'task_spec'\s+in\s+input/);
  assert.doesNotMatch(source, /TaskSpecSchema\.parse\(input\)/);
});

function sourcePath(fileName: string): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', '..', 'src', 'tool-surface', 'task', fileName);
}
