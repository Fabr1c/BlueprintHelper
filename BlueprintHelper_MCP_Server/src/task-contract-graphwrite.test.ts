import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { TASK_PROTOCOL_CONTRACT_V1 } from './task-contract.js';

describe('GraphWrite TaskPlan contract metadata', () => {
  it('keeps the Agent-facing TaskSpec first slice append-only', () => {
    assert.deepEqual(TASK_PROTOCOL_CONTRACT_V1.supported_first_slice, {
      task_type: 'edit_blueprint_graph',
      target_type: 'blueprint',
      graph_strategy: 'append_new_owned_graph',
      entry_types: ['custom_event'],
      statement_kinds: ['call_function', 'set_member_variable'],
      task_plan_capability: 'graph_write',
      runtime_lowering_adapters: ['append_blueprint_graph'],
      max_task_plan_steps: 1,
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
    ]);
    assert.equal(irContract.supported_structural_ops.includes('append_blueprint_graph'), false);
    assert.equal(irContract.supported_structural_ops.includes('replace_blueprint_graph'), false);
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
