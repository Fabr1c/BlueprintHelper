import { isRecord } from '../bridge-tool-result-utils.js';

const DUPLICATE_FLOW_KEYS = new Set(['flow', 'summary', 'description']);

export function buildLogicJsonDeltaAfterLogicFlowPayload(payload: Record<string, unknown>): Record<string, unknown> {
  const normalized: Record<string, unknown> = {
    schema: 'LogicJsonDeltaAfterLogicFlow.v1',
    ...payload,
  };
  normalized['schema'] = 'LogicJsonDeltaAfterLogicFlow.v1';
  delete normalized['format'];
  delete normalized['flow'];

  const logic = isRecord(normalized['logic']) ? stripDuplicateFlowFields(normalized['logic']) : undefined;
  if (logic) {
    normalized['logic'] = logic;
  }

  normalized['baseline_view'] = 'logic_flow';
  normalized['delta_policy'] = {
    removed_from_logic_flow: ['flow', 'summary', 'description'],
    retained_for_write_location: ['nodes', 'links', 'anchors', 'adapter_boundary', 'stats'],
  };
  return normalized;
}

function stripDuplicateFlowFields(value: unknown): Record<string, unknown> {
  const output: Record<string, unknown> = {};
  if (!isRecord(value)) {
    return output;
  }

  for (const [key, item] of Object.entries(value)) {
    if (DUPLICATE_FLOW_KEYS.has(key)) {
      continue;
    }
    if (Array.isArray(item)) {
      output[key] = item.map((entry) => isRecord(entry) ? stripDuplicateFlowFields(entry) : entry);
      continue;
    }
    output[key] = isRecord(item) ? stripDuplicateFlowFields(item) : item;
  }
  return output;
}
