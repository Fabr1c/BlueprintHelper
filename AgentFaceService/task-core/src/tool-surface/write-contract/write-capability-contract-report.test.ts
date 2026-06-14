import assert from 'node:assert/strict';
import test from 'node:test';

import { formatWriteCapabilityAuditAsJson, formatWriteCapabilityAuditAsMarkdown } from './write-capability-contract-report.js';
import type { WriteCapabilityAuditResult } from './write-capability-contract-types.js';

test('JSON report preserves machine readable audit rows', () => {
  const formatted = formatWriteCapabilityAuditAsJson(result());

  assert.equal(formatted.schema, 'BlueprintHelper.WriteCapabilityContractAuditResult.v1');
  assert.equal(formatted.rows[0]?.code, 'readback_evidence_missing');
});

test('Markdown report includes Summary and Rows tables', () => {
  const markdown = formatWriteCapabilityAuditAsMarkdown(result());

  assert.match(markdown, /## Summary/);
  assert.match(markdown, /\| Status \| Count \|/);
  assert.match(markdown, /## Rows/);
  assert.match(markdown, /\| Capability \| Dimension \| Status \| Code \| Remediation \|/);
  assert.match(markdown, /example\.write/);
});

function result(): WriteCapabilityAuditResult {
  return {
    schema: 'BlueprintHelper.WriteCapabilityContractAuditResult.v1',
    generated_at: '2026-06-14T00:00:00.000Z',
    contracts: [],
    summary: {
      pass: 1,
      gap: 1,
      blocked: 0,
      not_applicable: 0,
    },
    rows: [{
      capability_id: 'example.write',
      dimension: 'readback',
      status: 'gap',
      code: 'readback_evidence_missing',
      message: 'Readback missing.',
      evidence: [],
      remediation: 'Add readback evidence.',
    }],
  };
}
