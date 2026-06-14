import { strict as assert } from 'node:assert';
import test from 'node:test';

import type { BridgeResponse } from '../../bridge/bridge-client.js';
import type { BlueprintHelperToolContext } from '../types.js';
import { bridgeToolSource } from '../registry/bridge-tool-source.js';
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
          request_id: `req_${command}`,
          success: true,
          result: {
            ok: true,
            schema: 'BlueprintHelper.ToolResult.v1',
            operation: command,
            status: 'completed',
            modified: command === 'save_asset',
            target: { target_type: 'asset' },
            data: {
              command,
              payload,
            },
          },
        } satisfies BridgeResponse;
      },
    } as unknown as BlueprintHelperToolContext['bridge'],
    sleep: async () => {},
    calls,
  };
}

test('compile blueprint schema requires explicit target_blueprint', () => {
  const schema = bridgeToolSchemas['blueprint_compile_blueprint'];
  assert.ok(schema, 'blueprint_compile_blueprint schema must be registered');

  assert.deepEqual(schema.parse({
    target_blueprint: '/Game/BlueprintHelper/E2E/BP_SaveCompileSmoke.BP_SaveCompileSmoke',
  }), {
    target_blueprint: '/Game/BlueprintHelper/E2E/BP_SaveCompileSmoke.BP_SaveCompileSmoke',
  });
  assert.throws(() => schema.parse({}), /target_blueprint/u);
  assert.throws(() => schema.parse({
    asset_path: '/Game/BlueprintHelper/E2E/BP_SaveCompileSmoke.BP_SaveCompileSmoke',
  }), /target_blueprint|unrecognized/u);
  assert.throws(() => schema.parse({
    blueprint_path: '/Game/BlueprintHelper/E2E/BP_SaveCompileSmoke.BP_SaveCompileSmoke',
  }), /target_blueprint|unrecognized/u);
});

test('compile and save asset tools are available through the shared Bridge tool source', () => {
  assert.equal(bridgeToolSource.canHandle('blueprint_compile_blueprint'), true);
  assert.equal(bridgeToolSource.getInputSchema('blueprint_compile_blueprint'), bridgeToolSchemas['blueprint_compile_blueprint']);

  assert.equal(bridgeToolSource.canHandle('blueprint_save_asset'), true);
  assert.equal(bridgeToolSource.getInputSchema('blueprint_save_asset'), bridgeToolSchemas['blueprint_save_asset']);
});

test('save asset schema requires explicit asset path', () => {
  const schema = bridgeToolSchemas['blueprint_save_asset'];
  assert.ok(schema, 'blueprint_save_asset schema must be registered');

  assert.deepEqual(schema.parse({
    asset_path: '/Game/BlueprintHelper/E2E/BP_SaveCompileSmoke.BP_SaveCompileSmoke',
  }), {
    asset_path: '/Game/BlueprintHelper/E2E/BP_SaveCompileSmoke.BP_SaveCompileSmoke',
  });
  assert.throws(() => schema.parse({}));
});

test('compile blueprint dispatches to Bridge compile command', async () => {
  const context = makeContext();

  const result = await executeBridgeTool(
    'blueprint_compile_blueprint',
    {
      target_blueprint: '/Game/BlueprintHelper/E2E/BP_SaveCompileSmoke.BP_SaveCompileSmoke',
    },
    context,
  );

  assert.equal(result.ok, true);
  assert.equal(context.calls.length, 1);
  assert.equal(context.calls[0]?.command, 'compile_blueprint');
  assert.deepEqual(context.calls[0]?.payload, {
    target_blueprint: '/Game/BlueprintHelper/E2E/BP_SaveCompileSmoke.BP_SaveCompileSmoke',
  });
});

test('save asset dispatches to Bridge save command', async () => {
  const context = makeContext();

  const result = await executeBridgeTool(
    'blueprint_save_asset',
    {
      asset_path: '/Game/BlueprintHelper/E2E/BP_SaveCompileSmoke.BP_SaveCompileSmoke',
    },
    context,
  );

  assert.equal(result.ok, true);
  assert.equal(context.calls.length, 1);
  assert.equal(context.calls[0]?.command, 'save_asset');
  assert.deepEqual(context.calls[0]?.payload, {
    asset_path: '/Game/BlueprintHelper/E2E/BP_SaveCompileSmoke.BP_SaveCompileSmoke',
  });
});
