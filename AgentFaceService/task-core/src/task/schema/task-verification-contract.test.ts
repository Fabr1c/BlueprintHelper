import assert from 'node:assert/strict';
import test from 'node:test';

import { compileTaskSpecToTaskPlan } from '../compiler/task-compiler.js';
import {
  TaskPlanSchema,
  TaskSpecSchema,
  type TaskVerificationContract,
} from './task-schemas.js';

test('TaskSpecSchema rejects free-form verification text and accepts structured readback requirements', () => {
  const invalid = TaskSpecSchema.safeParse(makeVariableTaskSpec({
    verification: 'make sure it matches the user request',
  }));

  assert.equal(invalid.success, false);

  const valid = TaskSpecSchema.safeParse(makeVariableTaskSpec({
    verification: makeVerificationContract(),
  }));

  assert.equal(valid.success, true);
});

test('TaskVerification contract rejects unknown top-level and requirement fields', () => {
  const unknownTopLevel = TaskSpecSchema.safeParse(makeVariableTaskSpec({
    verification: {
      ...makeVerificationContract(),
      agent_note: 'same semantic expectation but different extra text',
    },
  }));

  assert.equal(unknownTopLevel.success, false);

  const unknownRequirementField = TaskSpecSchema.safeParse(makeVariableTaskSpec({
    verification: {
      ...makeVerificationContract(),
      requirements: [{
        ...makeVerificationContract().requirements[0],
        agent_note: 'same fact but different extra text',
      }],
    },
  }));

  assert.equal(unknownRequirementField.success, false);
});

test('compileTaskSpecToTaskPlan carries structured verification into the TaskPlan', () => {
  const taskSpec = TaskSpecSchema.parse(makeVariableTaskSpec({
    verification: makeVerificationContract(),
  }));

  const taskPlan = compileTaskSpecToTaskPlan(taskSpec);
  const parsedPlan = TaskPlanSchema.parse(taskPlan);
  const verification = (parsedPlan as Record<string, unknown>).verification as Record<string, unknown> | undefined;

  assert.equal(verification?.schema, 'BlueprintHelper.TaskVerification.v1');
  assert.deepEqual(
    (verification?.requirements as Array<Record<string, unknown>>).map((item) => item.id),
    ['health-variable-exists'],
  );
});

function makeVariableTaskSpec(extra: Record<string, unknown> = {}): Record<string, unknown> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_variables',
    target: {
      asset_path: '/Game/BP/BP_VerificationTarget',
    },
    behavior: {
      variable_strategy: 'member_variables',
      changes: [{
        kind: 'ensure_member_variable',
        name: 'Health',
        pin_type: { category: 'float' },
      }],
    },
    ...extra,
  };
}

function makeVerificationContract(): TaskVerificationContract {
  return {
    schema: 'BlueprintHelper.TaskVerification.v1',
    mode: 'required',
    requirements: [{
      id: 'health-variable-exists',
      fact: 'blueprint.member_variable.exists',
      target: {
        asset_path: '/Game/BP/BP_VerificationTarget',
        property_path: 'variables.Health',
      },
      expected: true,
      source_evidence: {
        read_context_id: 'ctx_variables_before',
        fingerprint: 'readctx_fp_001',
      },
    }],
  };
}
