import { isRecord } from '../bridge-tool-result-utils.js';

type LogicFlowMode = 'execflow' | 'dataflow';
type LogicFlowLinkType = 'exec' | 'data' | 'unknown';

type LogicFlowNode = {
  ref: string;
  name: string;
  kind?: string;
  externalAnchor?: Record<string, unknown>;
  externalAnchors?: Record<string, unknown>[];
};

type LogicFlowLink = {
  type: LogicFlowLinkType;
  fromNode: string;
  fromPin: string;
  toNode: string;
  toPin: string;
  externalAnchor?: Record<string, unknown>;
};

type LogicFlowGraph = {
  name?: string;
  nodes: LogicFlowNode[];
  links: LogicFlowLink[];
  anchors: Record<string, unknown>[];
  stats: Record<string, unknown>;
};

export type LogicFlowBuildResult = {
  payload: Record<string, unknown>;
  debug?: Record<string, unknown>;
};

const EXEC_PIN_ORDER = [
  'then',
  'execute',
  'true',
  'false',
  'then0',
  'then1',
  'then2',
  'loopbody',
  'completed',
  'finished',
];

export function buildLogicFlowPayload(payload: Record<string, unknown>): LogicFlowBuildResult {
  const graphs = normalizeLogicFlowGraphs(payload);
  const warnings = uniqueStrings(graphs.flatMap((graph) => buildLogicFlowWarnings(graph)));
  const anchors = collectLogicFlowAnchors(graphs);
  const debug = buildLogicFlowDebug(anchors);

  if (warnings.includes('unknown_link')) {
    return {
      payload: buildDegradedLogicJsonPayload(payload, 'unknown_link'),
      debug: {
        ...debug,
        degraded: true,
        reason: 'unknown_link',
      },
    };
  }

  const mode = chooseMode(graphs);
  const flow = graphs.map((graph) => buildGraphFlow(graph, mode)).filter(Boolean).join('\n\n');

  return {
    payload: {
      schema: 'LogicFlow.v1',
      mode,
      flow,
      stats: isRecord(payload['stats']) ? payload['stats'] : buildAggregateStats(graphs),
      warnings,
    },
    debug,
  };
}

function buildDegradedLogicJsonPayload(
  payload: Record<string, unknown>,
  reason: 'unknown_link',
): Record<string, unknown> {
  const degraded: Record<string, unknown> = {
    ...payload,
    schema: 'LogicJson.v1',
    format: 'logic_json',
    requested_format: 'logic_flow',
    warnings: [`logic_flow_degraded_${reason}`],
  };
  delete degraded['mode'];
  delete degraded['flow'];
  delete degraded['anchors'];
  return degraded;
}

function buildLogicFlowDebug(anchors: Record<string, unknown>[]): Record<string, unknown> | undefined {
  return anchors.length > 0 ? { anchors } : undefined;
}

function chooseMode(graphs: LogicFlowGraph[]): LogicFlowMode {
  return graphs.some((graph) => graph.links.some((link) => link.type === 'exec')) ? 'execflow' : 'dataflow';
}

function buildGraphFlow(graph: LogicFlowGraph, mode: LogicFlowMode): string {
  const execLinks = graph.links.filter((link) => link.type === 'exec');
  const dataLinks = graph.links.filter((link) => link.type === 'data');
  const body = execLinks.length > 0
    ? buildExecFlow(graph, execLinks, dataLinks)
    : buildDataFlow(graph, dataLinks);
  if (!graph.name || mode === 'dataflow' || graph.name === '<root>') {
    return body;
  }
  return `flow ${graph.name}:\n${indentLines(body)}`;
}

function buildExecFlow(
  graph: LogicFlowGraph,
  execLinks: LogicFlowLink[],
  dataLinks: LogicFlowLink[],
): string {
  const byRef = new Map(graph.nodes.map((node) => [node.ref, node]));
  const outgoingExec = groupLinks(execLinks, 'fromNode');
  const incomingExec = groupLinks(execLinks, 'toNode');
  const incomingData = groupLinks(dataLinks, 'toNode');
  const outgoingData = groupLinks(dataLinks, 'fromNode');
  const roots = findExecRoots(graph.nodes, execLinks, incomingExec);
  const lines = roots.map((root) => buildExecLine(
    root.ref,
    byRef,
    outgoingExec,
    incomingData,
    outgoingData,
    new Set(),
  ));
  const orphanSummary = buildOrphanSummary(graph.nodes, execLinks, dataLinks, roots.map((node) => node.ref));
  if (orphanSummary) {
    lines.push(orphanSummary);
  }
  return lines.filter(Boolean).join('\n');
}

function buildExecLine(
  nodeRef: string,
  byRef: Map<string, LogicFlowNode>,
  outgoingExec: Map<string, LogicFlowLink[]>,
  incomingData: Map<string, LogicFlowLink[]>,
  outgoingData: Map<string, LogicFlowLink[]>,
  visited: Set<string>,
): string {
  const node = byRef.get(nodeRef);
  if (!node) {
    return nodeRef;
  }

  const current = formatExecNode(node, incomingData.get(nodeRef) ?? [], outgoingData.get(nodeRef) ?? []);
  if (visited.has(nodeRef)) {
    return `${current} -> <cycle:${nodeRef}>`;
  }
  visited.add(nodeRef);

  const nextLinks = sortExecLinks(outgoingExec.get(nodeRef) ?? []);
  if (nextLinks.length === 0) {
    return current;
  }
  if (nextLinks.length === 1) {
    return `${current} -> ${buildExecLine(
      nextLinks[0].toNode,
      byRef,
      outgoingExec,
      incomingData,
      outgoingData,
      new Set(visited),
    )}`;
  }

  const branches = nextLinks.map((link) => (
    `  ${link.fromPin || 'then'} -> ${buildExecLine(
      link.toNode,
      byRef,
      outgoingExec,
      incomingData,
      outgoingData,
      new Set(visited),
    )}`
  ));
  return [current, ...branches].join('\n');
}

function buildDataFlow(graph: LogicFlowGraph, dataLinks: LogicFlowLink[]): string {
  const incomingData = groupLinks(dataLinks, 'toNode');
  const outgoingData = groupLinks(dataLinks, 'fromNode');
  const names = new Map<string, string>();
  const lines = ['dataflow:'];

  graph.nodes.forEach((node, index) => {
    const variable = isReturnNode(node) ? 'ReturnValue' : `$p${index}`;
    names.set(node.ref, variable);
    const inputs = (incomingData.get(node.ref) ?? []).map((link) => {
      const sourceName = names.get(link.fromNode);
      return sourceName ?? formatSourceReference(graph.nodes, link.fromNode, link.fromPin);
    });
    const outputPins = uniqueStrings((outgoingData.get(node.ref) ?? [])
      .map((link) => link.fromPin)
      .filter((pin) => pin.length > 0 && !isDefaultValuePin(pin)));
    const rendered = formatDataNode(node, inputs, outputPins);
    lines.push(`  ${variable} = ${rendered}`);
  });

  return lines.join('\n');
}

function formatExecNode(
  node: LogicFlowNode,
  incomingData: LogicFlowLink[],
  outgoingData: LogicFlowLink[],
): string {
  const inputs = incomingData
    .filter((link) => link.toPin.length > 0 && !isDefaultExecPin(link.toPin))
    .map((link) => `${link.toPin}=${formatRelativeDataReference(link.fromPin)}`);
  const outputs = uniqueStrings(outgoingData
    .map((link) => link.fromPin)
    .filter((pin) => pin.length > 0 && !isDefaultExecPin(pin) && !isDefaultValuePin(pin)));
  return formatNodeExpression(node.name, inputs, outputs);
}

function formatDataNode(node: LogicFlowNode, inputs: string[], outputs: string[]): string {
  return formatNodeExpression(node.name, inputs, outputs);
}

function formatNodeExpression(name: string, inputs: string[], outputs: string[]): string {
  const inputText = inputs.length > 0 ? `[${inputs.join(', ')}]` : '';
  const outputText = outputs.length > 0 ? `(${outputs.join(',')})` : '';
  return `${name}${inputText}${outputText}`;
}

function normalizeLogicFlowGraphs(payload: Record<string, unknown>): LogicFlowGraph[] {
  const logic = isRecord(payload['logic']) ? payload['logic'] : payload;
  if (Array.isArray(logic['groups']) && logic['groups'].length > 0) {
    return logic['groups']
      .map((group, index) => normalizeGroup(group, payload, index))
      .filter((graph): graph is LogicFlowGraph => Boolean(graph));
  }
  return [normalizeGraph(logic, payload)];
}

function normalizeGroup(value: unknown, payload: Record<string, unknown>, index: number): LogicFlowGraph | undefined {
  if (!isRecord(value)) {
    return undefined;
  }
  const entry = isRecord(value['entry']) ? value['entry'] : undefined;
  const name = readString(value, ['name', 'block_id', 'group_entry_node_path'])
    ?? readString(entry, ['name', 'node_ref'])
    ?? `group_${index}`;
  return normalizeGraph(value, payload, name);
}

function normalizeGraph(
  logic: Record<string, unknown>,
  payload: Record<string, unknown>,
  name?: string,
): LogicFlowGraph {
  const nodes = Array.isArray(logic['nodes'])
    ? logic['nodes'].map((node, index) => normalizeNode(node, index)).filter((node): node is LogicFlowNode => Boolean(node))
    : [];
  const links = collectLinks(logic);
  const stats = isRecord(payload['stats']) ? payload['stats'] : buildStats(nodes, links);
  const anchors = collectGraphAnchors(logic, nodes, links);
  return { name, nodes, links, anchors, stats };
}

function normalizeNode(value: unknown, index: number): LogicFlowNode | undefined {
  if (!isRecord(value)) {
    return undefined;
  }
  const ref = readString(value, ['node_ref', 'ref', 'id']) ?? `nodes[${index}]`;
  const name = readString(value, ['name', 'display_name', 'title', 'ref']) ?? ref;
  return {
    ref,
    name,
    kind: readString(value, ['kind', 'category']),
    externalAnchor: readObject(value, ['external_anchor', 'externalAnchor']),
    externalAnchors: readObjectArray(value, ['external_anchors', 'externalAnchors']),
  };
}

function collectLinks(logic: Record<string, unknown>): LogicFlowLink[] {
  const links: LogicFlowLink[] = [];
  if (Array.isArray(logic['links'])) {
    for (const value of logic['links']) {
      const link = normalizeLink(value);
      if (link) {
        links.push(link);
      }
    }
  }
  if (Array.isArray(logic['nodes'])) {
    for (const node of logic['nodes']) {
      if (!isRecord(node) || !Array.isArray(node['links'])) {
        continue;
      }
      const fromNode = readString(node, ['node_ref', 'ref', 'id']) ?? '';
      for (const value of node['links']) {
        const link = normalizeLink(value, fromNode);
        if (link) {
          links.push(link);
        }
      }
    }
  }
  return dedupeLinks(links);
}

function normalizeLink(value: unknown, fallbackFromNode = ''): LogicFlowLink | undefined {
  if (!isRecord(value)) {
    return undefined;
  }

  const fromNode = readString(value, ['from_node', 'source_node', 'source', 'fromNode']) ?? fallbackFromNode;
  const toNode = readString(value, ['to_node', 'target_node', 'target', 'toNode']) ?? '';
  if (!fromNode || !toNode) {
    return undefined;
  }

  return {
    type: normalizeLinkType(readString(value, ['type', 'kind'])),
    fromNode,
    fromPin: readString(value, ['from_pin', 'source_pin', 'fromPin', 'pin_ref']) ?? '',
    toNode,
    toPin: readString(value, ['to_pin', 'target_pin', 'toPin']) ?? '',
    externalAnchor: readObject(value, ['external_anchor', 'externalAnchor']),
  };
}

function normalizeLinkType(value: string | undefined): LogicFlowLinkType {
  if (value?.toLowerCase() === 'exec') return 'exec';
  if (value?.toLowerCase() === 'data') return 'data';
  return 'unknown';
}

function findExecRoots(
  nodes: LogicFlowNode[],
  execLinks: LogicFlowLink[],
  incomingExec: Map<string, LogicFlowLink[]>,
): LogicFlowNode[] {
  const withOutgoing = new Set(execLinks.map((link) => link.fromNode));
  const entryRoots = nodes.filter((node) => isEntryNode(node) && withOutgoing.has(node.ref));
  if (entryRoots.length > 0) {
    return entryRoots;
  }
  const roots = nodes.filter((node) => withOutgoing.has(node.ref) && !(incomingExec.get(node.ref)?.length));
  return roots.length > 0 ? roots : nodes.slice(0, 1);
}

function isEntryNode(node: LogicFlowNode): boolean {
  const kind = node.kind?.toLowerCase();
  return kind === 'event' || kind === 'function' || kind === 'custom_event' || node.name.startsWith('事件');
}

function isReturnNode(node: LogicFlowNode): boolean {
  const normalized = node.name.toLowerCase();
  return normalized === 'returnvalue' || normalized === 'return' || normalized === 'return node';
}

function buildOrphanSummary(
  nodes: LogicFlowNode[],
  execLinks: LogicFlowLink[],
  dataLinks: LogicFlowLink[],
  roots: string[],
): string | undefined {
  const touched = new Set<string>(roots);
  for (const link of [...execLinks, ...dataLinks]) {
    touched.add(link.fromNode);
    touched.add(link.toNode);
  }
  const orphans = nodes.filter((node) => !touched.has(node.ref));
  if (orphans.length === 0) {
    return undefined;
  }

  const counts = new Map<string, number>();
  for (const node of orphans) {
    counts.set(node.name, (counts.get(node.name) ?? 0) + 1);
  }
  return `orphans: ${[...counts.entries()]
    .map(([name, count]) => (count > 1 ? `${name} x${count}` : name))
    .join(', ')}`;
}

function groupLinks(links: LogicFlowLink[], key: 'fromNode' | 'toNode'): Map<string, LogicFlowLink[]> {
  const groups = new Map<string, LogicFlowLink[]>();
  for (const link of links) {
    const group = groups.get(link[key]) ?? [];
    group.push(link);
    groups.set(link[key], group);
  }
  return groups;
}

function sortExecLinks(links: LogicFlowLink[]): LogicFlowLink[] {
  return [...links].sort((a, b) => execPinRank(a.fromPin) - execPinRank(b.fromPin)
    || a.fromPin.localeCompare(b.fromPin)
    || a.toNode.localeCompare(b.toNode));
}

function execPinRank(pin: string): number {
  const normalized = pin.toLowerCase();
  const thenMatch = /^then\s*(\d+)$/i.exec(normalized);
  if (thenMatch) {
    return 10 + Number.parseInt(thenMatch[1], 10);
  }
  const index = EXEC_PIN_ORDER.indexOf(normalized);
  return index >= 0 ? index : EXEC_PIN_ORDER.length;
}

function buildLogicFlowWarnings(graph: LogicFlowGraph): string[] {
  const warnings: string[] = [];
  if (graph.links.some((link) => link.type === 'unknown')) {
    warnings.push('unknown_link');
  }
  if (graph.nodes.length === 0) {
    warnings.push('empty_logic');
  }
  const macroNames = ['macro', 'collapsed'];
  const hasAmbiguousMacro = graph.nodes.some((node) => (
    macroNames.some((marker) => node.kind?.toLowerCase().includes(marker))
    && graph.links.filter((link) => link.fromNode === node.ref && link.type === 'exec').length === 0
  ));
  if (hasAmbiguousMacro) {
    warnings.push('macro_boundary_ambiguous');
  }
  return warnings;
}

function buildStats(nodes: LogicFlowNode[], links: LogicFlowLink[]): Record<string, unknown> {
  return {
    nodes: nodes.length,
    exec_links: links.filter((link) => link.type === 'exec').length,
    data_links: links.filter((link) => link.type === 'data').length,
  };
}

function buildAggregateStats(graphs: LogicFlowGraph[]): Record<string, unknown> {
  const nodes = graphs.reduce((total, graph) => total + graph.nodes.length, 0);
  const links = graphs.flatMap((graph) => graph.links);
  return buildStats(Array.from({ length: nodes }, (_, index) => ({ ref: `${index}`, name: `${index}` })), links);
}

function readString(record: Record<string, unknown> | undefined, keys: string[]): string | undefined {
  if (!record) {
    return undefined;
  }
  for (const key of keys) {
    const value = record[key];
    if (typeof value === 'string' && value.length > 0) {
      return value;
    }
  }
  return undefined;
}

function readObject(record: Record<string, unknown> | undefined, keys: string[]): Record<string, unknown> | undefined {
  if (!record) {
    return undefined;
  }
  for (const key of keys) {
    const value = record[key];
    if (isRecord(value)) {
      return value;
    }
  }
  return undefined;
}

function readObjectArray(record: Record<string, unknown> | undefined, keys: string[]): Record<string, unknown>[] {
  if (!record) {
    return [];
  }
  for (const key of keys) {
    const value = record[key];
    if (Array.isArray(value)) {
      return value.filter((item): item is Record<string, unknown> => isRecord(item));
    }
  }
  return [];
}

function collectLogicFlowAnchors(graphs: LogicFlowGraph[]): Record<string, unknown>[] {
  return stableUniqueAnchors(graphs.flatMap((graph) => graph.anchors));
}

function collectGraphAnchors(
  logic: Record<string, unknown>,
  nodes: LogicFlowNode[],
  links: LogicFlowLink[],
): Record<string, unknown>[] {
  const anchors: Record<string, unknown>[] = [];
  anchors.push(...readObjectArray(logic, ['anchors']));
  for (const node of nodes) {
    if (node.externalAnchor) {
      anchors.push(node.externalAnchor);
    }
    anchors.push(...(node.externalAnchors ?? []));
  }
  for (const link of links) {
    if (link.externalAnchor) {
      anchors.push(link.externalAnchor);
    }
  }
  return stableUniqueAnchors(anchors);
}

function stableUniqueAnchors(anchors: Record<string, unknown>[]): Record<string, unknown>[] {
  const byKey = new Map<string, Record<string, unknown>>();
  for (const anchor of anchors) {
    byKey.set(anchorSortKey(anchor), anchor);
  }
  return [...byKey.entries()]
    .sort(([left], [right]) => left.localeCompare(right))
    .map(([, anchor]) => anchor);
}

function anchorSortKey(anchor: Record<string, unknown>): string {
  return [
    readAnchorString(anchor, 'schema'),
    readAnchorString(anchor, 'asset_path'),
    readAnchorString(anchor, 'graph_name'),
    readAnchorString(anchor, 'semantic_role'),
    readAnchorString(anchor, 'node_guid'),
    readAnchorString(anchor, 'pin_direction'),
    readAnchorString(anchor, 'pin_name'),
    readAnchorString(anchor, 'fingerprint'),
  ].join('|');
}

function readAnchorString(anchor: Record<string, unknown>, key: string): string {
  const value = anchor[key];
  return typeof value === 'string' ? value : '';
}

function formatRelativeDataReference(pin: string): string {
  return pin.length > 0 && !isDefaultValuePin(pin) ? `&.${pin}` : '&';
}

function formatSourceReference(nodes: LogicFlowNode[], nodeRef: string, pin: string): string {
  const source = nodes.find((node) => node.ref === nodeRef);
  if (!source) {
    return pin || 'Value';
  }
  return pin && !isDefaultValuePin(pin) ? `${source.name}.${pin}` : source.name;
}

function isDefaultExecPin(pin: string): boolean {
  const normalized = pin.toLowerCase();
  return normalized === 'then' || normalized === 'execute';
}

function isDefaultValuePin(pin: string): boolean {
  const normalized = pin.toLowerCase();
  return normalized === 'returnvalue' || normalized === 'return value' || normalized === 'value';
}

function uniqueStrings(values: string[]): string[] {
  return [...new Set(values)];
}

function dedupeLinks(links: LogicFlowLink[]): LogicFlowLink[] {
  const seen = new Set<string>();
  return links.filter((link) => {
    const key = `${link.type}|${link.fromNode}|${link.fromPin}|${link.toNode}|${link.toPin}`;
    if (seen.has(key)) {
      return false;
    }
    seen.add(key);
    return true;
  });
}

function indentLines(text: string): string {
  return text.split('\n').map((line) => `  ${line}`).join('\n');
}
