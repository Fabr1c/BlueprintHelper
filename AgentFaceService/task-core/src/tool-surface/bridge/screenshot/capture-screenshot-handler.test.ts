import { strict as assert } from 'node:assert';
import test from 'node:test';

import { BRIDGE_RESPONSE_SCHEMA } from '../../../bridge/bridge-response-schema.js';
import type { BridgeResponse } from '../../../bridge/bridge-client.js';
import type { BlueprintHelperToolContext } from '../../types.js';
import { executeCaptureScreenshot } from './capture-screenshot-handler.js';

function response(command: string, result: Record<string, unknown> = {}): BridgeResponse {
  return {
    schema: BRIDGE_RESPONSE_SCHEMA,
    request_id: `req_${command}`,
    success: true,
    result,
  };
}

function makeContext(): BlueprintHelperToolContext & {
  calls: Array<{ command: string; payload: Record<string, unknown> }>;
  sleeps: number[];
} {
  const calls: Array<{ command: string; payload: Record<string, unknown> }> = [];
  const sleeps: number[] = [];
  return {
    cwd: 'D:/UEProjects/Template/Plugins/BlueprintHelper',
    taskRunner: {} as BlueprintHelperToolContext['taskRunner'],
    bridge: {
      sendCommand: async (command: string, payload: Record<string, unknown> = {}) => {
        calls.push({ command, payload });
        if (command === 'capture_editor_screenshot') {
          return response(command, {
            schema: 'BlueprintHelper.ScreenshotCapture.v1',
            screenshot_path: 'D:/UEProjects/Template/Saved/BlueprintHelper/Debug/Screenshots/test.png',
            relative_path: 'Screenshots/test.png',
            width: 1600,
            height: 900,
          });
        }
        if (command === 'capture_focused_graph_screenshot') {
          return response(command, {
            schema: 'BlueprintHelper.GraphScreenshotResult.v1',
            ok: true,
            target: 'graph_panel',
            screenshot_count: 2,
            screenshots: [
              {
                schema: 'BlueprintHelper.EditorScreenshotResult.v1',
                screenshot_path: 'D:/UEProjects/Template/Saved/BlueprintHelper/Debug/Screenshots/graph_001.png',
                relative_path: 'Screenshots/graph_001.png',
                width: 1280,
                height: 720,
                tile_index: 0,
                tile_count: 2,
              },
              {
                schema: 'BlueprintHelper.EditorScreenshotResult.v1',
                screenshot_path: 'D:/UEProjects/Template/Saved/BlueprintHelper/Debug/Screenshots/graph_002.png',
                relative_path: 'Screenshots/graph_002.png',
                width: 1280,
                height: 720,
                tile_index: 1,
                tile_count: 2,
              },
            ],
          });
        }
        return response(command, { status: 'completed' });
      },
    } as BlueprintHelperToolContext['bridge'],
    sleep: async (ms: number) => {
      sleeps.push(ms);
    },
    calls,
    sleeps,
  };
}

test('capture screenshot opens asset, focuses readable blueprint target, waits, then captures graph-only screenshots', async () => {
  const context = makeContext();

  const result = await executeCaptureScreenshot({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    graph_name: 'EventGraph',
    block_ref: 'BeginPlaySetup',
    node_ref: 'nodes[0]',
    label: 'bp_player_eventgraph',
    settle_delay_ms: 125,
  }, context);

  assert.equal(result.ok, true);
  assert.deepEqual(context.calls.map((call) => call.command), [
    'open_asset',
    'focus_blueprint_editor_target',
    'capture_focused_graph_screenshot',
  ]);
  assert.deepEqual(context.calls[0]?.payload, {
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
  });
  assert.deepEqual(context.calls[1]?.payload, {
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    graph_name: 'EventGraph',
    block_ref: 'BeginPlaySetup',
    node_ref: 'nodes[0]',
  });
  assert.deepEqual(context.calls[2]?.payload, {
    label: 'bp_player_eventgraph',
  });
  assert.deepEqual(context.sleeps, [125]);
  assert.equal(result.data?.['schema'], 'BlueprintHelper.ScreenshotEvidence.v1');
  assert.equal(result.data?.['capture_scope'], 'graph');
  assert.equal(result.data?.['capture_command'], 'capture_focused_graph_screenshot');
  assert.equal(result.data?.['screenshot_count'], 2);
  const screenshots = result.data?.['screenshots'];
  assert.equal(Array.isArray(screenshots), true);
  assert.equal((screenshots as unknown[]).length, 2);
});

test('capture screenshot skips focus command for asset-only request', async () => {
  const context = makeContext();

  const result = await executeCaptureScreenshot({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    settle_delay_ms: 0,
  }, context);

  assert.equal(result.ok, true);
  assert.deepEqual(context.calls.map((call) => call.command), [
    'open_asset',
    'capture_editor_screenshot',
  ]);
  assert.deepEqual(context.calls[1]?.payload, {});
  assert.deepEqual(context.sleeps, []);
  assert.equal(result.data?.['capture_scope'], 'asset_window');
});

test('capture screenshot stops on open_asset failure', async () => {
  const calls: Array<{ command: string; payload: Record<string, unknown> }> = [];
  const context = {
    cwd: 'D:/UEProjects/Template/Plugins/BlueprintHelper',
    taskRunner: {} as BlueprintHelperToolContext['taskRunner'],
    bridge: {
      sendCommand: async (command: string, payload: Record<string, unknown> = {}) => {
        calls.push({ command, payload });
        return {
          schema: BRIDGE_RESPONSE_SCHEMA,
          request_id: `req_${command}`,
          success: false,
          error_code: 'asset_not_found',
          message: 'Asset was not found.',
        } satisfies BridgeResponse;
      },
    } as BlueprintHelperToolContext['bridge'],
  } satisfies BlueprintHelperToolContext;

  const result = await executeCaptureScreenshot({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Missing.Missing',
  }, context);

  assert.equal(result.ok, false);
  assert.equal(result.error?.code, 'asset_not_found');
  assert.equal(result.error?.stage, 'bridge');
  assert.equal(result.data?.['failed_step'], 'open_asset');
  assert.deepEqual(calls.map((call) => call.command), ['open_asset']);
});
