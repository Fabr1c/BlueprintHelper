import assert from 'node:assert/strict';
import test from 'node:test';
import { z } from 'zod';

import { ToolExecutorRegistry } from './tool-executor-registry.js';

test('ToolExecutorRegistry resolves schema and executor by tool name', async () => {
  const registry = new ToolExecutorRegistry()
    .register({
      id: 'test',
      canHandle: (name) => name === 'test_tool',
      getInputSchema: () => z.object({ value: z.string() }),
      execute: async (_name, input) => ({
        ok: true,
        schema: 'BlueprintHelper.ToolResult.v1',
        operation: 'test_tool',
        trace_id: 'trace-p2-tool-executor',
        status: 'completed',
        modified: false,
        data: { value: input.value },
      }),
    });

  assert.equal(registry.canHandle('test_tool'), true);
  assert.equal(registry.resolveInputSchema('test_tool').parse({ value: 'x' }).value, 'x');

  const result = await registry.execute('test_tool', { value: 'x' }, {} as never);
  assert.equal(result.ok, true);
  assert.deepEqual(result.data, { value: 'x' });
});

test('ToolExecutorRegistry reports unregistered tools with existing message shape', () => {
  const registry = new ToolExecutorRegistry();

  assert.equal(registry.canHandle('missing_tool'), false);
  assert.throws(
    () => registry.requireSource('missing_tool'),
    /Tool is registered without a handler: missing_tool/,
  );
});
