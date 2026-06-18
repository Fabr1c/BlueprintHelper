import assert from 'node:assert/strict';
import test from 'node:test';

import { compileTaskSpecToTaskPlan } from './task-compiler.js';
import { getGraphWriteSlotsForRoute } from './graphwrite/graphwrite-slot-registry.js';
import { requireGraphWriteRouteByScope } from './graphwrite/graphwrite-route-registry.js';

function requireGraphWriteStep(plan: ReturnType<typeof compileTaskSpecToTaskPlan>): Record<string, unknown> {
  const step = plan.steps.find((candidate) => (candidate as Record<string, unknown>).capability === 'graph_write') as Record<string, unknown> | undefined;
  assert.ok(step);
  return step;
}

test('function_body route stays scoped to function graphs', () => {
  const route = requireGraphWriteRouteByScope('replace_owned_graph', 'function_body');
  assert.equal(route.route_id, 'graph.replace.function_body');
  assert.equal(route.runtime_adapter_id, 'k2.function_body');
  assert.equal(route.selector?.expected_kind, 'function');
});

test('function param slot is visible only on function_body', () => {
  const functionSlots = getGraphWriteSlotsForRoute('graph.replace.function_body', 'expression');
  const customEventSlots = getGraphWriteSlotsForRoute('graph.append.custom_event', 'expression');

  assert.equal(functionSlots.some((slot) => slot.slot_id === 'graph.expression.get.function_param'), true);
  assert.equal(customEventSlots.some((slot) => slot.slot_id === 'graph.expression.get.function_param'), false);
});

test('function body task spec lowers to function_body replace scope', () => {
  const plan = compileTaskSpecToTaskPlan({
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_function_body_route',
    task_type: 'edit_blueprint_graph',
    feature_name: 'FunctionBodyRoute',
    target: { asset_path: '/Game/BH/BP_FunctionBody', target_type: 'blueprint' },
    scope_policy: { graph_name: 'ComputeValue', allow_modify_user_nodes: false },
    behavior: {
      graph_strategy: 'replace_owned_graph',
      replace: {
        scope: 'function_body',
        selector: { kind: 'function', name: 'ComputeValue' },
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements: [
            {
              kind: 'control',
              control: 'return',
              value: {
                kind: 'field',
                capability_id: 'field.function_param_get',
                field_operation: 'get',
                field_scope: 'variable',
                target: 'InputValue',
                function_name: 'ComputeValue',
                pin_type: { category: 'int' },
                capability_facts: {
                  'field.member_name': 'InputValue',
                  'field.function_name': 'ComputeValue',
                },
              },
            },
          ],
        },
      },
    },
  } as never);
  const graphWriteStep = requireGraphWriteStep(plan);
  const write = graphWriteStep.write as { ops?: Array<Record<string, unknown>> };

  assert.equal(plan.schema, 'BlueprintHelper.TaskPlan.v1');
  assert.equal(write.ops?.some((op) => op.op === 'replace_body' && op.replace_scope === 'function_body'), true);
});
