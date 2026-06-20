import assert from 'node:assert/strict';
import test from 'node:test';

import { BRIDGE_RESPONSE_SCHEMA } from '../../bridge/bridge-response-schema.js';
import type { BridgeResponse } from '../../bridge/bridge-client.js';
import type { TaskRunnerBridge } from './task-spec-runner.js';
import { runSourceControlWriteGate } from './source-control-write-gate.js';

function response(result: Record<string, unknown>): BridgeResponse {
  return { schema: BRIDGE_RESPONSE_SCHEMA, request_id: 'test', success: true, result };
}

function makeBridge(result: Record<string, unknown>): {
  bridge: TaskRunnerBridge;
  calls: Array<{ command: string; payload: unknown }>;
} {
  const calls: Array<{ command: string; payload: unknown }> = [];
  const bridge: TaskRunnerBridge = {
    async sendCommand(command, payload) {
      calls.push({ command, payload });
      return response(result);
    },
  };
  return { bridge, calls };
}

test('source-control write gate fails closed when target assets are missing', async () => {
  const { bridge, calls } = makeBridge({});

  const result = await runSourceControlWriteGate(bridge, [], { autoCheckout: false });

  assert.equal(result.ok, false);
  assert.equal(result.code, 'source_control_target_missing');
  assert.equal(calls.length, 0);
});

test('source-control write gate passes when all assets are editable', async () => {
  const { bridge, calls } = makeBridge({
    source_control: {
      status: 'editable',
      files: [{ asset_path: '/Game/BP.BP', status: 'editable', editable: true }],
    },
  });

  const result = await runSourceControlWriteGate(bridge, ['/Game/BP.BP'], { autoCheckout: false });

  assert.equal(result.ok, true);
  assert.equal(calls[0]?.command, 'source_control_status');
});

test('source-control write gate blocks checkout_required when autoCheckout is false', async () => {
  const { bridge } = makeBridge({
    source_control: {
      status: 'checkout_required',
      files: [{ asset_path: '/Game/BP.BP', status: 'checkout_required', editable: false }],
    },
  });

  const result = await runSourceControlWriteGate(bridge, ['/Game/BP.BP'], { autoCheckout: false });

  assert.equal(result.ok, false);
  assert.equal(result.code, 'checkout_required');
  assert.match(result.message, /blueprinthelper_source_control_checkout/u);
});

test('source-control write gate preserves source_control_status bridge failures', async () => {
  const bridge: TaskRunnerBridge = {
    async sendCommand() {
      return {
        schema: BRIDGE_RESPONSE_SCHEMA,
        request_id: 'status_failed',
        success: false,
        error_code: 'source_control_unavailable',
        message: 'Source control provider is unavailable.',
      };
    },
  };

  const result = await runSourceControlWriteGate(bridge, ['/Game/BP.BP'], { autoCheckout: false });

  assert.equal(result.ok, false);
  assert.equal(result.code, 'source_control_unavailable');
  assert.equal(result.message, 'Source control provider is unavailable.');
});

test('source-control write gate auto-checks out checkout_required assets when policy allows it', async () => {
  const calls: Array<{ command: string; payload: unknown }> = [];
  const bridge: TaskRunnerBridge = {
    async sendCommand(command, payload) {
      calls.push({ command, payload });
      if (command === 'source_control_status') {
        return response({
          source_control: {
            status: 'checkout_required',
            files: [{ asset_path: '/Game/BP.BP', status: 'checkout_required', editable: false }],
          },
        });
      }
      return response({
        source_control: {
          status: 'editable',
          files: [{ asset_path: '/Game/BP.BP', status: 'editable', editable: true }],
        },
      });
    },
  };

  const result = await runSourceControlWriteGate(bridge, ['/Game/BP.BP'], { autoCheckout: true });

  assert.equal(result.ok, true);
  assert.deepEqual(calls.map((call) => call.command), ['source_control_status', 'source_control_checkout']);
});

test('source-control write gate preserves source_control_checkout bridge failures', async () => {
  const bridge: TaskRunnerBridge = {
    async sendCommand(command) {
      if (command === 'source_control_status') {
        return response({
          source_control: {
            status: 'checkout_required',
            files: [{ asset_path: '/Game/BP.BP', status: 'checkout_required', editable: false }],
          },
        });
      }
      return {
        schema: BRIDGE_RESPONSE_SCHEMA,
        request_id: 'checkout_failed',
        success: false,
        error_code: 'checkout_failed',
        message: 'Checkout failed for /Game/BP.BP.',
      };
    },
  };

  const result = await runSourceControlWriteGate(bridge, ['/Game/BP.BP'], { autoCheckout: true });

  assert.equal(result.ok, false);
  assert.equal(result.code, 'checkout_failed');
  assert.equal(result.message, 'Checkout failed for /Game/BP.BP.');
});

test('source-control write gate blocks checked_out_by_other without checkout attempt', async () => {
  const { bridge, calls } = makeBridge({
    source_control: {
      status: 'checked_out_by_other',
      files: [{ asset_path: '/Game/BP.BP', status: 'checked_out_by_other', editable: false }],
    },
  });

  const result = await runSourceControlWriteGate(bridge, ['/Game/BP.BP'], { autoCheckout: true });

  assert.equal(result.ok, false);
  assert.equal(result.code, 'checked_out_by_other');
  assert.deepEqual(calls.map((call) => call.command), ['source_control_status']);
});
