import assert from 'node:assert/strict';
import test from 'node:test';
import { TaskRunJournalSchema } from './task-schemas.js';

test('TaskRunJournal schema accepts partial failure with blocked dependent steps and recovery guidance', () => {
  assert.doesNotThrow(() => TaskRunJournalSchema.parse({
    schema: 'BlueprintHelper.TaskRunJournal.v1',
    task_run_id: 'task_partial_failure',
    task_type: 'create_blueprint_feature',
    feature_name: 'DoorFeature',
    status: 'partial_failure',
    target_assets: ['/Game/BP/BP_Door'],
    steps: [
      {
        step_id: 'step_append_graph',
        capability: 'graph_write',
        operation: 'graph_write',
        adapter_operation: 'append_blueprint_graph',
        status: 'failed',
        result: {
          status: 'failed',
        },
      },
      {
        step_id: 'step_configure_variable',
        capability: 'blueprint_variable',
        operation: 'blueprint_variable',
        depends_on: ['step_append_graph'],
        status: 'blocked',
        blocked_by_step_ids: ['step_append_graph'],
        blocked_reason: 'dependency_failed',
        error: null,
      },
      {
        step_id: 'step_create_asset',
        capability: 'asset_factory',
        operation: 'asset_factory',
        status: 'completed',
        result: {
          status: 'applied',
        },
      },
    ],
    recovery: {
      recommended_action: 'inspect_task_result_then_submit_followup_taskspec',
      safe_to_retry: false,
      rollback_available: false,
      notes: [],
    },
  }));
});
