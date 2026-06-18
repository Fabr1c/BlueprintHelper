import assert from 'node:assert/strict';
import test from 'node:test';

import { TaskSpecSchema } from './task-schemas.js';

const hiddenReplicationConditions = [
  'dynamic',
  'never',
  'net_group',
  'max',
  'COND_Dynamic',
  'COND_Never',
  'COND_NetGroup',
  'COND_Max',
] as const;

function baseSpec(): Record<string, unknown> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_variables',
    feature_name: 'RejectBadReplication',
    target: { asset_path: '/Game/BH/P0C/BP_Door', target_type: 'blueprint' },
    behavior: {
      variable_strategy: 'member_variables',
      changes: [{
        kind: 'configure_member_variable',
        name: 'DoorState',
        properties: [],
      }],
    },
  };
}

function setReplicationProperty(spec: Record<string, unknown>, value: Record<string, unknown>): void {
  const behavior = spec.behavior as { changes: Array<{ properties: unknown[] }> };
  behavior.changes[0].properties = [{
    property_path: 'replication',
    value,
  }];
}

test('edit_blueprint_variables accepts member variable replication property', () => {
  const parsed = TaskSpecSchema.parse({
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_variables',
    feature_name: 'ReplicateDoorState',
    target: { asset_path: '/Game/BH/P0C/BP_Door', target_type: 'blueprint' },
    behavior: {
      variable_strategy: 'member_variables',
      changes: [{
        kind: 'configure_member_variable',
        name: 'DoorState',
        properties: [{
          property_path: 'replication',
          value: {
            mode: 'rep_notify',
            condition: 'owner_only',
            notify_function: 'OnRep_DoorState',
            create_notify_function: true,
            reuse_existing_notify_function: false,
          },
        }],
      }],
    },
  });

  assert.equal(parsed.task_type, 'edit_blueprint_variables');
});

test('edit_blueprint_variables rejects hidden replication conditions', () => {
  for (const condition of hiddenReplicationConditions) {
    const spec = baseSpec();
    setReplicationProperty(spec, { mode: 'replicated', condition });

    assert.throws(
      () => TaskSpecSchema.parse(spec),
      (error) => {
        assert.equal(error instanceof Error, true, condition);
        return true;
      },
    );
  }
});

test('edit_blueprint_variables rejects unknown replication mode', () => {
  const spec = baseSpec();
  setReplicationProperty(spec, { mode: 'replicate_on_owner' });

  assert.throws(() => TaskSpecSchema.parse(spec));
});

test('edit_blueprint_variables rejects replication condition for none mode', () => {
  const spec = baseSpec();
  setReplicationProperty(spec, { mode: 'none', condition: 'owner_only' });

  assert.throws(() => TaskSpecSchema.parse(spec));
});
