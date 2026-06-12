import { createHash } from 'node:crypto';

import { isRecord } from '../bridge-tool-result-utils.js';

type CompactAnchorContext = {
  owner: 'node' | 'body';
  label?: string;
};

export type CompactAnchorProjection = {
  anchor_type: string;
  kind?: string;
  label: string;
  anchor_ref: string;
};

export type CompactAnchorDiagnostic = {
  code: 'missing_link_ownership' | 'unsupported_link_ownership';
  path: string;
  message: string;
};

export type CompactAnchorEnrichmentResult = {
  payload: Record<string, unknown>;
  diagnostics: CompactAnchorDiagnostic[];
};

export type ParsedCompactAnchorRef =
  | {
    anchorType: 'external_node';
    version: 'v1';
    nodeKey: string;
    fingerprint: string;
  }
  | {
    anchorType: 'external_pin';
    version: 'v1';
    kind: string;
    nodeKey: string;
    pinKey: string;
    fingerprint: string;
  }
  | {
    anchorType: 'external_link';
    version: 'v1';
    kind: string;
    sourceNodeKey: string;
    sourcePinKey: string;
    targetNodeKey: string;
    targetPinKey: string;
    fingerprint: string;
  }
  | {
    anchorType: 'external_body';
    version: 'v1';
    scope: string;
    entryNodeKey: string;
    fingerprint: string;
  };

type AnchorFacts = {
  nodeGuid: string;
  nodeClass?: string;
  pinName?: string;
  pinDirection?: string;
  semanticRole: string;
  fingerprint: string;
};

export function enrichLogicJsonCompactAnchors(payload: Record<string, unknown>): CompactAnchorEnrichmentResult {
  const diagnostics: CompactAnchorDiagnostic[] = [];
  const logic = isRecord(payload['logic']) ? payload['logic'] : payload;
  const nextPayload = { ...payload };
  if (logic === payload) {
    return {
      payload: enrichLogicContainer(nextPayload, diagnostics, '$'),
      diagnostics,
    };
  }

  nextPayload['logic'] = enrichLogicContainer(logic, diagnostics, '$.logic');
  return { payload: nextPayload, diagnostics };
}

export function parseCompactAnchorRef(value: string): ParsedCompactAnchorRef | undefined {
  const text = value.trim();
  if (text.length === 0) {
    return undefined;
  }

  const nodeMatch = /^xnode:v1:([A-Za-z0-9_]+)#([A-Za-z0-9]+)$/u.exec(text);
  if (nodeMatch) {
    return {
      anchorType: 'external_node',
      version: 'v1',
      nodeKey: nodeMatch[1] ?? '',
      fingerprint: nodeMatch[2] ?? '',
    };
  }

  const pinMatch = /^xpin:v1:([edu]):([A-Za-z0-9_]+)\.([A-Za-z0-9_]+)#([A-Za-z0-9]+)$/u.exec(text);
  if (pinMatch) {
    return {
      anchorType: 'external_pin',
      version: 'v1',
      kind: expandKindPrefix(pinMatch[1] ?? 'u'),
      nodeKey: pinMatch[2] ?? '',
      pinKey: pinMatch[3] ?? '',
      fingerprint: pinMatch[4] ?? '',
    };
  }

  const linkMatch = /^xlink:v1:([edu]):([A-Za-z0-9_]+)\.([A-Za-z0-9_]+)>([A-Za-z0-9_]+)\.([A-Za-z0-9_]+)#([A-Za-z0-9]+)$/u.exec(text);
  if (linkMatch) {
    return {
      anchorType: 'external_link',
      version: 'v1',
      kind: expandKindPrefix(linkMatch[1] ?? 'u'),
      sourceNodeKey: linkMatch[2] ?? '',
      sourcePinKey: linkMatch[3] ?? '',
      targetNodeKey: linkMatch[4] ?? '',
      targetPinKey: linkMatch[5] ?? '',
      fingerprint: linkMatch[6] ?? '',
    };
  }

  const bodyMatch = /^xbody:v1:([A-Za-z0-9_]+):([A-Za-z0-9_]+)#([A-Za-z0-9]+)$/u.exec(text);
  if (bodyMatch) {
    return {
      anchorType: 'external_body',
      version: 'v1',
      scope: bodyMatch[1] ?? '',
      entryNodeKey: bodyMatch[2] ?? '',
      fingerprint: bodyMatch[3] ?? '',
    };
  }

  return undefined;
}

function enrichLogicContainer(
  container: Record<string, unknown>,
  diagnostics: CompactAnchorDiagnostic[],
  path: string,
): Record<string, unknown> {
  const next = { ...container };
  const nodeAnchors = collectNodeAnchors(container);

  if (Array.isArray(container['links'])) {
    next['links'] = container['links'].map((link, index) => (
      enrichLink(link, nodeAnchors, diagnostics, `${path}.links[${index}]`)
    ));
  }

  if (Array.isArray(container['nodes'])) {
    next['nodes'] = container['nodes'].map((node, index) => (
      enrichNode(node, nodeAnchors, diagnostics, `${path}.nodes[${index}]`)
    ));
  }

  if (Array.isArray(container['groups'])) {
    next['groups'] = container['groups'].map((group, index) => (
      isRecord(group) ? enrichLogicContainer(group, diagnostics, `${path}.groups[${index}]`) : group
    ));
  }

  return next;
}

function enrichNode(
  value: unknown,
  nodeAnchors: Map<string, AnchorFacts>,
  diagnostics: CompactAnchorDiagnostic[],
  path: string,
): unknown {
  if (!isRecord(value)) {
    return value;
  }

  const next = { ...value };
  const nodeRef = readString(next, ['node_ref', 'ref', 'id', 'name']);
  const nodeName = readString(next, ['name', 'display_name', 'title', 'node_ref', 'ref']);
  const compactNode = buildCompactAnchor(next['external_anchor'], {
    owner: 'node',
    label: nodeName ?? nodeRef,
  });
  if (compactNode) {
    applyCompact(next, compactNode);
  }

  const nodeAnchor = readNodeAnchorFacts(value);
  if (Array.isArray(next['pins']) && nodeAnchor) {
    next['pins'] = next['pins'].map((pin) => enrichPin(pin, nodeAnchor));
  }

  if (Array.isArray(next['links'])) {
    next['links'] = next['links'].map((link, index) => (
      enrichLink(link, nodeAnchors, diagnostics, `${path}.links[${index}]`, nodeRef)
    ));
  }
  return next;
}

function enrichPin(value: unknown, nodeAnchor: AnchorFacts): unknown {
  if (!isRecord(value)) {
    return value;
  }

  const next = { ...value };
  const pinName = readString(next, ['pin_name', 'pin_ref', 'name', 'ref']);
  if (!pinName) {
    return next;
  }

  const pinKind = normalizePinKind(readString(next, ['kind', 'type', 'pin_type', 'pin_category', 'category']));
  applyCompact(next, {
    anchor_type: 'external_pin',
    kind: pinKind,
    label: `${nodeAnchor.nodeGuid.slice(0, 8)}.${pinName}`,
    anchor_ref: `xpin:v1:${kindPrefix(pinKind)}:${shortToken(nodeAnchor.nodeGuid)}.${shortPin(pinName)}#${compactFingerprint([
      nodeAnchor.nodeGuid,
      pinName,
      nodeAnchor.fingerprint,
    ])}`,
  });
  return next;
}

function enrichLink(
  value: unknown,
  nodeAnchors: Map<string, AnchorFacts>,
  diagnostics: CompactAnchorDiagnostic[],
  path: string,
  fallbackFromNode?: string,
): unknown {
  if (!isRecord(value)) {
    return value;
  }

  const next = { ...value };
  const fromNode = readString(next, ['from_node', 'source_node', 'source', 'fromNode', 'from_id']) ?? fallbackFromNode;
  const fromPin = readString(next, ['from_pin', 'source_pin', 'fromPin', 'pin_ref']);
  const toNode = readString(next, ['to_node', 'target_node', 'target', 'toNode', 'to_id']);
  const toPin = readString(next, ['to_pin', 'target_pin', 'toPin']);
  const linkKind = normalizeKind(readString(next, ['kind', 'type']));
  const label = formatLinkLabel(fromNode, fromPin, toNode, toPin);
  const linkAnchor = readAnchorFacts(next['external_anchor']) ?? readAnchorFacts(next['externalAnchor']);
  const sourceNodeAnchor = fromNode ? nodeAnchors.get(fromNode) : undefined;
  const targetNodeAnchor = toNode ? nodeAnchors.get(toNode) : undefined;
  const sourceAnchor = selectEndpointAnchor(linkAnchor, 'output')
    ?? sourceNodeAnchor;
  const targetAnchor = selectEndpointAnchor(linkAnchor, 'input')
    ?? targetNodeAnchor;
  const compactLink = buildCompactLinkAnchor({
    sourceAnchor,
    targetAnchor,
    sourceNodeAnchor,
    targetNodeAnchor,
    linkKind,
    label,
    fromPin,
    toPin,
  });
  if (compactLink) {
    if (!canProjectExternalLinkAnchor(next, path, diagnostics)) {
      return next;
    }
    applyCompact(next, compactLink);
  }
  return next;
}

function canProjectExternalLinkAnchor(
  link: Record<string, unknown>,
  path: string,
  diagnostics: CompactAnchorDiagnostic[],
): boolean {
  const ownership = typeof link['ownership'] === 'string' ? link['ownership'].trim() : '';
  if (ownership === 'external_user' || ownership === 'external_boundary') {
    return true;
  }
  if (ownership === 'owned_internal') {
    return false;
  }
  if (ownership.length === 0) {
    diagnostics.push({
      code: 'missing_link_ownership',
      path,
      message: 'External compact link anchors require logic_json.links[].ownership.',
    });
    return false;
  }
  diagnostics.push({
    code: 'unsupported_link_ownership',
    path,
    message: `Unsupported link ownership '${ownership}'.`,
  });
  return false;
}

function buildCompactAnchor(value: unknown, context: CompactAnchorContext): CompactAnchorProjection | undefined {
  const facts = readAnchorFacts(value);
  if (!facts) {
    return undefined;
  }

  if (facts.semanticRole === 'body_entry' || context.owner === 'body') {
    return {
      anchor_type: 'external_body',
      kind: 'body',
      label: context.label ?? 'External body',
      anchor_ref: `xbody:v1:body:${shortToken(facts.nodeGuid)}#${shortFingerprint(facts.fingerprint)}`,
    };
  }

  return {
    anchor_type: 'external_node',
    kind: 'node',
    label: context.label ?? facts.nodeClass ?? 'External node',
    anchor_ref: `xnode:v1:${shortToken(facts.nodeGuid)}#${shortFingerprint(facts.fingerprint)}`,
  };
}

function buildCompactLinkAnchor(input: {
  sourceAnchor: AnchorFacts | undefined;
  targetAnchor: AnchorFacts | undefined;
  sourceNodeAnchor?: AnchorFacts;
  targetNodeAnchor?: AnchorFacts;
  linkKind: string;
  label: string | undefined;
  fromPin: string | undefined;
  toPin: string | undefined;
}): CompactAnchorProjection | undefined {
  if (!input.sourceAnchor || !input.targetAnchor) {
    return undefined;
  }
  const sourcePin = input.fromPin ?? input.sourceAnchor.pinName;
  const targetPin = input.toPin ?? input.targetAnchor.pinName;
  if (!sourcePin || !targetPin) {
    return undefined;
  }
  const source = `${shortToken(input.sourceAnchor.nodeGuid)}.${shortPin(sourcePin)}`;
  const target = `${shortToken(input.targetAnchor.nodeGuid)}.${shortPin(targetPin)}`;
  const fingerprint = compactFingerprint([
    input.linkKind,
    input.sourceAnchor.nodeGuid,
    sourcePin,
    input.sourceNodeAnchor?.fingerprint ?? input.sourceAnchor.fingerprint,
    input.targetAnchor.nodeGuid,
    targetPin,
    input.targetNodeAnchor?.fingerprint ?? input.targetAnchor.fingerprint,
  ]);
  return {
    anchor_type: 'external_link',
    kind: input.linkKind,
    label: input.label ?? `${sourcePin} -> ${targetPin}`,
    anchor_ref: `xlink:v1:${kindPrefix(input.linkKind)}:${source}>${target}#${fingerprint}`,
  };
}

function collectNodeAnchors(logic: Record<string, unknown>): Map<string, AnchorFacts> {
  const out = new Map<string, AnchorFacts>();
  if (!Array.isArray(logic['nodes'])) {
    return out;
  }
  for (const node of logic['nodes']) {
    if (!isRecord(node)) {
      continue;
    }
    const anchor = readNodeAnchorFacts(node);
    if (!anchor) {
      continue;
    }
    for (const ref of readNodeRefs(node)) {
      out.set(ref, anchor);
    }
  }
  return out;
}

function readNodeRefs(node: Record<string, unknown>): string[] {
  const refs = new Set<string>();
  for (const key of ['node_ref', 'ref', 'id', 'node_id', 'from_id', 'to_id']) {
    const value = node[key];
    if (typeof value === 'string' && value.trim().length > 0) {
      refs.add(value.trim());
    }
  }
  return [...refs];
}

function readAnchorFacts(value: unknown): AnchorFacts | undefined {
  if (!isRecord(value)) {
    return undefined;
  }
  const schema = readString(value, ['schema']);
  if (schema !== 'BlueprintHelper.ExternalGraphAnchor.v1') {
    return undefined;
  }
  const semanticRole = readString(value, ['semantic_role', 'semanticRole']);
  const nodeGuid = readString(value, ['node_guid', 'nodeGuid']);
  const fingerprint = readString(value, ['fingerprint']);
  if (!semanticRole || !nodeGuid || !fingerprint) {
    return undefined;
  }
  return {
    nodeGuid,
    nodeClass: readString(value, ['node_class', 'nodeClass']),
    pinName: readString(value, ['pin_name', 'pinName']),
    pinDirection: readString(value, ['pin_direction', 'pinDirection']),
    semanticRole,
    fingerprint,
  };
}

function readNodeAnchorFacts(node: Record<string, unknown>): AnchorFacts | undefined {
  const primary = readAnchorFacts(node['external_anchor']) ?? readAnchorFacts(node['externalAnchor']);
  if (primary && isNodeAnchorRole(primary.semanticRole)) {
    return primary;
  }

  for (const key of ['external_anchors', 'externalAnchors']) {
    const value = node[key];
    if (!Array.isArray(value)) {
      continue;
    }
    for (const entry of value) {
      const anchor = readAnchorFacts(entry);
      if (anchor && isNodeAnchorRole(anchor.semanticRole)) {
        return anchor;
      }
    }
  }

  return undefined;
}

function selectEndpointAnchor(anchor: AnchorFacts | undefined, pinDirection: 'input' | 'output'): AnchorFacts | undefined {
  if (!anchor) {
    return undefined;
  }
  if (!anchor.pinName || !anchor.pinDirection) {
    return undefined;
  }
  return anchor.pinDirection?.toLowerCase() === pinDirection ? anchor : undefined;
}

function isNodeAnchorRole(value: string): boolean {
  return value === 'node' || value === 'body_entry';
}

function applyCompact(target: Record<string, unknown>, compact: CompactAnchorProjection): void {
  if (target['anchor_type'] === undefined) {
    target['anchor_type'] = compact.anchor_type;
  }
  if (target['kind'] === undefined && compact.kind !== undefined) {
    target['kind'] = compact.kind;
  }
  if (target['label'] === undefined) {
    target['label'] = compact.label;
  }
  if (target['anchor_ref'] === undefined) {
    target['anchor_ref'] = compact.anchor_ref;
  }
}

function readString(record: Record<string, unknown>, keys: string[]): string | undefined {
  for (const key of keys) {
    const value = record[key];
    if (typeof value === 'string' && value.trim().length > 0) {
      return value.trim();
    }
  }
  return undefined;
}

function formatLinkLabel(
  fromNode: string | undefined,
  fromPin: string | undefined,
  toNode: string | undefined,
  toPin: string | undefined,
): string | undefined {
  if (!fromNode && !fromPin && !toNode && !toPin) {
    return undefined;
  }
  return `${fromNode ?? 'source'}.${fromPin ?? 'pin'} -> ${toNode ?? 'target'}.${toPin ?? 'pin'}`;
}

function normalizeKind(value: string | undefined): string {
  const normalized = value?.toLowerCase();
  if (normalized === 'exec' || normalized === 'execution') {
    return 'exec';
  }
  if (normalized === 'data') {
    return 'data';
  }
  return 'unknown';
}

function normalizePinKind(value: string | undefined): string {
  const normalized = normalizeKind(value);
  return normalized === 'exec' ? 'exec' : 'data';
}

function kindPrefix(kind: string): string {
  if (kind === 'exec') {
    return 'e';
  }
  if (kind === 'data') {
    return 'd';
  }
  return 'u';
}

function expandKindPrefix(kind: string): string {
  if (kind === 'e') {
    return 'exec';
  }
  if (kind === 'd') {
    return 'data';
  }
  return 'unknown';
}

function shortToken(value: string): string {
  const cleaned = value.replace(/[^a-zA-Z0-9_]/g, '');
  if (cleaned.length <= 8) {
    return cleaned || 'ref';
  }
  return cleaned.slice(0, 8);
}

function shortPin(value: string): string {
  const cleaned = value.replace(/[^a-zA-Z0-9_]/g, '');
  if (cleaned.length <= 16) {
    return cleaned || 'pin';
  }
  return cleaned.slice(0, 16);
}

function shortFingerprint(value: string): string {
  const cleaned = value.replace(/[^a-zA-Z0-9]/g, '');
  if (cleaned.length <= 10) {
    return cleaned || 'fp';
  }
  return cleaned.slice(0, 10);
}

function compactFingerprint(parts: string[]): string {
  return createHash('sha1').update(parts.join('|')).digest('hex').slice(0, 10);
}
