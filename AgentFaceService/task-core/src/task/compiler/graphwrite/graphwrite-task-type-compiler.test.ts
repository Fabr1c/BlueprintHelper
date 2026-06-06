import assert from 'node:assert/strict';
import test from 'node:test';

import {
  graphWriteAppendTaskSpecFixture,
  graphWriteReplaceTaskSpecFixture,
} from '../../fixtures/task-protocol.fixtures.js';
import { compileTaskSpecToTaskPlan } from '../task-compiler.js';
import { graphWriteTaskTypeCompiler } from './graphwrite-task-type-compiler.js';

test('graphwrite task compiler owns edit_blueprint_graph compilation', () => {
  const plan = graphWriteTaskTypeCompiler.compile(graphWriteReplaceTaskSpecFixture, { source: 'facade' });
  const graphWriteStep = plan.steps.find((step) => 'capability' in step && step.capability === 'graph_write');

  assert.equal(plan.schema, 'BlueprintHelper.TaskPlan.v1');
  assert.equal(plan.task_type, 'edit_blueprint_graph');
  assert.ok(graphWriteStep);
  assert.equal((graphWriteStep.write.ops[0] as Record<string, unknown>).op, 'replace_body');
});

test('facade compiles edit_blueprint_graph through registered graphwrite compiler', () => {
  const plan = compileTaskSpecToTaskPlan(graphWriteAppendTaskSpecFixture);

  assert.equal(plan.task_type, 'edit_blueprint_graph');
  assert.equal(plan.steps.some((step) => 'capability' in step && step.capability === 'graph_write'), true);
});
