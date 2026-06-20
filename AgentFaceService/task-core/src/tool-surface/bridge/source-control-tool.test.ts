import { strict as assert } from 'node:assert';
import test from 'node:test';

import { BRIDGE_RESPONSE_SCHEMA } from '../../bridge/bridge-response-schema.js';
import type { BridgeResponse } from '../../bridge/bridge-client.js';
import type { BlueprintHelperToolContext } from '../types.js';
import { executeBridgeTool } from './bridge-tool-dispatcher.js';
import { bridgeToolSchemas } from './bridge-tool-schemas.js';

function makeContext(): BlueprintHelperToolContext & {
  calls: Array<{ command: string; payload: Record<string, unknown> }>;
} {
  const calls: Array<{ command: string; payload: Record<string, unknown> }> = [];
  return {
    cwd: 'D:/UEProjects/Template/Plugins/BlueprintHelper',
    taskRunner: {} as BlueprintHelperToolContext['taskRunner'],
    bridge: {
      sendCommand: async (command: string, payload: Record<string, unknown> = {}) => {
        calls.push({ command, payload });
        return {
          schema: BRIDGE_RESPONSE_SCHEMA,
          request_id: `req_${command}`,
          success: true,
          result: {
            ok: true,
            schema: 'BlueprintHelper.ToolResult.v1',
            operation: command,
            status: 'completed',
            modified: false,
            target: { target_type: 'asset' },
            data: {
              schema: 'BlueprintHelper.SourceControlResult.v1',
              provider: 'Perforce',
              enabled: true,
              available: true,
              files: [],
            },
          },
        } satisfies BridgeResponse;
      },
    } as unknown as BlueprintHelperToolContext['bridge'],
    sleep: async () => {},
    calls,
  };
}

test('source control checkout schema accepts asset list requests', () => {
  const schema = bridgeToolSchemas['blueprinthelper_source_control_checkout'];
  assert.ok(schema, 'blueprinthelper_source_control_checkout schema must be registered');

  const parsed = schema.parse({
    asset_paths: ['/Game/BlueprintHelper/E2E/BP_SignatureExtension'],
  }) as Record<string, unknown>;

  assert.deepEqual(parsed.asset_paths, ['/Game/BlueprintHelper/E2E/BP_SignatureExtension']);
});

test('source control checkout dispatches to Bridge command', async () => {
  const context = makeContext();

  const result = await executeBridgeTool(
    'blueprinthelper_source_control_checkout',
    {
      asset_paths: ['/Game/BlueprintHelper/E2E/BP_SignatureExtension'],
      update_status: true,
    },
    context,
  );

  assert.equal(result.ok, true);
  assert.equal(context.calls.length, 1);
  assert.equal(context.calls[0]?.command, 'source_control_checkout');
  assert.deepEqual(context.calls[0]?.payload, {
    asset_paths: ['/Game/BlueprintHelper/E2E/BP_SignatureExtension'],
    update_status: true,
  });
});
