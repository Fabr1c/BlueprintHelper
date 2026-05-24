import assert from 'node:assert/strict';
import test from 'node:test';
import {
  TaskPlanSchema,
  TaskSpecSchema,
} from '../../task/schema/task-schemas.js';
import { compileTaskSpecToTaskPlan } from '../../task/compiler/task-compiler.js';

function baseTaskSpec(taskType: string, behavior: Record<string, unknown>) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: taskType,
    feature_name: 'P2Smoke',
    target: {
      asset_path: '/Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke',
      target_type: 'blueprint',
    },
    execution_policy: { dry_run_mode: 'full' },
    validation: { should_compile: false, should_save: true },
    behavior,
  };
}

test('compiles edit_object_properties into object_property TaskPlan IR', () => {
  const taskSpec = TaskSpecSchema.parse(baseTaskSpec('edit_object_properties', {
    property_strategy: 'property_edit',
    changes: [
      { kind: 'set_property', property_path: 'DisplayName', value: { kind: 'literal', value_type: 'string', value: 'Door' } },
      { kind: 'set_property', property_path: 'OpenSpeed', value: { kind: 'literal', value_type: 'float', value: 1.5 } },
    ],
  }));

  const taskPlan = compileTaskSpecToTaskPlan(taskSpec);
  TaskPlanSchema.parse(taskPlan);
  const step = taskPlan.steps[0] as Record<string, any>;
  assert.equal(step.capability, 'object_property');
  assert.equal(step.write.strategy, 'property_edit');
  assert.equal(step.write.ops[0].op, 'set_object_properties');
});

test('compiles interface function and interface event signatures into distinct blueprint_signature IR', () => {
  const taskSpec = TaskSpecSchema.parse(baseTaskSpec('edit_blueprint_signature', {
    signature_strategy: 'signature_edit',
    changes: [
      {
        kind: 'ensure_interface_function',
        interface_path: '/Game/Interfaces/BPI_Door',
        function_name: 'CanInteract',
        inputs: [
          { name: 'Instigator', pin_type: { category: 'object', subcategory_object: '/Script/Engine.Actor' } },
        ],
        outputs: [
          { name: 'bCanInteract', pin_type: { category: 'bool' } },
        ],
      },
      {
        kind: 'ensure_interface_event',
        interface_path: '/Game/Interfaces/BPI_Door',
        event_name: 'OnInteract',
        graph_name: 'EventGraph',
        inputs: [
          { name: 'Instigator', pin_type: { category: 'object', subcategory_object: '/Script/Engine.Actor' } },
        ],
      },
    ],
  }));

  const taskPlan = compileTaskSpecToTaskPlan(taskSpec);
  TaskPlanSchema.parse(taskPlan);
  const functionStep = taskPlan.steps[0] as Record<string, any>;
  const eventStep = taskPlan.steps[1] as Record<string, any>;

  assert.equal(functionStep.capability, 'blueprint_signature');
  assert.equal(functionStep.write.strategy, 'function_signature');
  assert.deepEqual(functionStep.write.ops[0], {
    op: 'ensure_function',
    function_name: 'CanInteract',
    interface_path: '/Game/Interfaces/BPI_Door',
    interface_entry_kind: 'function',
    inputs: [
      { name: 'Instigator', pin_type: { category: 'object', subcategory_object: '/Script/Engine.Actor' } },
    ],
    outputs: [
      { name: 'bCanInteract', pin_type: { category: 'bool' } },
    ],
    name_collision_policy: 'reuse_if_exists',
  });
  assert.equal(eventStep.capability, 'blueprint_signature');
  assert.equal(eventStep.write.strategy, 'custom_event_signature');
  assert.deepEqual(eventStep.write.ops[0], {
    op: 'ensure_custom_event',
    event_name: 'OnInteract',
    graph_name: 'EventGraph',
    interface_path: '/Game/Interfaces/BPI_Door',
    interface_entry_kind: 'event',
    inputs: [
      { name: 'Instigator', pin_type: { category: 'object', subcategory_object: '/Script/Engine.Actor' } },
    ],
    name_collision_policy: 'reuse_if_exists',
  });
});

test('compiles dispatcher override and remove signature policies into blueprint_signature IR', () => {
  const taskSpec = TaskSpecSchema.parse(baseTaskSpec('edit_blueprint_signature', {
    signature_strategy: 'signature_edit',
    changes: [
      {
        kind: 'ensure_event_dispatcher',
        dispatcher_name: 'OnDoorOpened',
        inputs: [
          { name: 'bIsOpen', pin_type: { category: 'bool' } },
        ],
        signature_mismatch_policy: 'block',
      },
      {
        kind: 'ensure_override_event',
        event_name: 'ReceiveBeginPlay',
        event_kind: 'native_event',
        execute_policy: 'blocked_preflight',
      },
      {
        kind: 'remove_signature',
        signature_kind: 'event_dispatcher',
        signature_name: 'OnDeprecatedDoorOpened',
        execute_policy: 'blocked_preflight',
        require_reference_context: true,
      },
    ],
  }));

  const taskPlan = compileTaskSpecToTaskPlan(taskSpec);
  TaskPlanSchema.parse(taskPlan);
  const dispatcherStep = taskPlan.steps[0] as Record<string, any>;
  const overrideStep = taskPlan.steps[1] as Record<string, any>;
  const removeStep = taskPlan.steps[2] as Record<string, any>;

  assert.equal(dispatcherStep.write.strategy, 'event_dispatcher_signature');
  assert.deepEqual(dispatcherStep.write.ops[0], {
    op: 'ensure_event_dispatcher',
    dispatcher_name: 'OnDoorOpened',
    inputs: [
      { name: 'bIsOpen', pin_type: { category: 'bool' } },
    ],
    name_collision_policy: 'reuse_if_exists',
    signature_mismatch_policy: 'block',
  });
  assert.equal(overrideStep.write.strategy, 'override_event_signature');
  assert.deepEqual(overrideStep.write.ops[0], {
    op: 'ensure_override_event',
    event_name: 'ReceiveBeginPlay',
    event_kind: 'native_event',
    execute_policy: 'blocked_preflight',
  });
  assert.equal(removeStep.write.strategy, 'event_dispatcher_signature');
  assert.deepEqual(removeStep.write.ops[0], {
    op: 'remove_signature',
    signature_kind: 'event_dispatcher',
    signature_name: 'OnDeprecatedDoorOpened',
    execute_policy: 'blocked_preflight',
    require_reference_context: true,
  });
});

test('compiles custom_event_definition into signature then graph body steps', () => {
  const taskSpec = TaskSpecSchema.parse({
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
    feature_name: 'P2CustomEventDefinition',
    target: {
      asset_path: '/Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
    },
    execution_policy: { dry_run_mode: 'full' },
    validation: { should_compile: false, should_save: true },
    behavior: {
      graph_strategy: 'replace_owned_graph',
      replace: {
        scope: 'custom_event_definition',
        selector: {
          kind: 'custom_event',
          name: 'OnInteract',
        },
        inputs: [
          { name: 'Instigator', pin_type: { category: 'object', subcategory_object: '/Script/Engine.Actor' } },
        ],
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements: [
            {
              kind: 'call',
              target: 'PrintString',
              args: {
                InString: { kind: 'literal', value_type: 'string', value: 'interact' },
              },
            },
          ],
        },
      },
    },
  });

  const taskPlan = compileTaskSpecToTaskPlan(taskSpec);
  TaskPlanSchema.parse(taskPlan);

  assert.equal(taskPlan.steps.length, 2);
  const signatureStep = taskPlan.steps[0] as Record<string, any>;
  const graphStep = taskPlan.steps[1] as Record<string, any>;
  assert.equal(signatureStep.capability, 'blueprint_signature');
  assert.equal(signatureStep.write.strategy, 'custom_event_signature');
  assert.deepEqual(signatureStep.write.ops[0], {
    op: 'ensure_custom_event',
    event_name: 'OnInteract',
    graph_name: 'EventGraph',
    inputs: [
      { name: 'Instigator', pin_type: { category: 'object', subcategory_object: '/Script/Engine.Actor' } },
    ],
    name_collision_policy: 'reuse_if_exists',
  });
  assert.equal(graphStep.capability, 'graph_write');
  assert.deepEqual(graphStep.depends_on, ['step_001']);
  assert.equal(graphStep.write.ops[0].op, 'replace_body');
  assert.equal(graphStep.write.ops[0].replace_scope, 'custom_event_body');
  assert.equal(graphStep.write.ops[0].selector.entry_name, 'OnInteract');
});

test('compiles append_new_owned_graph custom event into signature dependency before graph body', () => {
  const taskSpec = TaskSpecSchema.parse({
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
    feature_name: 'P2AppendCustomEventBoundary',
    target: {
      asset_path: '/Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EG_CustomEventBoundary',
      allow_modify_user_nodes: false,
    },
    execution_policy: { dry_run_mode: 'full' },
    validation: { should_compile: false, should_save: true },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [{
        entry_type: 'custom_event',
        name: 'RunBoundarySmoke',
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements: [],
        },
      }],
    },
  });

  const taskPlan = compileTaskSpecToTaskPlan(taskSpec);
  TaskPlanSchema.parse(taskPlan);

  assert.equal(taskPlan.steps.length, 2);
  const signatureStep = taskPlan.steps[0] as Record<string, any>;
  const graphStep = taskPlan.steps[1] as Record<string, any>;
  assert.equal(signatureStep.capability, 'blueprint_signature');
  assert.equal(signatureStep.write.strategy, 'custom_event_signature');
  assert.deepEqual(signatureStep.write.ops.map((op: Record<string, unknown>) => op.op), ['ensure_custom_event']);
  assert.equal(signatureStep.write.ops[0].event_name, 'RunBoundarySmoke');
  assert.equal(signatureStep.write.ops[0].graph_name, 'EG_CustomEventBoundary');
  assert.equal(graphStep.capability, 'graph_write');
  assert.deepEqual(graphStep.depends_on, ['step_001']);
  assert.equal(graphStep.write.ops[0].op, 'ensure_entry');
  assert.equal(graphStep.write.ops[0].entry_type, 'custom_event');
});

test('rejects append_new_owned_graph entry types outside GraphWrite custom event body boundary', () => {
  const taskSpec = TaskSpecSchema.parse({
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
    feature_name: 'P2RejectNonCustomEventEntry',
    target: {
      asset_path: '/Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EG_CustomEventBoundary',
      allow_modify_user_nodes: false,
    },
    execution_policy: { dry_run_mode: 'full' },
    validation: { should_compile: false, should_save: true },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [{
        entry_type: 'native_event',
        name: 'ReceiveBeginPlay',
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements: [],
        },
      }],
    },
  });

  assert.throws(
    () => compileTaskSpecToTaskPlan(taskSpec),
    (error: unknown) => error instanceof Error
      && error.message.includes('Only custom_event entries are supported'),
  );
});

test('compiles override event create_if_missing policy into blueprint_signature IR', () => {
  const taskSpec = TaskSpecSchema.parse(baseTaskSpec('edit_blueprint_signature', {
    signature_strategy: 'signature_edit',
    changes: [
      {
        kind: 'ensure_override_event',
        event_name: 'ReceiveBeginPlay',
        event_kind: 'native_event',
        graph_name: 'EventGraph',
        execute_policy: 'create_if_missing',
      },
    ],
  }));

  const taskPlan = compileTaskSpecToTaskPlan(taskSpec);
  TaskPlanSchema.parse(taskPlan);
  const step = taskPlan.steps[0] as Record<string, any>;
  assert.equal(step.write.strategy, 'override_event_signature');
  assert.deepEqual(step.write.ops[0], {
    op: 'ensure_override_event',
    event_name: 'ReceiveBeginPlay',
    event_kind: 'native_event',
    graph_name: 'EventGraph',
    execute_policy: 'create_if_missing',
  });
});

test('rejects remove_signature when reference context is explicitly disabled', () => {
  const parsed = TaskSpecSchema.safeParse(baseTaskSpec('edit_blueprint_signature', {
    signature_strategy: 'signature_edit',
    changes: [
      {
        kind: 'remove_signature',
        signature_kind: 'event_dispatcher',
        signature_name: 'OnDeprecatedDoorOpened',
        execute_policy: 'blocked_preflight',
        require_reference_context: false,
      },
    ],
  }));

  assert.equal(parsed.success, false);
});
