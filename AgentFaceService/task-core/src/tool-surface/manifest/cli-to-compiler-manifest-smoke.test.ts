import assert from 'node:assert/strict';
import test from 'node:test';

import { compileTaskSpecToTaskPlan } from '../../task/compiler/task-compiler.js';
import { getToolTemplateDispatch } from '../tool-registry.js';
import {
  buildReadonlyToolCommandManifestRegistry,
} from './tool-command-manifest-builder.js';

test('preview manifest route compiles minimal append container_action TaskSpec locally', () => {
  const manifest = buildReadonlyToolCommandManifestRegistry().require('blueprint.plan.taskspec.preview');
  assert.equal(manifest.route_refs.includes('graph.append.container_action'), true);

  const taskPlan = compileTaskSpecToTaskPlan({
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_manifest_append_container',
    task_type: 'edit_blueprint_graph',
    feature_name: 'ManifestAppendContainer',
    target: {
      asset_path: '/Game/BH_Tests/BP_ManifestAppend',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [{
        entry_type: 'custom_event',
        name: 'BH_ManifestAppendContainer',
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements: [{
            kind: 'container_action',
            container_kind: 'array',
            container_operation: 'add',
            target: { kind: 'get', name: 'Items' },
            item: { kind: 'literal', value_type: 'number', value: 1 },
            element_type: 'int',
          }],
        },
      }],
    },
    execution_policy: {
      dry_run_mode: 'full',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
  } as never);

  assert.equal(taskPlan.schema, 'BlueprintHelper.TaskPlan.v1');
  const graphWriteStep = requireStepWithCapability(taskPlan, 'graph_write');
  const write = graphWriteStep['write'] as { ops?: Array<Record<string, unknown>> };
  assert.equal(write.ops?.some((op) => {
    const body = op['body'] as { statements?: Array<Record<string, unknown>> } | undefined;
    return op['op'] === 'ensure_entry'
      && op['entry_type'] === 'custom_event'
      && body?.statements?.some((statement) => statement['kind'] === 'container_action');
  }), true);
});

test('execute manifest route compiles minimal replace function body TaskSpec locally', () => {
  const manifest = buildReadonlyToolCommandManifestRegistry().require('blueprint.write.taskspec.execute');
  assert.equal(manifest.route_refs.includes('graph.replace.function_body'), true);

  const taskPlan = compileTaskSpecToTaskPlan({
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_manifest_replace_function',
    task_type: 'edit_blueprint_graph',
    feature_name: 'ManifestReplaceFunction',
    target: {
      asset_path: '/Game/BH_Tests/BP_ManifestReplace',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'replace_owned_graph',
      replace: {
        scope: 'function_body',
        selector: {
          kind: 'function',
          name: 'ComputeManifestValue',
        },
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements: [{
            kind: 'control',
            control: 'return',
            values: {
              ReturnValue: { kind: 'literal', value_type: 'number', value: 1 },
            },
          }],
        },
      },
    },
    execution_policy: {
      dry_run_mode: 'full',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
  } as never);

  assert.equal(taskPlan.schema, 'BlueprintHelper.TaskPlan.v1');
  const graphWriteStep = requireStepWithCapability(taskPlan, 'graph_write');
  const write = graphWriteStep['write'] as { ops?: Array<Record<string, unknown>> };
  assert.equal(write.ops?.some((op) => op['op'] === 'replace_body' && op['replace_scope'] === 'function_body'), true);
});

test('preview manifest route compiles minimal create_blueprint_feature TaskSpec locally', () => {
  const manifest = buildReadonlyToolCommandManifestRegistry().require('blueprint.plan.taskspec.preview');
  assert.equal(manifest.route_refs.includes('blueprint.create_feature'), true);

  const taskPlan = compileTaskSpecToTaskPlan({
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_manifest_create_feature',
    task_type: 'create_blueprint_feature',
    feature_name: 'ManifestCreateFeature',
    target: {
      asset_path: '/Game/BH_Tests/BP_ManifestCreate',
      target_type: 'blueprint',
    },
    variables: [{
      name: 'ManifestValue',
      type: 'int',
      default: 1,
    }],
    execution_policy: {
      dry_run_mode: 'full',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
  } as never);

  assert.equal(taskPlan.schema, 'BlueprintHelper.TaskPlan.v1');
  assert.ok(taskPlan.steps.length > 0);
  assert.equal(
    taskPlan.steps.some((step) => (step as Record<string, unknown>)['capability'] === 'blueprint_variable'),
    true,
  );
});

test('unknown template route still fails through catalog dispatch', () => {
  assert.throws(
    () => getToolTemplateDispatch('blueprint.write.taskspec.execute', { route: 'graph.unknown.route' }),
    /Unknown BlueprintHelper template route/,
  );
});

function requireStepWithCapability(
  taskPlan: { steps: unknown[] },
  capability: string,
): Record<string, unknown> {
  const step = taskPlan.steps.find((entry) => (entry as Record<string, unknown>)['capability'] === capability);
  assert.ok(step, `Expected TaskPlan to contain capability: ${capability}`);
  return step as Record<string, unknown>;
}
