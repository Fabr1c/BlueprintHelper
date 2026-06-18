import assert from 'node:assert/strict';
import test from 'node:test';

import { compactTargetLabel, createMetricsTaskKey } from './task-key.js';

test('createMetricsTaskKey uses task type, feature name, target type, and target hash', () => {
  const key = createMetricsTaskKey({
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'create_blueprint_feature',
    feature_name: 'ReviewPanel_UI',
    target: {
      target_type: 'blueprint',
      asset_path: '/Game/UI/WBP_ReviewPanel',
      ref: {
        graph: 'EventGraph',
      },
    },
  } as never);

  assert.equal(key.task_type, 'create_blueprint_feature');
  assert.equal(key.feature_name, 'ReviewPanel_UI');
  assert.equal(key.target_type, 'blueprint');
  assert.equal(key.target_ref_label, '/Game/.../WBP_ReviewPanel');
  assert.match(key.target_ref_hash, /^sha256:[a-f0-9]{64}$/);
});

test('compactTargetLabel keeps long paths bounded', () => {
  const longPath = `/Game/${'VeryLongFolderName'.repeat(8)}/WBP_ReviewPanel`;
  const label = compactTargetLabel(longPath);

  assert.ok(label.length <= 96);
  assert.ok(label.startsWith('/Game/'));
  assert.ok(label.includes('...'));
  assert.ok(label.endsWith('/WBP_ReviewPanel'));
});

test('compactTargetLabel redacts middle segments for short asset paths too', () => {
  const label = compactTargetLabel('/Game/UI/WBP_ReviewPanel');

  assert.equal(label, '/Game/.../WBP_ReviewPanel');
  assert.ok(label.length <= 96);
});

test('createMetricsTaskKey falls back to target asset_class when asset_path is absent', () => {
  const first = createMetricsTaskKey({
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'create_asset',
    target: {
      target_type: 'asset',
      asset_class: '/Script/Engine.DataAsset',
    },
  } as never);
  const second = createMetricsTaskKey({
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'create_asset',
    target: {
      asset_class: '/Script/Engine.DataAsset',
      target_type: 'asset',
    },
  } as never);

  assert.equal(first.target_type, 'asset');
  assert.equal(first.target_ref_label, '/Script/.../Engine.DataAsset');
  assert.match(first.target_ref_hash, /^sha256:[a-f0-9]{64}$/);
  assert.equal(first.target_ref_hash, second.target_ref_hash);
});
