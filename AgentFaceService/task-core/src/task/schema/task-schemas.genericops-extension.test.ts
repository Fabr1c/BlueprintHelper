import assert from 'node:assert/strict';
import { describe, it } from 'node:test';

import {
  GENERIC_OPS_EVIDENCE_KEYS,
  GENERIC_OPS_FORBIDDEN_RUNTIME_CLUSTER_IDS,
  GENERIC_OPS_OPERATION_GROUP_IDS,
  GENERIC_OPS_OPERATION_IDS,
  GRAPHWRITE_CAPABILITY_CONTRACT,
  GraphWriteTaskSpecSchema,
} from './task-schemas.js';

describe('GraphWrite GenericOps task schema exports', () => {
  it('exports logical groups without leaking runtime clusters', () => {
    const operationGroupIds: readonly string[] = GENERIC_OPS_OPERATION_GROUP_IDS;

    assert.ok(GENERIC_OPS_OPERATION_GROUP_IDS.includes('generic_ops.control'));
    assert.ok(GENERIC_OPS_OPERATION_GROUP_IDS.includes('generic_ops.transform'));
    assert.ok(GENERIC_OPS_OPERATION_GROUP_IDS.includes('generic_ops.create'));
    assert.ok(GENERIC_OPS_OPERATION_GROUP_IDS.includes('generic_ops.struct_select'));
    assert.equal(operationGroupIds.includes('generic_ops.container'), false);
    assert.equal(operationGroupIds.includes('generic_ops.schedule'), false);

    const runtimeClusterIds: readonly string[] = GRAPHWRITE_CAPABILITY_CONTRACT.clusters.map((cluster) => cluster.id);
    for (const forbidden of GENERIC_OPS_FORBIDDEN_RUNTIME_CLUSTER_IDS) {
      assert.equal(runtimeClusterIds.includes(forbidden), false, forbidden);
    }
  });

  it('exports representative operation ids and context evidence keys', () => {
    const operationIds: readonly string[] = GENERIC_OPS_OPERATION_IDS;
    const evidenceKeys: readonly string[] = GENERIC_OPS_EVIDENCE_KEYS;

    assert.ok(GENERIC_OPS_OPERATION_IDS.includes('generic_ops.control.switch_enum'));
    assert.ok(GENERIC_OPS_OPERATION_IDS.includes('generic_ops.control.multi_gate'));
    assert.ok(GENERIC_OPS_OPERATION_IDS.includes('generic_ops.control.do_once'));
    assert.ok(GENERIC_OPS_OPERATION_IDS.includes('generic_ops.transform.dynamic_cast'));
    assert.ok(GENERIC_OPS_OPERATION_IDS.includes('generic_ops.transform.type_promotion'));
    assert.ok(GENERIC_OPS_OPERATION_IDS.includes('generic_ops.create.spawn_actor'));
    assert.ok(GENERIC_OPS_OPERATION_IDS.includes('generic_ops.struct_select.select'));

    assert.ok(GENERIC_OPS_EVIDENCE_KEYS.includes('generic.control.operation'));
    assert.ok(GENERIC_OPS_EVIDENCE_KEYS.includes('generic.control.enum_path'));
    assert.ok(GENERIC_OPS_EVIDENCE_KEYS.includes('generic.macro.pin_shape_snapshot'));
    assert.ok(GENERIC_OPS_EVIDENCE_KEYS.includes('generic.create.expose_on_spawn'));
    assert.ok(GENERIC_OPS_EVIDENCE_KEYS.includes('generic.select.result_type_proof'));
    assert.equal(operationIds.some((id) => id.startsWith('generic_ops.container.')), false);
    assert.equal(operationIds.includes('generic_ops.transform.function_conversion'), false);
    assert.equal(operationIds.includes('generic_ops.create.asset_action'), false);
    assert.equal(operationIds.includes('generic_ops.create.function_backed_create'), false);
    assert.equal(operationIds.some((id) => id.startsWith('generic_ops.schedule.')), false);
    assert.equal(operationIds.includes('generic_ops.schedule.timer_delegate_node'), false);
    assert.equal(operationIds.includes('generic_ops.schedule.latent_or_async_node'), false);
    assert.equal(operationIds.includes('generic_ops.struct_select.set_fields_in_struct'), false);
    assert.equal(evidenceKeys.includes('container.collection_pin_type'), false);
    assert.equal(evidenceKeys.includes('generic.create.asset_path'), false);
    assert.equal(evidenceKeys.includes('generic.schedule.graph_latent_allowed'), false);
  });

  it('does not export UE node class names as GenericOps evidence keys', () => {
    for (const key of GENERIC_OPS_EVIDENCE_KEYS) {
      assert.equal(key.includes('UK2Node_'), false, key);
      assert.equal(key.includes('/Script/BlueprintGraph.'), false, key);
    }
  });

  it('accepts representative GenericOps TaskSpec shapes without top-level kind expansion', () => {
    const taskSpec = {
      schema: 'BlueprintHelper.TaskSpec.v1',
      task_type: 'edit_blueprint_graph',
      target: { asset_path: '/Game/Test/BP_GenericOps' },
      scope_policy: { graph_name: 'EventGraph' },
      behavior: {
        graph_strategy: 'append_new_owned_graph',
        entries: [{
          entry_type: 'custom_event',
          name: 'RunGenericOps',
          body: {
            schema: 'BlueprintLogicSpec.v2',
            statements: [{
              id: 'stmt_switch',
              kind: 'control',
              control_operation: 'switch_enum',
              context_evidence: {
                'generic.control.operation': 'switch_enum',
                'generic.control.case_values': 'Idle,Running',
                'generic.control.enum_path': '/Script/Engine.EEndPlayReason',
              },
            }, {
              id: 'stmt_array_add',
              kind: 'container_action',
              container_kind: 'array',
              container_operation: 'add',
              target: { kind: 'get', name: 'Items' },
              item: { kind: 'literal', value_type: 'int', value: 7 },
              element_type: 'int',
            }],
          },
        }],
      },
    };

    assert.equal(GraphWriteTaskSpecSchema.safeParse(taskSpec).success, true);
  });

  it('publishes required evidence failures through the contract rather than UE class fields', () => {
    const switchEnum = GRAPHWRITE_CAPABILITY_CONTRACT.operationGroups
      .flatMap((group) => group.operations)
      .find((operation) => operation.id === 'generic_ops.control.switch_enum');
    assert.ok(switchEnum);
    assert.ok(switchEnum.requiredEvidenceKeys?.includes('generic.control.case_values'));
    assert.ok(switchEnum.requiredEvidenceKeys?.includes('generic.control.enum_path'));
    assert.ok(switchEnum.requiredEvidenceKeys?.includes('generic.control.default_policy'));

    const doOnce = GRAPHWRITE_CAPABILITY_CONTRACT.operationGroups
      .flatMap((group) => group.operations)
      .find((operation) => operation.id === 'generic_ops.control.do_once');
    assert.ok(doOnce);
    assert.ok(doOnce.requiredEvidenceKeys?.includes('generic.macro.graph_path'));
    assert.ok(doOnce.requiredEvidenceKeys?.includes('generic.macro.pin_shape_snapshot'));
  });

  it('publishes rejected GenericOps with stable excluded reasons', () => {
    const rejected = new Map(
      GRAPHWRITE_CAPABILITY_CONTRACT.operationGroups
        .flatMap((group) => group.operations)
        .filter((operation) => operation.supportStatus === 'rejected' && operation.id.startsWith('generic_ops.'))
        .map((operation) => [operation.id, operation.excludedReason]),
    );

    assert.equal(
      rejected.get('generic_ops.transform.link_time_auto_conversion'),
      'link_time_auto_conversion_requires_linker_readback',
    );
    assert.equal(
      rejected.get('generic_ops.struct_select.split_pin'),
      'split_pin_is_not_a_graphwrite_statement_operation',
    );
    assert.equal(
      rejected.get('generic_ops.struct_select.recombine_pin'),
      'recombine_pin_is_not_a_graphwrite_statement_operation',
    );
  });
});
