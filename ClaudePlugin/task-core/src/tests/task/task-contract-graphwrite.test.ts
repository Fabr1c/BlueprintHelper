import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { TASK_PROTOCOL_CONTRACT_V1 } from '../../task/schema/task-contract.js';

describe('GraphWrite TaskPlan contract metadata', () => {
  it('keeps the Agent-facing GraphWrite slice semantic and TaskSpec-first', () => {
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
      step_batching: 'append custom_event entries and custom_event_definition replacements compile signature dependency steps before graph_write body steps; other replace/patch/merge paths compile to one structural op per step',
    });
  });

  it('publishes GraphWrite TaskPlan as structured IR, not low-level operation list', () => {
    const irContract = TASK_PROTOCOL_CONTRACT_V1.graph_write_taskplan_ir_contract;

    assert.equal(irContract.ownership, 'compiler_owned_internal_protocol');
    assert.equal(irContract.agent_authorship, 'forbidden');
    assert.equal(irContract.step_shape.capability, 'graph_write');
    assert.deepEqual(irContract.step_shape.target_required_paths, [
      'target.asset_path',
      'target.graph',
    ]);
    assert.deepEqual(irContract.step_shape.write_required_paths, [
      'write.strategy',
      'write.ops[]',
    ]);
    assert.deepEqual(irContract.supported_structural_ops, [
      'ensure_entry',
      'replace_body',
      'set_pin_default',
      'set_node_comment',
      'set_node_position',
      'insert_flow',
    ]);
    assert.deepEqual(irContract.runtime_supported_structural_ops, [
      'ensure_entry',
      'replace_body',
      'set_pin_default',
      'set_node_comment',
      'set_node_position',
      'insert_flow',
    ]);
    assert.equal(irContract.supported_structural_ops.includes('append_blueprint_graph'), false);
    assert.equal(irContract.supported_structural_ops.includes('replace_blueprint_graph'), false);
  });

  it('publishes the fixed GraphWrite TaskSpec field contract per strategy', () => {
    const taskSpecContract = TASK_PROTOCOL_CONTRACT_V1.graph_write_taskspec_contract;

    assert.equal(taskSpecContract.ownership, 'agent_authored_semantic_protocol');
    assert.deepEqual(taskSpecContract.strategy_fields, {
      append_new_owned_graph: 'behavior.entries[]',
      replace_owned_graph: 'behavior.replace',
      patch_owned_graph: 'behavior.patches[]',
      merge_owned_graph: 'behavior.merges[]',
    });
    assert.deepEqual(taskSpecContract.forbidden_agent_shapes, [
      'replace/patch/merge in behavior.entries[]',
      'generic behavior.ops[]',
      'Bridge payload fields as TaskSpec body',
    ]);
    assert.deepEqual(taskSpecContract.replace_owned_graph.selector_kinds_by_scope, {
      custom_event_definition: 'custom_event',
      custom_event_body: 'custom_event',
      function_body: 'function',
      event_body: 'event',
      block_implementation: 'block',
    });
    assert.deepEqual(taskSpecContract.patch_owned_graph.scope_derivation, {
      set_pin_default: 'pin_default',
      set_node_comment: 'node_comment',
      set_node_position: 'node_position',
    });
    assert.deepEqual(taskSpecContract.patch_owned_graph.block_scoped_target_ref_fields, [
      'target_ref.block_id',
      'target_ref.group_entry_node_path',
      'target_ref.node_ref',
      'target_ref.pin_ref',
      'target_ref.link_ref',
    ]);
    assert.deepEqual(taskSpecContract.merge_owned_graph.block_scoped_anchor_fields, [
      'anchor.block_id',
      'anchor.group_entry_node_path',
      'anchor.node_ref',
      'anchor.pin_ref',
      'anchor.link_ref',
    ]);
    assert.deepEqual(taskSpecContract.non_blueprinthelper_owned_graph_content, {
      normal_agent_write_contract: 'blocked_until_stable_anchor_contract_exists',
      read_contract: 'read_context/read_reference_context only',
      preview_blocker_code: 'unsupported_scope_policy',
      blocked_scope_policy_path: 'scope_policy.allow_modify_user_nodes',
      required_scope_policy_value: false,
    });
    assert.deepEqual(taskSpecContract.patch_owned_graph.forbidden_mainline_anchor_shapes, [
      'raw LogicJson array indexes such as nodes[0]',
      'display names as locators',
      'ad hoc JSONPath strings',
      'GUID-first selectors',
    ]);
    assert.equal(taskSpecContract.merge_owned_graph.sequence_order, 'branch_fork_only');
  });

  it('publishes the fixed GraphWrite lowering adapter operation contract', () => {
    const adapterContract = TASK_PROTOCOL_CONTRACT_V1.graph_write_lowering_adapter_contract;
    const operations = adapterContract.operations;

    assert.equal(adapterContract.ownership, 'runtime_owned_adapter_protocol');
    assert.equal(adapterContract.taskplan_source, 'graph_write_taskplan_ir_contract');
    assert.equal(adapterContract.agent_authorship, 'forbidden');
    assert.deepEqual(operations.map((entry) => entry.operation), [
      'append_blueprint_graph',
      'replace_blueprint_graph',
      'patch_blueprint_graph',
      'merge_blueprint_graph',
    ]);
    assert.deepEqual(operations.map((entry) => entry.ue_command), [
      'append_blueprint_graph',
      'replace_blueprint_graph',
      'patch_blueprint_graph',
      'merge_blueprint_graph',
    ]);
  });

  it('pins GraphWrite dry-run Bridge payload placement', () => {
    const dryRunByOperation = Object.fromEntries(
      TASK_PROTOCOL_CONTRACT_V1.graph_write_lowering_adapter_contract.operations.map((entry) => [
        entry.operation,
        entry.dry_run_bridge_path,
      ]),
    );

    assert.deepEqual(dryRunByOperation, {
      append_blueprint_graph: 'root.dry_run',
      replace_blueprint_graph: 'options.dry_run',
      patch_blueprint_graph: 'root.dry_run',
      merge_blueprint_graph: 'root.dry_run',
    });
  });

  it('keeps compile/save out of validation policy fields', () => {
    assert.deepEqual(TASK_PROTOCOL_CONTRACT_V1.validation_policy, {
      task_spec_fields: ['should_compile', 'should_save'],
      task_plan_fields: ['should_compile', 'should_save'],
      forbidden_fields: ['compile', 'save'],
    });
  });
});
