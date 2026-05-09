import assert from 'node:assert/strict';
import test from 'node:test';
import { invokeTool, registerWithBridge } from './test-harness.js';

test('blueprinthelper_get_debug_case returns summary only', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> | undefined }> = [];
  const tools = registerWithBridge(async (command, payload) => {
    calls.push({ command, payload });
    return {
      request_id: 'test',
      success: true,
      result: {
        ok: true,
        schema: 'BlueprintHelper.McpToolResult.v1',
        operation: 'get_debug_case',
        trace_id: 'trace_debug_case',
        status: 'completed',
        modified: false,
        data: {
          schema: 'DebugCaseSummary.v1',
          debug_case: {
            debug_case_id: 'dbg_1',
            severity: 'error',
            status: 'needs_action',
            event_count: 1,
            last_error_code: 'bridge_transport_failure',
            review_record_ids: ['review_1'],
          },
        },
      },
    };
  });

  const tool = tools.get('blueprinthelper_get_debug_case');
  assert.ok(tool);

  const result = await invokeTool(tool, { debug_case_id: 'dbg_1' });

  assert.deepEqual(calls, [
    { command: 'get_debug_case', payload: { debug_case_id: 'dbg_1' } },
  ]);
  assert.equal(result.isError, false);
  assert.equal(result.structuredContent?.operation, 'get_debug_case');
  assert.deepEqual(
    (result.structuredContent?.data as Record<string, unknown>)?.debug_case,
    {
      debug_case_id: 'dbg_1',
      severity: 'error',
      status: 'needs_action',
      event_count: 1,
      last_error_code: 'bridge_transport_failure',
      review_record_ids: ['review_1'],
    },
  );
  assert.equal(JSON.stringify(result.structuredContent).includes('artifact'), false);
  assert.equal(JSON.stringify(result.structuredContent).includes('bundle_path'), false);
  assert.equal(JSON.stringify(result.structuredContent).includes('raw_json'), false);
});

test('MCP does not expose DebugBundle artifact or large-payload debug readers', () => {
  const tools = registerWithBridge(async () => ({ request_id: 'test', success: true }));
  const forbidden = /debug_bundle|bundle_artifact|artifact_reader|large_payload_debug|raw_debug_payload/i;

  for (const name of tools.keys()) {
    assert.doesNotMatch(name, forbidden);
  }
});
