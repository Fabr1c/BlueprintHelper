import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { NON_GRAPHWRITE_TEMPLATE_FAMILIES } from './non-graphwrite-template-metadata.js';

const PLUGIN_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../../..');

test('non-GraphWrite metadata declares every TaskSpec family from design', () => {
  assert.deepEqual(
    NON_GRAPHWRITE_TEMPLATE_FAMILIES.map((entry) => entry.family).sort(),
    [
      'asset_factory',
      'blueprint_class_settings',
      'blueprint_components',
      'blueprint_create_feature',
      'blueprint_signature',
      'blueprint_variables',
      'data_table',
      'object_properties',
      'umg_widget',
    ],
  );
});

test('supported non-GraphWrite metadata points to real templates', () => {
  for (const entry of NON_GRAPHWRITE_TEMPLATE_FAMILIES) {
    if (entry.status !== 'supported') continue;
    assert.equal(fs.existsSync(path.join(PLUGIN_ROOT, entry.base_template_path)), true, `${entry.family} template exists`);
    assert.equal(entry.insert_targets.length > 0, true, `${entry.family} insert targets`);
  }
});
