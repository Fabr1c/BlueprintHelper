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
              kind: 'call_function',
              name: 'SetActorEnableCollision',
              args: {
                bNewActorEnableCollision: {
                  kind: 'literal',
                  value_type: 'bool',
                  value: true,
                },
              },
            },
            {
              kind: 'set_member_variable',
              name: 'bGateUnlocked',
              value: {
                kind: 'literal',
                value_type: 'bool',
                value: false,
              },
            },
            {
              kind: 'call_function',
              name: 'PrintString',
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
  execution_policy: {
    dry_run_mode: 'full',
    on_missing_capability: 'stop_and_report',
  },
  validation: {
    should_compile: true,
    should_save: false,
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
    should_compile: true,
    should_save: false,
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
            body: {
              schema: 'BlueprintLogicSpec.v1',
              statements: [
                {
                  kind: 'call_function',
                  name: 'SetActorEnableCollision',
                  args: {
                    bNewActorEnableCollision: {
                      kind: 'literal',
                      value_type: 'bool',
                      value: true,
                    },
                  },
                },
                {
                  kind: 'set_member_variable',
                  name: 'bGateUnlocked',
                  value: {
                    kind: 'literal',
                    value_type: 'bool',
                    value: false,
                  },
                },
                {
                  kind: 'call_function',
                  name: 'PrintString',
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
            kind: 'call_function',
            name: 'PrintString',
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
        preserve_layout: false,
      },
    },
  },
  execution_policy: {
    dry_run_mode: 'full',
    on_missing_capability: 'stop_and_report',
  },
  validation: {
    should_compile: true,
    should_save: false,
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
    should_compile: true,
    should_save: false,
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
              preserve_layout: false,
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
  execution_policy: {
    dry_run_mode: 'full',
    on_missing_capability: 'stop_and_report',
  },
  validation: {
    should_compile: true,
    should_save: false,
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
    should_compile: true,
    should_save: false,
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
  execution_policy: {
    dry_run_mode: 'full',
    on_missing_capability: 'stop_and_report',
  },
  validation: {
    should_compile: true,
    should_save: false,
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
    should_compile: true,
    should_save: false,
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
    should_compile: true,
    should_save: false,
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
            kind: 'set',
            var: 'bGateUnlocked',
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
    should_compile: true,
    should_save: false,
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
          preserve_layout: false,
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
    should_compile: true,
    should_save: false,
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
    should_compile: true,
    should_save: false,
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
    should_compile: true,
    should_save: false,
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
                  kind: 'call_function',
                  name: 'PrintString',
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
  execution_policy: {
    dry_run_mode: 'full',
    on_missing_capability: 'stop_and_report',
  },
  validation: {
    should_compile: true,
    should_save: false,
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
    should_compile: true,
    should_save: false,
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
  execution_policy: {
    dry_run_mode: 'full',
    on_missing_capability: 'stop_and_report',
  },
  validation: {
    should_compile: true,
    should_save: false,
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
    should_compile: true,
    should_save: false,
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
  execution_policy: {
    dry_run_mode: 'full',
    on_missing_capability: 'stop_and_report',
  },
  validation: {
    should_compile: true,
    should_save: false,
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
    should_compile: true,
    should_save: false,
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
  blueprintVariableLocalChangesTaskSpecFixture,
] satisfies TaskSpec[];

export const blueprintVariableTaskPlanFixtures = [
  blueprintVariableMemberChangesTaskPlanFixture,
  blueprintVariableMemberDefaultsTaskPlanFixture,
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
  execution_policy: {
    dry_run_mode: 'full',
    on_missing_capability: 'stop_and_report',
  },
  validation: {
    should_compile: true,
    should_save: false,
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
    should_compile: true,
    should_save: false,
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
  execution_policy: {
    dry_run_mode: 'full',
    on_missing_capability: 'stop_and_report',
  },
  validation: {
    should_compile: true,
    should_save: false,
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
    should_compile: true,
    should_save: false,
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
  },
  execution_policy: {
    dry_run_mode: 'full',
    on_missing_capability: 'stop_and_report',
  },
  validation: {
    should_compile: true,
    should_save: false,
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
    should_compile: true,
    should_save: false,
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
        parent_widget_name: 'Root',
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
  execution_policy: {
    dry_run_mode: 'full',
    on_missing_capability: 'stop_and_report',
  },
  validation: {
    should_compile: true,
    should_save: false,
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
    should_compile: true,
    should_save: false,
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
            parent_widget_name: 'Root',
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
  execution_policy: {
    dry_run_mode: 'full',
    on_missing_capability: 'stop_and_report',
  },
  validation: {
    should_compile: true,
    should_save: false,
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
    should_compile: true,
    should_save: false,
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
  widgetTaskSpecFixture,
  dataTableTaskSpecFixture,
] satisfies TaskSpec[];

export const p1TaskPlanFixtures = [
  createAssetTaskPlanFixture,
  componentTaskPlanFixture,
  classSettingsTaskPlanFixture,
  widgetTaskPlanFixture,
  dataTableTaskPlanFixture,
] satisfies TaskPlan[];
