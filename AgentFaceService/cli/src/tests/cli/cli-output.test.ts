import { strict as assert } from 'node:assert';
import test from 'node:test';
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
import { resolveArtifactRoot } from '../../cli/artifacts.js';
import { buildCliSummary, omitCliFields, projectCliFields } from '../../cli/output.js';

test('summary output omits full task_plan and points to artifacts', () => {
  const result = buildCliSummary({
    command: {
      kind: 'task.preview',
      format: 'summary',
      resultPolicyId: 'task.preview.default',
      artifactDir: 'artifacts',
      maxBytes: 4096,
    },
    toolResult: {
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
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

  assert.equal('schema' in result, false);
  assert.equal(result.operation, 'task.preview');
  assert.equal(result.status, 'preview_passed');
  assert.equal(JSON.stringify(result).includes('step_1'), false);
  assert.equal((result.artifacts as Record<string, unknown>).task_plan, 'artifacts/preview_001/task_plan.json');
});

test('output budget failure still points to full result artifact', () => {
  const result = buildCliSummary({
    command: {
      kind: 'task.preview',
      format: 'summary',
      resultPolicyId: 'task.preview.default',
      artifactDir: 'artifacts',
      maxBytes: 40,
    },
    toolResult: {
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
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

test('execute summary does not expose preview_id even if the tool result contains one', () => {
  const result = buildCliSummary({
    command: { kind: 'task.execute', format: 'summary', resultPolicyId: 'task.execute.default' },
    toolResult: {
      ok: true,
      schema: 'BlueprintHelper.ToolResult.v1',
      operation: 'execute_task',
      trace_id: 'trace_execute',
      status: 'completed',
      modified: true,
      data: {
        schema: 'BlueprintHelper.TaskExecution.v1',
        task_run_id: 'task_001',
        preview_id: 'preview_should_not_surface',
        task: {
          task_run_id: 'task_001',
          target_assets: ['/Game/BP_Player'],
          applied_steps: 1,
        },
      },
    },
    artifactRefs: {
      full_result: 'artifacts/task_001/result.json',
    },
  });

  assert.equal(result.status, 'executed');
  assert.equal(result.task_run_id, 'task_001');
  assert.equal('preview_id' in result, false);
});

test('artifact root reads project and user setting defaults', () => {
  const projectRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-cli-artifact-setting-'));
  const cwd = path.join(projectRoot, 'Plugins', 'BlueprintHelper');
  fs.mkdirSync(path.join(projectRoot, '.blueprinthelper'), { recursive: true });
  fs.mkdirSync(path.join(projectRoot, 'Saved', 'BlueprintHelper'), { recursive: true });
  fs.mkdirSync(cwd, { recursive: true });
  fs.writeFileSync(
    path.join(projectRoot, '.blueprinthelper', 'setting.json'),
    JSON.stringify({ cli: { artifacts: { default_output_dir: 'ProjectArtifacts' } } }),
  );
  fs.writeFileSync(
    path.join(projectRoot, 'Saved', 'BlueprintHelper', 'setting.user.json'),
    JSON.stringify({ cli: { artifacts: { default_output_dir: 'UserArtifacts' } } }),
  );

  assert.equal(resolveArtifactRoot({ cwd }), path.resolve(projectRoot, 'UserArtifacts'));
});

test('artifact root keeps explicit CLI and env overrides above setting defaults', () => {
  const cwd = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-cli-artifact-priority-'));
  fs.mkdirSync(path.join(cwd, '.blueprinthelper'), { recursive: true });
  fs.writeFileSync(
    path.join(cwd, '.blueprinthelper', 'setting.json'),
    JSON.stringify({ cli: { artifacts: { default_output_dir: 'SettingArtifacts' } } }),
  );

  const previousArtifactDir = process.env['BPH_CLI_ARTIFACT_DIR'];
  process.env['BPH_CLI_ARTIFACT_DIR'] = 'EnvArtifacts';
  try {
    assert.equal(resolveArtifactRoot({ cwd }), 'EnvArtifacts');
    assert.equal(resolveArtifactRoot({ cwd, cliDir: 'CliArtifacts' }), 'CliArtifacts');
  } finally {
    if (previousArtifactDir === undefined) {
      delete process.env['BPH_CLI_ARTIFACT_DIR'];
    } else {
      process.env['BPH_CLI_ARTIFACT_DIR'] = previousArtifactDir;
    }
  }
});

test('field projection keeps only requested top-level and nested fields', () => {
  const result = projectCliFields({
    ok: true,
    schema: 'BlueprintHelper.CliResult.v1',
    operation: 'task.preview',
    status: 'preview_passed',
    summary: {
      target_assets: ['/Game/BP_Player'],
      planned_steps: 2,
    },
    artifacts: {
      full_result: 'artifacts/preview_001/result.json',
      task_plan: 'artifacts/preview_001/task_plan.json',
    },
  }, ['status', 'summary.target_assets', 'artifacts.full_result']);

  assert.deepEqual(result, {
    status: 'preview_passed',
    summary: {
      target_assets: ['/Game/BP_Player'],
    },
    artifacts: {
      full_result: 'artifacts/preview_001/result.json',
    },
  });
});

test('field omission drops requested top-level and nested fields', () => {
  const result = omitCliFields({
    ok: true,
    schema: 'BlueprintHelper.CliResult.v1',
    operation: 'task.preview',
    status: 'preview_passed',
    summary: {
      target_assets: ['/Game/BP_Player'],
      planned_steps: 2,
    },
    artifacts: {
      full_result: 'artifacts/preview_001/result.json',
      task_plan: 'artifacts/preview_001/task_plan.json',
    },
  }, ['operation', 'status', 'artifacts.task_plan']);

  assert.deepEqual(result, {
    ok: true,
    schema: 'BlueprintHelper.CliResult.v1',
    summary: {
      target_assets: ['/Game/BP_Player'],
      planned_steps: 2,
    },
    artifacts: {
      full_result: 'artifacts/preview_001/result.json',
    },
  });
});

