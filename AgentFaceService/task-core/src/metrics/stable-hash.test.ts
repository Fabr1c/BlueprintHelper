import assert from 'node:assert/strict';
import test from 'node:test';

import { hashStableJson, stableStringify } from './stable-hash.js';

test('stableStringify sorts object keys recursively and removes undefined object entries', () => {
  const left = stableStringify({
    b: 2,
    a: {
      d: 4,
      c: 3,
      skip: undefined,
    },
    skip: undefined,
  });
  const right = stableStringify({
    a: {
      c: 3,
      d: 4,
    },
    b: 2,
  });

  assert.equal(left, right);
  assert.equal(left, '{"a":{"c":3,"d":4},"b":2}');
});

test('stableStringify uses deterministic code-point key ordering for mixed case and non ASCII keys', () => {
  assert.equal(
    stableStringify({
      中: 4,
      b: 2,
      ä: 3,
      A: 1,
    }),
    '{"A":1,"b":2,"ä":3,"中":4}',
  );
});

test('hashStableJson returns sha256-prefixed deterministic hashes', () => {
  const first = hashStableJson({ b: 2, a: 1 });
  const second = hashStableJson({ a: 1, b: 2 });

  assert.match(first, /^sha256:[a-f0-9]{64}$/);
  assert.equal(first, second);
});

test('stableStringify preserves array order while still sorting nested object keys', () => {
  const value = [
    { z: 1, a: 2 },
    'repeat',
    undefined,
    { b: [{ d: 4, c: 3 }, 'repeat'], a: 1 },
    'repeat',
  ];

  assert.equal(
    stableStringify(value),
    '[{"a":2,"z":1},"repeat",null,{"a":1,"b":[{"c":3,"d":4},"repeat"]},"repeat"]',
  );
});

test('stableStringify serializes top-level undefined as null and hashStableJson treats it deterministically', () => {
  assert.equal(stableStringify(undefined), 'null');
  assert.equal(hashStableJson(undefined), hashStableJson(null));
});
