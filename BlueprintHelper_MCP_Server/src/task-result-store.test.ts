import assert from 'node:assert/strict';
import test from 'node:test';
import { getTaskResult, storeTaskResult } from './task-result-store.js';
import type { TaskPlan } from './task-schemas.js';

test('stores GraphWrite IR task results without requiring adapter operation on the TaskPlan step', () => {
  const taskPlan = {
    schema: 'BlueprintHelper.TaskPlan.v1',
    task_name: 'PureGraphWriteIr',
    task_type: 'edit_blueprint_graph',
    target_assets: ['/Game/BP/BP_Door'],
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
          asset_path: '/Game/BP/BP_Door',
          graph: 'BH_Door',
        },
        write: {
          strategy: 'owned_graph_edit',
          ops: [
            {
              op: 'ensure_entry',
              entry_type: 'custom_event',
              name: 'OpenDoor',
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

  const journal = storeTaskResult({
    taskRunId: 'task_graphwrite_ir_store',
    previewId: 'preview_graphwrite_ir_store',
    taskPlan,
    status: 'completed',
    bridgeResult: {
      data: {
        steps: [
          {
            step_id: 'step_001',
            capability: 'graph_write',
            operation: 'graph_write',
            adapter_operation: 'append_blueprint_graph',
            result: {
              operation: 'append_blueprint_graph',
              data: {
                write_ref: {
                  transaction_id: 'tx_graphwrite_ir_store',
                },
              },
            },
          },
        ],
      },
    },
  });

  const step = journal.steps[0] as Record<string, unknown> | undefined;
  assert.equal(journal.schema, 'BlueprintHelper.TaskRunJournal.v1');
  assert.equal(journal.task_run_id, 'task_graphwrite_ir_store');
  assert.equal(step?.capability, 'graph_write');
  assert.equal(step?.operation, 'graph_write');
  assert.equal(step?.adapter_operation, 'append_blueprint_graph');
  assert.equal(step?.transaction_id, 'tx_graphwrite_ir_store');
  assert.deepEqual(getTaskResult('task_graphwrite_ir_store'), journal);
});
