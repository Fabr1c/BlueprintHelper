import assert from 'node:assert/strict';
import test from 'node:test';

import { compileTaskSpecToTaskPlan } from '../../task/compiler/task-compiler.js';
import { executeTaskTool } from './task-tool-dispatcher.js';

const bareTaskSpec = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  task_type: 'edit_blueprint_variables',
  feature_name: 'P2_CLI_To_Compiler',
  context_id: 'ctx-p2-cli-compiler',
  target: { asset_path: '/Game/BP_P2_CLI_To_Compiler', target_type: 'blueprint' },
  execution_policy: { dry_run_mode: 'quick' },
  validation: { should_compile: false, should_save: false },
  behavior: {
    variable_strategy: 'member_variables',
    changes: [{
      kind: 'ensure_member_variable',
      name: 'CompilerValue',
      variable_type: { category: 'bool' },
    }],
  },
};

function makeCompilerOnlyContext() {
  return {
    taskRunner: {
      async previewTask(taskSpec: typeof bareTaskSpec) {
        const taskPlan = compileTaskSpecToTaskPlan(taskSpec as never);
        return {
          toolResult: {
            ok: true,
            operation: 'preview_task',
            task_type: taskPlan.task_type,
            step_count: taskPlan.steps.length,
          },
        };
      },
      async executeTask() {
        throw new Error('executeTask is outside this P2 compiler-only E2E test');
      },
      async getTaskResult() {
        throw new Error('getTaskResult is outside this P2 compiler-only E2E test');
      },
    },
  };
}

test('blueprinthelper_preview_task wrapped input reaches TaskCompiler without UE', async () => {
  const result = await executeTaskTool(
    'blueprinthelper_preview_task',
    { task_spec: bareTaskSpec },
    makeCompilerOnlyContext() as never,
  );

  assert.deepEqual(result, {
    ok: true,
    operation: 'preview_task',
    task_type: 'edit_blueprint_variables',
    step_count: 1,
  });
});

test('blueprinthelper_preview_task bare input is adapted before handler and reaches the same compiler path', async () => {
  const result = await executeTaskTool(
    'blueprinthelper_preview_task',
    bareTaskSpec,
    makeCompilerOnlyContext() as never,
  );

  assert.deepEqual(result, {
    ok: true,
    operation: 'preview_task',
    task_type: 'edit_blueprint_variables',
    step_count: 1,
  });
});
