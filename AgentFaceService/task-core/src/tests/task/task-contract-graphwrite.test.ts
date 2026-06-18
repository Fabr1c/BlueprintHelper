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
        'merge_external_flow',
        'patch_external_graph',
        'patch_external_links',
        'replace_external_body',
      ],
      entry_types: ['custom_event'],
      statement_kinds: [
        'call',
        'set',
        'set_property',
        'let',
        'control',
        'create',
        'convert',
        'schedule',
        'container_action',
        'component_bound_event',
        'delegate.bind',
        'delegate.assign',
        'delegate.unbind',
        'delegate.unbind_all',
        'delegate.call',
      ],
      expression_kinds: ['literal', 'get', 'get_property', 'call', 'op', 'construct', 'deconstruct', 'select', 'create', 'convert', 'schedule', 'container_action'],
      task_plan_capability: 'graph_write',
      task_plan_dependency_capabilities: ['blueprint_signature'],
      runtime_lowering_adapters: [
        'append_blueprint_graph',
        'replace_blueprint_graph',
        'patch_blueprint_graph',
        'merge_blueprint_graph',
        'merge_external_flow',
        'patch_external_graph',
        'patch_external_links',
        'replace_external_body',
      ],
      step_batching: 'append custom_event entries and custom_event_definition replacements compile signature dependency steps before graph_write body steps; other replace/patch/merge/external_merge paths compile to one structural op per step',
    });
  });

  it('pins the AgentFace P1 statement and expression kind surface', () => {
    const firstSlice = TASK_PROTOCOL_CONTRACT_V1.supported_first_slice;

    assert.deepEqual(firstSlice.statement_kinds, [
      'call',
      'set',
      'set_property',
      'let',
      'control',
      'create',
      'convert',
      'schedule',
      'container_action',
      'component_bound_event',
      'delegate.bind',
      'delegate.assign',
      'delegate.unbind',
      'delegate.unbind_all',
      'delegate.call',
    ]);
    assert.equal(firstSlice.statement_kinds.includes('branch'), false);
    assert.equal(firstSlice.statement_kinds.includes('sequence'), false);
    assert.equal(firstSlice.statement_kinds.includes('return'), false);
    assert.equal(Object.hasOwn(firstSlice, 'legacy_statement_kinds'), false);
    assert.deepEqual(firstSlice.expression_kinds, [
      'literal',
      'get',
      'get_property',
      'call',
      'op',
      'construct',
      'deconstruct',
      'select',
      'create',
      'convert',
      'schedule',
      'container_action',
    ]);
  });

  it('pins EventDelegate public schema and internal lowering boundary', () => {
    const firstSlice = TASK_PROTOCOL_CONTRACT_V1.supported_first_slice;
    const boundary = TASK_PROTOCOL_CONTRACT_V1.graph_write_taskspec_contract.event_delegate_use_site_boundary;

    assert.deepEqual(boundary.agent_facing_statement_kinds, [
      'component_bound_event',
      'delegate.bind',
      'delegate.assign',
      'delegate.unbind',
      'delegate.unbind_all',
      'delegate.call',
    ]);
    assert.deepEqual(boundary.compiler_internal_statement_kinds, [
      'component_bound_event',
      'delegate',
    ]);
    assert.deepEqual(boundary.delegate_operations, [
      'bind',
      'assign',
      'unbind',
      'clear',
      'call',
    ]);
    assert.deepEqual(boundary.public_to_internal_lowering, {
      component_bound_event: { kind: 'component_bound_event' },
      'delegate.bind': { kind: 'delegate', delegate_operation: 'bind' },
      'delegate.assign': { kind: 'delegate', delegate_operation: 'assign' },
      'delegate.unbind': { kind: 'delegate', delegate_operation: 'unbind', unbind_mode: 'single' },
      'delegate.unbind_all': { kind: 'delegate', delegate_operation: 'clear', unbind_mode: 'all' },
      'delegate.call': { kind: 'delegate', delegate_operation: 'call' },
    });
    assert.deepEqual(boundary.forbidden_internal_top_level_statement_kinds, [
      'bind',
      'assign',
      'unbind',
      'unbind_all',
      'delegate.bind',
      'delegate.assign',
      'delegate.unbind',
      'delegate.unbind_all',
      'delegate.call',
      'delegate_call',
      'delegate_clear',
    ]);
    assert.equal(firstSlice.statement_kinds.includes('delegate.bind'), true);
    assert.equal((firstSlice.statement_kinds as readonly string[]).includes('delegate'), false);
    assert.equal(JSON.stringify(boundary).includes('assign_auto_attached_event_policy'), false);
    assert.equal(JSON.stringify(boundary).includes('attached_custom_event'), false);
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
    assert.deepEqual(irContract.step_shape.constraints_required_paths, [
      'constraints.allow_modify_user_nodes',
      'constraints.ownership_scope',
      'constraints.external_mutation_policy.strategy for external_graph_edit',
      'constraints.external_mutation_policy.allowed_mutations[] for external_graph_edit',
    ]);
    assert.deepEqual(irContract.supported_write_strategies, [
      'owned_graph_edit',
      'external_graph_edit',
    ]);
    assert.deepEqual(irContract.supported_structural_ops, [
      'ensure_entry',
      'replace_body',
      'set_pin_default',
      'set_node_comment',
      'connect_pins',
      'disconnect_link',
      'replace_link',
      'delete_owned_node',
      'insert_flow',
      'insert_external_flow',
      'set_external_pin_default',
      'set_external_node_comment',
      'set_external_node_property',
      'connect_external_pins',
      'disconnect_external_link',
      'replace_external_link',
      'replace_external_body',
    ]);
    assert.deepEqual(irContract.runtime_supported_structural_ops, [
      'ensure_entry',
      'replace_body',
      'set_pin_default',
      'set_node_comment',
      'connect_pins',
      'disconnect_link',
      'replace_link',
      'delete_owned_node',
      'insert_flow',
      'insert_external_flow',
      'set_external_pin_default',
      'set_external_node_comment',
      'set_external_node_property',
      'connect_external_pins',
      'disconnect_external_link',
      'replace_external_link',
      'replace_external_body',
    ]);
    assert.deepEqual(irContract.external_graph_edit_constraints, {
      ownership_scope: 'external_user_authored',
      external_mutation_policy: [
        {
          strategy: 'merge_external_flow',
          allowed_mutations: ['exec_boundary_link'],
        },
        {
          strategy: 'patch_external_graph',
          allowed_mutations: ['pin_default', 'node_comment', 'node_property'],
        },
        {
          strategy: 'patch_external_links',
          allowed_mutations: ['link_connect', 'link_disconnect', 'link_replace'],
        },
        {
          strategy: 'replace_external_body',
          allowed_mutations: ['body_replace'],
        },
      ],
    });
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
      merge_external_flow: 'behavior.external_merges[]',
      patch_external_graph: 'behavior.external_patches[]',
      patch_external_links: 'behavior.external_link_patches[]',
      replace_external_body: 'behavior.external_replace',
    });
    assert.deepEqual(taskSpecContract.forbidden_agent_shapes, [
      'replace/patch/merge in behavior.entries[]',
      'generic behavior.ops[]',
      'Bridge payload fields as TaskSpec body',
    ]);
    assert.deepEqual(taskSpecContract.replace_owned_graph.selector_kinds_by_scope, {
      graph: 'graph',
      custom_event_definition: 'custom_event',
      custom_event_body: 'custom_event',
      function_body: 'function',
      event_body: 'event',
      block_implementation: 'block',
    });
    assert.deepEqual(taskSpecContract.patch_owned_graph.scope_derivation, {
      set_pin_default: 'pin_default',
      set_node_comment: 'node_comment',
      connect_pins: 'connect_pins',
      disconnect_link: 'disconnect_link',
      replace_link: 'replace_link',
      delete_owned_node: 'node_delete',
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
    assert.deepEqual(taskSpecContract.merge_external_flow.executable_anchor_required_fields, [
      'anchor.schema',
      'anchor.asset_path',
      'anchor.graph_name',
      'anchor.node_guid',
      'anchor.node_class',
      'anchor.pin_name',
      'anchor.pin_direction',
      'anchor.semantic_role',
      'anchor.fingerprint',
    ]);
    assert.deepEqual(taskSpecContract.merge_external_flow.supported_anchor_shapes, [
      {
        schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
        pin_direction: 'output',
        semantic_role: 'exec_boundary',
      },
      {
        schema: 'BlueprintHelper.LogicJsonAnchorSelector.v1',
        required_fields: ['asset_path', 'graph_name or graph', 'node_ref + pin_ref or link_ref'],
        normalized_output: 'graph alias is lowered to graph_name; preview/runtime resolves selector to ExternalGraphAnchor.v1',
      },
    ]);
    assert.deepEqual(taskSpecContract.merge_external_flow.scope_policy_contract, {
      allow_modify_user_nodes: false,
      external_mutation_policy: {
        strategy: 'merge_external_flow',
        allowed_mutations: ['exec_boundary_link'],
      },
    });
    assert.deepEqual(taskSpecContract.patch_external_graph.kinds, [
      'set_external_pin_default',
      'set_external_node_comment',
      'set_external_node_property',
    ]);
    assert.deepEqual(taskSpecContract.patch_external_graph.node_property_descriptor_ids, [
      'k2.node.comment',
      'k2.call.function_target',
      'k2.field.member_reference',
    ]);
    assert.deepEqual(taskSpecContract.patch_external_graph.scope_policy_contract, {
      allow_modify_user_nodes: false,
      external_mutation_policy: {
        strategy: 'patch_external_graph',
        allowed_mutations: ['pin_default', 'node_comment', 'node_property'],
      },
    });
    assert.deepEqual(taskSpecContract.patch_external_links.kinds, [
      'connect_pins',
      'disconnect_link',
      'replace_link',
    ]);
    assert.deepEqual(taskSpecContract.patch_external_links.compact_anchor_shapes, {
      pin: {
        anchor_type: 'external_pin',
        anchor_ref_prefix: 'xpin:v1:',
      },
      link: {
        anchor_type: 'external_link',
        anchor_ref_prefix: 'xlink:v1:',
      },
    });
    assert.deepEqual(taskSpecContract.patch_external_links.scope_policy_contract, {
      allow_modify_user_nodes: false,
      external_mutation_policy: {
        strategy: 'patch_external_links',
        allowed_mutations: ['link_connect', 'link_disconnect', 'link_replace'],
      },
    });
    assert.deepEqual(taskSpecContract.replace_external_body.scopes, [
      'custom_event_body',
      'event_body',
      'function_body',
    ]);
    assert.deepEqual(taskSpecContract.replace_external_body.supported_anchor_shape, {
      schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
      semantic_role: 'body_entry',
    });
    assert.deepEqual(taskSpecContract.replace_external_body.scope_policy_contract, {
      allow_modify_user_nodes: false,
      external_mutation_policy: {
        strategy: 'replace_external_body',
        allowed_mutations: ['body_replace'],
      },
    });
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
      'merge_external_flow',
      'patch_external_graph',
      'patch_external_links',
      'replace_external_body',
    ]);
    assert.deepEqual(operations.map((entry) => entry.ue_command), [
      'append_blueprint_graph',
      'replace_blueprint_graph',
      'patch_blueprint_graph',
      'merge_blueprint_graph',
      'merge_external_flow',
      'patch_external_graph',
      'patch_external_links',
      'replace_external_body',
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
      merge_external_flow: 'root.dry_run',
      patch_external_graph: 'root.dry_run',
      patch_external_links: 'root.dry_run',
      replace_external_body: 'root.dry_run',
    });
  });

  it('keeps runtime validation policy out of Agent-authored TaskSpec fields', () => {
    assert.deepEqual(TASK_PROTOCOL_CONTRACT_V1.validation_policy, {
      task_spec_fields: [],
      task_plan_fields: ['should_compile', 'should_save'],
      forbidden_fields: ['execution_policy', 'validation', 'compile', 'save'],
    });
  });
});
