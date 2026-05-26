import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { basename, join } from 'node:path';

import type { GraphWriteGeneralityOperation } from './graphwrite-generality-matrix.js';

export type GraphWriteGeneralityStatus = 'pass' | 'fail' | 'skipped';
export type GraphWriteGeneralityFailureKind =
  | 'none'
  | 'setup_failure'
  | 'preview_failure'
  | 'execute_failure'
  | 'missing_evidence'
  | 'unsupported_intent'
  | 'readback_failure'
  | 'silent_wrong_graph';

export interface GraphWriteGeneralityVariantResult {
  operationId: string;
  variantName: string;
  assetPath?: string;
  graphName?: string;
  nodeCandidate?: string;
  variantMode?: string;
  requestedVariantCount?: number;
  attemptedVariantTarget?: number;
  availableSpawnCount?: number;
  actualSpawnCount?: number;
  requiredVariantCount: number;
  setupStatus?: GraphWriteGeneralityStatus;
  previewStatus: GraphWriteGeneralityStatus;
  executeStatus: GraphWriteGeneralityStatus;
  readbackStatus: GraphWriteGeneralityStatus;
  failureKind: GraphWriteGeneralityFailureKind;
  expectedReadback: string;
  operation?: GraphWriteGeneralityOperation;
}

export interface GraphWriteGeneralityOperationSummary {
  operationId: string;
  assetPath?: string;
  graphName?: string;
  variantMode?: string;
  passed: boolean;
  requestedVariantCount: number;
  availableSpawnCount: number;
  actualSpawnCount: number;
  passedVariants: number;
  requiredVariantCount: number;
  failureKinds: Record<string, number>;
}

export interface GraphWriteGeneralitySummary {
  schema: 'BlueprintHelper.GraphWriteGeneralitySummary.v2';
  totalOperations: number;
  passedOperations: number;
  failedOperations: number;
  totalVariants: number;
  passedVariants: number;
  failedVariants: number;
  operationPassRate: number;
  variantPassRate: number;
  allOperationsPassed: boolean;
  operations: GraphWriteGeneralityOperationSummary[];
}

export function summarizeGraphWriteGeneralityResults(results: GraphWriteGeneralityVariantResult[]): GraphWriteGeneralitySummary {
  const operationIds = [...new Set(results.map((result) => result.operationId))].sort();
  const operations = operationIds.map((operationId) => {
    const variants = results.filter((result) => result.operationId === operationId);
    const requiredVariantCount = Math.max(...variants.map((variant) => variant.requiredVariantCount), 0);
    const passedVariants = variants.filter((variant) => variant.failureKind === 'none').length;
    const first = variants[0];
    const failureKinds: Record<string, number> = {};
    for (const variant of variants) {
      failureKinds[variant.failureKind] = (failureKinds[variant.failureKind] ?? 0) + 1;
    }
    return {
      operationId,
      assetPath: first?.assetPath,
      graphName: first?.graphName,
      variantMode: first?.variantMode,
      passed: passedVariants === requiredVariantCount,
      requestedVariantCount: Math.max(...variants.map((variant) => variant.requestedVariantCount ?? variant.attemptedVariantTarget ?? variant.requiredVariantCount), 0),
      availableSpawnCount: Math.max(...variants.map((variant) => variant.availableSpawnCount ?? variant.requiredVariantCount), 0),
      actualSpawnCount: Math.max(...variants.map((variant) => variant.actualSpawnCount ?? 0), 0),
      passedVariants,
      requiredVariantCount,
      failureKinds,
    };
  });

  const passedOperations = operations.filter((operation) => operation.passed).length;
  const totalVariants = results.length;
  const passedVariants = results.filter((result) => result.failureKind === 'none').length;
  return {
    schema: 'BlueprintHelper.GraphWriteGeneralitySummary.v2',
    totalOperations: operations.length,
    passedOperations,
    failedOperations: operations.length - passedOperations,
    totalVariants,
    passedVariants,
    failedVariants: totalVariants - passedVariants,
    operationPassRate: operations.length === 0 ? 0 : passedOperations / operations.length,
    variantPassRate: totalVariants === 0 ? 0 : passedVariants / totalVariants,
    allOperationsPassed: operations.length > 0 && passedOperations === operations.length,
    operations,
  };
}

export function renderGraphWriteGeneralityCsv(results: GraphWriteGeneralityVariantResult[]): string {
  const rows = [
    'operation_id,variant_name,asset_path,graph_name,node_candidate,variant_mode,requested_variant_count,available_spawn_count,actual_spawn_count,required_variant_count,setup_status,preview_status,execute_status,readback_status,failure_kind,expected_readback',
    ...results.map((result) => [
      result.operationId,
      result.variantName,
      result.assetPath ?? '',
      result.graphName ?? '',
      result.nodeCandidate ?? '',
      result.variantMode ?? '',
      String(result.requestedVariantCount ?? result.attemptedVariantTarget ?? result.requiredVariantCount),
      String(result.availableSpawnCount ?? result.requiredVariantCount),
      String(result.actualSpawnCount ?? 0),
      String(result.requiredVariantCount),
      result.setupStatus ?? '',
      result.previewStatus,
      result.executeStatus,
      result.readbackStatus,
      result.failureKind,
      result.expectedReadback,
    ].map(csvCell).join(',')),
  ];
  return `${rows.join('\n')}\n`;
}

export function renderGraphWriteGeneralityMarkdown(
  summary: GraphWriteGeneralitySummary,
  chartFiles: { operationChart: string; failureChart: string; dataCsv: string; summaryJson: string },
): string {
  const rows = summary.operations.map((operation) => [
    `| \`${operation.assetPath ?? ''}\` | \`${operation.operationId}\` | ${operation.variantMode ?? ''} | ${operation.requestedVariantCount} | ${operation.availableSpawnCount} | ${operation.actualSpawnCount} | ${operation.passed ? 'PASS' : 'FAIL'} | ${operation.passedVariants}/${operation.requiredVariantCount} | ${formatFailureKinds(operation.failureKinds)} |`,
  ]).flat();
  return [
    '# GraphWrite Generality Preflight E2E Report',
    '',
    `Gate: ${summary.allOperationsPassed ? 'PASS' : 'FAIL'}`,
    '',
    `- Operation pass rate: ${(summary.operationPassRate * 100).toFixed(2)}% (${summary.passedOperations}/${summary.totalOperations})`,
    `- Variant pass rate: ${(summary.variantPassRate * 100).toFixed(2)}% (${summary.passedVariants}/${summary.totalVariants})`,
    `- Data: \`${chartFiles.dataCsv}\``,
    `- Summary JSON: \`${chartFiles.summaryJson}\``,
    '',
    'Failure taxonomy: `unsupported_intent` is reserved for current implementation diagnostics that explicitly reject an operation or statement kind. Missing projected spawner identity, stale fixture proof, or missing proof evidence is reported as `missing_evidence`.',
    '',
    `![Operation pass/fail](${chartFiles.operationChart})`,
    '',
    `![Failure distribution](${chartFiles.failureChart})`,
    '',
    '## Operation Table',
    '',
    '| Asset | Operation | Mode | Requested | Candidate count | Actual spawned | Result | Variants | Failure kinds |',
    '|---|---|---|---:|---:|---:|---|---:|---|',
    ...rows,
    '',
  ].join('\n');
}

export function renderOperationPassSvg(summary: GraphWriteGeneralitySummary): string {
  const width = 720;
  const height = 220;
  const passWidth = Math.round(520 * summary.operationPassRate);
  const failWidth = 520 - passWidth;
  return [
    `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">`,
    '<rect width="100%" height="100%" fill="#ffffff"/>',
    '<text x="24" y="34" font-family="Arial" font-size="20" fill="#111111">GraphWrite Generality Operation Pass Rate</text>',
    '<rect x="120" y="80" width="520" height="42" fill="#e5e7eb"/>',
    `<rect x="120" y="80" width="${passWidth}" height="42" fill="#16a34a"/>`,
    `<rect x="${120 + passWidth}" y="80" width="${failWidth}" height="42" fill="#dc2626"/>`,
    `<text x="120" y="154" font-family="Arial" font-size="16" fill="#111111">PASS ${summary.passedOperations}/${summary.totalOperations}</text>`,
    `<text x="360" y="154" font-family="Arial" font-size="16" fill="#111111">VARIANTS ${summary.passedVariants}/${summary.totalVariants}</text>`,
    '</svg>',
  ].join('\n');
}

export function renderFailureDistributionSvg(summary: GraphWriteGeneralitySummary): string {
  const counts = new Map<string, number>();
  for (const operation of summary.operations) {
    for (const [kind, count] of Object.entries(operation.failureKinds)) {
      if (kind !== 'none') counts.set(kind, (counts.get(kind) ?? 0) + count);
    }
  }
  const entries = [...counts.entries()].sort((a, b) => b[1] - a[1]);
  const width = 840;
  const rowHeight = 32;
  const height = Math.max(160, 80 + entries.length * rowHeight);
  const max = Math.max(1, ...entries.map((entry) => entry[1]));
  const bars = entries.map(([kind, count], index) => {
    const y = 72 + index * rowHeight;
    const barWidth = Math.round(520 * count / max);
    return `<text x="24" y="${y + 18}" font-family="Arial" font-size="14" fill="#111111">${escapeXml(kind)}</text><rect x="260" y="${y}" width="${barWidth}" height="22" fill="#2563eb"/><text x="${270 + barWidth}" y="${y + 17}" font-family="Arial" font-size="14" fill="#111111">${count}</text>`;
  });
  return [
    `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">`,
    '<rect width="100%" height="100%" fill="#ffffff"/>',
    '<text x="24" y="34" font-family="Arial" font-size="20" fill="#111111">GraphWrite Generality Failure Distribution</text>',
    ...bars,
    '</svg>',
  ].join('\n');
}

export function readRunResults(runRoot: string): GraphWriteGeneralityVariantResult[] {
  const specRoot = join(runRoot, 'specs');
  const resultRoot = join(runRoot, 'results');
  const setupOk = existsSync(join(resultRoot, '_setup', 'setup_summary.json'))
    ? asRecord(readJson(join(resultRoot, '_setup', 'setup_summary.json')))?.ok === true
    : true;
  if (!existsSync(specRoot)) return [];
  return readdirSync(specRoot, { withFileTypes: true })
    .filter((entry) => entry.isDirectory() && entry.name !== '_setup')
    .flatMap((entry) => readOperationResults(join(specRoot, entry.name), join(resultRoot, entry.name), setupOk));
}

export function writeGraphWriteGeneralityReport(input: { runRoot: string; reportDir: string; dateStamp?: string }): GraphWriteGeneralitySummary {
  const results = readRunResults(input.runRoot);
  const summary = summarizeGraphWriteGeneralityResults(results);
  mkdirSync(input.reportDir, { recursive: true });
  const date = input.dateStamp ?? new Date().toISOString().slice(0, 10).replaceAll('-', '');
  const dataCsv = `BlueprintHelper_GraphWrite_GeneralityPreflight_Data_${date}.csv`;
  const operationChart = `BlueprintHelper_GraphWrite_GeneralityPreflight_OperationChart_${date}.svg`;
  const failureChart = `BlueprintHelper_GraphWrite_GeneralityPreflight_FailureChart_${date}.svg`;
  const reportMd = `BlueprintHelper_GraphWrite_GeneralityPreflight_Report_${date}_CN.md`;
  const summaryJson = 'BlueprintHelper_GraphWrite_GeneralityPreflight_LatestSummary.json';
  writeFileSync(join(input.reportDir, dataCsv), renderGraphWriteGeneralityCsv(results), 'utf8');
  writeFileSync(join(input.reportDir, operationChart), renderOperationPassSvg(summary), 'utf8');
  writeFileSync(join(input.reportDir, failureChart), renderFailureDistributionSvg(summary), 'utf8');
  writeFileSync(join(input.reportDir, reportMd), renderGraphWriteGeneralityMarkdown(summary, {
    operationChart,
    failureChart,
    dataCsv,
    summaryJson,
  }), 'utf8');
  writeFileSync(join(input.reportDir, summaryJson), `${JSON.stringify(summary, null, 2)}\n`, 'utf8');
  return summary;
}

function readOperationResults(specDir: string, resultDir: string, setupOk: boolean): GraphWriteGeneralityVariantResult[] {
  const expected = readJson(join(specDir, 'expected_variants.json')) as {
    operation: GraphWriteGeneralityOperation;
    assetPath?: string;
    graphName?: string;
    expectedVariantNames: string[];
    expectedNodeCandidates?: string[];
    expectedReadback: string;
  } | undefined;
  if (!expected) return [];
  const operationSetupOk = asRecord(readJson(join(resultDir, 'setup', 'setup_summary.json')))?.ok === true;
  const effectiveSetupOk = setupOk && operationSetupOk;
  const preview = readJson(join(resultDir, 'graph_write_preview.json'));
  const execute = readJson(join(resultDir, 'graph_write_execute.json'));
  const previewRecord = asRecord(preview);
  const executeRecord = asRecord(execute);
  const readbackText = readText(join(resultDir, 'readback.json'));
  const previewPass = previewRecord?.ok === true && previewRecord?.status === 'preview_passed';
  const executePass = executeRecord?.ok === true && executeRecord?.status === 'executed';
  const presentVariantNames = expected.expectedVariantNames.filter((variant) => readbackText.includes(variant));
  const actualSpawnCount = presentVariantNames.length;
  const hasAnyReadback = readbackText.length > 0;
  return expected.expectedVariantNames.map((variantName, index) => {
    const variantPresent = presentVariantNames.includes(variantName);
    const failureKind = classifyFailure(effectiveSetupOk, preview, execute, variantPresent, hasAnyReadback);
    return {
      operationId: expected.operation.operationId,
      variantName,
      assetPath: expected.assetPath,
      graphName: expected.graphName,
      nodeCandidate: expected.expectedNodeCandidates?.[index],
      variantMode: expected.operation.variantMode,
      requestedVariantCount: expected.operation.attemptedVariantTarget,
      attemptedVariantTarget: expected.operation.attemptedVariantTarget,
      availableSpawnCount: expected.operation.availableSpawnCount,
      actualSpawnCount,
      requiredVariantCount: expected.operation.requiredVariantCount,
      setupStatus: effectiveSetupOk ? 'pass' : 'fail',
      previewStatus: effectiveSetupOk && previewPass ? 'pass' : effectiveSetupOk ? 'fail' : 'skipped',
      executeStatus: effectiveSetupOk && previewPass && executePass ? 'pass' : previewPass ? 'fail' : 'skipped',
      readbackStatus: effectiveSetupOk && previewPass && executePass && variantPresent ? 'pass' : executePass ? 'fail' : 'skipped',
      failureKind,
      expectedReadback: expected.expectedReadback,
      operation: expected.operation,
    };
  });
}

function classifyFailure(setupOk: boolean, preview: unknown, execute: unknown, variantPresent: boolean, hasAnyReadback: boolean): GraphWriteGeneralityFailureKind {
  if (!setupOk) return 'setup_failure';
  if (!isCliSuccess(preview, 'preview_passed')) {
    if (hasMissingEvidence(preview)) return 'missing_evidence';
    return hasUnsupportedIntent(preview) ? 'unsupported_intent' : 'preview_failure';
  }
  if (!isCliSuccess(execute, 'executed')) {
    if (hasMissingEvidence(execute)) return 'missing_evidence';
    return hasUnsupportedIntent(execute) ? 'unsupported_intent' : 'execute_failure';
  }
  if (!variantPresent) return hasAnyReadback ? 'silent_wrong_graph' : 'readback_failure';
  return 'none';
}

function isCliSuccess(value: unknown, status: string): boolean {
  return isRecord(value) && value.ok === true && value.status === status;
}

function hasUnsupportedIntent(value: unknown): boolean {
  const text = JSON.stringify(value ?? {});
  return /unsupported_|statement_kind_unsupported|unsupported kind/i.test(text);
}

function hasMissingEvidence(value: unknown): boolean {
  const text = JSON.stringify(value ?? {});
  return /missing_required_evidence|missing_.*evidence|spawner evidence|projected evidence|requires .*evidence|requires .*proof|select_result_type_unresolved|graph_latent_allowed=true evidence/i.test(text);
}

function readJson(file: string): unknown | undefined {
  if (!existsSync(file)) return undefined;
  try {
    return JSON.parse(readFileSync(file, 'utf8'));
  } catch {
    return undefined;
  }
}

function readText(file: string): string {
  return existsSync(file) ? readFileSync(file, 'utf8') : '';
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return isRecord(value) ? value : undefined;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function csvCell(value: string): string {
  return `"${value.replaceAll('"', '""')}"`;
}

function formatFailureKinds(failureKinds: Record<string, number>): string {
  return Object.entries(failureKinds)
    .filter(([, count]) => count > 0)
    .map(([kind, count]) => `${kind}:${count}`)
    .join(', ');
}

function escapeXml(value: string): string {
  return value
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;');
}
