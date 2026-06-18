import { strict as assert } from 'node:assert';
import test from 'node:test';

import { GraphWriteTaskSpecSchema } from './task-schemas.js';

function makeReplaceSpec(replace: Record<string, unknown>): Record<string, unknown> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_graphwrite_route_descriptor_schema_ts',
    task_type: 'edit_blueprint_graph',
    feature_name: 'GraphWriteRouteDescriptorSchemaTs',
    target: {
      asset_path: '/Game/BP/BP_GraphWriteRouteDescriptorSchema',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'replace_owned_graph',
      replace: {
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements: [{
            kind: 'call',
            target: 'PrintString',
          }],
        },
        ...replace,
      },
    },
  };
}

test('GraphWrite replace schema accepts descriptor-backed hidden custom_event_definition scope', () => {
  const result = GraphWriteTaskSpecSchema.safeParse(makeReplaceSpec({
    scope: 'custom_event_definition',
    selector: {
      kind: 'custom_event',
      name: 'OnDescriptorRoute',
    },
  }));

  assert.equal(result.success, true, JSON.stringify(result, null, 2));
});

test('GraphWrite replace schema accepts active macro_body descriptor scope', () => {
  const result = GraphWriteTaskSpecSchema.safeParse(makeReplaceSpec({
    scope: 'macro_body',
    selector: {
      kind: 'macro',
      name: 'RuntimeBackedMacro',
    },
  }));

  assert.equal(result.success, true, JSON.stringify(result, null, 2));
});

test('GraphWrite replace schema validates selector kind from descriptor metadata', () => {
  const result = GraphWriteTaskSpecSchema.safeParse(makeReplaceSpec({
    scope: 'function_body',
    selector: {
      kind: 'event',
      name: 'ComputeDescriptorRoute',
    },
  }));

  assert.equal(result.success, false, JSON.stringify(result, null, 2));
  if (!result.success) {
    assert.match(JSON.stringify(result.error.issues), /function_body requires selector.kind=\\"function\\"/);
  }
});
