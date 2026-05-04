import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { TaskSpecSchema } from './task-schemas.js';
import {
  PythonTaskOrchestratorError,
  compileGraphWriteAppendWithPython,
} from './task-python-orchestrator.js';

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

describe('Python task orchestrator adapter', () => {
  it('compiles GraphWrite Append TaskSpec through Python into a TaskPlan and Bridge payload', async () => {
    const result = await compileGraphWriteAppendWithPython(TaskSpecSchema.parse(makeTaskSpec()), true);

    assert.equal(result.schema, 'BlueprintHelper.TaskCompilerResult.v1');
    assert.equal(result.task_plan.schema, 'BlueprintHelper.TaskPlan.v1');
    assert.deepEqual(result.task_plan.execution_policy, {
      dry_run_mode: 'full',
      should_compile: false,
      should_save: false,
    });
    assert.deepEqual(result.bridge_payload, {
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

  it('maps Python semantic errors into TaskSpec compile errors', async () => {
    const spec = TaskSpecSchema.parse(makeTaskSpec({
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
    }));

    await assert.rejects(
      () => compileGraphWriteAppendWithPython(spec, true),
      (err: unknown) => {
        if (!(err instanceof PythonTaskOrchestratorError)) return false;
        const compileError = err as PythonTaskOrchestratorError;
        return compileError.code === 'unsupported_graph_strategy' &&
          compileError.issues[0]?.path === 'behavior.graph_strategy';
      },
    );
  });
});
