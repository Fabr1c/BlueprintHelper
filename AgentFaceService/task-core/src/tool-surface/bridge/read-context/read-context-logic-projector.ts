import { buildLogicFlowPayload } from './read-context-logic-flow.js';
import { LOGIC_PROJECTION_OWNER } from './read-context-schemas.js';
import { isRecord } from '../bridge-tool-result-utils.js';

export type ReadContextLogicFormat = 'logic_flow' | 'logic_md' | 'logic_json';

export type LogicProjectionInput = {
  requestedFormat: ReadContextLogicFormat;
  bridgePayloadSchema: 'LogicMd.v1' | 'LogicJson.v1' | 'LogicFlow.v1' | 'LogicSnapshot.v1';
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

  if (input.requestedFormat === 'logic_md') {
    const payload = buildLogicMdPayload(source.payload, input);
    return {
      format: 'logic_md',
      payload: withProjectionMetadata(payload, source.metadata),
      debug: mergeDebug(source.metadata, collectAnchorDebug(source.payload)),
    };
  }

  return {
    format: 'logic_json',
    payload: withProjectionMetadata(buildLogicJsonPayload(source.payload), source.metadata),
    debug: mergeDebug(source.metadata, collectAnchorDebug(source.payload)),
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

function buildLogicJsonPayload(payload: Record<string, unknown>): Record<string, unknown> {
  const normalized: Record<string, unknown> = {
    schema: 'LogicJson.v1',
    ...payload,
  };
  normalized['schema'] = 'LogicJson.v1';
  delete normalized['format'];
  return normalized;
}

function buildLogicMdPayload(payload: Record<string, unknown>, input: LogicProjectionInput): Record<string, unknown> {
  if (payload['schema'] === 'LogicMd.v1' && typeof payload['markdown'] === 'string') {
    const normalized: Record<string, unknown> = {
      schema: 'LogicMd.v1',
      ...payload,
    };
    delete normalized['format'];
    return normalized;
  }

  const title = readTargetTitle(input.target);
  return {
    schema: 'LogicMd.v1',
    markdown: [
      `# ${title}`,
      '',
      renderLogicJsonAsMarkdown(payload),
    ].join('\n'),
    stats: isRecord(payload['stats']) ? payload['stats'] : undefined,
  };
}

function renderLogicJsonAsMarkdown(payload: Record<string, unknown>): string {
  const logic = isRecord(payload['logic']) ? payload['logic'] : payload;
  const nodes = Array.isArray(logic['nodes']) ? logic['nodes'] : [];
  const links = Array.isArray(logic['links']) ? logic['links'] : [];
  const lines = [
    `Nodes: ${nodes.length}`,
    `Links: ${links.length}`,
  ];

  for (const node of nodes) {
    if (!isRecord(node)) {
      continue;
    }
    const name = readString(node, ['name', 'display_name', 'title', 'node_ref', 'ref']) ?? '<unnamed>';
    const kind = readString(node, ['kind', 'category']);
    lines.push(kind ? `- ${name} (${kind})` : `- ${name}`);
  }

  return lines.join('\n');
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

function collectAnchorDebug(payload: Record<string, unknown>): Record<string, unknown> | undefined {
  const anchors = collectAnchors(payload);
  return anchors.length > 0 ? { anchors } : undefined;
}

function collectAnchors(payload: Record<string, unknown>): Record<string, unknown>[] {
  const anchors: Record<string, unknown>[] = [];
  const logic = isRecord(payload['logic']) ? payload['logic'] : payload;
  pushRecordArray(anchors, logic['anchors']);
  pushRecordArray(anchors, payload['anchors']);
  if (Array.isArray(logic['nodes'])) {
    for (const node of logic['nodes']) {
      if (!isRecord(node)) {
        continue;
      }
      pushRecord(anchors, node['external_anchor']);
      pushRecord(anchors, node['externalAnchor']);
      pushRecordArray(anchors, node['external_anchors']);
      pushRecordArray(anchors, node['externalAnchors']);
    }
  }
  if (Array.isArray(logic['links'])) {
    for (const link of logic['links']) {
      if (!isRecord(link)) {
        continue;
      }
      pushRecord(anchors, link['external_anchor']);
      pushRecord(anchors, link['externalAnchor']);
    }
  }
  return stableUniqueRecords(anchors);
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

function readTargetTitle(target: Record<string, unknown>): string {
  return readString(target, ['target_name', 'graph', 'asset_path']) ?? 'ReadContext LogicMD';
}

function readString(record: Record<string, unknown>, keys: string[]): string | undefined {
  for (const key of keys) {
    const value = record[key];
    if (typeof value === 'string' && value.length > 0) {
      return value;
    }
  }
  return undefined;
}
