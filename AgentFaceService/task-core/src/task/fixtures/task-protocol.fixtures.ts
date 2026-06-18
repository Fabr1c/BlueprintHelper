import type { TaskPlan, TaskSpec } from '../schema/task-schemas.js';

export const graphWriteAppendTaskSpecFixture = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  context_id: 'ctx_stone_gate_activation',
  task_type: 'edit_blueprint_graph',
  feature_name: 'StoneGateActivation',
  target: {
    asset_path: '/Game/Blueprints/BP_StoneGate',
    target_type: 'blueprint',
  },
  scope_policy: {
    graph_name: 'BH_StoneGateActivation',
    allow_modify_user_nodes: false,
  },
  behavior: {
    graph_strategy: 'append_new_owned_graph',
    entries: [
      {
        entry_type: 'custom_event',
        name: 'InitializeStoneGate',
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements: [
            {
              kind: 'call',
              target: 'SetActorEnableCollision',
              args: {
                bNewActorEnableCollision: {
                  kind: 'literal',
                  value_type: 'bool',
                  value: true,
                },
              },
            },
            {
              kind: 'set',
              target: 'bGateUnlocked',
              value: {
                kind: 'literal',
                value_type: 'bool',
                value: false,
              },
            },
            {
              kind: 'call',
              target: 'PrintString',
              args: {
                InString: {
                  kind: 'literal',
                  value_type: 'string',
                  value: 'Stone gate initialized',
                },
                Duration: {
                  kind: 'literal',
                  value_type: 'float',
                  value: 2,
                },
              },
            },
          ],
        },
      },
    ],
  },
} satisfies TaskSpec;

export const graphWriteAppendExpectedTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'StoneGateActivation',
  task_type: 'edit_blueprint_graph',
  context_id: 'ctx_stone_gate_activation',
  target_assets: ['/Game/Blueprints/BP_StoneGate'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      capability: 'blueprint_signature',
      target: {
        asset_path: '/Game/Blueprints/BP_StoneGate',
      },
      write: {
        strategy: 'custom_event_signature',
        ops: [
          {
            op: 'ensure_custom_event',
            event_name: 'InitializeStoneGate',
            graph_name: 'BH_StoneGateActivation',
            name_collision_policy: 'reuse_if_exists',
          },
        ],
      },
    },
    {
      step_id: 'step_002',
      capability: 'graph_write',
      target: {
        asset_path: '/Game/Blueprints/BP_StoneGate',
        graph: 'BH_StoneGateActivation',
      },
      write: {
        strategy: 'owned_graph_edit',
        ops: [
          {
            op: 'ensure_entry',
            entry_type: 'custom_event',
            name: 'InitializeStoneGate',
            signature_evidence_id: 'signature:custom_event:InitializeStoneGate',
            body: {
              schema: 'BlueprintLogicSpec.v2',
              statements: [
                {
                  id: 'InitializeStoneGate_stmt_1',
                  kind: 'call',
                  target: 'SetActorEnableCollision',
                  args: {
                    bNewActorEnableCollision: {
                      id: 'InitializeStoneGate_stmt_1_arg_bNewActorEnableCollision',
                      kind: 'literal',
                      value_type: 'bool',
                      value: true,
                    },
                  },
                },
                {
                  id: 'InitializeStoneGate_stmt_2',
                  kind: 'field',
                  field_operation: 'set',
                  field_scope: 'variable',
                  target: 'bGateUnlocked',
                  value: {
                    id: 'InitializeStoneGate_stmt_2_value',
                    kind: 'literal',
                    value_type: 'bool',
                    value: false,
                  },
                },
                {
                  id: 'InitializeStoneGate_stmt_3',
                  kind: 'call',
                  target: 'PrintString',
                  args: {
                    InString: {
                      id: 'InitializeStoneGate_stmt_3_arg_InString',
                      kind: 'literal',
                      value_type: 'string',
                      value: 'Stone gate initialized',
                    },
                    Duration: {
                      id: 'InitializeStoneGate_stmt_3_arg_Duration',
                      kind: 'literal',
                      value_type: 'float',
                      value: 2,
                    },
                  },
                },
              ],
            },
          },
        ],
      },
      constraints: {
        allow_modify_user_nodes: false,
        ownership_scope: 'blueprinthelper_owned',
      },
      depends_on: ['step_001'],
    },
  ],
} satisfies TaskPlan;

export const graphWriteReplaceTaskSpecFixture = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  context_id: 'ctx_stone_gate_replace',
  task_type: 'edit_blueprint_graph',
  feature_name: 'StoneGateActivationReplace',
  target: {
    asset_path: '/Game/Blueprints/BP_StoneGate',
    target_type: 'blueprint',
  },
  scope_policy: {
    graph_name: 'EventGraph',
    allow_modify_user_nodes: false,
  },
  behavior: {
    graph_strategy: 'replace_owned_graph',
    replace: {
      scope: 'custom_event_body',
      selector: {
        kind: 'custom_event',
        name: 'InitializeStoneGate',
        graph_id: 'EventGraph',
        node_ref: 'InitializeStoneGateEntry',
      },
      body: {
        schema: 'BlueprintLogicSpec.v1',
        statements: [
          {
            kind: 'call',
            target: 'PrintString',
            args: {
              InString: {
                kind: 'literal',
                value_type: 'string',
                value: 'Stone gate replaced',
              },
            },
          },
        ],
      },
      options: {
        strict: true,
      },
    },
  },
} satisfies TaskSpec;

export const graphWriteReplaceExpectedTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'StoneGateActivationReplace',
  task_type: 'edit_blueprint_graph',
  context_id: 'ctx_stone_gate_replace',
  target_assets: ['/Game/Blueprints/BP_StoneGate'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      capability: 'graph_write',
      target: {
        asset_path: '/Game/Blueprints/BP_StoneGate',
        graph: 'EventGraph',
      },
      write: {
        strategy: 'owned_graph_edit',
        ops: [
          {
            op: 'replace_body',
            replace_scope: 'custom_event_body',
            selector: {
              entry_name: 'InitializeStoneGate',
              graph_id: 'EventGraph',
              node_ref: 'InitializeStoneGateEntry',
            },
            logic_spec: {
              schema: 'BlueprintLogicSpec.v2',
              statements: [
                {
                  kind: 'call',
                  target: 'PrintString',
                  args: {
                    InString: {
                      id: 'replace_stmt_1_arg_InString',
                      kind: 'literal',
                      value_type: 'string',
                      value: 'Stone gate replaced',
                    },
                  },
                  id: 'replace_stmt_1',
                },
              ],
            },
            options: {
              strict: true,
            },
          },
        ],
      },
      constraints: {
        allow_modify_user_nodes: false,
        ownership_scope: 'blueprinthelper_owned',
      },
    },
  ],
} satisfies TaskPlan;

export const graphWritePatchTaskSpecFixture = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  context_id: 'ctx_stone_gate_patch',
  task_type: 'edit_blueprint_graph',
  feature_name: 'StoneGateActivationPatch',
  target: {
    asset_path: '/Game/Blueprints/BP_StoneGate',
    target_type: 'blueprint',
  },
  scope_policy: {
    graph_name: 'EventGraph',
    allow_modify_user_nodes: false,
  },
  behavior: {
    graph_strategy: 'patch_owned_graph',
    patches: [
      {
        kind: 'set_pin_default',
        target_ref: {
          block_id: 'BH_StoneGateActivation_InitializeStoneGate',
          group_entry_node_path: 'logic.groups[0].entry.node_path',
          node_ref: 'node:branch_condition',
          pin_ref: 'pin:condition',
          link_ref: 'link:condition_input',
        },
        value: {
          kind: 'literal',
          value_type: 'bool',
          value: true,
        },
        expected_old_state: {
          value: {
            kind: 'literal',
            value_type: 'bool',
            value: false,
          },
        },
      },
    ],
  },
} satisfies TaskSpec;

export const graphWritePatchExpectedTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'StoneGateActivationPatch',
  task_type: 'edit_blueprint_graph',
  context_id: 'ctx_stone_gate_patch',
  target_assets: ['/Game/Blueprints/BP_StoneGate'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      capability: 'graph_write',
      target: {
        asset_path: '/Game/Blueprints/BP_StoneGate',
        graph: 'EventGraph',
      },
      write: {
        strategy: 'owned_graph_edit',
        ops: [
          {
            op: 'set_pin_default',
            patch_scope: 'pin_default',
            patched_ref: {
              block_id: 'BH_StoneGateActivation_InitializeStoneGate',
              group_entry_node_path: 'logic.groups[0].entry.node_path',
              node_ref: 'node:branch_condition',
              pin_ref: 'pin:condition',
              link_ref: 'link:condition_input',
            },
            patch: {
              value: 'true',
            },
            expected_old_state: {
              value: 'false',
            },
          },
        ],
      },
      constraints: {
        allow_modify_user_nodes: false,
        ownership_scope: 'blueprinthelper_owned',
      },
    },
  ],
} satisfies TaskPlan;

export const graphWriteMergeTaskSpecFixture = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  context_id: 'ctx_stone_gate_merge',
  task_type: 'edit_blueprint_graph',
  feature_name: 'StoneGateActivationMerge',
  target: {
    asset_path: '/Game/Blueprints/BP_StoneGate',
    target_type: 'blueprint',
  },
  scope_policy: {
    graph_name: 'EventGraph',
    allow_modify_user_nodes: false,
  },
  behavior: {
    graph_strategy: 'merge_owned_graph',
    merges: [
      {
        kind: 'insert_flow',
        scope: 'function_call',
        insert_strategy: 'insert_between',
        anchor: {
          block_id: 'BH_StoneGateActivation_InitializeStoneGate',
          group_entry_node_path: 'logic.groups[0].entry.node_path',
          node_ref: 'node:entry',
          pin_ref: 'pin:then',
          link_ref: 'link:entry_then',
        },
        inserted: {
          call_kind: 'function_call',
          name: 'InitializeStoneGate',
        },
      },
    ],
  },
} satisfies TaskSpec;

export const graphWriteMergeExpectedTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'StoneGateActivationMerge',
  task_type: 'edit_blueprint_graph',
  context_id: 'ctx_stone_gate_merge',
  target_assets: ['/Game/Blueprints/BP_StoneGate'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      capability: 'graph_write',
      target: {
        asset_path: '/Game/Blueprints/BP_StoneGate',
        graph: 'EventGraph',
      },
      write: {
        strategy: 'owned_graph_edit',
        ops: [
          {
            op: 'insert_flow',
            merge_scope: 'function_call',
            insert_strategy: 'insert_between',
            anchor: {
              block_id: 'BH_StoneGateActivation_InitializeStoneGate',
              group_entry_node_path: 'logic.groups[0].entry.node_path',
              node_ref: 'node:entry',
              pin_ref: 'pin:then',
              link_ref: 'link:entry_then',
            },
            inserted: {
              function: 'InitializeStoneGate',
            },
          },
        ],
      },
      constraints: {
        allow_modify_user_nodes: false,
        ownership_scope: 'blueprinthelper_owned',
      },
    },
  ],
} satisfies TaskPlan;

export const graphWriteAppendLoweringAdapterTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'StoneGateActivation',
  task_type: 'edit_blueprint_graph',
  context_id: 'ctx_stone_gate_activation',
  target_assets: ['/Game/Blueprints/BP_StoneGate'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      operation: 'append_blueprint_graph',
      target: {
        asset_path: '/Game/Blueprints/BP_StoneGate',
        graph: 'BH_StoneGateActivation',
      },
      args: {
        feature_name: 'StoneGateActivation',
        nodes: [
          {
            id: 'InitializeStoneGate_entry',
            kind: 'custom_event',
            name: 'InitializeStoneGate',
          },
          {
            id: 'InitializeStoneGate_stmt_1',
            kind: 'call',
            function: 'SetActorEnableCollision',
            inputs: {
              bNewActorEnableCollision: true,
            },
          },
          {
            id: 'InitializeStoneGate_stmt_2',
            kind: 'field',
            field_operation: 'set',
            field_scope: 'variable',
            var: 'bGateUnlocked',
            target: 'bGateUnlocked',
            value: 'false',
          },
          {
            id: 'InitializeStoneGate_stmt_3',
            kind: 'call',
            function: 'PrintString',
            inputs: {
              InString: 'Stone gate initialized',
              Duration: 2,
            },
          },
        ],
        links: [
          {
            kind: 'exec',
            from: 'InitializeStoneGate_entry.then',
            to: 'InitializeStoneGate_stmt_1.execute',
          },
          {
            kind: 'exec',
            from: 'InitializeStoneGate_stmt_1.then',
            to: 'InitializeStoneGate_stmt_2.execute',
          },
          {
            kind: 'exec',
            from: 'InitializeStoneGate_stmt_2.then',
            to: 'InitializeStoneGate_stmt_3.execute',
          },
        ],
      },
    },
  ],
} satisfies TaskPlan;

export const graphWriteReplaceTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'StoneGateActivationReplace',
  task_type: 'edit_blueprint_graph',
  context_id: 'ctx_stone_gate_replace',
  target_assets: ['/Game/Blueprints/BP_StoneGate'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      operation: 'replace_blueprint_graph',
      target: {
        asset_path: '/Game/Blueprints/BP_StoneGate',
        graph: 'EventGraph',
        replace_scope: 'custom_event_body',
      },
      args: {
        selector: {
          entry_name: 'InitializeStoneGate',
          node_path: 'logic.groups[0].entry.node_path',
        },
        replacement: {
          nodes: [
            {
              id: 'InitializeStoneGate_replacement_1',
              kind: 'call',
              function: 'PrintString',
              inputs: {
                InString: 'Stone gate replaced',
              },
            },
          ],
          links: [],
        },
        options: {
          strict: true,
        },
      },
    },
  ],
} satisfies TaskPlan;

export const graphWritePatchTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'StoneGateActivationPatch',
  task_type: 'edit_blueprint_graph',
  context_id: 'ctx_stone_gate_patch',
  target_assets: ['/Game/Blueprints/BP_StoneGate'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      operation: 'patch_blueprint_graph',
      target: {
        asset_path: '/Game/Blueprints/BP_StoneGate',
        graph: 'EventGraph',
        patch_scope: 'pin_default',
      },
      args: {
        patch_type: 'set_pin_default',
        patched_ref: {
          block_id: 'BH_StoneGateActivation_InitializeStoneGate',
          group_entry_node_path: 'logic.groups[0].entry.node_path',
          node_ref: 'node:branch_condition',
          pin_ref: 'pin:condition',
        },
        patch: {
          value: true,
        },
        expected_old_state: {
          value: false,
        },
      },
    },
  ],
} satisfies TaskPlan;

export const graphWriteMergeTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'StoneGateActivationMerge',
  task_type: 'edit_blueprint_graph',
  context_id: 'ctx_stone_gate_merge',
  target_assets: ['/Game/Blueprints/BP_StoneGate'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      operation: 'merge_blueprint_graph',
      target: {
        asset_path: '/Game/Blueprints/BP_StoneGate',
        graph: 'EventGraph',
        merge_scope: 'owned_block_call',
        insert_strategy: 'insert_between',
      },
      args: {
        anchor: {
          block_id: 'BH_StoneGateActivation_InitializeStoneGate',
          group_entry_node_path: 'logic.groups[0].entry.node_path',
          node_ref: 'node:entry',
          pin_ref: 'pin:then',
        },
        inserted: {
          block_id: 'BH_StoneGateActivation_InitializeStoneGate',
          block_ref: 'block:BH_StoneGateActivation_InitializeStoneGate',
        },
        sequence_order: [
          'original_successor',
          'inserted_logic',
        ],
      },
    },
  ],
} satisfies TaskPlan;

export const graphWriteStructuredIrTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'StoneGateActivationStructuredGraphWrite',
  task_type: 'edit_blueprint_graph',
  context_id: 'ctx_stone_gate_graphwrite_ir',
  target_assets: ['/Game/Blueprints/BP_StoneGate'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      capability: 'graph_write',
      target: {
        asset_path: '/Game/Blueprints/BP_StoneGate',
        graph: 'BH_StoneGateActivation',
      },
      write: {
        strategy: 'owned_graph_edit',
        ops: [
          {
            op: 'ensure_entry',
            entry_type: 'custom_event',
            name: 'InitializeStoneGate',
            body: {
              schema: 'BlueprintLogicSpec.v1',
              statements: [
                {
                  kind: 'call',
                  target: 'PrintString',
                  args: {
                    InString: {
                      kind: 'literal',
                      value_type: 'string',
                      value: 'Stone gate initialized',
                    },
                  },
                },
              ],
            },
          },
          {
            op: 'set_pin_default',
            selector: {
              block_id: 'BH_StoneGateActivation_InitializeStoneGate',
              group_entry_node_path: 'logic.groups[0].entry.node_path',
              node_ref: 'node:branch_condition',
              pin_ref: 'pin:condition',
            },
            value: {
              kind: 'literal',
              value_type: 'bool',
              value: true,
            },
          },
          {
            op: 'insert_flow',
            anchor: {
              block_id: 'BH_StoneGateActivation_InitializeStoneGate',
              group_entry_node_path: 'logic.groups[0].entry.node_path',
              node_ref: 'node:entry',
              pin_ref: 'pin:then',
            },
            body: {
              schema: 'BlueprintLogicSpec.v1',
              statements: [],
            },
          },
        ],
      },
      constraints: {
        allow_modify_user_nodes: false,
        ownership_scope: 'blueprinthelper_owned',
      },
    },
  ],
} satisfies TaskPlan;

export const graphWriteLoweringAdapterTaskPlanFixtures = [
  graphWriteAppendLoweringAdapterTaskPlanFixture,
  graphWriteReplaceTaskPlanFixture,
  graphWritePatchTaskPlanFixture,
  graphWriteMergeTaskPlanFixture,
] satisfies TaskPlan[];

export const graphWriteTaskSpecFixtures = [
  graphWriteAppendTaskSpecFixture,
  graphWriteReplaceTaskSpecFixture,
  graphWritePatchTaskSpecFixture,
  graphWriteMergeTaskSpecFixture,
] satisfies TaskSpec[];

export const graphWriteExpectedTaskPlanFixtures = [
  graphWriteAppendExpectedTaskPlanFixture,
  graphWriteReplaceExpectedTaskPlanFixture,
  graphWritePatchExpectedTaskPlanFixture,
  graphWriteMergeExpectedTaskPlanFixture,
] satisfies TaskPlan[];

export const graphWriteTaskPlanFixtures = [
  ...graphWriteExpectedTaskPlanFixtures,
  graphWriteStructuredIrTaskPlanFixture,
  ...graphWriteLoweringAdapterTaskPlanFixtures,
] satisfies TaskPlan[];

export const blueprintVariableMemberChangesTaskSpecFixture = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  context_id: 'ctx_p1_blueprint_variables',
  task_type: 'edit_blueprint_variables',
  feature_name: 'P1BlueprintVariables',
  target: {
    asset_path: '/Game/Blueprints/BP_Door',
    target_type: 'blueprint',
  },
  behavior: {
    variable_strategy: 'member_variables',
    changes: [
      {
        kind: 'ensure_member_variable',
        name: 'Health',
        variable_type: {
          category: 'float',
        },
        category: 'Stats',
      },
      {
        kind: 'configure_member_variable',
        name: 'Health',
        properties: [
          {
            property_path: 'Tooltip',
            value: 'Current health.',
          },
        ],
      },
      {
        kind: 'remove_member_variable',
        name: 'DeprecatedHealth',
      },
    ],
  },
} satisfies TaskSpec;

export const blueprintVariableMemberChangesTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'P1BlueprintVariables',
  task_type: 'edit_blueprint_variables',
  context_id: 'ctx_p1_blueprint_variables',
  target_assets: ['/Game/Blueprints/BP_Door'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      capability: 'blueprint_variable',
      target: {
        asset_path: '/Game/Blueprints/BP_Door',
      },
      write: {
        strategy: 'member_variables',
        ops: [
          {
            op: 'ensure_member_variable',
            name: 'Health',
            pin_type: {
              category: 'float',
            },
            category: 'Stats',
          },
          {
            op: 'set_member_variable_properties',
            name: 'Health',
            settings: [
              {
                property_path: 'Tooltip',
                value: 'Current health.',
              },
            ],
          },
          {
            op: 'remove_member_variable',
            name: 'DeprecatedHealth',
          },
        ],
      },
      constraints: {
        allow_remove_referenced_variables: false,
      },
    },
  ],
} satisfies TaskPlan;

export const blueprintVariableMemberDefaultsTaskSpecFixture = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  context_id: 'ctx_p1_blueprint_variable_defaults',
  task_type: 'edit_blueprint_variables',
  feature_name: 'P1BlueprintVariableDefaults',
  target: {
    asset_path: '/Game/Blueprints/BP_Door',
    target_type: 'blueprint',
  },
  behavior: {
    variable_strategy: 'member_defaults',
    defaults: [
      {
        name: 'Health',
        value: {
          kind: 'literal',
          value_type: 'float',
          value: 100,
        },
      },
    ],
  },
} satisfies TaskSpec;

export const blueprintVariableMemberDefaultsTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'P1BlueprintVariableDefaults',
  task_type: 'edit_blueprint_variables',
  context_id: 'ctx_p1_blueprint_variable_defaults',
  target_assets: ['/Game/Blueprints/BP_Door'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      capability: 'blueprint_variable',
      target: {
        asset_path: '/Game/Blueprints/BP_Door',
      },
      write: {
        strategy: 'member_defaults',
        ops: [
          {
            op: 'set_member_default',
            name: 'Health',
            value: 100,
          },
        ],
      },
      constraints: {
        allow_remove_referenced_variables: false,
      },
    },
  ],
} satisfies TaskPlan;

export const blueprintVariableMemberReplicationTaskSpecFixture = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  context_id: 'ctx_p0c_blueprint_variable_replication',
  task_type: 'edit_blueprint_variables',
  feature_name: 'P0CBlueprintVariableReplication',
  target: {
    asset_path: '/Game/BH/P0C/BP_Door',
    target_type: 'blueprint',
  },
  behavior: {
    variable_strategy: 'member_variables',
    changes: [
      {
        kind: 'ensure_member_variable',
        name: 'DoorState',
        variable_type: {
          category: 'bool',
        },
        category: 'Network',
      },
      {
        kind: 'configure_member_variable',
        name: 'DoorState',
        properties: [
          {
            property_path: 'replication',
            value: {
              mode: 'rep_notify',
              condition: 'owner_only',
            },
          },
        ],
      },
    ],
  },
} satisfies TaskSpec;

export const blueprintVariableMemberReplicationTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'P0CBlueprintVariableReplication',
  task_type: 'edit_blueprint_variables',
  context_id: 'ctx_p0c_blueprint_variable_replication',
  target_assets: ['/Game/BH/P0C/BP_Door'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      capability: 'blueprint_variable',
      target: {
        asset_path: '/Game/BH/P0C/BP_Door',
      },
      write: {
        strategy: 'member_variables',
        ops: [
          {
            op: 'ensure_member_variable',
            name: 'DoorState',
            pin_type: {
              category: 'bool',
            },
            category: 'Network',
          },
          {
            op: 'set_member_variable_properties',
            name: 'DoorState',
            settings: [
              {
                property_path: 'replication',
                value: {
                  mode: 'rep_notify',
                  condition: 'owner_only',
                  notify_function: 'OnRep_DoorState',
                  create_notify_function: true,
                  reuse_existing_notify_function: false,
                },
              },
            ],
          },
        ],
      },
      constraints: {
        allow_remove_referenced_variables: false,
      },
    },
  ],
} satisfies TaskPlan;

export const blueprintVariableLocalChangesTaskSpecFixture = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  context_id: 'ctx_p1_blueprint_local_variables',
  task_type: 'edit_blueprint_variables',
  feature_name: 'P1BlueprintLocalVariables',
  target: {
    asset_path: '/Game/Blueprints/BP_Door',
    target_type: 'blueprint',
  },
  behavior: {
    variable_strategy: 'local_variables',
    function_name: 'CalculateDamage',
    changes: [
      {
        kind: 'ensure_local_variable',
        name: 'DamageScale',
        variable_type: {
          category: 'float',
        },
      },
      {
        kind: 'configure_local_variable',
        name: 'DamageScale',
        properties: [
          {
            property_path: 'Tooltip',
            value: 'Current damage scale.',
          },
        ],
      },
      {
        kind: 'remove_local_variable',
        name: 'OldDamageScale',
      },
    ],
  },
} satisfies TaskSpec;

export const blueprintVariableLocalChangesTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'P1BlueprintLocalVariables',
  task_type: 'edit_blueprint_variables',
  context_id: 'ctx_p1_blueprint_local_variables',
  target_assets: ['/Game/Blueprints/BP_Door'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      capability: 'blueprint_variable',
      target: {
        asset_path: '/Game/Blueprints/BP_Door',
        function_name: 'CalculateDamage',
      },
      write: {
        strategy: 'local_variables',
        ops: [
          {
            op: 'ensure_local_variable',
            function_name: 'CalculateDamage',
            name: 'DamageScale',
            pin_type: {
              category: 'float',
            },
          },
          {
            op: 'set_local_variable_properties',
            function_name: 'CalculateDamage',
            name: 'DamageScale',
            settings: [
              {
                property_path: 'Tooltip',
                value: 'Current damage scale.',
              },
            ],
          },
          {
            op: 'remove_local_variable',
            function_name: 'CalculateDamage',
            name: 'OldDamageScale',
          },
        ],
      },
      constraints: {
        allow_remove_referenced_variables: false,
      },
    },
  ],
} satisfies TaskPlan;

export const blueprintVariableTaskSpecFixtures = [
  blueprintVariableMemberChangesTaskSpecFixture,
  blueprintVariableMemberDefaultsTaskSpecFixture,
  blueprintVariableMemberReplicationTaskSpecFixture,
  blueprintVariableLocalChangesTaskSpecFixture,
] satisfies TaskSpec[];

export const blueprintVariableTaskPlanFixtures = [
  blueprintVariableMemberChangesTaskPlanFixture,
  blueprintVariableMemberDefaultsTaskPlanFixture,
  blueprintVariableMemberReplicationTaskPlanFixture,
  blueprintVariableLocalChangesTaskPlanFixture,
] satisfies TaskPlan[];

export const createAssetTaskSpecFixture = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  context_id: 'ctx_p1_create_asset',
  task_type: 'create_asset',
  feature_name: 'P1Asset',
  target: {
    asset_path: '/Game/Input/IA_Interact',
    target_type: 'asset',
  },
  behavior: {
    asset_strategy: 'ensure_asset',
    asset: {
      asset_type: 'input_action',
      value_type: 'bool',
      collision_policy: 'reuse_if_exists',
    },
  },
} satisfies TaskSpec;

export const createAssetTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'P1Asset',
  task_type: 'create_asset',
  context_id: 'ctx_p1_create_asset',
  target_assets: ['/Game/Input/IA_Interact'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      capability: 'asset_factory',
      target: {
        asset_path: '/Game/Input/IA_Interact',
      },
      write: {
        strategy: 'asset_create',
        ops: [
          {
            op: 'create_asset',
            asset_type: 'input_action',
            value_type: 'bool',
            collision: 'reuse_if_exists',
          },
        ],
      },
    },
  ],
} satisfies TaskPlan;

export const componentTaskSpecFixture = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  context_id: 'ctx_p1_blueprint_components',
  task_type: 'edit_blueprint_components',
  feature_name: 'P1Components',
  target: {
    asset_path: '/Game/Blueprints/BP_Door',
    target_type: 'blueprint',
  },
  behavior: {
    component_strategy: 'component_tree',
    changes: [
      {
        kind: 'ensure_component_present',
        name: 'DoorRoot',
        class: 'SceneComponent',
      },
      {
        kind: 'configure_component',
        name: 'DoorRoot',
        properties: [
          {
            property_path: 'CollisionEnabled',
            value: true,
          },
        ],
      },
      {
        kind: 'remove_component',
        name: 'DeprecatedMarker',
      },
    ],
  },
} satisfies TaskSpec;

export const componentTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'P1Components',
  task_type: 'edit_blueprint_components',
  context_id: 'ctx_p1_blueprint_components',
  target_assets: ['/Game/Blueprints/BP_Door'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      capability: 'blueprint_component',
      target: {
        asset_path: '/Game/Blueprints/BP_Door',
      },
      write: {
        strategy: 'component_tree',
        ops: [
          {
            op: 'add_component',
            component_name: 'DoorRoot',
            component_class: 'SceneComponent',
          },
        ],
      },
    },
    {
      step_id: 'step_002',
      capability: 'blueprint_component',
      target: {
        asset_path: '/Game/Blueprints/BP_Door',
      },
      write: {
        strategy: 'component_tree',
        ops: [
          {
            op: 'set_component_properties',
            component_name: 'DoorRoot',
            settings: [
              {
                property_path: 'CollisionEnabled',
                value: true,
              },
            ],
          },
        ],
      },
    },
    {
      step_id: 'step_003',
      capability: 'blueprint_component',
      target: {
        asset_path: '/Game/Blueprints/BP_Door',
      },
      write: {
        strategy: 'component_tree',
        ops: [
          {
            op: 'remove_component',
            component_name: 'DeprecatedMarker',
          },
        ],
      },
    },
  ],
} satisfies TaskPlan;

export const componentExpansionTaskSpecFixture = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  context_id: 'ctx_component_expansion',
  task_type: 'edit_blueprint_components',
  feature_name: 'ComponentExpansion',
  target: {
    asset_path: '/Game/Blueprints/BP_ComponentExpansion',
    target_type: 'blueprint',
  },
  behavior: {
    component_strategy: 'component_tree',
    changes: [
      {
        kind: 'ensure_component_present',
        name: 'DoorRoot',
        class: '/Script/Engine.SceneComponent',
        name_collision_policy: 'block_if_class_mismatch',
      },
      {
        kind: 'configure_component',
        name: 'DoorRoot',
        properties: [
          {
            property_path: 'Mobility',
            value: 'Movable',
          },
        ],
      },
      {
        kind: 'rename_component',
        name: 'DoorMesh',
        new_name: 'DoorVisual',
      },
      {
        kind: 'reparent_component',
        name: 'DoorVisual',
        new_parent: 'DoorRoot',
        socket: 'DoorSocket',
        attach_rule: 'keep_world',
        transform_policy: 'preserve_world',
      },
      {
        kind: 'attach_component',
        name: 'DoorVisual',
        parent: 'DoorRoot',
        socket: 'DoorSocket',
        attach_rule: 'snap_to_target',
        transform_policy: 'reset_relative',
      },
      {
        kind: 'detach_component',
        name: 'DoorVisual',
        transform_policy: 'preserve_relative',
        default_root_policy: 'create_default_scene_root_when_needed',
      },
      {
        kind: 'set_root_component',
        name: 'DoorRoot',
        old_root_policy: 'remove_default_scene_root_when_empty',
        default_root_policy: 'require_scene_component',
      },
      {
        kind: 'remove_component',
        name: 'DeprecatedMarker',
        delete_policy: 'promote_children',
      },
    ],
  },
} satisfies TaskSpec;

export const componentExpansionExpectedTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'ComponentExpansion',
  task_type: 'edit_blueprint_components',
  context_id: 'ctx_component_expansion',
  target_assets: ['/Game/Blueprints/BP_ComponentExpansion'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      capability: 'blueprint_component',
      target: {
        asset_path: '/Game/Blueprints/BP_ComponentExpansion',
      },
      write: {
        strategy: 'component_tree',
        ops: [
          {
            op: 'add_component',
            component_name: 'DoorRoot',
            component_class: '/Script/Engine.SceneComponent',
            name_collision_policy: 'block_if_class_mismatch',
          },
        ],
      },
    },
    {
      step_id: 'step_002',
      capability: 'blueprint_component',
      target: {
        asset_path: '/Game/Blueprints/BP_ComponentExpansion',
      },
      write: {
        strategy: 'component_tree',
        ops: [
          {
            op: 'set_component_properties',
            component_name: 'DoorRoot',
            settings: [
              {
                property_path: 'Mobility',
                value: 'Movable',
              },
            ],
          },
        ],
      },
    },
    {
      step_id: 'step_003',
      capability: 'blueprint_component',
      target: {
        asset_path: '/Game/Blueprints/BP_ComponentExpansion',
      },
      write: {
        strategy: 'component_tree',
        ops: [
          {
            op: 'rename_component',
            component_name: 'DoorMesh',
            new_component_name: 'DoorVisual',
          },
        ],
      },
    },
    {
      step_id: 'step_004',
      capability: 'blueprint_component',
      target: {
        asset_path: '/Game/Blueprints/BP_ComponentExpansion',
      },
      write: {
        strategy: 'component_tree',
        ops: [
          {
            op: 'reparent_component',
            component_name: 'DoorVisual',
            new_parent_component: 'DoorRoot',
            socket_name: 'DoorSocket',
            attach_rule: 'keep_world',
            transform_policy: 'preserve_world',
          },
        ],
      },
    },
    {
      step_id: 'step_005',
      capability: 'blueprint_component',
      target: {
        asset_path: '/Game/Blueprints/BP_ComponentExpansion',
      },
      write: {
        strategy: 'component_tree',
        ops: [
          {
            op: 'attach_component',
            component_name: 'DoorVisual',
            parent_component: 'DoorRoot',
            socket_name: 'DoorSocket',
            attach_rule: 'snap_to_target',
            transform_policy: 'reset_relative',
          },
        ],
      },
    },
    {
      step_id: 'step_006',
      capability: 'blueprint_component',
      target: {
        asset_path: '/Game/Blueprints/BP_ComponentExpansion',
      },
      write: {
        strategy: 'component_tree',
        ops: [
          {
            op: 'detach_component',
            component_name: 'DoorVisual',
            transform_policy: 'preserve_relative',
            default_root_policy: 'create_default_scene_root_when_needed',
          },
        ],
      },
    },
    {
      step_id: 'step_007',
      capability: 'blueprint_component',
      target: {
        asset_path: '/Game/Blueprints/BP_ComponentExpansion',
      },
      write: {
        strategy: 'component_tree',
        ops: [
          {
            op: 'set_root_component',
            component_name: 'DoorRoot',
            old_root_policy: 'remove_default_scene_root_when_empty',
            default_root_policy: 'require_scene_component',
          },
        ],
      },
    },
    {
      step_id: 'step_008',
      capability: 'blueprint_component',
      target: {
        asset_path: '/Game/Blueprints/BP_ComponentExpansion',
      },
      write: {
        strategy: 'component_tree',
        ops: [
          {
            op: 'remove_component',
            component_name: 'DeprecatedMarker',
            delete_policy: 'promote_children',
          },
        ],
      },
    },
  ],
} satisfies TaskPlan;

export const classSettingsTaskSpecFixture = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  context_id: 'ctx_p1_blueprint_class_settings',
  task_type: 'edit_blueprint_class_settings',
  feature_name: 'P1ClassSettings',
  target: {
    asset_path: '/Game/Blueprints/BP_Door',
    target_type: 'blueprint',
  },
  behavior: {
    class_settings_strategy: 'class_settings',
    interfaces: {
      ensure_present: ['/Game/Interfaces/BPI_Interact'],
      ensure_absent: ['/Game/Interfaces/BPI_Legacy'],
    },
    class_defaults: [
      {
        property_path: 'bCanBeDamaged',
        value: true,
      },
    ],
    reparent: {
      new_parent_class: '/Script/Engine.Pawn',
    },
  },
} satisfies TaskSpec;

export const classSettingsTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'P1ClassSettings',
  task_type: 'edit_blueprint_class_settings',
  context_id: 'ctx_p1_blueprint_class_settings',
  target_assets: ['/Game/Blueprints/BP_Door'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      capability: 'blueprint_class_settings',
      target: {
        asset_path: '/Game/Blueprints/BP_Door',
      },
      write: {
        strategy: 'class_settings',
        ops: [
          {
            op: 'add_implemented_interfaces',
            interface_paths: ['/Game/Interfaces/BPI_Interact'],
          },
        ],
      },
    },
    {
      step_id: 'step_002',
      capability: 'blueprint_class_settings',
      target: {
        asset_path: '/Game/Blueprints/BP_Door',
      },
      write: {
        strategy: 'class_settings',
        ops: [
          {
            op: 'remove_implemented_interfaces',
            interface_paths: ['/Game/Interfaces/BPI_Legacy'],
          },
        ],
      },
    },
    {
      step_id: 'step_003',
      capability: 'blueprint_class_settings',
      target: {
        asset_path: '/Game/Blueprints/BP_Door',
      },
      write: {
        strategy: 'class_settings',
        ops: [
          {
            op: 'set_class_default_properties',
            settings: [
              {
                property_path: 'bCanBeDamaged',
                value: true,
              },
            ],
          },
        ],
      },
    },
    {
      step_id: 'step_004',
      capability: 'blueprint_class_settings',
      target: {
        asset_path: '/Game/Blueprints/BP_Door',
      },
      write: {
        strategy: 'class_settings',
        ops: [
          {
            op: 'reparent_blueprint',
            new_parent_class: '/Script/Engine.Pawn',
          },
        ],
      },
    },
  ],
} satisfies TaskPlan;

export const signatureTaskSpecFixture = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  context_id: 'ctx_signature_compute_score',
  task_type: 'edit_blueprint_signature',
  feature_name: 'SignatureComputeScore',
  target: {
    asset_path: '/Game/Blueprints/BP_SignatureExample',
    target_type: 'blueprint',
  },
  behavior: {
    signature_strategy: 'signature_edit',
    changes: [
      {
        kind: 'ensure_function',
        function_name: 'ComputeScore',
        inputs: [
          {
            name: 'BaseScore',
            pin_type: {
              category: 'int',
            },
            default_value: {
              mode: 'literal',
              value: '0',
            },
          },
        ],
        outputs: [
          {
            name: 'FinalScore',
            pin_type: {
              category: 'int',
            },
          },
        ],
        is_pure: true,
        name_collision_policy: 'reuse_if_exists',
      },
    ],
  },
} satisfies TaskSpec;

export const signatureTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'SignatureComputeScore',
  task_type: 'edit_blueprint_signature',
  context_id: 'ctx_signature_compute_score',
  target_assets: ['/Game/Blueprints/BP_SignatureExample'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      capability: 'blueprint_signature',
      target: {
        asset_path: '/Game/Blueprints/BP_SignatureExample',
      },
      write: {
        strategy: 'function_signature',
        ops: [
          {
            op: 'ensure_function',
            function_name: 'ComputeScore',
            inputs: [
              {
                name: 'BaseScore',
                pin_type: {
                  category: 'int',
                },
                default_value: {
                  mode: 'literal',
                  value: '0',
                },
              },
            ],
            outputs: [
              {
                name: 'FinalScore',
                pin_type: {
                  category: 'int',
                },
              },
            ],
            is_pure: true,
            name_collision_policy: 'reuse_if_exists',
          },
        ],
      },
    },
  ],
} satisfies TaskPlan;

export const widgetTaskSpecFixture = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  context_id: 'ctx_p1_umg_widget',
  task_type: 'edit_umg_widget',
  feature_name: 'P1UMG',
  target: {
    asset_path: '/Game/UI/WBP_MainMenu',
    target_type: 'widget_blueprint',
  },
  behavior: {
    widget_strategy: 'widget_blueprint_edit',
    changes: [
      {
        kind: 'create_widget',
        widget_class: 'TextBlock',
        widget_name: 'TitleText',
        parent_name: 'Root',
        virtual_index: 0,
      },
      {
        kind: 'update_widget_property',
        widget_name: 'TitleText',
        property_path: 'Text',
        value: 'Ready',
      },
      {
        kind: 'delete_widget',
        widget_name: 'LegacyText',
      },
    ],
  },
} satisfies TaskSpec;

export const widgetTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'P1UMG',
  task_type: 'edit_umg_widget',
  context_id: 'ctx_p1_umg_widget',
  target_assets: ['/Game/UI/WBP_MainMenu'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      capability: 'umg_widget',
      target: {
        asset_path: '/Game/UI/WBP_MainMenu',
      },
      write: {
        strategy: 'widget_tree_edit',
        ops: [
          {
            op: 'add_widget',
            widget_name: 'TitleText',
            widget_class: 'TextBlock',
            parent_name: 'Root',
            virtual_index: 0,
          },
        ],
      },
    },
    {
      step_id: 'step_002',
      capability: 'umg_widget',
      target: {
        asset_path: '/Game/UI/WBP_MainMenu',
      },
      write: {
        strategy: 'widget_property_edit',
        ops: [
          {
            op: 'set_widget_property',
            widget_name: 'TitleText',
            property_path: 'Text',
            value: 'Ready',
          },
        ],
      },
    },
    {
      step_id: 'step_003',
      capability: 'umg_widget',
      target: {
        asset_path: '/Game/UI/WBP_MainMenu',
      },
      write: {
        strategy: 'widget_tree_edit',
        ops: [
          {
            op: 'remove_widget',
            widget_name: 'LegacyText',
          },
        ],
      },
    },
  ],
} satisfies TaskPlan;

export const dataTableTaskSpecFixture = {
  schema: 'BlueprintHelper.TaskSpec.v1',
  context_id: 'ctx_p1_data_table',
  task_type: 'edit_data_table',
  feature_name: 'P1DataTable',
  target: {
    asset_path: '/Game/Data/DT_Weapons',
    target_type: 'data_table',
  },
  behavior: {
    row_strategy: 'row_edit',
    rows: [
      {
        action: 'add',
        row_name: 'Pistol',
        fields: {
          Damage: '12',
          Ammo: '10',
        },
      },
      {
        action: 'update',
        row_name: 'Shotgun',
        fields: {
          Ammo: '16',
        },
      },
      {
        action: 'delete',
        row_name: 'LegacyGun',
      },
    ],
  },
} satisfies TaskSpec;

export const dataTableTaskPlanFixture = {
  schema: 'BlueprintHelper.TaskPlan.v1',
  task_name: 'P1DataTable',
  task_type: 'edit_data_table',
  context_id: 'ctx_p1_data_table',
  target_assets: ['/Game/Data/DT_Weapons'],
  execution_policy: {
    dry_run_mode: 'full',
    should_compile: false,
    should_save: false,
    review_baseline_dirty_asset_policy: 'block',
  },
  steps: [
    {
      step_id: 'step_001',
      capability: 'data_table',
      target: {
        asset_path: '/Game/Data/DT_Weapons',
      },
      write: {
        strategy: 'row_edit',
        ops: [
          {
            op: 'add_row',
            row_name: 'Pistol',
            fields: {
              Damage: '12',
              Ammo: '10',
            },
          },
        ],
      },
    },
    {
      step_id: 'step_002',
      capability: 'data_table',
      target: {
        asset_path: '/Game/Data/DT_Weapons',
      },
      write: {
        strategy: 'row_edit',
        ops: [
          {
            op: 'update_row',
            row_name: 'Shotgun',
            fields: {
              Ammo: '16',
            },
          },
        ],
      },
    },
    {
      step_id: 'step_003',
      capability: 'data_table',
      target: {
        asset_path: '/Game/Data/DT_Weapons',
      },
      write: {
        strategy: 'row_edit',
        ops: [
          {
            op: 'delete_row',
            row_name: 'LegacyGun',
          },
        ],
      },
    },
  ],
} satisfies TaskPlan;

export const p1TaskSpecFixtures = [
  createAssetTaskSpecFixture,
  componentTaskSpecFixture,
  classSettingsTaskSpecFixture,
  signatureTaskSpecFixture,
  widgetTaskSpecFixture,
  dataTableTaskSpecFixture,
] satisfies TaskSpec[];

export const p1TaskPlanFixtures = [
  createAssetTaskPlanFixture,
  componentTaskPlanFixture,
  classSettingsTaskPlanFixture,
  signatureTaskPlanFixture,
  widgetTaskPlanFixture,
  dataTableTaskPlanFixture,
] satisfies TaskPlan[];
