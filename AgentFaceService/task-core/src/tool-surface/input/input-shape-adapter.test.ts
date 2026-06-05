import assert from 'node:assert/strict';
import test from 'node:test';
import { z } from 'zod';

import {
  InputShapeAdapterError,
  InputShapeAdapterRegistry,
  adaptToolInput,
} from './input-shape-adapter.js';
import {
  createTaskSpecInputShapeAdapterRegistry,
} from './taskspec-input-adapters.js';

const bareTaskSpec = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  task_type: 'edit_blueprint_variables',
  feature_name: 'P2_InputAdapter_Bare',
  context_id: 'ctx-p2-input-adapter',
  target: { asset_path: '/Game/BP_P2_InputAdapter', target_type: 'blueprint' },
  execution_policy: { dry_run_mode: 'quick' },
  validation: { should_compile: false, should_save: false },
  behavior: {
    variable_strategy: 'member_variables',
    changes: [{
      kind: 'ensure_member_variable',
      name: 'P2Value',
      variable_type: { category: 'bool' },
    }],
  },
};

test('bare_taskspec adapter wraps a public TaskSpec into handler input', () => {
  const registry = createTaskSpecInputShapeAdapterRegistry();
  const adapted = registry.require('bare_taskspec').adapt(bareTaskSpec);

  assert.deepEqual(adapted, { task_spec: bareTaskSpec });
});

test('wrapped_taskspec_preview adapter preserves wrapped preview input', () => {
  const registry = createTaskSpecInputShapeAdapterRegistry();
  const adapted = registry.require('wrapped_taskspec_preview').adapt({ task_spec: bareTaskSpec });

  assert.deepEqual(adapted, { task_spec: bareTaskSpec, develop: false });
});

test('wrapped_taskspec_execute adapter preserves preview_token only on wrapped input', () => {
  const registry = createTaskSpecInputShapeAdapterRegistry();
  const adapted = registry.require('wrapped_taskspec_execute').adapt({
    task_spec: bareTaskSpec,
    preview_token: '0123456789abcdef0123456789abcdef',
  });

  assert.deepEqual(adapted, {
    task_spec: bareTaskSpec,
    develop: false,
    preview_token: '0123456789abcdef0123456789abcdef',
  });
});

test('bare execute adapter rejects preview_token without wrapper', () => {
  const registry = createTaskSpecInputShapeAdapterRegistry();

  let error: unknown;
  try {
    registry.require('bare_taskspec').adapt({
      ...bareTaskSpec,
      preview_token: 'invalid-token-placement',
    });
  } catch (caught) {
    error = caught;
  }

  assert.equal(error instanceof InputShapeAdapterError, true);
  assert.equal((error as InputShapeAdapterError | undefined)?.code, 'preview_token_requires_task_spec_wrapper');
});

test('adaptToolInput tries the next shape after a schema mismatch', () => {
  const registry = new InputShapeAdapterRegistry()
    .register({
      id: 'wrapped_taskspec_preview',
      inputSchema: z.object({ task_spec: z.object({}) }),
      adapt(input) {
        return z.object({ task_spec: z.object({}) }).parse(input);
      },
    })
    .register({
      id: 'empty_object',
      inputSchema: z.object({ value: z.string() }),
      adapt(input) {
        return z.object({ value: z.string() }).parse(input);
      },
    });

  assert.deepEqual(
    adaptToolInput(registry, ['wrapped_taskspec_preview', 'empty_object'], { value: 'ok' }),
    { value: 'ok' },
  );
});

test('adaptToolInput rethrows unexpected adapter defects instead of trying later shapes', () => {
  const registry = new InputShapeAdapterRegistry()
    .register({
      id: 'bare_taskspec',
      inputSchema: z.unknown(),
      adapt() {
        throw new Error('unexpected adapter failure');
      },
    })
    .register({
      id: 'empty_object',
      inputSchema: z.object({}),
      adapt(input) {
        return z.object({}).parse(input);
      },
    });

  assert.throws(
    () => adaptToolInput(registry, ['bare_taskspec', 'empty_object'], {}),
    /unexpected adapter failure/,
  );
});
