import assert from 'node:assert/strict';

import {
  clearReadContextCapabilityCacheForTests,
  getCachedReadContextCapabilityPayload,
} from './read-context-capability-cache.js';

clearReadContextCapabilityCacheForTests();

let buildCount = 0;
const firstPayload = getCachedReadContextCapabilityPayload(() => {
  buildCount += 1;
  return {
    schema: 'ReadContextCapabilities.v1',
    read_type_ids: ['blueprint_logic'],
  };
});
firstPayload['schema'] = 'mutated';

const secondPayload = getCachedReadContextCapabilityPayload(() => {
  buildCount += 1;
  return {
    schema: 'ReadContextCapabilities.v1',
    read_type_ids: [],
  };
});

assert.equal(buildCount, 1);
assert.equal(secondPayload['schema'], 'ReadContextCapabilities.v1');
assert.deepEqual(secondPayload['read_type_ids'], ['blueprint_logic']);

clearReadContextCapabilityCacheForTests();
