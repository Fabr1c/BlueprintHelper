import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { TaskPlanSchema } from './task-schemas.js';
import {
  graphWriteAppendExpectedTaskPlanFixture,
  graphWriteLoweringAdapterTaskPlanFixtures,
  graphWriteStructuredIrTaskPlanFixture,
  graphWriteTaskPlanFixtures,
} from './task-protocol.fixtures.js';

describe('GraphWrite TaskPlan canonical fixtures', () => {
  it('treats append TaskSpec compilation as structured GraphWrite IR first', () => {
    assert.doesNotThrow(() => TaskPlanSchema.parse(graphWriteAppendExpectedTaskPlanFixture));

    const step = graphWriteAppendExpectedTaskPlanFixture.steps[0];
    assert.ok(step && 'capability' in step);
    assert.equal(step.capability, 'graph_write');
    assert.equal(Object.hasOwn(step as Record<string, unknown>, 'operation'), false);
    assert.equal(step.target.graph, 'BH_StoneGateActivation');
    assert.equal(step.write.strategy, 'owned_graph_edit');
    assert.deepEqual(step.write.ops.map((op) => op.op), ['ensure_entry']);
    assert.deepEqual(step.constraints, {
      allow_modify_user_nodes: false,
      ownership_scope: 'blueprinthelper_owned',
    });
  });

  it('rejects structured GraphWrite IR steps that carry adapter operation compatibility fields', () => {
    const taskPlan = structuredClone(graphWriteAppendExpectedTaskPlanFixture);
    const step = taskPlan.steps[0] as Record<string, unknown>;
    step.operation = 'append_blueprint_graph';

    assert.throws(() => TaskPlanSchema.parse(taskPlan), /operation/);
  });

  it('parses the canonical structured GraphWrite TaskPlan IR fixture', () => {
    assert.doesNotThrow(() => TaskPlanSchema.parse(graphWriteStructuredIrTaskPlanFixture));

    const step = graphWriteStructuredIrTaskPlanFixture.steps[0];
    assert.ok(step && 'capability' in step);
    assert.equal(step.capability, 'graph_write');
    assert.equal(step.write.strategy, 'owned_graph_edit');
    assert.deepEqual(step.write.ops.map((op) => op.op), [
      'ensure_entry',
      'set_pin_default',
      'insert_flow',
    ]);
    assert.deepEqual(step.constraints, {
      allow_modify_user_nodes: false,
      ownership_scope: 'blueprinthelper_owned',
    });
  });

  it('parses all fixed GraphWrite lowering adapter fixtures', () => {
    assert.deepEqual(
      graphWriteLoweringAdapterTaskPlanFixtures.map((fixture) => fixture.steps[0]?.operation),
      [
        'append_blueprint_graph',
        'replace_blueprint_graph',
        'patch_blueprint_graph',
        'merge_blueprint_graph',
      ],
    );

    for (const fixture of graphWriteLoweringAdapterTaskPlanFixtures) {
      assert.doesNotThrow(() => TaskPlanSchema.parse(fixture), fixture.steps[0]?.operation);
    }
  });

  it('keeps legacy compile/save keys out of GraphWrite TaskPlan fixtures', () => {
    for (const fixture of graphWriteTaskPlanFixtures) {
      assertNoForbiddenKeys(fixture, ['compile', 'save']);
      assert.deepEqual(Object.keys(fixture.execution_policy), [
        'dry_run_mode',
        'should_compile',
        'should_save',
      ]);
    }
  });
});

function assertNoForbiddenKeys(value: unknown, forbiddenKeys: readonly string[], path = '$'): void {
  if (Array.isArray(value)) {
    value.forEach((item, index) => assertNoForbiddenKeys(item, forbiddenKeys, `${path}[${index}]`));
    return;
  }

  if (!value || typeof value !== 'object') {
    return;
  }

  for (const [key, child] of Object.entries(value as Record<string, unknown>)) {
    assert.equal(forbiddenKeys.includes(key), false, `${path}.${key}`);
    assertNoForbiddenKeys(child, forbiddenKeys, `${path}.${key}`);
  }
}
