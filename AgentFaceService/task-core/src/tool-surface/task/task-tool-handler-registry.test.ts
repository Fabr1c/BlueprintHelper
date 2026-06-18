import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import {
  createDefaultTaskToolHandlerRegistry,
} from './task-tool-handler-registry.js';

const bareTaskSpec = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  task_type: 'edit_blueprint_variables',
  feature_name: 'P2_Handler_Registry',
  context_id: 'ctx-p2-handler-registry',
  target: { asset_path: '/Game/BP_P2_Handler_Registry', target_type: 'blueprint' },
  behavior: {
    variable_strategy: 'member_variables',
    changes: [{
      kind: 'ensure_member_variable',
      name: 'RegistryValue',
      variable_type: { category: 'bool' },
    }],
  },
};

test('default task tool registry exposes all current public task tools', () => {
  const registry = createDefaultTaskToolHandlerRegistry();

  assert.equal(registry.has('blueprinthelper_read_reference_context'), true);
  assert.equal(registry.has('blueprinthelper_preview_task'), true);
  assert.equal(registry.has('blueprinthelper_execute_task'), true);
  assert.equal(registry.has('blueprinthelper_get_task_result'), true);
});

test('task tool descriptors carry input shape ids and schemas', () => {
  const registry = createDefaultTaskToolHandlerRegistry();
  const readReference = registry.require('blueprinthelper_read_reference_context');
  const preview = registry.require('blueprinthelper_preview_task');
  const execute = registry.require('blueprinthelper_execute_task');

  assert.deepEqual(readReference.inputShapeIds, ['read_reference_context']);
  assert.deepEqual(preview.inputShapeIds, ['wrapped_taskspec_preview', 'bare_taskspec']);
  assert.deepEqual(execute.inputShapeIds, ['wrapped_taskspec_execute', 'bare_taskspec']);
  assert.equal(typeof readReference.inputSchema.parse, 'function');
  assert.equal(typeof preview.inputSchema.parse, 'function');
  assert.equal(typeof execute.inputSchema.parse, 'function');
});

test('read reference handler receives adapter-normalized input', async () => {
  const registry = createDefaultTaskToolHandlerRegistry();
  const readReference = registry.require('blueprinthelper_read_reference_context');
  let capturedInput: Record<string, unknown> | undefined;

  await readReference.execute({ asset_path: '/Game/BP_ReadRef_Handler' }, {
    taskRunner: {
      readReferenceContext: async (input: Record<string, unknown>) => {
        capturedInput = input;
        return {
          ok: true,
          schema: 'BlueprintHelper.ToolResult.v1',
          operation: 'read_reference_context',
          trace_id: 'trace_read_ref_handler',
          status: 'completed',
          modified: false,
        };
      },
    },
  } as never);

  assert.equal(capturedInput?.asset_path, '/Game/BP_ReadRef_Handler');
  assert.equal(capturedInput?.target_type, 'asset');
  assert.equal(capturedInput?.resolution_policy, 'ue_then_name');
});

test('execute input shape failures preserve develop timing diagnostics', async () => {
  const registry = createDefaultTaskToolHandlerRegistry();
  const execute = registry.require('blueprinthelper_execute_task');
  const result = await execute.execute({
    ...bareTaskSpec,
    develop: true,
    preview_token: 'invalid-token-placement',
  }, {} as never);

  assert.equal(result.ok, false);
  assert.equal(result.error?.code, 'preview_token_requires_task_spec_wrapper');
  assert.equal((result.data?.['timing'] as Record<string, unknown> | undefined)?.['operation'], 'execute_task');
});

test('dispatcher facade no longer contains public task tool switch cases', () => {
  const source = fs.readFileSync(
    sourcePath('task-tool-dispatcher.ts'),
    'utf8',
  );

  assert.doesNotMatch(source, /switch\s*\(name\)/);
  assert.doesNotMatch(source, /case 'blueprinthelper_preview_task'/);
  assert.doesNotMatch(source, /case 'blueprinthelper_execute_task'/);
});

function sourcePath(fileName: string): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', '..', 'src', 'tool-surface', 'task', fileName);
}
