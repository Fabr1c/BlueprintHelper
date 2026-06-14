import { buildLogicFlowPayload } from './read-context-logic-flow.js';
import { enrichLogicJsonCompactAnchors } from './read-context-compact-anchor.js';
import { LOGIC_PROJECTION_OWNER } from './read-context-schemas.js';
import { isRecord } from '../bridge-tool-result-utils.js';

export type ReadContextLogicFormat = 'logic_flow' | 'logic_json' | 'logic_md';

export type LogicProjectionInput = {
  requestedFormat: ReadContextLogicFormat;
  bridgePayloadSchema: 'LogicJson.v1' | 'LogicFlow.v1' | 'LogicMd.v1' | 'LogicSnapshot.v1';
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
    return {
      format: 'logic_md',
      payload: withProjectionMetadata(buildLogicMdPayload(source.payload), source.metadata),
      debug: mergeDebug(source.metadata, undefined),
    };
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

function buildLogicMdPayload(payload: Record<string, unknown>): Record<string, unknown> {
  const markdown = firstString(payload['markdown'], payload['logic_md'], payload['md'], payload['text']);
  return {
    schema: 'LogicMd.v1',
    format: 'logic_md',
    markdown: markdown ?? renderFallbackLogicMarkdown(payload),
  };
}

function firstString(...values: unknown[]): string | undefined {
  for (const value of values) {
    if (typeof value === 'string' && value.length > 0) {
      return value;
    }
  }
  return undefined;
}

function renderFallbackLogicMarkdown(payload: Record<string, unknown>): string {
  const lines = ['# Logic'];
  const logic = isRecord(payload['logic']) ? payload['logic'] : payload;
  const material = isRecord(payload['material']) ? payload['material'] : undefined;
  const parameters = Array.isArray(material?.['parameters']) ? material['parameters'] : [];
  const outputs = Array.isArray(material?.['outputs']) ? material['outputs'] : [];
  const materialLinks = collectMaterialDataFlowLinks(logic);
  const anchors = collectAnchors(payload);

  lines.push('', '## Parameters');
  if (parameters.length === 0) {
    lines.push('- none');
  } else {
    for (const parameter of parameters) {
      if (isRecord(parameter)) {
        lines.push(`- ${String(parameter['name'] ?? '<unnamed>')} (${String(parameter['type'] ?? 'unknown')})`);
      }
    }
  }

  lines.push('', '## Material Outputs');
  if (outputs.length === 0) {
    lines.push('- none');
  } else {
    for (const output of outputs) {
      if (isRecord(output)) {
        lines.push(`- ${String(output['property'] ?? '<unknown>')}`);
      }
    }
  }

  lines.push('', '## Material Data Flow');
  if (materialLinks.length === 0) {
    lines.push('- none');
  } else {
    for (const link of materialLinks) {
      lines.push(`- ${formatMaterialLinkEndpoint(link, 'from')} -> ${formatMaterialLinkEndpoint(link, 'to')}`);
    }
  }

  lines.push('', '## Owned Anchors');
  if (anchors.length === 0) {
    lines.push('- none');
  } else {
    for (const anchor of anchors) {
      lines.push(`- ${formatAnchorSummary(anchor)}`);
    }
  }

  return lines.join('\n');
}

function collectMaterialDataFlowLinks(logic: Record<string, unknown>): Record<string, unknown>[] {
  const links = Array.isArray(logic['links']) ? logic['links'] : [];
  return links
    .filter((link): link is Record<string, unknown> => isRecord(link))
    .filter((link) => {
      const kind = String(link['kind'] ?? link['type'] ?? '').toLowerCase();
      return kind === 'material_expression_link' || kind === 'material_output_link';
    });
}

function formatMaterialLinkEndpoint(link: Record<string, unknown>, side: 'from' | 'to'): string {
  const node = firstString(
    link[`${side}_node_key`],
    link[`${side}_node_ref`],
    side === 'from' ? link['source_node_key'] : link['target_node_key'],
    side === 'from' ? link['source_node_ref'] : link['target_node_ref'],
  ) ?? '<unknown>';
  const pin = firstString(
    link[`${side}_pin`],
    side === 'from' ? link['source_pin'] : link['target_pin'],
  );
  return pin ? `${node}.${pin}` : node;
}

function formatAnchorSummary(anchor: Record<string, unknown>): string {
  const target = isRecord(anchor['target']) ? anchor['target'] : anchor;
  const parts = [
    firstString(target['block_id'], anchor['block_id']),
    firstString(target['node_key'], anchor['node_key']),
    firstString(target['expression_guid'], anchor['expression_guid']),
    firstString(target['ownership'], anchor['ownership']),
  ].filter((part): part is string => Boolean(part));
  return parts.length > 0 ? parts.join(' | ') : JSON.stringify(anchor);
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
