import assert from 'node:assert/strict';
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import test from 'node:test';

import {
  readRunResults,
  renderFailureDistributionSvg,
  renderGraphWriteGeneralityCsv,
  renderGraphWriteGeneralityMarkdown,
  renderOperationPassSvg,
  summarizeGraphWriteGeneralityResults,
  type GraphWriteGeneralityVariantResult,
} from './graphwrite-generality-report.js';

test('GraphWrite generality report requires all required variants for an operation pass and renders charts', () => {
  const results: GraphWriteGeneralityVariantResult[] = Array.from({ length: 10 }, (_, index) => ({
    operationId: 'function_action.call_function',
    variantName: `GWGen_function_action_call_function_${String(index).padStart(2, '0')}`,
    requiredVariantCount: 10,
    previewStatus: 'pass',
    executeStatus: index === 9 ? 'fail' : 'pass',
    readbackStatus: index === 9 ? 'fail' : 'pass',
    failureKind: index === 9 ? 'execute_failure' : 'none',
    expectedReadback: 'K2Node_CallFunction',
  }));
  const summary = summarizeGraphWriteGeneralityResults(results);
  assert.equal(summary.totalOperations, 1);
  assert.equal(summary.passedOperations, 0);
  assert.equal(summary.allOperationsPassed, false);
  assert.equal(summary.passedVariants, 9);
  assert.match(renderGraphWriteGeneralityCsv(results), /execute_failure/);
  const markdown = renderGraphWriteGeneralityMarkdown(summary, {
    operationChart: 'operation.svg',
    failureChart: 'failure.svg',
    dataCsv: 'data.csv',
    summaryJson: 'summary.json',
  });
  assert.match(markdown, /Gate: FAIL/);
  assert.match(markdown, /unsupported_intent.*explicitly reject/);
  assert.match(renderOperationPassSvg(summary), /<svg/);
  assert.match(renderFailureDistributionSvg(summary), /execute_failure/);
});

test('GraphWrite generality report accepts one successful singleton variant', () => {
  const summary = summarizeGraphWriteGeneralityResults([{
    operationId: 'generic_ops.control.branch',
    variantName: 'GWGen_generic_ops_control_branch_00',
    requiredVariantCount: 1,
    previewStatus: 'pass',
    executeStatus: 'pass',
    readbackStatus: 'pass',
    failureKind: 'none',
    expectedReadback: 'branch',
  }]);
  assert.equal(summary.totalOperations, 1);
  assert.equal(summary.passedOperations, 1);
  assert.equal(summary.allOperationsPassed, true);
  assert.equal(summary.passedVariants, 1);
});

test('GraphWrite generality report separates missing evidence from actual unsupported diagnostics', () => {
  const tempParent = join(process.cwd(), 'build');
  mkdirSync(tempParent, { recursive: true });
  const runRoot = mkdtempSync(join(tempParent, 'graphwrite-report-test-'));
  try {
    writeRunOperation(runRoot, 'missing_evidence_op', {
      operationId: 'schedule.timer_delegate_node',
      preview: {
        ok: false,
        status: 'preview_failed',
        issues: [{ message: 'requires projected schedule spawner evidence' }],
      },
    });
    writeRunOperation(runRoot, 'unsupported_op', {
      operationId: 'statement.unsupported_probe',
      preview: {
        ok: false,
        status: 'preview_failed',
        error_code: 'unsupported_statement_kind',
      },
    });

    const results = readRunResults(runRoot);
    assert.equal(results.find((result) => result.operationId === 'schedule.timer_delegate_node')?.failureKind, 'missing_evidence');
    assert.equal(results.find((result) => result.operationId === 'statement.unsupported_probe')?.failureKind, 'unsupported_intent');
  } finally {
    rmSync(runRoot, { recursive: true, force: true });
  }
});

function writeRunOperation(runRoot: string, key: string, input: { operationId: string; preview: unknown }): void {
  const specDir = join(runRoot, 'specs', key);
  const resultDir = join(runRoot, 'results', key);
  mkdirSync(specDir, { recursive: true });
  mkdirSync(join(resultDir, 'setup'), { recursive: true });
  writeJson(join(specDir, 'expected_variants.json'), {
    operation: {
      operationId: input.operationId,
      source: 'operation_group',
      sourceId: 'test',
      supportStatus: 'supported',
      variantMode: 'singleton_1',
      attemptedVariantTarget: 1,
      availableSpawnCount: 1,
      requiredVariantCount: 1,
      requiredEvidenceKeys: [],
      spawnCandidateNames: [input.operationId],
    },
    assetPath: `/Game/BlueprintHelper/Generality/BP_${key}`,
    graphName: `EG_${key}`,
    expectedVariantNames: [`GWGen_${key}_00`],
    expectedNodeCandidates: [input.operationId],
    expectedReadback: `EG_${key}`,
  });
  writeJson(join(resultDir, 'setup', 'setup_summary.json'), { ok: true });
  writeJson(join(resultDir, 'graph_write_preview.json'), input.preview);
  writeJson(join(resultDir, 'graph_write_execute.json'), {});
}

function writeJson(file: string, value: unknown): void {
  writeFileSync(file, `${JSON.stringify(value, null, 2)}\n`, 'utf8');
}
