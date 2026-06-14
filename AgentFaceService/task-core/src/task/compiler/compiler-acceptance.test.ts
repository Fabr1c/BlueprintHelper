import assert from 'node:assert/strict';
import test from 'node:test';

import type { TaskSpec } from '../schema/task-schemas.js';
import {
  blueprintVariableMemberChangesTaskSpecFixture,
  classSettingsTaskSpecFixture,
  componentTaskSpecFixture,
  createAssetTaskSpecFixture,
  dataTableTaskSpecFixture,
  graphWriteAppendTaskSpecFixture,
  graphWriteReplaceTaskSpecFixture,
  signatureTaskSpecFixture,
  widgetTaskSpecFixture,
} from '../fixtures/task-protocol.fixtures.js';
import {
  compileTaskSpecToTaskPlan,
  TaskSpecCompileError,
} from './task-compiler.js';
import {
  createDefaultTaskTypeCompilerRegistry,
} from './compilers/default-task-type-compilers.js';

test('compileTaskSpecToTaskPlan delegates every public task_type through registry', () => {
  const registry = createDefaultTaskTypeCompilerRegistry();
  const nonGraphWriteSpecs: TaskSpec[] = [
    createAssetTaskSpecFixture,
    blueprintVariableMemberChangesTaskSpecFixture,
    makeObjectPropertiesTaskSpec(),
    signatureTaskSpecFixture,
    classSettingsTaskSpecFixture,
    componentTaskSpecFixture,
    widgetTaskSpecFixture,
    dataTableTaskSpecFixture,
  ];

  for (const taskSpec of nonGraphWriteSpecs) {
    assert.equal(registry.has(taskSpec.task_type), true, `${taskSpec.task_type} must be registry-owned`);
    const plan = compileTaskSpecToTaskPlan(taskSpec);
    assert.equal(plan.schema, 'BlueprintHelper.TaskPlan.v1');
    assert.equal(plan.task_type, taskSpec.task_type);
  }
});

test('GraphWrite replace function body compiles through route registry', () => {
  const plan = compileTaskSpecToTaskPlan(makeReplaceFunctionBodyTaskSpec());
  const graphWriteStep = requireGraphWriteStep(plan);
  const write = graphWriteStep['write'] as { ops?: Array<Record<string, unknown>> };

  assert.equal(plan.schema, 'BlueprintHelper.TaskPlan.v1');
  assert.equal(plan.task_type, 'edit_blueprint_graph');
  assert.equal(write.ops?.some((op) => op['op'] === 'replace_body' && op['replace_scope'] === 'function_body'), true);
});

test('GraphWrite append owned graph compiles through operation compiler registry', () => {
  const plan = compileTaskSpecToTaskPlan(graphWriteAppendTaskSpecFixture);
  const graphWriteStep = requireGraphWriteStep(plan);
  const write = graphWriteStep['write'] as { ops?: Array<Record<string, unknown>> };

  assert.equal(plan.schema, 'BlueprintHelper.TaskPlan.v1');
  assert.equal(plan.task_type, 'edit_blueprint_graph');
  assert.equal(write.ops?.some((op) => op['op'] === 'ensure_entry'), true);
});

test('unsupported task_type reports registry-owned unsupported_task_type', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan({
      ...createAssetTaskSpecFixture,
      task_type: 'edit_unknown_public_surface',
    } as never),
    (error: unknown) => {
      assert.equal(error instanceof TaskSpecCompileError, true);
      const compileError = error as TaskSpecCompileError;
      assert.equal(compileError.code, 'unsupported_task_type');
      assert.equal(compileError.issues[0]?.path, 'task_type');
      return true;
    },
  );
});

test('unsupported graph route reports descriptor-owned unsupported route error', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan({
      ...graphWriteReplaceTaskSpecFixture,
      behavior: {
        graph_strategy: 'replace_owned_graph',
        replace: {
          scope: 'unsupported_body',
          selector: {
            kind: 'function',
            name: 'UnsupportedBody',
          },
          body: {
            schema: 'BlueprintLogicSpec.v1',
            statements: [{
              kind: 'call',
              target: 'PrintString',
              args: {
                InString: { kind: 'literal', value_type: 'string', value: 'blocked' },
              },
            }],
          },
        },
      },
    } as never),
    (error: unknown) => {
      assert.equal(error instanceof TaskSpecCompileError, true);
      const compileError = error as TaskSpecCompileError;
      assert.equal(compileError.code, 'unsupported_graphwrite_route');
      assert.equal(compileError.issues[0]?.path, 'behavior.graph_strategy');
      return true;
    },
  );
});

function makeObjectPropertiesTaskSpec(): TaskSpec {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_p7_object_properties',
    task_type: 'edit_object_properties',
    feature_name: 'P7ObjectProperties',
    target: {
      asset_path: '/Game/Data/DA_P7Object',
      target_type: 'data_asset',
    },
    behavior: {
      property_strategy: 'property_edit',
      changes: [{
        property_path: 'DisplayName',
        value: 'P7',
      }],
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
  } as TaskSpec;
}

function makeReplaceFunctionBodyTaskSpec(returnStatement?: Record<string, unknown>): TaskSpec {
  return {
    ...graphWriteReplaceTaskSpecFixture,
    behavior: {
      graph_strategy: 'replace_owned_graph',
      replace: {
        scope: 'function_body',
        selector: {
          kind: 'function',
          name: 'ComputeP7Value',
        },
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements: [returnStatement ?? {
            kind: 'control',
            control: 'return',
            outputs: {
              bCompleted: { kind: 'get', target: 'bCompleted' },
              bIsNewRecord: { kind: 'get', target: 'bIsNewRecord' },
            },
          }],
        },
      },
    },
  } as TaskSpec;
}

function requireGraphWriteStep(plan: { steps: unknown[] }): Record<string, unknown> {
  const step = plan.steps.find((entry) => (entry as Record<string, unknown>)['capability'] === 'graph_write');
  assert.ok(step, 'Expected TaskPlan to include graph_write step.');
  return step as Record<string, unknown>;
}
