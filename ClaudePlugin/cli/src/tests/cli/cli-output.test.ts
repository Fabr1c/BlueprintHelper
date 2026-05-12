import { strict as assert } from 'node:assert';
import test from 'node:test';
import { buildCliSummary } from '../../cli/output.js';

test('summary output omits full task_plan and points to artifacts', () => {
  const result = buildCliSummary({
    command: { kind: 'task.preview', format: 'summary', artifactDir: 'artifacts', maxBytes: 4096 },
    toolResult: {
      ok: true,
      schema: 'BlueprintHelper.McpToolResult.v1',
      operation: 'preview_task',
      trace_id: 'trace_001',
      status: 'dry_run',
      modified: false,
      target: { target_type: 'blueprint', asset_path: '/Game/BP_Player' },
      data: {
        schema: 'BlueprintHelper.TaskPreview.v1',
        preview_id: 'preview_001',
        passed: true,
        blocked: false,
        task_plan: { steps: [{ step_id: 'step_1' }] },
        issues: [],
      },
    },
    artifactRefs: {
      full_result: 'artifacts/preview_001/result.json',
      task_plan: 'artifacts/preview_001/task_plan.json',
    },
  });

  assert.equal(result.schema, 'BlueprintHelper.CliResult.v1');
  assert.equal(result.operation, 'task.preview');
  assert.equal(result.status, 'preview_passed');
  assert.equal(JSON.stringify(result).includes('step_1'), false);
  assert.equal((result.artifacts as Record<string, unknown>).task_plan, 'artifacts/preview_001/task_plan.json');
});

test('output budget failure still points to full result artifact', () => {
  const result = buildCliSummary({
    command: { kind: 'task.preview', format: 'summary', artifactDir: 'artifacts', maxBytes: 40 },
    toolResult: {
      ok: true,
      schema: 'BlueprintHelper.McpToolResult.v1',
      operation: 'preview_task',
      trace_id: 'trace_001',
      status: 'dry_run',
      modified: false,
      data: {
        preview_id: 'preview_001',
        passed: true,
        issues: [],
      },
    },
    artifactRefs: {
      full_result: 'artifacts/preview_001/result.json',
    },
    forceBudgetFailure: true,
  });

  assert.equal(result.ok, false);
  assert.equal(result.status, 'output_too_large');
  assert.equal((result.artifacts as Record<string, unknown>).full_result, 'artifacts/preview_001/result.json');
});
