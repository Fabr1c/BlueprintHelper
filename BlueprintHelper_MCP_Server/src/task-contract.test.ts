import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import {
  TASK_CONTEXT_PACK_SCHEMA,
  TASK_PLAN_SCHEMA,
  TASK_RUN_JOURNAL_SCHEMA,
  TASK_SPEC_SCHEMA,
} from './task-schemas.js';
import {
  TASK_PROTOCOL_CONTRACT_V1,
  TASK_PROTOCOL_CONTRACT_VERSION,
} from './task-contract.js';
import {
  graphWriteAppendExpectedTaskPlanFixture,
  graphWriteAppendTaskSpecFixture,
} from './task-protocol.fixtures.js';

describe('TaskSpec/TaskPlan protocol contract', () => {
  it('publishes one versioned contract for the TaskSpec-first surface', () => {
    assert.equal(TASK_PROTOCOL_CONTRACT_VERSION, 'BlueprintHelper.TaskProtocolContract.v1');
    assert.equal(TASK_PROTOCOL_CONTRACT_V1.schema, TASK_PROTOCOL_CONTRACT_VERSION);
    assert.equal(TASK_PROTOCOL_CONTRACT_V1.task_spec_schema, TASK_SPEC_SCHEMA);
    assert.equal(TASK_PROTOCOL_CONTRACT_V1.task_plan_schema, TASK_PLAN_SCHEMA);
    assert.equal(TASK_PROTOCOL_CONTRACT_V1.context_pack_schema, TASK_CONTEXT_PACK_SCHEMA);
    assert.equal(TASK_PROTOCOL_CONTRACT_V1.task_run_journal_schema, TASK_RUN_JOURNAL_SCHEMA);

    assert.deepEqual(TASK_PROTOCOL_CONTRACT_V1.agent_facing_tools, [
      'blueprinthelper_get_runtime_profile',
      'blueprinthelper_diagnostics',
      'blueprinthelper_read_task_context',
      'blueprinthelper_preview_task',
      'blueprinthelper_execute_task',
      'blueprinthelper_get_task_result',
    ]);
  });

  it('keeps Agent-owned TaskSpec fields separate from compiler-owned TaskPlan fields', () => {
    assert.deepEqual(TASK_PROTOCOL_CONTRACT_V1.ownership, {
      agent_writes: ['BlueprintHelper.TaskSpec.v1'],
      compiler_writes: ['BlueprintHelper.TaskPlan.v1'],
      runtime_writes: ['BlueprintHelper.TaskRunJournal.v1'],
      agent_must_not_write: ['BlueprintHelper.TaskPlan.v1'],
    });

    assert.deepEqual(TASK_PROTOCOL_CONTRACT_V1.task_spec_required_paths, [
      'schema',
      'task_type',
      'target.asset_path',
      'scope_policy.graph_name',
      'scope_policy.allow_modify_user_nodes',
      'behavior.graph_strategy',
      'behavior.entries[] for append_new_owned_graph',
      'behavior.replace for replace_owned_graph',
      'behavior.patches[] for patch_owned_graph',
      'behavior.merges[] for merge_owned_graph',
      'validation.should_compile',
      'validation.should_save',
    ]);

    assert.deepEqual(TASK_PROTOCOL_CONTRACT_V1.task_plan_required_paths, [
      'schema',
      'task_type',
      'target_assets[]',
      'execution_policy.dry_run_mode',
      'execution_policy.should_compile',
      'execution_policy.should_save',
      'steps[].step_id',
      'steps[].capability',
      'steps[].target.asset_path',
      'steps[].target.graph',
      'steps[].write.strategy',
      'steps[].write.ops[]',
      'steps[].constraints.allow_modify_user_nodes',
      'steps[].constraints.ownership_scope',
    ]);
  });

  it('pins the supported values for the first slice', () => {
    assert.deepEqual(TASK_PROTOCOL_CONTRACT_V1.supported_first_slice, {
      task_type: 'edit_blueprint_graph',
      target_type: 'blueprint',
      graph_strategies: [
        'append_new_owned_graph',
        'replace_owned_graph',
        'patch_owned_graph',
        'merge_owned_graph',
      ],
      entry_types: ['custom_event'],
      statement_kinds: ['call_function', 'set_member_variable'],
      task_plan_capability: 'graph_write',
      task_plan_dependency_capabilities: ['blueprint_signature'],
      runtime_lowering_adapters: [
        'append_blueprint_graph',
        'replace_blueprint_graph',
        'patch_blueprint_graph',
        'merge_blueprint_graph',
      ],
      step_batching: 'append custom_event entries compile signature dependency steps before one graph_write body step; replace/patch/merge compile to one structural op per step',
    });

    assert.deepEqual(TASK_PROTOCOL_CONTRACT_V1.supported_second_slice, {
      task_type: 'edit_blueprint_variables',
      target_type: 'blueprint',
      variable_strategies: ['member_variables', 'member_defaults', 'local_variables'],
      agent_semantic_paths: [
        'behavior.variable_strategy',
        'behavior.changes[].kind',
        'behavior.defaults[].name',
        'behavior.defaults[].value',
        'behavior.function_name',
      ],
      forbidden_agent_fields: [
        'steps[].operation',
        'write.ops[].op',
        'adapter_operation',
      ],
      task_plan_capability: 'blueprint_variable',
      compiler_supported_structural_ops: [
        'ensure_member_variable',
        'set_member_variable_properties',
        'remove_member_variable',
        'set_member_default',
        'ensure_local_variable',
        'set_local_variable_properties',
        'remove_local_variable',
      ],
      runtime_supported_structural_ops: [
        'ensure_member_variable',
        'set_member_variable_properties',
        'remove_member_variable',
        'set_member_default',
        'ensure_local_variable',
        'set_local_variable_properties',
        'remove_local_variable',
      ],
      runtime_lowering_pending_ops: [],
      runtime_service_pending_ops: [
        'ensure_local_variable',
        'set_local_variable_properties',
        'remove_local_variable',
      ],
      runtime_lowering_adapters: ['add_blueprint_member_variables', 'blueprint_variable_batch'],
      adapter_dry_run_supported: false,
      max_task_plan_steps: 1,
    });

    assert.deepEqual(
      TASK_PROTOCOL_CONTRACT_V1.supported_p1_slices.map((slice) => slice.task_type),
      [
        'create_asset',
        'edit_blueprint_components',
        'edit_blueprint_class_settings',
        'edit_umg_widget',
        'edit_data_table',
      ],
    );
    assert.deepEqual(
      TASK_PROTOCOL_CONTRACT_V1.supported_p1_slices.map((slice) => slice.task_plan_capability),
      [
        'asset_factory',
        'blueprint_component',
        'blueprint_class_settings',
        'umg_widget',
        'data_table',
      ],
    );

    assert.deepEqual(TASK_PROTOCOL_CONTRACT_V1.supported_composite_slice, {
      task_type: 'create_blueprint_feature',
      target_type: 'blueprint',
      compiler_role: 'decompose semantic feature TaskSpec into existing capability TaskPlan steps',
      supported_sections: [
        'components[]',
        'variables[]',
        'class_settings.implemented_interfaces[]',
        'class_settings.class_defaults',
        'behavior',
        'integration.interface',
      ],
      emitted_taskplan_capabilities: [
        'blueprint_component',
        'blueprint_variable',
        'blueprint_class_settings',
        'blueprint_signature',
        'graph_write',
      ],
      supported_integration_semantics: [
        'integration.interface.interface_asset',
        'integration.interface.function',
        'integration.interface.implementation.call',
        'integration.interface.implementation.body',
      ],
      rejected_semantics: [
        'integration.input',
        'scope_policy.allow_create_assets=true',
      ],
      forbidden_agent_fields: [
        'steps[].operation',
        'write.ops[].op',
        'adapter_operation',
      ],
    });
  });

  it('separates Blueprint Variable compiler IR lowering from remaining pending UE service execution', () => {
    const secondSlice = TASK_PROTOCOL_CONTRACT_V1.supported_second_slice;
    assert.deepEqual(secondSlice.agent_semantic_paths, [
      'behavior.variable_strategy',
      'behavior.changes[].kind',
      'behavior.defaults[].name',
      'behavior.defaults[].value',
      'behavior.function_name',
    ]);
    assert.deepEqual(secondSlice.forbidden_agent_fields, [
      'steps[].operation',
      'write.ops[].op',
      'adapter_operation',
    ]);
    assert.deepEqual(secondSlice.compiler_supported_structural_ops, [
      'ensure_member_variable',
      'set_member_variable_properties',
      'remove_member_variable',
      'set_member_default',
      'ensure_local_variable',
      'set_local_variable_properties',
      'remove_local_variable',
    ]);
    assert.deepEqual(secondSlice.runtime_supported_structural_ops, [
      'ensure_member_variable',
      'set_member_variable_properties',
      'remove_member_variable',
      'set_member_default',
      'ensure_local_variable',
      'set_local_variable_properties',
      'remove_local_variable',
    ]);
    assert.deepEqual(secondSlice.runtime_lowering_pending_ops, []);
    assert.deepEqual(secondSlice.runtime_service_pending_ops, [
      'ensure_local_variable',
      'set_local_variable_properties',
      'remove_local_variable',
    ]);

    assert.deepEqual(TASK_PROTOCOL_CONTRACT_V1.blueprint_variable_taskplan_ir_contract.supported_write_strategies, [
      'member_variables',
      'member_defaults',
      'local_variables',
    ]);
    assert.deepEqual(
      TASK_PROTOCOL_CONTRACT_V1.blueprint_variable_taskplan_ir_contract.compiler_supported_structural_ops,
      secondSlice.compiler_supported_structural_ops,
    );
    assert.deepEqual(
      TASK_PROTOCOL_CONTRACT_V1.blueprint_variable_taskplan_ir_contract.runtime_supported_structural_ops,
      secondSlice.runtime_supported_structural_ops,
    );
    assert.match(
      TASK_PROTOCOL_CONTRACT_V1.blueprint_variable_taskplan_ir_contract.lowering_policy,
      /blueprint_variable_batch/,
    );
  });

  it('keeps P1 Agent-facing fields semantic and forbids TaskPlan adapter terms there', () => {
    const p1ByType = Object.fromEntries(
      TASK_PROTOCOL_CONTRACT_V1.supported_p1_slices.map((slice) => [slice.task_type, slice]),
    );

    assert.deepEqual(p1ByType.create_asset.agent_semantic_paths, [
      'behavior.asset_strategy=ensure_asset',
      'behavior.asset.collision_policy',
    ]);
    assert.deepEqual(p1ByType.create_asset.forbidden_agent_fields, [
      'behavior.asset_strategy=asset_create',
      'behavior.asset.collision',
      'steps[].operation',
      'write.ops[].op',
    ]);

    assert.deepEqual(p1ByType.edit_data_table.agent_semantic_paths, [
      'behavior.row_strategy=row_edit',
      'behavior.rows[].action',
    ]);
    assert.deepEqual(p1ByType.edit_data_table.forbidden_agent_fields, [
      'behavior.rows[].op',
      'steps[].operation',
      'write.ops[].op',
    ]);

    for (const slice of TASK_PROTOCOL_CONTRACT_V1.supported_p1_slices) {
      assert.equal(slice.forbidden_agent_fields.includes('steps[].operation'), true, slice.task_type);
      assert.equal(slice.forbidden_agent_fields.includes('write.ops[].op'), true, slice.task_type);
    }

    assert.deepEqual(TASK_PROTOCOL_CONTRACT_V1.supported_composite_slice.forbidden_agent_fields, [
      'steps[].operation',
      'write.ops[].op',
      'adapter_operation',
    ]);
  });

  it('maps TaskPlan capabilities to v0.3.6 DoneImplementation sources', () => {
    assert.deepEqual(TASK_PROTOCOL_CONTRACT_V1.capability_catalog.source_root, 'Resources/v0.3.6/DoneImplementaion');
    assert.deepEqual(TASK_PROTOCOL_CONTRACT_V1.capability_catalog.agent_default_surface, [
      'blueprinthelper_read_task_context',
      'blueprinthelper_preview_task',
      'blueprinthelper_execute_task',
      'blueprinthelper_get_task_result',
    ]);

    assert.deepEqual(
      TASK_PROTOCOL_CONTRACT_V1.capability_catalog.task_runtime_clusters.map((cluster) => cluster.cluster),
      [
        'graph_write',
        'blueprint_signature',
        'graph_cleanup_ownership',
        'blueprint_variables',
        'asset_factory',
        'blueprint_component',
        'blueprint_class_settings',
        'umg_widget_blueprint',
        'data_asset',
        'data_table',
        'compile_save',
      ],
    );

    const graphWrite = TASK_PROTOCOL_CONTRACT_V1.capability_catalog.task_runtime_clusters[0];
    assert.equal(graphWrite.taskplan_capability, 'graph_write');
    assert.deepEqual(graphWrite.runtime_adapter_operations, [
      'append_blueprint_graph',
      'replace_blueprint_graph',
      'patch_blueprint_graph',
      'merge_blueprint_graph',
    ]);
    assert.deepEqual(graphWrite.agent_exposure, 'taskspec_only');
    assert.equal(graphWrite.documents[0], 'BlueprintHelper_AppendBlueprintGraph_UE_CPP_Implementation_Plan_20260503.md');

    const blueprintSignature = TASK_PROTOCOL_CONTRACT_V1.capability_catalog.task_runtime_clusters[1];
    assert.equal(blueprintSignature.taskplan_capability, 'blueprint_signature');
    assert.deepEqual(blueprintSignature.runtime_adapter_operations, [
      'ensure_function',
      'ensure_custom_event',
    ]);
    assert.deepEqual(blueprintSignature.agent_exposure, 'taskplan_internal');

    const blueprintVariables = TASK_PROTOCOL_CONTRACT_V1.capability_catalog.task_runtime_clusters[3];
    assert.equal(blueprintVariables.taskplan_capability, 'blueprint_variable');
    assert.deepEqual(blueprintVariables.runtime_adapter_operations, [
      'add_blueprint_member_variables',
    ]);
    assert.deepEqual(blueprintVariables.agent_exposure, 'taskspec_only');

    const assetFactory = TASK_PROTOCOL_CONTRACT_V1.capability_catalog.task_runtime_clusters[4];
    assert.equal(assetFactory.taskplan_capability, 'asset_factory');
    assert.deepEqual(assetFactory.runtime_adapter_operations, ['create_asset']);
    assert.deepEqual(assetFactory.agent_exposure, 'taskspec_only');

    const component = TASK_PROTOCOL_CONTRACT_V1.capability_catalog.task_runtime_clusters[5];
    assert.equal(component.taskplan_capability, 'blueprint_component');
    assert.deepEqual(component.runtime_adapter_operations, [
      'add_component',
      'set_component_properties',
      'remove_component',
    ]);

    const classSettings = TASK_PROTOCOL_CONTRACT_V1.capability_catalog.task_runtime_clusters[6];
    assert.equal(classSettings.taskplan_capability, 'blueprint_class_settings');
    assert.deepEqual(classSettings.runtime_adapter_operations, [
      'add_implemented_interfaces',
      'remove_implemented_interfaces',
      'set_class_default_properties',
    ]);

    const supportClusters = TASK_PROTOCOL_CONTRACT_V1.capability_catalog.support_clusters.map((cluster) => cluster.cluster);
    assert.deepEqual(supportClusters, [
      'runtime_profile',
      'diagnostics_discovery',
      'transaction_journal',
      'editor_lifecycle',
      'common_envelope',
    ]);
  });

  it('uses should_compile and should_save as the only validation policy fields', () => {
    assert.deepEqual(TASK_PROTOCOL_CONTRACT_V1.validation_policy, {
      task_spec_fields: ['should_compile', 'should_save'],
      task_plan_fields: ['should_compile', 'should_save'],
      forbidden_fields: ['compile', 'save'],
    });

    assertNoForbiddenKeys(graphWriteAppendTaskSpecFixture, TASK_PROTOCOL_CONTRACT_V1.validation_policy.forbidden_fields);
    assertNoForbiddenKeys(graphWriteAppendExpectedTaskPlanFixture, TASK_PROTOCOL_CONTRACT_V1.validation_policy.forbidden_fields);
  });

  it('matches the canonical GraphWrite Append fixtures', () => {
    assert.equal(getPath(graphWriteAppendTaskSpecFixture, 'schema'), TASK_SPEC_SCHEMA);
    assert.equal(getPath(graphWriteAppendTaskSpecFixture, 'task_type'), TASK_PROTOCOL_CONTRACT_V1.supported_first_slice.task_type);
    assert.equal(getPath(graphWriteAppendTaskSpecFixture, 'target.target_type'), TASK_PROTOCOL_CONTRACT_V1.supported_first_slice.target_type);
    assert.equal(
      (TASK_PROTOCOL_CONTRACT_V1.supported_first_slice.graph_strategies as readonly string[]).includes(
        String(getPath(graphWriteAppendTaskSpecFixture, 'behavior.graph_strategy')),
      ),
      true,
    );
    assert.equal(getPath(graphWriteAppendTaskSpecFixture, 'behavior.entries.0.entry_type'), 'custom_event');
    assert.deepEqual(Object.keys(getRecordPath(graphWriteAppendTaskSpecFixture, 'validation')), ['should_compile', 'should_save']);

    assert.equal(getPath(graphWriteAppendExpectedTaskPlanFixture, 'schema'), TASK_PLAN_SCHEMA);
    assert.equal(getPath(graphWriteAppendExpectedTaskPlanFixture, 'steps.0.capability'), 'blueprint_signature');
    assert.equal(getPath(graphWriteAppendExpectedTaskPlanFixture, 'steps.0.write.strategy'), 'custom_event_signature');
    assert.equal(getPath(graphWriteAppendExpectedTaskPlanFixture, 'steps.0.write.ops.0.op'), 'ensure_custom_event');
    assert.equal(getPath(graphWriteAppendExpectedTaskPlanFixture, 'steps.1.capability'), 'graph_write');
    assert.equal(getPath(graphWriteAppendExpectedTaskPlanFixture, 'steps.1.write.strategy'), 'owned_graph_edit');
    assert.deepEqual(getPath(graphWriteAppendExpectedTaskPlanFixture, 'steps.1.depends_on'), ['step_001']);
    assert.deepEqual(Object.keys(getRecordPath(graphWriteAppendExpectedTaskPlanFixture, 'execution_policy')), [
      'dry_run_mode',
      'should_compile',
      'should_save',
    ]);
  });
});

function assertNoForbiddenKeys(value: unknown, forbiddenKeys: readonly string[], path = '$'): void {
  if (Array.isArray(value)) {
    value.forEach((item, index) => assertNoForbiddenKeys(item, forbiddenKeys, `${path}[${index}]`));
    return;
  }

  if (!value || typeof value !== 'object') {
    return;
  }

  for (const [key, child] of Object.entries(value as Record<string, unknown>)) {
    assert.equal(forbiddenKeys.includes(key), false, `${path}.${key}`);
    assertNoForbiddenKeys(child, forbiddenKeys, `${path}.${key}`);
  }
}

function getRecordPath(record: Record<string, unknown>, path: string): Record<string, unknown> {
  const value = getPath(record, path);
  assert.equal(typeof value, 'object', path);
  assert.notEqual(value, null, path);
  assert.equal(Array.isArray(value), false, path);
  return value as Record<string, unknown>;
}

function getPath(record: Record<string, unknown>, path: string): unknown {
  let current: unknown = record;
  for (const part of path.split('.')) {
    if (/^\d+$/.test(part)) {
      assert.equal(Array.isArray(current), true, path);
      current = (current as unknown[])[Number(part)];
      continue;
    }

    assert.equal(typeof current, 'object', path);
    assert.notEqual(current, null, path);
    current = (current as Record<string, unknown>)[part];
  }
  return current;
}
