import type { TaskPlan, TaskSpec } from './task-schemas.js';

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
