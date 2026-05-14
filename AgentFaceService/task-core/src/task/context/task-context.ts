import type { BridgeClient } from '../../bridge/bridge-client.js';
import { TASK_CONTEXT_PACK_SCHEMA, type ReadTaskContextInput } from '../schema/task-schemas.js';

export async function buildTaskContextPack(bridge: BridgeClient, input: ReadTaskContextInput) {
  const assetPath = input.target.asset_path;
  const [runtime, asset, graphs] = await Promise.all([
    readBridgeResult(bridge, 'get_runtime_profile', {}),
    readBridgeResult(bridge, 'get_asset_info', { asset_path: assetPath }),
    readBridgeResult(bridge, 'list_graphs', { target_blueprint: assetPath }),
  ]);
  const assetInfo = isBridgeErrorResult(asset) ? undefined : asset;

  return {
    schema: TASK_CONTEXT_PACK_SCHEMA,
    context_id: `ctx_${Date.now()}`,
    feature_name: input.feature_name,
    runtime: {
      bridge_reachable: true,
      profile: runtime,
    },
    target: {
      asset_path: assetPath,
      exists: assetInfo !== undefined,
      asset_info: assetInfo,
    },
    blueprint_summary: {
      graphs: extractGraphs(graphs),
    },
    recommended_constraints: {
      prefer_new_graph: true,
      allow_modify_user_nodes: false,
      graph_strategy: 'append_new_owned_graph',
    },
  };
}

async function readBridgeResult(bridge: BridgeClient, command: string, payload: Record<string, unknown>) {
  const response = await bridge.sendCommand(command, payload);
  if (!response.success) {
    return {
      error_code: response.error_code,
      message: response.message,
    };
  }
  return response.result;
}

function extractGraphs(graphsResult: unknown) {
  if (!isRecord(graphsResult)) return [];
  const data = isRecord(graphsResult['data']) ? graphsResult['data'] : graphsResult;
  const graphs = data['graphs'];
  return Array.isArray(graphs) ? graphs : [];
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function isBridgeErrorResult(value: unknown): boolean {
  return isRecord(value) && typeof value['error_code'] === 'string';
}
