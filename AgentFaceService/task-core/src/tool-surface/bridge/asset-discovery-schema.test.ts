import { strict as assert } from 'node:assert';
import test from 'node:test';
import type { z } from 'zod';
import { bridgeToolSchemas } from './bridge-tool-schemas.js';

function getFindAssetsSchema(): z.ZodTypeAny {
  const schema = bridgeToolSchemas['blueprinthelper_find_assets'];
  assert.ok(schema, 'blueprinthelper_find_assets schema must be registered');
  return schema;
}

test('find assets input accepts query, /Game path prefix, and semantic asset type', () => {
  const parsed = getFindAssetsSchema().parse({
    schema: 'BlueprintHelper.FindAssetsRequest.v1',
    query: 'Player',
    path_prefixes: ['/Game'],
    asset_types: ['blueprint'],
    recursive: true,
    limit: 25,
  });

  assert.deepEqual(parsed, {
    schema: 'BlueprintHelper.FindAssetsRequest.v1',
    query: 'Player',
    path_prefixes: ['/Game'],
    asset_types: ['blueprint'],
    recursive: true,
    limit: 25,
  });
});

test('find assets input accepts full asset class paths', () => {
  const parsed = getFindAssetsSchema().parse({
    schema: 'BlueprintHelper.FindAssetsRequest.v1',
    asset_classes: ['/Script/Engine.Blueprint', '/Script/UMG.WidgetBlueprint'],
    include_plugin_content: true,
    include_engine_content: false,
    include_redirectors: false,
  });

  assert.deepEqual(parsed.asset_classes, ['/Script/Engine.Blueprint', '/Script/UMG.WidgetBlueprint']);
});

test('find assets input rejects limit below P0 bounds', () => {
  const result = getFindAssetsSchema().safeParse({
    schema: 'BlueprintHelper.FindAssetsRequest.v1',
    limit: 0,
  });

  assert.equal(result.success, false);
});

test('find assets input rejects limit above P0 bounds', () => {
  const result = getFindAssetsSchema().safeParse({
    schema: 'BlueprintHelper.FindAssetsRequest.v1',
    limit: 101,
  });

  assert.equal(result.success, false);
});

test('find assets input rejects cursor in P0', () => {
  const result = getFindAssetsSchema().safeParse({
    schema: 'BlueprintHelper.FindAssetsRequest.v1',
    cursor: 'next-page',
  });

  assert.equal(result.success, false);
});

test('find assets input rejects unknown semantic asset types', () => {
  const result = getFindAssetsSchema().safeParse({
    schema: 'BlueprintHelper.FindAssetsRequest.v1',
    asset_types: ['material'],
  });

  assert.equal(result.success, false);
});

test('find assets input rejects malformed full asset class paths', () => {
  const result = getFindAssetsSchema().safeParse({
    schema: 'BlueprintHelper.FindAssetsRequest.v1',
    asset_classes: ['/Script/Engine'],
  });

  assert.equal(result.success, false);
});

test('find assets input rejects class paths with more than module and class segments', () => {
  const result = getFindAssetsSchema().safeParse({
    schema: 'BlueprintHelper.FindAssetsRequest.v1',
    asset_classes: ['/Script/Foo.Bar.Baz'],
  });

  assert.equal(result.success, false);
});
