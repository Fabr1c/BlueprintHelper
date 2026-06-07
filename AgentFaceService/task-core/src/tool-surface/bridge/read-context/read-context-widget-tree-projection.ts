import { isRecord } from '../bridge-tool-result-utils.js';

type WidgetNode = Record<string, unknown> & {
  widget_name: string;
  widget_class_path: string;
  virtual_index: number;
  children: WidgetNode[];
  slot_properties?: Record<string, unknown>;
};

export const WIDGET_TREE_JSON_SCHEMA = 'WidgetTreeJson.v1' as const;

export function buildWidgetTreeJsonPayload(payload: Record<string, unknown>): Record<string, unknown> {
  return {
    schema: WIDGET_TREE_JSON_SCHEMA,
    format: 'tree_json',
    domain: 'widget_blueprint',
    asset_path: readString(payload, 'asset_path') ?? '',
    ...normalizeWidgetTree(payload),
  };
}

export function buildWidgetTreeLogicFlowPayload(payload: Record<string, unknown>): Record<string, unknown> {
  const treeJsonPayload = buildWidgetTreeJsonPayload(payload);
  const root = treeJsonPayload['root'];
  const namedSlots = readRecordArray(treeJsonPayload, 'named_slots');
  const index = isRecord(treeJsonPayload['index']) ? treeJsonPayload['index'] : {};
  const warnings = collectWidgetFlowWarnings(root, namedSlots, index);
  return {
    schema: 'WidgetTreeLogicFlow.v1',
    format: 'logic_flow',
    domain: 'widget_blueprint',
    asset_path: treeJsonPayload['asset_path'],
    flow: buildWidgetFlow(root, namedSlots, index),
    warnings,
  };
}

function normalizeWidgetTree(payload: Record<string, unknown>): Record<string, unknown> {
  const root = normalizeRoot(payload);
  const namedSlots = normalizeNamedSlots(payload);
  const index = {
    ...buildWidgetIndex(root),
    ...normalizeWidgetIndex(payload),
  };
  return {
    asset_class: readString(payload, 'asset_class'),
    parent_class: readString(payload, 'parent_class'),
    root,
    index,
    named_slots: namedSlots,
  };
}

function normalizeRoot(payload: Record<string, unknown>): WidgetNode | null {
  const root = payload['root'];
  if (isRecord(root)) {
    return normalizeNode(root, undefined, 0);
  }

  const widgets = payload['widgets'];
  if (!Array.isArray(widgets)) {
    return null;
  }

  const records = widgets.filter(isRecord);
  const nodes = new Map<string, WidgetNode>();
  for (const record of records) {
    const name = readWidgetName(record);
    if (!name) continue;
    nodes.set(name, normalizeNode(record, undefined, 0));
  }

  for (const record of records) {
    const name = readWidgetName(record);
    const parentName = readString(record, 'parent') ?? readString(record, 'parent_name');
    if (!name || !parentName) continue;
    const node = nodes.get(name);
    const parent = nodes.get(parentName);
    if (!node || !parent) continue;
    node.parent_name = parentName;
    parent.children.push(node);
  }

  const rootName = readString(payload, 'root_widget') ?? readString(payload, 'root_widget_name');
  const explicitRoot = rootName ? nodes.get(rootName) : undefined;
  const firstRoot = [...nodes.values()].find((node) => !node.parent_name);
  const resolvedRoot = explicitRoot ?? firstRoot ?? null;
  if (resolvedRoot) {
    assignVirtualIndexes(resolvedRoot);
  }
  return resolvedRoot;
}

function normalizeNode(node: Record<string, unknown>, parentName: string | undefined, fallbackVirtualIndex: number): WidgetNode {
  const children = readChildren(node).map((child, index) => normalizeNode(child, readWidgetName(node), index));
  const normalized: WidgetNode = {
    widget_name: readWidgetName(node) ?? '',
    widget_class_path: readWidgetClass(node),
    virtual_index: readNonNegativeInt(node, 'virtual_index') ?? fallbackVirtualIndex,
    is_variable: readBoolean(node, 'is_variable') ?? false,
    is_inherited: readBoolean(node, 'is_inherited') ?? false,
    children,
  };

  const parent = parentName ?? readString(node, 'parent_name') ?? readString(node, 'parent');
  if (parent) normalized.parent_name = parent;
  const slotClassPath = readString(node, 'slot_class_path') ?? readString(node, 'slot_class');
  if (slotClassPath) normalized.slot_class_path = slotClassPath;
  const slotName = readString(node, 'slot_name');
  if (slotName) normalized.slot_name = slotName;
  const slotProperties = readSlotProperties(node);
  if (slotProperties) normalized.slot_properties = slotProperties;
  assignVirtualIndexes(normalized);
  return normalized;
}

function readChildren(node: Record<string, unknown>): Record<string, unknown>[] {
  const children = node['children'];
  return Array.isArray(children) ? children.filter(isRecord) : [];
}

function assignVirtualIndexes(node: WidgetNode): void {
  node.children = node.children
    .map((child, index) => ({
      ...child,
      parent_name: child.parent_name ?? node.widget_name,
      virtual_index: readNonNegativeInt(child, 'virtual_index') ?? index,
    }))
    .sort((left, right) => left.virtual_index - right.virtual_index);
  node.children.forEach(assignVirtualIndexes);
}

function buildWidgetIndex(root: WidgetNode | null): Record<string, unknown> {
  const index: Record<string, unknown> = {};
  visitWidgetTree(root, (node) => {
    const entry: Record<string, unknown> = {
      widget_class_path: node.widget_class_path,
      virtual_index: node.virtual_index,
      is_variable: node.is_variable,
      is_inherited: node.is_inherited,
    };
    if (typeof node.parent_name === 'string' && node.parent_name.length > 0) {
      entry['parent_name'] = node.parent_name;
    }
    if (typeof node.slot_class_path === 'string' && node.slot_class_path.length > 0) {
      entry['slot_class_path'] = node.slot_class_path;
    }
    if (typeof node.slot_name === 'string' && node.slot_name.length > 0) {
      entry['slot_name'] = node.slot_name;
    }
    if (isRecord(node.slot_properties) && Object.keys(node.slot_properties).length > 0) {
      entry['slot_properties'] = node.slot_properties;
    }
    index[node.widget_name] = entry;
  });
  return index;
}

function normalizeWidgetIndex(payload: Record<string, unknown>): Record<string, unknown> {
  const rawIndex = payload['index'];
  if (!isRecord(rawIndex)) return {};

  const normalized: Record<string, unknown> = {};
  for (const [widgetName, rawEntry] of Object.entries(rawIndex)) {
    if (typeof widgetName !== 'string' || widgetName.length === 0 || !isRecord(rawEntry)) {
      continue;
    }

    const entry: Record<string, unknown> = {
      widget_class_path: readWidgetClass(rawEntry),
      virtual_index: readNonNegativeInt(rawEntry, 'virtual_index') ?? 0,
      is_variable: readBoolean(rawEntry, 'is_variable') ?? false,
      is_inherited: readBoolean(rawEntry, 'is_inherited') ?? false,
    };
    const parentName = readString(rawEntry, 'parent_name') ?? readString(rawEntry, 'parent');
    if (parentName) entry['parent_name'] = parentName;
    const slotClassPath = readString(rawEntry, 'slot_class_path') ?? readString(rawEntry, 'slot_class');
    if (slotClassPath) entry['slot_class_path'] = slotClassPath;
    const slotName = readString(rawEntry, 'slot_name');
    if (slotName) entry['slot_name'] = slotName;
    const slotProperties = readSlotProperties(rawEntry);
    if (slotProperties) entry['slot_properties'] = slotProperties;
    normalized[widgetName] = entry;
  }
  return normalized;
}

function readSlotProperties(record: Record<string, unknown>): Record<string, unknown> | undefined {
  const rawProperties = record['slot_properties'];
  if (!isRecord(rawProperties)) return undefined;

  const properties: Record<string, unknown> = {};
  for (const [key, value] of Object.entries(rawProperties)) {
    if (key.length === 0) continue;
    if (typeof value === 'string' || typeof value === 'number' || typeof value === 'boolean') {
      properties[key] = value;
    }
  }
  return Object.keys(properties).length > 0 ? properties : undefined;
}

function normalizeNamedSlots(payload: Record<string, unknown>): Record<string, unknown>[] {
  const namedSlots = payload['named_slots'];
  if (!Array.isArray(namedSlots)) return [];
  return namedSlots.filter(isRecord).map((slot) => {
    const normalized: Record<string, unknown> = {
      host_widget_name: readString(slot, 'host_widget_name') ?? readString(slot, 'host') ?? '',
      slot_name: readString(slot, 'slot_name') ?? readString(slot, 'name') ?? '',
      virtual_index: readNonNegativeInt(slot, 'virtual_index') ?? 0,
    };
    const content = readString(slot, 'content_widget_name') ?? readString(slot, 'content');
    if (content) normalized['content_widget_name'] = content;
    return normalized;
  });
}

function buildWidgetFlow(
  root: unknown,
  namedSlots: Record<string, unknown>[],
  index: Record<string, unknown>,
): string {
  if (!isRecord(root)) return 'widgetroot[]';
  const node = root as WidgetNode;
  const childFlows = readChildren(node)
    .map((child) => buildNodeFlow(child as WidgetNode, namedSlots, index));
  const suffix = childFlows.length > 0 ? ` -> (${childFlows.join(', ')})` : '';
  return `widgetroot[${displayClass(node.widget_class_path)}]${suffix}`;
}

function buildNodeFlow(
  node: WidgetNode,
  namedSlots: Record<string, unknown>[],
  index: Record<string, unknown>,
): string {
  const children = readChildren(node)
    .map((child) => buildNodeFlow(child as WidgetNode, namedSlots, index));
  const slotFlows = readNodeNamedSlots(node, namedSlots, index);
  const nested = [...children, ...slotFlows];
  const suffix = nested.length > 0 ? `(${nested.join(', ')})` : '';
  return `${node.widget_name}[${displayClass(node.widget_class_path)}]${suffix}`;
}

function collectWidgetFlowWarnings(
  root: unknown,
  namedSlots: Record<string, unknown>[],
  index: Record<string, unknown>,
): string[] {
  const warnings = new Set<string>();
  const representedWidgetNames = new Set<string>();

  if (!isRecord(root)) {
    warnings.add('widget_tree_logic_flow_degraded_missing_root');
  } else {
    visitWidgetTree(root as WidgetNode, (node) => {
      representedWidgetNames.add(node.widget_name);
      collectNodeFlowWarnings(node, warnings);
    });
  }

  for (const slot of namedSlots) {
    const hostWidgetName = readString(slot, 'host_widget_name');
    const contentWidgetName = readString(slot, 'content_widget_name');
    if (hostWidgetName && !representedWidgetNames.has(hostWidgetName)) {
      warnings.add('widget_tree_logic_flow_degraded_missing_named_slot_host');
    }
    if (contentWidgetName) {
      representedWidgetNames.add(contentWidgetName);
      const indexedContent = index[contentWidgetName];
      if (!isRecord(indexedContent)) {
        warnings.add('widget_tree_logic_flow_degraded_missing_named_slot_content');
      } else {
        collectIndexEntryFlowWarnings(indexedContent, warnings);
      }
    }
  }

  for (const [widgetName, entry] of Object.entries(index)) {
    if (!representedWidgetNames.has(widgetName)) {
      warnings.add('widget_tree_logic_flow_degraded_unrepresented_index_entry');
    }
    if (isRecord(entry)) {
      collectIndexEntryFlowWarnings(entry, warnings);
    }
  }

  return [...warnings];
}

function collectNodeFlowWarnings(node: WidgetNode, warnings: Set<string>): void {
  if (!node.widget_name || !node.widget_class_path) {
    warnings.add('widget_tree_logic_flow_degraded_missing_widget_identity');
  }
  if (node.is_inherited === true) {
    warnings.add('widget_tree_logic_flow_degraded_inherited_widget');
  }
}

function collectIndexEntryFlowWarnings(entry: Record<string, unknown>, warnings: Set<string>): void {
  if (readBoolean(entry, 'is_inherited') === true) {
    warnings.add('widget_tree_logic_flow_degraded_inherited_widget');
  }
  if (!readWidgetClass(entry)) {
    warnings.add('widget_tree_logic_flow_degraded_missing_widget_identity');
  }
}

function readNodeNamedSlots(
  node: WidgetNode,
  namedSlots: Record<string, unknown>[],
  index: Record<string, unknown>,
): string[] {
  const embeddedSlots = readRecordArray(node, 'named_slots');
  const topLevelSlots = namedSlots.filter((slot) => readString(slot, 'host_widget_name') === node.widget_name);
  const seen = new Set<string>();
  return [...embeddedSlots, ...topLevelSlots]
    .filter((slot) => {
      const key = `${readString(slot, 'slot_name') ?? ''}|${readString(slot, 'content_widget_name') ?? ''}`;
      if (seen.has(key)) return false;
      seen.add(key);
      return true;
    })
    .map((slot) => renderNamedSlotFlow(slot, namedSlots, index));
}

function renderNamedSlotFlow(
  slot: Record<string, unknown>,
  namedSlots: Record<string, unknown>[],
  index: Record<string, unknown>,
): string {
  const slotName = readString(slot, 'slot_name') ?? '';
  const content = slot['content'];
  if (isRecord(content)) {
    return `${slotName}[NamedSlot](${buildNodeFlow(content as WidgetNode, namedSlots, index)})`;
  }

  const contentName = readString(slot, 'content_widget_name');
  if (!contentName) return `${slotName}[NamedSlot]()`;

  const indexedContent = index[contentName];
  const contentClass = readString(slot, 'content_widget_class_path')
    ?? readString(slot, 'content_widget_class')
    ?? (isRecord(indexedContent) ? readWidgetClass(indexedContent) : '');
  return `${slotName}[NamedSlot](${contentName}[${displayClass(contentClass)}])`;
}

function readRecordArray(record: Record<string, unknown>, field: string): Record<string, unknown>[] {
  const value = record[field];
  return Array.isArray(value) ? value.filter(isRecord) : [];
}

function visitWidgetTree(node: WidgetNode | null, visitor: (node: WidgetNode) => void): void {
  if (!node) return;
  visitor(node);
  node.children.forEach((child) => visitWidgetTree(child, visitor));
}

function readWidgetName(record: Record<string, unknown>): string | undefined {
  return readString(record, 'widget_name') ?? readString(record, 'name');
}

function readWidgetClass(record: Record<string, unknown>): string {
  return readString(record, 'widget_class_path')
    ?? readString(record, 'class_path')
    ?? readString(record, 'widget_class')
    ?? readString(record, 'class')
    ?? '';
}

function readString(record: Record<string, unknown>, field: string): string | undefined {
  const value = record[field];
  return typeof value === 'string' && value.length > 0 ? value : undefined;
}

function readBoolean(record: Record<string, unknown>, field: string): boolean | undefined {
  const value = record[field];
  return typeof value === 'boolean' ? value : undefined;
}

function readNonNegativeInt(record: Record<string, unknown>, field: string): number | undefined {
  const value = record[field];
  return Number.isInteger(value) && (value as number) >= 0 ? value as number : undefined;
}

function displayClass(classPath: unknown): string {
  if (typeof classPath !== 'string' || classPath.length === 0) return '';
  const className = classPath.split('.').pop()?.split('/').pop() ?? classPath;
  return className.startsWith('U') ? className.slice(1) : className;
}
