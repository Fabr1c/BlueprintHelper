import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import type { BridgeClient } from '../../bridge/bridge-client.js';
import { buildTaskContextPack } from '../../task/context/task-context.js';

function makeBridge(getAssetInfoResult: Record<string, unknown>): BridgeClient {
  return {
    async sendCommand(command: string) {
      if (command === 'get_runtime_profile') {
        return {
          request_id: 'test_runtime',
          success: true,
          result: { status: 'ok' },
        };
      }
      if (command === 'get_asset_info') {
        return {
          request_id: 'test_asset',
          success: true,
          result: getAssetInfoResult,
        };
      }
      if (command === 'list_graphs') {
        return {
          request_id: 'test_graphs',
          success: true,
          result: { data: { graphs: [] } },
        };
      }
      return {
        request_id: 'test_unknown',
        success: false,
        error_code: 'unknown_command',
        message: command,
      };
    },
  } as unknown as BridgeClient;
}

describe('buildTaskContextPack asset existence', () => {
  it('does not treat an empty get_asset_info result as an existing asset', async () => {
    const pack = await buildTaskContextPack(makeBridge({}), {
      target: {
        target_type: 'blueprint',
        asset_path: '/Game/Missing/BP_Missing',
      },
    });

    assert.equal(pack.target.exists, false);
    assert.equal('asset_info' in pack.target, true);
    assert.equal(pack.target.asset_info, undefined);
  });

  it('keeps valid asset info for an existing asset', async () => {
    const pack = await buildTaskContextPack(makeBridge({
      path: '/Game/BP_Door.BP_Door',
      name: 'BP_Door',
      class: 'Blueprint',
    }), {
      target: {
        target_type: 'blueprint',
        asset_path: '/Game/BP_Door',
      },
    });

    assert.equal(pack.target.exists, true);
    assert.deepEqual(pack.target.asset_info, {
      path: '/Game/BP_Door.BP_Door',
      name: 'BP_Door',
      class: 'Blueprint',
    });
  });
});
