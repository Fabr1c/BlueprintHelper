import assert from 'node:assert/strict';
import test from 'node:test';

import { compileTaskSpecToTaskPlan } from './task-compiler.js';
import { createTaskSpecCompiler } from './task-compiler-service.js';

const variableSpec = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  task_type: 'edit_blueprint_variables',
  feature_name: 'P3_Facade_Variables',
  context_id: 'ctx-p3-facade-variables',
  target: { asset_path: '/Game/BP_P3_Facade', target_type: 'blueprint' },
  behavior: {
    variable_strategy: 'member_variables',
    changes: [{
      kind: 'ensure_member_variable',
      name: 'P3Value',
      variable_type: { category: 'bool' },
    }],
  },
};

test('compileTaskSpecToTaskPlan facade compiles P3 non-GraphWrite task through registry', () => {
  const taskPlan = compileTaskSpecToTaskPlan(variableSpec as never);

  assert.equal(taskPlan.schema, 'BlueprintHelper.TaskPlan.v1');
  assert.equal(taskPlan.task_type, 'edit_blueprint_variables');
  assert.equal(taskPlan.steps.length, 1);
});

test('createTaskSpecCompiler strategy facade still compiles P3 non-GraphWrite task', async () => {
  const compiler = createTaskSpecCompiler();
  const compiled = await compiler(variableSpec as never, { dryRun: true });

  assert.equal(compiled.schema, 'BlueprintHelper.TaskCompilerResult.v1');
  assert.equal(compiled.strategyId, 'canonical_ts');
  assert.equal(compiled.taskPlan.task_type, 'edit_blueprint_variables');
});
