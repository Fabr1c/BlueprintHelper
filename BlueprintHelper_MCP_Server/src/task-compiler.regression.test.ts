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

  it('compiles call_function statements into append_blueprint_graph payload', () => {
    const plan = compileTaskSpecToTaskPlan(TaskSpecSchema.parse(makeTaskSpec()));
    const payload = taskPlanToAppendBridgePayload(plan, true);

    assert.deepEqual(plan.execution_policy, {
      dry_run_mode: 'full',
      should_compile: false,
      should_save: false,
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
