import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import {
  normalizeBlueprintPayload,
  getBlueprintPayloadBody,
} from '../../mcp/result/mcp-response.js';

describe('normalizeBlueprintPayload (object-first)', () => {
  const rawObj = {
    version: '2.2',
    schema: 'BlueprintHelper.JsonToBlueprint',
    nodes: [],
    links: [],
  };

  it('preserves object payload', () => {
    const input = { payload: rawObj, json: rawObj };
    const result = normalizeBlueprintPayload(input) as Record<string, unknown>;
    assert.ok(typeof result['payload'] === 'object');
    assert.ok(typeof result['json'] === 'object');
    // payload should not be replaced by json
    assert.deepStrictEqual(result['payload'], rawObj);
  });

  it('parses string json to object', () => {
    const input = { json: JSON.stringify(rawObj) };
    const result = normalizeBlueprintPayload(input) as Record<string, unknown>;
    assert.ok(typeof result['json'] === 'object');
  });

  it('parses legacy json_text to object', () => {
    const input = { json_text: JSON.stringify(rawObj) };
    const result = normalizeBlueprintPayload(input) as Record<string, unknown>;
    assert.ok(typeof result['json_text'] === 'object');
  });

  it('handles non-record input', () => {
    const result = normalizeBlueprintPayload('hello');
    assert.equal(result, 'hello');
  });

  it('handles null gracefully', () => {
    const result = normalizeBlueprintPayload(null);
    assert.equal(result, null);
  });
});

describe('getBlueprintPayloadBody', () => {
  const rawObj = {
    version: '2.2',
    schema: 'BlueprintHelper.JsonToBlueprint',
    nodes: [],
    links: [],
  };

  it('prefers payload over json', () => {
    const input = { payload: rawObj, json: { ...rawObj, version: '2.0' } };
    const body = getBlueprintPayloadBody(input) as Record<string, unknown>;
    assert.equal(body['version'], '2.2');
    assert.deepStrictEqual(body, rawObj);
  });

  it('falls back to json when no payload', () => {
    const input = { json: rawObj };
    const body = getBlueprintPayloadBody(input) as Record<string, unknown>;
    assert.deepStrictEqual(body, rawObj);
  });

  it('falls back to json_text when no payload or json', () => {
    const input = { json_text: rawObj };
    const body = getBlueprintPayloadBody(input) as Record<string, unknown>;
    assert.deepStrictEqual(body, rawObj);
  });

  it('returns normalized input when no known fields', () => {
    const input = { other: 'value' };
    const body = getBlueprintPayloadBody(input);
    assert.deepStrictEqual(body, input);
  });
});
