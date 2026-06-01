import { strict as assert } from 'node:assert';
import test from 'node:test';

import { CaptureScreenshotInputSchema } from './capture-screenshot-schema.js';

test('capture screenshot schema accepts asset-only request', () => {
  const parsed = CaptureScreenshotInputSchema.parse({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    label: 'bp_player_asset',
  });

  assert.equal(parsed.asset_path, '/Game/Blueprints/BP_Player.BP_Player');
  assert.equal(parsed.capture_target, 'auto');
  assert.equal(parsed.settle_delay_ms, 250);
});

test('capture screenshot schema accepts readable blueprint location request', () => {
  const parsed = CaptureScreenshotInputSchema.parse({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    graph_name: 'EventGraph',
    block_ref: 'BeginPlaySetup',
    node_ref: 'nodes[0]',
    settle_delay_ms: 250,
  });

  assert.equal(parsed.graph_name, 'EventGraph');
  assert.equal(parsed.block_ref, 'BeginPlaySetup');
  assert.equal(parsed.node_ref, 'nodes[0]');
  assert.equal(parsed.capture_target, 'auto');
  assert.equal(parsed.settle_delay_ms, 250);
});

test('capture screenshot schema only permits auto target for graph requests', () => {
  const parsed = CaptureScreenshotInputSchema.safeParse({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    graph_name: 'EventGraph',
    capture_target: 'active_window',
  });

  assert.equal(parsed.success, false);
  assert.match(String(parsed.error?.issues[0]?.message), /Graph area/);
});

test('capture screenshot schema requires graph_name when block_ref is provided', () => {
  const parsed = CaptureScreenshotInputSchema.safeParse({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    block_ref: 'BeginPlaySetup',
  });

  assert.equal(parsed.success, false);
  assert.match(String(parsed.error?.issues[0]?.message), /graph_name/);
});

test('capture screenshot schema requires graph_name when node_ref is provided', () => {
  const parsed = CaptureScreenshotInputSchema.safeParse({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    node_ref: 'nodes[0]',
  });

  assert.equal(parsed.success, false);
  assert.match(String(parsed.error?.issues[0]?.message), /graph_name/);
});

test('capture screenshot schema rejects unknown fields and unsafe labels', () => {
  assert.equal(CaptureScreenshotInputSchema.safeParse({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    unexpected: true,
  }).success, false);

  assert.equal(CaptureScreenshotInputSchema.safeParse({
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    label: '../escape',
  }).success, false);
});
