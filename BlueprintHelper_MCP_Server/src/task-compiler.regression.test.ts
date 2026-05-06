import assert from 'node:assert/strict';
import test, { describe, it } from 'node:test';
import {
  TASK_CONTEXT_PACK_SCHEMA,
  TASK_ERROR_SCHEMA,
  TASK_RUN_JOURNAL_SCHEMA,
  TaskContextPackSchema,
  TaskErrorSchema,
  TaskPlanSchema,
  TaskRunJournalSchema,
  TaskSpecSchema,
} from './task-schemas.js';
import {
  TaskSpecCompileError,
  compileTaskSpecToTaskPlan,
  taskPlanToAppendBridgePayload,
} from './task-compiler.js';

function makeTaskSpec(overrides: Record<string, unknown> = {}) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_test',
    task_type: 'edit_blueprint_graph',
    feature_name: 'DoorFeature',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EG_DoorFeature',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [
        {
          entry_type: 'custom_event',
          name: 'ToggleDoor',
          body: {
            schema: 'BlueprintLogicSpec.v1',
            statements: [
              {
                kind: 'call_function',
                name: 'PrintString',
                args: {
                  InString: {
                    kind: 'literal',
                    value_type: 'string',
                    value: 'hello',
                  },
                },
              },
            ],
          },
        },
      ],
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
    ...overrides,
  };
}

function makeVariableTaskSpec(overrides: Record<string, unknown> = {}) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_variables',
    task_type: 'edit_blueprint_variables',
    feature_name: 'DoorVariables',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    behavior: {
      variable_strategy: 'member_variables',
      variables: [
        {
          op: 'ensure_member_variable',
          name: 'bDoorOpen',
          pin_type: {
            category: 'bool',
          },
          category: 'Door',
          flags: {
            expose_on_spawn: false,
          },
        },
      ],
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: true,
      should_save: false,
    },
    ...overrides,
  };
}

function makeCompositePhysicsDoorTaskSpec(overrides: Record<string, unknown> = {}) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_physics_door',
    task_type: 'create_blueprint_feature',
    feature_name: 'PhysicsDoor',
    target: {
      asset_path: '/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor',
      target_type: 'blueprint',
    },
    scope_policy: {
      prefer_new_graph: true,
      graph_name: 'EG_PhysicsDoor',
      allow_modify_user_nodes: false,
      allow_create_assets: false,
    },
    asset_policy: {
      if_target_asset_missing: 'fail',
      if_referenced_asset_missing: 'fail',
      if_component_exists: 'reuse_if_type_matches',
    },
    resources: {
      static_meshes: {
        door_mesh: '/Game/BlueprintHelperTest/Meshes/SM_Door',
      },
    },
    components: [
      {
        name: 'SceneRoot',
        class: 'SceneComponent',
        set_as_root: true,
      },
      {
        name: 'DoorMesh',
        class: 'StaticMeshComponent',
        attach_to: 'SceneRoot',
        properties: {
          StaticMesh: '$resources.static_meshes.door_mesh',
          Mobility: 'Movable',
          CollisionProfileName: 'PhysicsActor',
          'BodyInstance.bSimulatePhysics': true,
        },
      },
    ],
    variables: [
      {
        name: 'bDoorOpen',
        type: 'bool',
        default: false,
        category: 'Door',
      },
      {
        name: 'OpenImpulse',
        type: 'float',
        default: 50000.0,
        category: 'Door',
      },
    ],
    class_settings: {
      implemented_interfaces: [
        '/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable',
      ],
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [
        {
          entry_type: 'custom_event',
          name: 'OpenPhysicsDoor',
          body: {
            schema: 'BlueprintLogicSpec.v1',
            statements: [
              {
                kind: 'set_member_variable',
                name: 'bDoorOpen',
                value: {
                  kind: 'literal',
                  value_type: 'bool',
                  value: true,
                },
              },
              {
                kind: 'call_function',
                name: 'DoorMesh.AddAngularImpulseInDegrees',
                args: {
                  VelChange: {
                    kind: 'literal',
                    value_type: 'bool',
                    value: true,
                  },
                },
              },
            ],
          },
        },
      ],
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: true,
      should_save: false,
    },
    ...overrides,
  };
}

describe('TaskSpec schema validation', () => {
  it('rejects missing schema', () => {
    const spec = makeTaskSpec();
    delete (spec as Record<string, unknown>).schema;

    assert.throws(() => TaskSpecSchema.parse(spec), /schema/);
  });

  it('rejects missing target asset path', () => {
    const spec = makeTaskSpec({
      target: { target_type: 'blueprint' },
    });

    assert.throws(() => TaskSpecSchema.parse(spec), /asset_path/);
  });

  it('rejects missing graph name', () => {
    const spec = makeTaskSpec({
      scope_policy: { allow_modify_user_nodes: false },
    });

    assert.throws(() => TaskSpecSchema.parse(spec), /graph_name/);
  });

  it('rejects legacy validation compile/save fields', () => {
    const spec = makeTaskSpec({
      validation: {
        compile: true,
        save: true,
      },
    });

    assert.throws(() => TaskSpecSchema.parse(spec), /should_compile/);
  });

  it('validates task protocol output schemas', () => {
    const plan = compileTaskSpecToTaskPlan(TaskSpecSchema.parse(makeTaskSpec()));

    assert.doesNotThrow(() => TaskPlanSchema.parse(plan));
    assert.doesNotThrow(() => TaskContextPackSchema.parse({
      schema: TASK_CONTEXT_PACK_SCHEMA,
      context_id: 'ctx_test',
      runtime: {
        bridge_reachable: true,
        profile: {},
      },
      target: {
        asset_path: '/Game/BP/BP_Door',
        exists: true,
        asset_info: {},
      },
      blueprint_summary: {
        graphs: [],
      },
      recommended_constraints: {
        prefer_new_graph: true,
        allow_modify_user_nodes: false,
        graph_strategy: 'append_new_owned_graph',
      },
    }));
    assert.doesNotThrow(() => TaskErrorSchema.parse({
      schema: TASK_ERROR_SCHEMA,
      code: 'unsupported_graph_strategy',
      category: 'semantic_error',
      stage: 'parse_input',
      message: 'Only append_new_owned_graph is supported.',
      retryable: true,
      rollback_result: 'not_needed',
      agent_action: 'fix_taskspec_and_retry',
      issues: [],
    }));
    assert.doesNotThrow(() => TaskRunJournalSchema.parse({
      schema: TASK_RUN_JOURNAL_SCHEMA,
      task_run_id: 'task_test',
      preview_id: 'preview_test',
      task_type: 'edit_blueprint_graph',
      feature_name: 'DoorFeature',
      status: 'partial_failure',
      target_assets: ['/Game/BP/BP_Door'],
      steps: [
        {
          step_id: 'step_append_graph',
          operation: 'graph_write',
          adapter_operation: 'append_blueprint_graph',
          status: 'failed',
        },
        {
          step_id: 'step_configure_variable',
          operation: 'blueprint_variable',
          depends_on: ['step_append_graph'],
          status: 'blocked',
          blocked_by_step_ids: ['step_append_graph'],
          blocked_reason: 'dependency_failed',
          error: null,
        },
        {
          step_id: 'step_create_asset',
          operation: 'asset_factory',
          status: 'completed',
        },
      ],
      recovery: {
        recommended_action: 'inspect_task_result_then_submit_followup_taskspec',
        safe_to_retry: false,
        rollback_available: false,
        notes: [],
      },
      bridge_result: {},
    }));
  });
});

describe('Composite create_blueprint_feature compiler', () => {
  it('compiles composite create_blueprint_feature into existing capability TaskPlan steps', () => {
    const taskPlan = compileTaskSpecToTaskPlan(TaskSpecSchema.parse(makeCompositePhysicsDoorTaskSpec()));
    const steps = taskPlan.steps as Array<Record<string, any>>;

    assert.equal(taskPlan.task_type, 'create_blueprint_feature');
    assert.deepEqual(taskPlan.target_assets, ['/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor']);
    assert.deepEqual(
      steps.map((step) => ('capability' in step ? step['capability'] : step['operation'])),
      [
        'blueprint_component',
        'blueprint_component',
        'blueprint_component',
        'blueprint_variable',
        'blueprint_variable',
        'blueprint_class_settings',
        'blueprint_signature',
        'graph_write',
      ],
    );

    assert.equal(steps[0]['write'].ops[0].op, 'add_component');
    assert.equal(steps[1]['write'].ops[0].op, 'add_component');
    assert.equal(steps[2]['write'].ops[0].op, 'set_component_properties');
    assert.deepEqual(steps[2]['depends_on'], ['step_002']);
    assert.equal(steps[3]['write'].strategy, 'member_variables');
    assert.equal(steps[3]['write'].ops.length, 2);
    assert.equal(steps[4]['write'].strategy, 'member_defaults');
    assert.equal(steps[4]['write'].ops[1].value, 50000);
    assert.equal(steps[5]['write'].ops[0].op, 'add_implemented_interfaces');
    assert.equal(steps[6]['write'].ops[0].op, 'ensure_custom_event');
    assert.equal(steps[7]['target'].graph, 'EG_PhysicsDoor');
    assert.equal(steps[7]['write'].ops[0].op, 'ensure_entry');
    assert.deepEqual(steps[7]['depends_on'], ['step_007']);
  });

  it('compiles composite interface integration through signature and graph_write capability steps', () => {
    const spec = makeCompositePhysicsDoorTaskSpec({
      class_settings: undefined,
      integration: {
        interface: {
          interface_asset: '/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable',
          function: 'Interact',
          implementation: {
            call: 'OpenPhysicsDoor',
          },
        },
      },
    });

    const taskPlan = compileTaskSpecToTaskPlan(TaskSpecSchema.parse(spec));
    const steps = taskPlan.steps as Array<Record<string, any>>;

    assert.deepEqual(
      steps.map((step) => step['capability']),
      [
        'blueprint_component',
        'blueprint_component',
        'blueprint_component',
        'blueprint_variable',
        'blueprint_variable',
        'blueprint_signature',
        'graph_write',
        'blueprint_class_settings',
        'blueprint_signature',
        'graph_write',
      ],
    );
    assert.equal(steps[5]['write'].ops[0].op, 'ensure_custom_event');
    assert.equal(steps[6]['write'].ops[0].op, 'ensure_entry');
    assert.deepEqual(steps[6]['depends_on'], ['step_006']);
    assert.deepEqual(steps[7]['write'].ops[0], {
      op: 'add_implemented_interfaces',
      interface_paths: ['/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable'],
    });
    assert.deepEqual(steps[8]['write'].ops[0], {
      op: 'ensure_function',
      function_name: 'Interact',
      interface_path: '/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable',
      name_collision_policy: 'reuse_if_exists',
    });
    assert.deepEqual(steps[8]['depends_on'], ['step_008']);
    assert.deepEqual(steps[9]['target'], {
      asset_path: '/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor',
      graph: 'Interact',
    });
    assert.deepEqual(steps[9]['depends_on'], ['step_009']);
    assert.equal(steps[9]['write'].ops[0].op, 'replace_body');
    assert.equal(steps[9]['write'].ops[0].replace_scope, 'function_body');
    assert.deepEqual(steps[9]['write'].ops[0].selector, { function_name: 'Interact' });
    assert.deepEqual(steps[9]['write'].ops[0].replacement.nodes, [
      {
        id: 'interface_Interact_stmt_1',
        kind: 'call',
        function: 'OpenPhysicsDoor',
        inputs: {},
      },
    ]);
  });

  it('rejects unsupported composite input integration instead of silently ignoring it', () => {
    const spec = makeCompositePhysicsDoorTaskSpec({
      integration: {
        input: {
          mode: 'reference_existing_input_action',
          input_action: '/Game/Input/IA_Interact',
        },
      },
    });

    assert.throws(
      () => compileTaskSpecToTaskPlan(TaskSpecSchema.parse(spec)),
      /unsupported composite integration/i,
    );
  });

  it('rejects composite property settings that omit value', () => {
    const spec = makeCompositePhysicsDoorTaskSpec({
      components: [
        {
          name: 'DoorMesh',
          class: 'StaticMeshComponent',
          properties: [
            {
              property_path: 'Mobility',
            },
          ],
        },
      ],
    });

    assert.throws(
      () => compileTaskSpecToTaskPlan(TaskSpecSchema.parse(spec)),
      /Property setting requires value/,
    );
  });
});

describe('TaskSpec GraphWrite Append compiler', () => {
  it('accepts replace_blueprint_graph TaskPlan steps from the GraphWrite capability family', () => {
    const taskPlan = {
      schema: 'BlueprintHelper.TaskPlan.v1',
      task_name: 'DoorBodyRewrite',
      task_type: 'edit_blueprint_graph',
      context_id: 'ctx_replace',
      target_assets: ['/Game/BP/BP_Door'],
      execution_policy: {
        dry_run_mode: 'full',
        should_compile: true,
        should_save: false,
      },
      steps: [
        {
          step_id: 'step_001',
          operation: 'replace_blueprint_graph',
          target: {
            asset_path: '/Game/BP/BP_Door',
            graph: 'EventGraph',
            replace_scope: 'custom_event_body',
          },
          args: {
            selector: {
              entry_name: 'ToggleDoor',
              node_path: 'logic.groups[0].entry.node_path',
            },
            replacement: {
              nodes: [],
              links: [],
            },
            options: {
              strict: true,
              preserve_layout: false,
            },
          },
        },
      ],
    };

    assert.doesNotThrow(() => TaskPlanSchema.parse(taskPlan));
  });

  it('accepts patch_blueprint_graph TaskPlan steps from the GraphWrite capability family', () => {
    const taskPlan = {
      schema: 'BlueprintHelper.TaskPlan.v1',
      task_name: 'DoorConditionPatch',
      task_type: 'edit_blueprint_graph',
      context_id: 'ctx_patch',
      target_assets: ['/Game/BP/BP_Door'],
      execution_policy: {
        dry_run_mode: 'full',
        should_compile: true,
        should_save: true,
      },
      steps: [
        {
          step_id: 'step_001',
          operation: 'patch_blueprint_graph',
          target: {
            asset_path: '/Game/BP/BP_Door',
            graph: 'EventGraph',
            patch_scope: 'pin_default',
          },
          args: {
            patch_type: 'set_pin_default',
            patched_ref: {
              block_id: 'BH_DoorFeature_ToggleDoor',
              group_entry_node_path: 'logic.groups[0].entry.node_path',
              node_ref: 'nodes[1]',
              pin_ref: 'Condition',
            },
            patch: {
              value: true,
            },
            expected_old_state: {
              value: false,
            },
          },
        },
      ],
    };

    assert.doesNotThrow(() => TaskPlanSchema.parse(taskPlan));
  });

  it('accepts merge_blueprint_graph TaskPlan steps from the GraphWrite capability family', () => {
    const taskPlan = {
      schema: 'BlueprintHelper.TaskPlan.v1',
      task_name: 'DoorFlowMerge',
      task_type: 'edit_blueprint_graph',
      context_id: 'ctx_merge',
      target_assets: ['/Game/BP/BP_Door'],
      execution_policy: {
        dry_run_mode: 'quick',
        should_compile: true,
        should_save: true,
      },
      steps: [
        {
          step_id: 'step_001',
          operation: 'merge_blueprint_graph',
          target: {
            asset_path: '/Game/BP/BP_Door',
            graph: 'EventGraph',
            merge_scope: 'owned_block_call',
            insert_strategy: 'insert_between',
          },
          args: {
            anchor: {
              block_id: 'BH_DoorFeature_ToggleDoor',
              group_entry_node_path: 'logic.groups[0].entry.node_path',
              node_ref: 'nodes[0]',
              pin_ref: 'Then',
            },
            inserted: {
              block_id: 'EG_DoorFeature_ToggleDoor0',
            },
          },
        },
      ],
    };

    assert.doesNotThrow(() => TaskPlanSchema.parse(taskPlan));
  });

  it('rejects non-append graph strategies', () => {
    const spec = makeTaskSpec({
      behavior: {
        graph_strategy: 'replace_graph',
        entries: [
          {
            entry_type: 'custom_event',
            name: 'ToggleDoor',
            body: { schema: 'BlueprintLogicSpec.v1', statements: [] },
          },
        ],
      },
    });

    assert.throws(
      () => compileTaskSpecToTaskPlan(TaskSpecSchema.parse(spec)),
      (err: unknown) => {
        if (!(err instanceof TaskSpecCompileError)) return false;
        const compileError = err as TaskSpecCompileError;
        return compileError.code === 'unsupported_graph_strategy' &&
          compileError.issues[0]?.path === 'behavior.graph_strategy';
      },
    );
  });

  it('compiles replace_owned_graph into structured graph_write IR', () => {
    const spec = makeTaskSpec({
      behavior: {
        graph_strategy: 'replace_owned_graph',
        replace: {
          scope: 'custom_event_body',
          selector: {
            kind: 'custom_event',
            name: 'ToggleDoor',
            graph_id: 'EventGraph',
            node_ref: 'ToggleDoorEntry',
          },
          body: {
            schema: 'BlueprintLogicSpec.v1',
            statements: [
              {
                kind: 'call_function',
                name: 'PrintString',
                args: {
                  InString: {
                    kind: 'literal',
                    value_type: 'string',
                    value: 'replaced',
                  },
                },
              },
            ],
          },
          options: {
            strict: true,
            preserve_layout: false,
          },
        },
      },
    });

    const plan = compileTaskSpecToTaskPlan(TaskSpecSchema.parse(spec));
    const step = plan.steps[0];

    assert.ok(step && 'capability' in step);
    assert.equal(Object.hasOwn(step as Record<string, unknown>, 'operation'), false);
    assert.equal(step.capability, 'graph_write');
    assert.equal(step.write.strategy, 'owned_graph_edit');
    assert.deepEqual(step.write.ops, [
      {
        op: 'replace_body',
        replace_scope: 'custom_event_body',
        selector: {
          entry_name: 'ToggleDoor',
          graph_id: 'EventGraph',
          node_ref: 'ToggleDoorEntry',
        },
        replacement: {
          nodes: [
            {
              id: 'replace_stmt_1',
              kind: 'call',
              function: 'PrintString',
              inputs: {
                InString: 'replaced',
              },
            },
          ],
          links: [],
        },
        options: {
          strict: true,
          preserve_layout: false,
        },
      },
    ]);
  });

  it('compiles patch_owned_graph into structured graph_write IR', () => {
    const spec = makeTaskSpec({
      behavior: {
        graph_strategy: 'patch_owned_graph',
        patches: [
          {
            kind: 'set_pin_default',
            target_ref: {
              block_id: 'BH_DoorFeature_ToggleDoor',
              group_entry_node_path: 'logic.groups[0].entry.node_path',
              node_ref: 'nodes[1]',
              pin_ref: 'Condition',
              link_ref: 'links[0]',
            },
            value: {
              kind: 'literal',
              value_type: 'bool',
              value: true,
            },
            expected_old_state: {
              value: {
                kind: 'literal',
                value_type: 'bool',
                value: false,
              },
            },
          },
        ],
      },
    });

    const plan = compileTaskSpecToTaskPlan(TaskSpecSchema.parse(spec));
    const step = plan.steps[0];

    assert.ok(step && 'capability' in step);
    assert.equal(Object.hasOwn(step as Record<string, unknown>, 'operation'), false);
    assert.deepEqual(step.write.ops, [
      {
        op: 'set_pin_default',
        patch_scope: 'pin_default',
        patched_ref: {
          block_id: 'BH_DoorFeature_ToggleDoor',
          group_entry_node_path: 'logic.groups[0].entry.node_path',
          node_ref: 'nodes[1]',
          pin_ref: 'Condition',
          link_ref: 'links[0]',
        },
        patch: {
          value: 'true',
        },
        expected_old_state: {
          value: 'false',
        },
      },
    ]);
  });

  it('compiles node comment and node position patches into runtime-ready structured graph_write IR', () => {
    const spec = makeTaskSpec({
      behavior: {
        graph_strategy: 'patch_owned_graph',
        patches: [
          {
            kind: 'set_node_comment',
            target_ref: {
              block_id: 'BH_DoorFeature_ToggleDoor',
              group_entry_node_path: 'logic.groups[0].entry.node_path',
              node_ref: 'nodes[0]',
            },
            value: 'Check whether the door is open.',
          },
          {
            kind: 'set_node_position',
            target_ref: {
              block_id: 'BH_DoorFeature_ToggleDoor',
              group_entry_node_path: 'logic.groups[0].entry.node_path',
              node_ref: 'nodes[0]',
            },
            patch: {
              x: 320,
              y: 160,
            },
          },
        ],
      },
    });

    const plan = compileTaskSpecToTaskPlan(TaskSpecSchema.parse(spec));

    assert.deepEqual((plan.steps as Array<Record<string, any>>).map((step) => step.write.ops[0]), [
      {
        op: 'set_node_comment',
        patch_scope: 'node_comment',
        patched_ref: {
          block_id: 'BH_DoorFeature_ToggleDoor',
          group_entry_node_path: 'logic.groups[0].entry.node_path',
          node_ref: 'nodes[0]',
        },
        patch: {
          comment: 'Check whether the door is open.',
        },
      },
      {
        op: 'set_node_position',
        patch_scope: 'node_position',
        patched_ref: {
          block_id: 'BH_DoorFeature_ToggleDoor',
          group_entry_node_path: 'logic.groups[0].entry.node_path',
          node_ref: 'nodes[0]',
        },
        patch: {
          x: 320,
          y: 160,
        },
      },
    ]);
  });

  it('compiles merge_owned_graph into structured graph_write IR', () => {
    const spec = makeTaskSpec({
      behavior: {
        graph_strategy: 'merge_owned_graph',
        merges: [
          {
            kind: 'insert_flow',
            scope: 'function_call',
            insert_strategy: 'insert_between',
            anchor: {
              block_id: 'BH_DoorFeature_ToggleDoor',
              group_entry_node_path: 'logic.groups[0].entry.node_path',
              node_ref: 'nodes[0]',
              pin_ref: 'Then',
              link_ref: 'links[0]',
            },
            inserted: {
              call_kind: 'function_call',
              name: 'OpenDoor',
            },
          },
        ],
      },
    });

    const plan = compileTaskSpecToTaskPlan(TaskSpecSchema.parse(spec));
    const step = plan.steps[0];

    assert.ok(step && 'capability' in step);
    assert.equal(Object.hasOwn(step as Record<string, unknown>, 'operation'), false);
    assert.deepEqual(step.write.ops, [
      {
        op: 'insert_flow',
        merge_scope: 'function_call',
        insert_strategy: 'insert_between',
        anchor: {
          block_id: 'BH_DoorFeature_ToggleDoor',
          group_entry_node_path: 'logic.groups[0].entry.node_path',
          node_ref: 'nodes[0]',
          pin_ref: 'Then',
          link_ref: 'links[0]',
        },
        inserted: {
          function: 'OpenDoor',
        },
      },
    ]);
  });

  it('requires branch_fork sequence_order and rejects sequence_order on other merge strategies', () => {
    const branchSpec = makeTaskSpec({
      behavior: {
        graph_strategy: 'merge_owned_graph',
        merges: [
          {
            kind: 'insert_flow',
            scope: 'custom_event_call',
            insert_strategy: 'branch_fork',
            anchor: {
              block_id: 'BH_DoorFeature_ToggleDoor',
              group_entry_node_path: 'logic.groups[0].entry.node_path',
              node_ref: 'nodes[0]',
              pin_ref: 'Then',
            },
            inserted: {
              call_kind: 'custom_event_call',
              name: 'OpenDoorEvent',
            },
            sequence_order: [
              'inserted_logic',
              'original_successor',
            ],
          },
        ],
      },
    });

    const branchPlan = compileTaskSpecToTaskPlan(TaskSpecSchema.parse(branchSpec));
    assert.deepEqual((branchPlan.steps[0] as Record<string, any>).write.ops[0], {
      op: 'insert_flow',
      merge_scope: 'custom_event_call',
      insert_strategy: 'branch_fork',
      anchor: {
        block_id: 'BH_DoorFeature_ToggleDoor',
        group_entry_node_path: 'logic.groups[0].entry.node_path',
        node_ref: 'nodes[0]',
        pin_ref: 'Then',
      },
      inserted: {
        custom_event: 'OpenDoorEvent',
      },
      sequence_order: [
        'inserted_logic',
        'original_successor',
      ],
    });

    const nonBranchSpec = makeTaskSpec({
      behavior: {
        graph_strategy: 'merge_owned_graph',
        merges: [
          {
            kind: 'insert_flow',
            scope: 'function_call',
            insert_strategy: 'append_after',
            anchor: {
              block_id: 'BH_DoorFeature_ToggleDoor',
              group_entry_node_path: 'logic.groups[0].entry.node_path',
              node_ref: 'nodes[0]',
              pin_ref: 'Then',
            },
            inserted: {
              call_kind: 'function_call',
              name: 'OpenDoor',
            },
            sequence_order: [
              'inserted_logic',
              'original_successor',
            ],
          },
        ],
      },
    });

    assert.throws(
      () => compileTaskSpecToTaskPlan(TaskSpecSchema.parse(nonBranchSpec)),
      /sequence_order/,
    );
  });

  it('rejects patch_owned_graph with bare nodes index anchors', () => {
    const spec = makeTaskSpec({
      behavior: {
        graph_strategy: 'patch_owned_graph',
        patches: [
          {
            kind: 'set_pin_default',
            target_ref: {
              graph_id: 'EventGraph',
              node_ref: 'nodes[0]',
              pin_ref: 'Condition',
            },
            value: true,
          },
        ],
      },
    });

    assert.throws(
      () => compileTaskSpecToTaskPlan(TaskSpecSchema.parse(spec)),
      (error: unknown) => error instanceof TaskSpecCompileError &&
        error.code === 'unsupported_graph_write_anchor' &&
        error.issues[0]?.path === 'behavior.patches[0].target_ref.node_ref',
    );
  });

  it('rejects merge_owned_graph with bare nodes index anchors', () => {
    const spec = makeTaskSpec({
      behavior: {
        graph_strategy: 'merge_owned_graph',
        merges: [
          {
            kind: 'insert_flow',
            scope: 'function_call',
            insert_strategy: 'insert_between',
            anchor: {
              graph_id: 'EventGraph',
              node_ref: 'nodes[0]',
              pin_ref: 'Then',
            },
            inserted: {
              call_kind: 'function_call',
              name: 'OpenDoor',
            },
          },
        ],
      },
    });

    assert.throws(
      () => compileTaskSpecToTaskPlan(TaskSpecSchema.parse(spec)),
      (error: unknown) => error instanceof TaskSpecCompileError &&
        error.code === 'unsupported_graph_write_anchor' &&
        error.issues[0]?.path === 'behavior.merges[0].anchor.node_ref',
    );
  });

  it('rejects replace selectors that do not match the replace scope', () => {
    const spec = makeTaskSpec({
      behavior: {
        graph_strategy: 'replace_owned_graph',
        replace: {
          scope: 'custom_event_body',
          selector: {
            kind: 'function',
            name: 'ToggleDoor',
          },
          body: {
            schema: 'BlueprintLogicSpec.v1',
            statements: [
              {
                kind: 'call_function',
                name: 'PrintString',
              },
            ],
          },
        },
      },
    });

    assert.throws(
      () => compileTaskSpecToTaskPlan(TaskSpecSchema.parse(spec)),
      /custom_event_body requires selector.kind/,
    );
  });

  it('rejects non-custom-event entries', () => {
    const spec = makeTaskSpec({
      behavior: {
        graph_strategy: 'append_new_owned_graph',
        entries: [
          {
            entry_type: 'function',
            name: 'OpenDoor',
            body: { schema: 'BlueprintLogicSpec.v1', statements: [] },
          },
        ],
      },
    });

    assert.throws(
      () => compileTaskSpecToTaskPlan(TaskSpecSchema.parse(spec)),
      (err: unknown) => {
        if (!(err instanceof TaskSpecCompileError)) return false;
        const compileError = err as TaskSpecCompileError;
        return compileError.code === 'unsupported_entry_type' &&
          compileError.issues[0]?.path === 'behavior.entries[0].entry_type';
      },
    );
  });

  it('compiles call_function statements into structured graph_write IR and lowers to append bridge payload', () => {
    const plan = compileTaskSpecToTaskPlan(TaskSpecSchema.parse(makeTaskSpec()));
    const payload = taskPlanToAppendBridgePayload(plan, true);
    const signatureStep = plan.steps[0] as Record<string, any>;
    const step = plan.steps[1] as Record<string, any>;

    assert.deepEqual(plan.execution_policy, {
      dry_run_mode: 'full',
      should_compile: false,
      should_save: false,
    });
    assert.equal(signatureStep.capability, 'blueprint_signature');
    assert.equal(signatureStep.write.ops[0].op, 'ensure_custom_event');
    assert.equal(Object.hasOwn(signatureStep, 'operation'), false);
    assert.ok(step && 'capability' in step);
    assert.equal(Object.hasOwn(step, 'operation'), false);
    assert.deepEqual(step, {
      step_id: 'step_002',
      capability: 'graph_write',
      target: {
        asset_path: '/Game/BP/BP_Door',
        graph: 'EG_DoorFeature',
      },
      write: {
        strategy: 'owned_graph_edit',
        ops: [
          {
            op: 'ensure_entry',
            entry_type: 'custom_event',
            name: 'ToggleDoor',
            body: {
              schema: 'BlueprintLogicSpec.v1',
              statements: [
                {
                  kind: 'call_function',
                  name: 'PrintString',
                  args: {
                    InString: {
                      kind: 'literal',
                      value_type: 'string',
                      value: 'hello',
                    },
                  },
                },
              ],
            },
          },
        ],
      },
      constraints: {
        allow_modify_user_nodes: false,
        ownership_scope: 'blueprinthelper_owned',
      },
      depends_on: ['step_001'],
    });

    assert.deepEqual(payload, {
      target: {
        asset_path: '/Game/BP/BP_Door',
        graph: 'EG_DoorFeature',
      },
      feature_name: 'DoorFeature',
      nodes: [
        { id: 'ToggleDoor_entry', kind: 'custom_event', name: 'ToggleDoor' },
        {
          id: 'ToggleDoor_stmt_1',
          kind: 'call',
          function: 'PrintString',
          inputs: { InString: 'hello' },
        },
      ],
      links: [
        { kind: 'exec', from: 'ToggleDoor_entry.then', to: 'ToggleDoor_stmt_1.execute' },
      ],
      dry_run: true,
    });
  });

  it('emits signature dependencies before graph_write ensure_entry ops for custom_event entries', () => {
    const spec = makeTaskSpec({
      behavior: {
        graph_strategy: 'append_new_owned_graph',
        entries: [
          {
            entry_type: 'custom_event',
            name: 'ToggleDoor',
            body: {
              schema: 'BlueprintLogicSpec.v1',
              statements: [
                {
                  kind: 'call_function',
                  name: 'PrintString',
                  args: {
                    InString: {
                      kind: 'literal',
                      value_type: 'string',
                      value: 'hello',
                    },
                  },
                },
              ],
            },
          },
          {
            entry_type: 'custom_event',
            name: 'CloseDoor',
            body: {
              schema: 'BlueprintLogicSpec.v1',
              statements: [
                {
                  kind: 'set_member_variable',
                  name: 'bDoorOpen',
                  value: {
                    kind: 'literal',
                    value_type: 'bool',
                    value: false,
                  },
                },
              ],
            },
          },
        ],
      },
    });

    const plan = compileTaskSpecToTaskPlan(TaskSpecSchema.parse(spec));
    const steps = plan.steps as Array<Record<string, unknown>>;
    const signatureSteps = steps.filter((step) => step.capability === 'blueprint_signature');
    const graphWriteStep = steps.find((step) => step.capability === 'graph_write');

    assert.deepEqual(signatureSteps.map((step) => ({
      step_id: step.step_id,
      strategy: ((step.write as Record<string, unknown>).strategy),
      op: (((step.write as Record<string, unknown>).ops as Array<Record<string, unknown>>)[0]).op,
      name: (((step.write as Record<string, unknown>).ops as Array<Record<string, unknown>>)[0]).event_name,
    })), [
      { step_id: 'step_001', strategy: 'custom_event_signature', op: 'ensure_custom_event', name: 'ToggleDoor' },
      { step_id: 'step_002', strategy: 'custom_event_signature', op: 'ensure_custom_event', name: 'CloseDoor' },
    ]);
    assert.ok(graphWriteStep);
    assert.deepEqual((graphWriteStep as Record<string, unknown>).depends_on, ['step_001', 'step_002']);
    assert.deepEqual(((graphWriteStep.write as Record<string, unknown>).ops as Array<Record<string, unknown>>).map((op) => ({
      op: op.op,
      name: op.name,
    })), [
      { op: 'ensure_entry', name: 'ToggleDoor' },
      { op: 'ensure_entry', name: 'CloseDoor' },
    ]);
  });

  it('compiles set_member_variable statements into append_blueprint_graph payload', () => {
    const spec = makeTaskSpec({
      behavior: {
        graph_strategy: 'append_new_owned_graph',
        entries: [
          {
            entry_type: 'custom_event',
            name: 'OpenDoor',
            body: {
              schema: 'BlueprintLogicSpec.v1',
              statements: [
                {
                  kind: 'set_member_variable',
                  name: 'bDoorOpen',
                  value: {
                    kind: 'literal',
                    value_type: 'bool',
                    value: true,
                  },
                },
              ],
            },
          },
        ],
      },
    });

    const plan = compileTaskSpecToTaskPlan(TaskSpecSchema.parse(spec));
    const payload = taskPlanToAppendBridgePayload(plan, false);

    assert.deepEqual(payload.nodes, [
      { id: 'OpenDoor_entry', kind: 'custom_event', name: 'OpenDoor' },
      {
        id: 'OpenDoor_stmt_1',
        kind: 'set',
        var: 'bDoorOpen',
        value: 'true',
      },
    ]);
    assert.equal(payload.dry_run, false);
  });
});

describe('TaskSpec Blueprint Variables compiler', () => {
  it('compiles ensure_member_variable into structured blueprint_variable IR', () => {
    const plan = compileTaskSpecToTaskPlan(TaskSpecSchema.parse(makeVariableTaskSpec()));
    const step = plan.steps[0];

    assert.deepEqual(plan.execution_policy, {
      dry_run_mode: 'full',
      should_compile: true,
      should_save: false,
    });
    assert.ok(step && 'capability' in step);
    assert.equal(Object.hasOwn(step as Record<string, unknown>, 'operation'), false);
    assert.deepEqual(step, {
      step_id: 'step_001',
      capability: 'blueprint_variable',
      target: {
        asset_path: '/Game/BP/BP_Door',
      },
      write: {
        strategy: 'member_variables',
        ops: [
          {
            op: 'ensure_member_variable',
            name: 'bDoorOpen',
            pin_type: {
              category: 'bool',
            },
            category: 'Door',
            flags: {
              expose_on_spawn: false,
            },
          },
        ],
      },
      constraints: {
        allow_remove_referenced_variables: false,
      },
    });
  });

  it('rejects unsupported variable strategies', () => {
    const spec = makeVariableTaskSpec({
      behavior: {
        variable_strategy: 'graph_reference_rewrite',
        variables: [
          {
            op: 'ensure_member_variable',
            name: 'TempValue',
            pin_type: { category: 'float' },
          },
        ],
      },
    });

    assert.throws(
      () => compileTaskSpecToTaskPlan(TaskSpecSchema.parse(spec)),
      (err: unknown) => {
        if (!(err instanceof TaskSpecCompileError)) return false;
        const compileError = err as TaskSpecCompileError;
        return compileError.code === 'unsupported_variable_strategy' &&
          compileError.issues[0]?.path === 'behavior.variable_strategy';
      },
    );
  });

  it('rejects unsupported variable ops', () => {
    const spec = makeVariableTaskSpec({
      behavior: {
        variable_strategy: 'member_variables',
        variables: [
          {
            op: 'remove_member_variable',
            name: 'bDoorOpen',
          },
        ],
      },
    });

    assert.throws(
      () => compileTaskSpecToTaskPlan(TaskSpecSchema.parse(spec)),
      (err: unknown) => {
        if (!(err instanceof TaskSpecCompileError)) return false;
        const compileError = err as TaskSpecCompileError;
        return compileError.code === 'unsupported_variable_op' &&
          compileError.issues[0]?.path === 'behavior.variables[0].op';
      },
    );
  });

  it('compiles semantic member variable changes into structured blueprint_variable IR', () => {
    const spec = makeVariableTaskSpec({
      behavior: {
        variable_strategy: 'member_variables',
        changes: [
          {
            kind: 'ensure_member_variable',
            name: 'Health',
            variable_type: { category: 'float' },
            category: 'Stats',
          },
          {
            kind: 'configure_member_variable',
            name: 'Health',
            properties: [
              {
                property_path: 'Tooltip',
                value: 'Current health.',
              },
            ],
          },
          {
            kind: 'remove_member_variable',
            name: 'DeprecatedHealth',
          },
        ],
      },
    });

    const plan = compileTaskSpecToTaskPlan(TaskSpecSchema.parse(spec));
    const step = plan.steps[0];

    assert.ok(step && 'capability' in step);
    assert.equal(Object.hasOwn(step as Record<string, unknown>, 'operation'), false);
    assert.deepEqual(step.write, {
      strategy: 'member_variables',
      ops: [
        {
          op: 'ensure_member_variable',
          name: 'Health',
          pin_type: { category: 'float' },
          category: 'Stats',
        },
        {
          op: 'set_member_variable_properties',
          name: 'Health',
          settings: [
            {
              property_path: 'Tooltip',
              value: 'Current health.',
            },
          ],
        },
        {
          op: 'remove_member_variable',
          name: 'DeprecatedHealth',
        },
      ],
    });
  });

  it('compiles member defaults and local variable changes into structured blueprint_variable IR', () => {
    const defaultsPlan = compileTaskSpecToTaskPlan(TaskSpecSchema.parse(makeVariableTaskSpec({
      behavior: {
        variable_strategy: 'member_defaults',
        defaults: [
          {
            name: 'Health',
            value: {
              kind: 'literal',
              value_type: 'float',
              value: 100,
            },
          },
        ],
      },
    })));
    const defaultsStep = defaultsPlan.steps[0];
    assert.ok(defaultsStep && 'capability' in defaultsStep);
    assert.deepEqual(defaultsStep.write, {
      strategy: 'member_defaults',
      ops: [
        {
          op: 'set_member_default',
          name: 'Health',
          value: 100,
        },
      ],
    });

    const localPlan = compileTaskSpecToTaskPlan(TaskSpecSchema.parse(makeVariableTaskSpec({
      behavior: {
        variable_strategy: 'local_variables',
        function_name: 'CalculateDamage',
        changes: [
          {
            kind: 'ensure_local_variable',
            name: 'DamageScale',
            variable_type: { category: 'float' },
          },
          {
            kind: 'remove_local_variable',
            name: 'OldDamageScale',
          },
        ],
      },
    })));
    const localStep = localPlan.steps[0];
    assert.ok(localStep && 'capability' in localStep);
    assert.deepEqual(localStep.target, {
      asset_path: '/Game/BP/BP_Door',
      function_name: 'CalculateDamage',
    });
    assert.deepEqual(localStep.write, {
      strategy: 'local_variables',
      ops: [
        {
          op: 'ensure_local_variable',
          function_name: 'CalculateDamage',
          name: 'DamageScale',
          pin_type: { category: 'float' },
        },
        {
          op: 'remove_local_variable',
          function_name: 'CalculateDamage',
          name: 'OldDamageScale',
        },
      ],
    });
  });
});
