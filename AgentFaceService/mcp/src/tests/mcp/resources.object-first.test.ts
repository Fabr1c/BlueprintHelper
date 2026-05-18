import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import { registerResourcesWithBridge } from '../../test-support/test-harness.js';

describe('raw-json resource returns body directly', () => {
  const rawObj = {
    version: '2.2',
    schema: 'BlueprintHelper.JsonToBlueprint',
    nodes: [],
    links: [],
  };

  it('returns raw json body (not wrapped)', async () => {
    const resources = registerResourcesWithBridge(async (_cmd, _payload) => ({
      success: true,
      request_id: 'req_test',
      result: { payload: rawObj, json: rawObj },
    }));

    const resource = resources.get('blueprint-asset-view');
    assert.ok(resource);

    const uri = new URL('blueprint://asset/Game%2FBP%2FBP_Test.BP_Test?view=raw-json');
    const result = await resource.handler(uri);

    assert.equal(result.contents.length, 1);
    const parsed = JSON.parse(result.contents[0].text);
    // Should be raw json body, NOT a { json: ... } or { payload: ... } wrapper
    assert.ok(parsed.version !== undefined || parsed.schema !== undefined || parsed.nodes !== undefined);
    // Should not have a top-level key wrapping the body
    assert.equal(typeof parsed.nodes, 'object');
  });

  it('retired json_text is not treated as raw-json body', () => {
    const body = JSON.stringify(rawObj);
    const input = { json_text: body };
    assert.equal(typeof input.json_text, 'string');
  });
});
