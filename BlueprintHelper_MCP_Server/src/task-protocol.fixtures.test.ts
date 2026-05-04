import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { compileTaskSpecToTaskPlan } from './task-compiler.js';
import { TaskSpecSchema } from './task-schemas.js';
import {
  graphWriteAppendExpectedTaskPlanFixture,
  graphWriteAppendTaskSpecFixture,
} from './task-protocol.fixtures.js';

describe('Task protocol fixtures', () => {
  it('parses and compiles the GraphWrite Append TaskSpec fixture into the expected TaskPlan', () => {
    const taskSpec = TaskSpecSchema.parse(graphWriteAppendTaskSpecFixture);
    const taskPlan = compileTaskSpecToTaskPlan(taskSpec);

    assert.deepEqual(taskPlan, graphWriteAppendExpectedTaskPlanFixture);
  });

  it('uses should_compile and should_save instead of legacy compile and save fields', () => {
    const taskSpec = graphWriteAppendTaskSpecFixture as Record<string, unknown>;
    const taskPlan = graphWriteAppendExpectedTaskPlanFixture as Record<string, unknown>;

    assertNoLegacyCompileSaveFields('TaskSpec.validation', getRecord(taskSpec, 'validation'));
    assertNoLegacyCompileSaveFields('TaskSpec.execution_policy', getRecord(taskSpec, 'execution_policy'));
    assertNoLegacyCompileSaveFields('TaskPlan.execution_policy', getRecord(taskPlan, 'execution_policy'));
  });
});

function assertNoLegacyCompileSaveFields(label: string, value: Record<string, unknown>) {
  assert.equal(Object.hasOwn(value, 'compile'), false, `${label}.compile`);
  assert.equal(Object.hasOwn(value, 'save'), false, `${label}.save`);
}

function getRecord(record: Record<string, unknown>, field: string): Record<string, unknown> {
  const value = record[field];
  assert.equal(typeof value, 'object', field);
  assert.notEqual(value, null, field);
  assert.equal(Array.isArray(value), false, field);
  return value as Record<string, unknown>;
}
