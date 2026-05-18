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

  it('does not parse retired string json alias', () => {
    const input = { json: JSON.stringify(rawObj) };
    const result = normalizeBlueprintPayload(input) as Record<string, unknown>;
    assert.equal(typeof result['json'], 'string');
  });

  it('does not parse retired json_text field', () => {
    const input = { json_text: JSON.stringify(rawObj) };
    const result = normalizeBlueprintPayload(input) as Record<string, unknown>;
    assert.equal(typeof result['json_text'], 'string');
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

  it('does not use retired json_text as payload body', () => {
    const input = { json_text: rawObj };
    const body = getBlueprintPayloadBody(input) as Record<string, unknown>;
    assert.deepStrictEqual(body, input);
  });

  it('returns normalized input when no known fields', () => {
    const input = { other: 'value' };
    const body = getBlueprintPayloadBody(input);
    assert.deepStrictEqual(body, input);
  });
});
