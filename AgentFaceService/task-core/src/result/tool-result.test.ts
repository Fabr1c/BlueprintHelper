import test from 'node:test';
import assert from 'node:assert/strict';

import {
  sanitizeAgentFacingToolResult,
  type ToolResultBase,
} from './tool-result.js';

function makeToolResultWithHiddenDebug(): ToolResultBase {
  const result = {
    ok: true,
    schema: 'BlueprintHelper.ToolResult.v1',
    operation: 'read_context',
    trace_id: 'trace_hidden_debug',
    status: 'completed',
    modified: false,
    data: {
      schema: 'ReadContextPack.v1',
      payload: {
        schema: 'LogicFlow.v1',
      },
    },
  } as ToolResultBase;

  Object.defineProperty(result, 'debug', {
    value: {
      logic_flow: {
        anchors: [{ fingerprint: 'anchorfp' }],
      },
    },
    enumerable: false,
    configurable: true,
  });

  return result;
}

test('sanitizeAgentFacingToolResult drops hidden debug by default', () => {
  const sanitized = sanitizeAgentFacingToolResult(makeToolResultWithHiddenDebug());

  assert.equal((sanitized as ToolResultBase & { debug?: unknown }).debug, undefined);
  assert.equal(Object.hasOwn(sanitized, 'debug'), false);
});

test('sanitizeAgentFacingToolResult preserves hidden debug for expert artifacts', () => {
  const sanitized = sanitizeAgentFacingToolResult(makeToolResultWithHiddenDebug(), {
    preserveDebug: true,
  }) as ToolResultBase & { debug?: Record<string, unknown> };

  assert.equal(Object.prototype.propertyIsEnumerable.call(sanitized, 'debug'), false);
  const logicFlow = sanitized.debug?.['logic_flow'] as Record<string, unknown>;
  const anchors = logicFlow['anchors'] as Record<string, unknown>[];
  assert.equal(anchors[0]?.['fingerprint'], 'anchorfp');
});
