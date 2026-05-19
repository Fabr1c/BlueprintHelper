import assert from 'node:assert/strict';

import {
  buildPayloadSizeMetric,
  estimateJsonPayloadBytes,
} from './read-context-payload-metrics.js';

assert.equal(
  estimateJsonPayloadBytes({ value: 'abc' }),
  Buffer.byteLength(JSON.stringify({ value: 'abc' }), 'utf8'),
);

assert.equal(estimateJsonPayloadBytes(undefined), 0);

assert.deepEqual(
  buildPayloadSizeMetric('read_context.payload_bytes', { value: 'abc' }),
  {
    name: 'read_context.payload_bytes',
    duration_ms: 0,
    bytes: Buffer.byteLength(JSON.stringify({ value: 'abc' }), 'utf8'),
  },
);
