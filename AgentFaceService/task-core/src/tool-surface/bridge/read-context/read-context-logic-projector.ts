import { buildLogicFlowPayload } from './read-context-logic-flow.js';
import { enrichLogicJsonCompactAnchors } from './read-context-compact-anchor.js';
import { LOGIC_PROJECTION_OWNER } from './read-context-schemas.js';
import { isRecord } from '../bridge-tool-result-utils.js';

export type ReadContextLogicFormat = 'logic_flow' | 'logic_json';

export type LogicProjectionInput = {
  requestedFormat: ReadContextLogicFormat;
  bridgePayloadSchema: 'LogicJson.v1' | 'LogicFlow.v1' | 'LogicSnapshot.v1';
  bridgePayload: Record<string, unknown>;
  target: Record<string, unknown>;
  view?: Record<string, unknown>;
};

export type LogicProjectionResult = {
  format: ReadContextLogicFormat;
  payload: Record<string, unknown>;
  debug?: Record<string, unknown>;
};

type ProjectionSource = {
  payload: Record<string, unknown>;
  metadata: Record<string, unknown>;
};

export function projectReadContextLogic(input: LogicProjectionInput): LogicProjectionResult {
  const source = resolveProjectionSource(input);

  if (input.requestedFormat === 'logic_flow') {
    const result = buildLogicFlowPayload(source.payload);
    const format = result.payload['schema'] === 'LogicJson.v1' ? 'logic_json' : 'logic_flow';
    return {
      format,
      payload: withProjectionMetadata(result.payload, source.metadata),
      debug: mergeDebug(source.metadata, result.debug ? { logic_flow: result.debug } : undefined),
    };
  }

  if (input.requestedFormat !== 'logic_json') {
    throw new Error(`unsupported_logic_format: ${String(input.requestedFormat)}`);
  }

  const logicJson = buildLogicJsonPayload(source.payload);
  return {
    format: 'logic_json',
    payload: withProjectionMetadata(logicJson.payload, source.metadata),
    debug: mergeDebug(
      source.metadata,
      mergeDebugObjects(
        collectAnchorDebug(source.payload),
        logicJson.diagnostics ? { compact_anchor_diagnostics: logicJson.diagnostics } : undefined,
      ),
    ),
  };
}

function resolveProjectionSource(input: LogicProjectionInput): ProjectionSource {
  if (input.bridgePayloadSchema !== 'LogicSnapshot.v1') {
    return {
      payload: input.bridgePayload,
      metadata: {},
    };
  }

  const rawSnapshot = input.bridgePayload['raw_snapshot'];
  return {
    payload: isRecord(rawSnapshot) ? rawSnapshot : {},
    metadata: {
      projection_owner: input.bridgePayload['projection_owner'] ?? LOGIC_PROJECTION_OWNER,
      ue_callback_schema: input.bridgePayload['ue_callback_schema'] ?? 'LogicSnapshot.v1',
      source_format: input.bridgePayload['format'],
    },
  };
}

function buildLogicJsonPayload(payload: Record<string, unknown>): {
  payload: Record<string, unknown>;
  diagnostics: Record<string, unknown>[] | undefined;
} {
  const normalized: Record<string, unknown> = {
    schema: 'LogicJson.v1',
    ...payload,
  };
  normalized['schema'] = 'LogicJson.v1';
  delete normalized['format'];
  const enriched = enrichLogicJsonCompactAnchors(normalized);
  return {
    payload: enriched.payload,
    diagnostics: enriched.diagnostics.length > 0 ? enriched.diagnostics : undefined,
  };
}

function withProjectionMetadata(
  payload: Record<string, unknown>,
  metadata: Record<string, unknown>,
): Record<string, unknown> {
  const filtered = Object.fromEntries(
    Object.entries(metadata).filter(([, value]) => value !== undefined),
  );
  return Object.keys(filtered).length > 0
    ? { ...payload, ...filtered }
    : payload;
}

function mergeDebug(
  metadata: Record<string, unknown>,
  debug: Record<string, unknown> | undefined,
): Record<string, unknown> | undefined {
  const filtered = Object.fromEntries(
    Object.entries(metadata).filter(([, value]) => value !== undefined),
  );
  if (Object.keys(filtered).length === 0) {
    return debug;
  }
  return {
    ...(debug ?? {}),
    projection: filtered,
  };
}

function mergeDebugObjects(
  left: Record<string, unknown> | undefined,
  right: Record<string, unknown> | undefined,
): Record<string, unknown> | undefined {
  if (!left) {
    return right;
  }
  if (!right) {
    return left;
  }
  return { ...left, ...right };
}

function collectAnchorDebug(payload: Record<string, unknown>): Record<string, unknown> | undefined {
  const anchors = collectAnchors(payload);
  return anchors.length > 0 ? { anchors } : undefined;
}

function collectAnchors(payload: Record<string, unknown>): Record<string, unknown>[] {
  const anchors: Record<string, unknown>[] = [];
  const logic = isRecord(payload['logic']) ? payload['logic'] : payload;
  collectAnchorsFromContainer(anchors, logic);
  if (logic !== payload) {
    pushRecordArray(anchors, payload['anchors']);
  }
  return stableUniqueRecords(anchors);
}

function collectAnchorsFromContainer(anchors: Record<string, unknown>[], container: Record<string, unknown>): void {
  pushRecordArray(anchors, container['anchors']);
  if (Array.isArray(container['nodes'])) {
    for (const node of container['nodes']) {
      if (!isRecord(node)) {
        continue;
      }
      pushRecord(anchors, node['external_anchor']);
      pushRecord(anchors, node['externalAnchor']);
      pushRecordArray(anchors, node['external_anchors']);
      pushRecordArray(anchors, node['externalAnchors']);
      if (Array.isArray(node['links'])) {
        collectAnchorsFromLinks(anchors, node['links']);
      }
    }
  }
  if (Array.isArray(container['links'])) {
    collectAnchorsFromLinks(anchors, container['links']);
  }
  if (Array.isArray(container['groups'])) {
    for (const group of container['groups']) {
      if (isRecord(group)) {
        collectAnchorsFromContainer(anchors, group);
      }
    }
  }
}

function collectAnchorsFromLinks(anchors: Record<string, unknown>[], links: unknown[]): void {
  for (const link of links) {
    if (!isRecord(link)) {
      continue;
    }
    pushRecord(anchors, link['external_anchor']);
    pushRecord(anchors, link['externalAnchor']);
  }
}

function pushRecord(target: Record<string, unknown>[], value: unknown): void {
  if (isRecord(value)) {
    target.push(value);
  }
}

function pushRecordArray(target: Record<string, unknown>[], value: unknown): void {
  if (!Array.isArray(value)) {
    return;
  }
  for (const item of value) {
    pushRecord(target, item);
  }
}

function stableUniqueRecords(records: Record<string, unknown>[]): Record<string, unknown>[] {
  const byKey = new Map<string, Record<string, unknown>>();
  for (const record of records) {
    byKey.set(JSON.stringify(record), record);
  }
  return [...byKey.entries()]
    .sort(([left], [right]) => left.localeCompare(right))
    .map(([, record]) => record);
}
