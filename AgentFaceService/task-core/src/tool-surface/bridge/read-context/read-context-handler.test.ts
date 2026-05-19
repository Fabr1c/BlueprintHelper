import assert from 'node:assert/strict';

import type { BridgeResponse } from '../../../bridge/bridge-client.js';
import { TaskTimingTrace } from '../../../task/service/task-timing.js';
import type { BlueprintHelperToolContext } from '../../types.js';
import { executeReadContext } from './read-context-handler.js';

const timing = TaskTimingTrace.start('read_context_test', 'agentface_test');
const bridgeResponse: BridgeResponse = {
  request_id: 'test',
  success: true,
  result: {
    schema: 'LogicJson.v1',
    format: 'logic_json',
    importable: false,
    scope: 'blueprint',
    logic: {
      asset_path: '/Game/BP_Test',
      groups: [],
    },
    stats: {
      nodes: 0,
      exec_links: 0,
      data_links: 0,
      orphan_nodes: 0,
    },
    timing: {
      schema: 'BlueprintHelper.TimingTrace.v1',
      source: 'ue_bridge_router',
      operation: 'read_blueprint_logic_json',
      timing_id: 'ue_test',
      total_ms: 1,
      stages: [{ name: 'route_execute', started_at_ms: 0, duration_ms: 1 }],
    },
  },
};

const context: BlueprintHelperToolContext = {
  cwd: process.cwd(),
  bridge: {
    sendCommand: async () => bridgeResponse,
  } as never,
  taskRunner: {} as never,
  timing,
};

const result = await executeReadContext({
  schema: 'BlueprintHelper.ReadSpec.v1',
  read_type: 'blueprint_logic',
  target: {
    asset_path: '/Game/BP_Test',
    target_type: 'blueprint',
  },
  view: {
    format: 'logic_json',
  },
}, context);

assert.equal(result.ok, true);

const snapshot = timing.snapshot();
const stageNames = snapshot.stages.map((stage) => stage.name);
assert.ok(stageNames.includes('read_context.bridge_send_receive'));
assert.ok(stageNames.includes('read_context.bridge_payload_extract'));
assert.ok(stageNames.includes('read_context.ue_timing_extract'));
assert.ok(stageNames.includes('read_context.post_process_payload'));
assert.ok(stageNames.includes('read_context.bridge_payload_bytes'));
assert.ok(stageNames.includes('read_context.ue_raw_payload_bytes'));
assert.ok(stageNames.includes('read_context.post_processed_payload_bytes'));
assert.equal(snapshot.nested?.[0]?.['name'], 'ue.read_blueprint_logic_json');

const payload = result.data?.['payload'] as Record<string, unknown>;
assert.equal(payload['timing'], undefined);
