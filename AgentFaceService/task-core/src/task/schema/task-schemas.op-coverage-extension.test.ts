import assert from 'node:assert/strict';
import { describe, it } from 'node:test';

import {
  OP_COVERAGE_EVIDENCE_KEYS,
  OP_COVERAGE_EXCLUDED_OPERATION_IDS,
  OP_COVERAGE_SUPPORTED_OPERATION_IDS,
} from './task-schemas.js';

describe('GraphWrite op coverage task schema exports', () => {
  it('exports stable operation allowlists and statement-local evidence keys', () => {
    const supportedOperationIds: readonly string[] = OP_COVERAGE_SUPPORTED_OPERATION_IDS;
    const evidenceKeys: readonly string[] = OP_COVERAGE_EVIDENCE_KEYS;

    assert.ok(OP_COVERAGE_SUPPORTED_OPERATION_IDS.includes('boolean_and'));
    assert.ok(OP_COVERAGE_SUPPORTED_OPERATION_IDS.includes('abs'));
    assert.equal(supportedOperationIds.includes('array_identical'), false);
    assert.ok(OP_COVERAGE_EXCLUDED_OPERATION_IDS.includes('enum_equal'));
    assert.ok(OP_COVERAGE_EXCLUDED_OPERATION_IDS.includes('enum_not_equal'));
    assert.ok(OP_COVERAGE_EVIDENCE_KEYS.includes('op.operation_id'));
    assert.equal(evidenceKeys.includes('op.array_lhs_pin_type'), false);
    assert.equal(evidenceKeys.includes('op.array_rhs_pin_type'), false);
  });
});
