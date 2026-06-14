import type { WriteCapabilityAuditResult } from './write-capability-contract-types.js';

export function formatWriteCapabilityAuditAsJson(
  result: WriteCapabilityAuditResult,
): WriteCapabilityAuditResult {
  return {
    ...result,
    contracts: result.contracts.map((contract) => ({ ...contract })),
    rows: result.rows.map((row) => ({
      ...row,
      evidence: [...row.evidence],
    })),
    summary: { ...result.summary },
  };
}

export function formatWriteCapabilityAuditAsMarkdown(
  result: WriteCapabilityAuditResult,
): string {
  const lines: string[] = [
    '# BlueprintHelper Write Capability Contract Audit',
    '',
    `Generated: ${result.generated_at}`,
    '',
    '## Summary',
    '',
    '| Status | Count |',
    '| --- | ---: |',
    `| pass | ${result.summary.pass} |`,
    `| gap | ${result.summary.gap} |`,
    `| blocked | ${result.summary.blocked} |`,
    `| not_applicable | ${result.summary.not_applicable} |`,
    '',
    '## Rows',
    '',
    '| Capability | Dimension | Status | Code | Remediation |',
    '| --- | --- | --- | --- | --- |',
    ...result.rows.map((row) => [
      escapeMarkdownCell(row.capability_id),
      escapeMarkdownCell(row.dimension),
      escapeMarkdownCell(row.status),
      escapeMarkdownCell(row.code),
      escapeMarkdownCell(row.remediation),
    ].join(' | ')).map((row) => `| ${row} |`),
    '',
  ];
  return `${lines.join('\n')}`;
}

function escapeMarkdownCell(value: string): string {
  return value
    .replaceAll('\\', '\\\\')
    .replaceAll('|', '\\|')
    .replaceAll('\r', ' ')
    .replaceAll('\n', ' ');
}
