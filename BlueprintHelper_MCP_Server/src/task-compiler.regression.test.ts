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
      status: 'completed',
      target_assets: ['/Game/BP/BP_Door'],
      steps: [{
        step_id: 'step_append_graph',
        operation: 'append_blueprint_graph',
        status: 'applied',
      }],
      bridge_result: {},
    }));
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
              graph_id: 'EventGraph',
              node_ref: 'Branch0',
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
              group_entry_node_path: 'logic.groups[0].entry.node_path',
              node_ref: 'BeginPlay0',
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
    const step = plan.steps[0];

    assert.deepEqual(plan.execution_policy, {
      dry_run_mode: 'full',
      should_compile: false,
      should_save: false,
    });
    assert.ok(step && 'capability' in step);
    assert.equal(Object.hasOwn(step as Record<string, unknown>, 'operation'), false);
    assert.deepEqual(step, {
      step_id: 'step_001',
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

  it('emits one ensure_entry op per custom_event entry', () => {
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
    const step = plan.steps[0];

    assert.ok(step && 'capability' in step);
    assert.deepEqual(step.write.ops.map((op) => ({
      op: op.op,
      name: (op as Record<string, unknown>).name,
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
