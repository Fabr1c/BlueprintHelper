import {
  getAgentVisibleGraphWriteRoutes,
  getGraphWriteRouteById,
  getGraphWriteRoutesForTemplateDiscovery,
} from '../../task/compiler/graphwrite/graphwrite-route-registry.js';
import { getAllGraphWriteSlotDescriptors } from '../../task/compiler/graphwrite/graphwrite-slot-registry.js';
import type { GraphWriteRouteDescriptor } from '../../task/compiler/graphwrite/graphwrite-route-descriptor.js';
import type { GraphWriteSlotDescriptor } from '../../task/compiler/graphwrite/graphwrite-slot-descriptor.js';
import type {
  GraphWriteTemplateWriteMode,
  TaskSpecTemplateClusterItem,
  TaskSpecTemplateOperationItem,
  TaskSpecTemplateQuickAccessItem,
  TaskSpecTemplateWriteModeItem,
} from './taskspec-template-types.js';

const CANONICAL_ROUTE_BY_WRITE_MODE: Record<GraphWriteTemplateWriteMode, string> = {
  'graph.append': 'graph.append.custom_event',
  'graph.replace': 'graph.replace.event_body',
  'graph.merge': 'graph.merge_external_flow.insert_between',
  'graph.patch': 'graph.patch.connect_pins',
};

const GRAPH_WRITE_MODE_DESCRIPTIONS: Readonly<Record<GraphWriteTemplateWriteMode, string>> = {
  'graph.append': 'Create new owned graph content, usually a custom event or owned entry body.',
  'graph.replace': 'Replace an existing event, function, macro, graph, or owned block body.',
  'graph.merge': 'Insert owned logic at an existing stable graph anchor.',
  'graph.patch': 'Patch BlueprintHelper-owned refs or external user graph compact anchors for links and node fields.',
};

const GRAPH_WRITE_CLUSTER_DESCRIPTIONS: Readonly<Record<string, string>> = {
  container: 'Array, set, and map operation statements.',
  event_delegate: 'Component-bound event and delegate binding statements.',
  external_body: 'External event or function body replacement through adapter-backed body anchors.',
  generic_ops: 'General Blueprint statements and expressions such as call, set, let, branch, return, construct, and literal values.',
  patch: 'Owned graph refs plus external compact-anchor link, pin default, and descriptor-backed node property patches.',
  schedule: 'Delay, timer, and scheduled execution statements.',
};

const GRAPH_WRITE_OPERATION_DESCRIPTIONS: Readonly<Record<string, string>> = {
  action: 'Run array, set, or map container operations.',
  call: 'Invoke Blueprint functions or methods, optionally with preview candidate search or result symbols.',
  component: 'Declare component-bound event handler relationships.',
  control: 'Build control-flow statements such as branch, switch, and function return.',
  convert: 'Run conversion or transform operations backed by GenericOps evidence.',
  create: 'Create objects, components, or assets through supported create operations.',
  delegate: 'Bind, assign, unbind, clear, or call delegates.',
  entry: 'Create or select an owned graph entry route and optionally fill its body.',
  expression: 'Build nested value inputs such as literals, variables, function parameters, operators, structs, and selects.',
  field: 'Read or write structured field and property values.',
  let: 'Create a reusable graph-local symbol from an expression.',
  merge: 'Insert owned graph logic at a supported merge point or external flow anchor.',
  patch: 'Patch owned graph refs, links, pin defaults, comments, or owned nodes.',
  external_link_patch: 'Connect, disconnect, or replace user graph links through external compact anchors.',
  external_property_patch: 'Patch external user graph pin defaults or descriptor-backed node properties.',
  replace_body: 'Replace an adapter-backed external graph body using read_context body boundary evidence.',
  set: 'Assign member variables or object/component properties.',
  timer: 'Run delay, timer, or other scheduled execution operations.',
};

export function listGraphWriteTemplateWriteModes(): TaskSpecTemplateWriteModeItem[] {
  return uniqueBy(
    getGraphWriteRoutesForTemplateDiscovery()
      .map((route) => ({
        family: 'graph_write' as const,
        write_mode: route.write_mode,
        description: GRAPH_WRITE_MODE_DESCRIPTIONS[route.write_mode],
        base_template_path: getBaseTemplatePathForWriteMode(route.write_mode),
      })),
    (item) => item.write_mode,
  ).sort((a, b) => a.write_mode.localeCompare(b.write_mode));
}

export function listGraphWriteTemplateClusters(): TaskSpecTemplateClusterItem[] {
  const quickAccessItems = listGraphWriteTemplateQuickAccessItems();
  const allWriteModes = listGraphWriteTemplateWriteModes().map((item) => item.write_mode);
  const byCluster = new Map<string, TaskSpecTemplateQuickAccessItem[]>();
  for (const item of quickAccessItems) {
    const items = byCluster.get(item.cluster_id) ?? [];
    items.push(item);
    byCluster.set(item.cluster_id, items);
  }
  return [...byCluster.entries()]
    .map(([clusterId, items]) => ({
      family: 'graph_write' as const,
      cluster_id: clusterId,
      description: GRAPH_WRITE_CLUSTER_DESCRIPTIONS[clusterId] ?? `GraphWrite ${clusterId} templates.`,
      unsupported_write_modes: allWriteModes
        .filter((writeMode) => !items.some((item) => supportsWriteMode(item, writeMode)))
        .sort(),
    }))
    .sort((a, b) => a.cluster_id.localeCompare(b.cluster_id));
}

export function listGraphWriteTemplateOperations(input: {
  cluster: string;
  writeMode: string;
}): TaskSpecTemplateOperationItem[] {
  return uniqueBy(
    listGraphWriteTemplateQuickAccessItems()
      .filter((item) => item.cluster_id === input.cluster)
      .filter((item) => supportsWriteMode(item, input.writeMode))
      .map((item) => ({
        family: 'graph_write' as const,
        cluster_id: item.cluster_id,
        operation_id: item.operation_id,
        description: describeOperation(item.operation_id, item.cluster_id),
        validation_classification: item.validation_classification,
        runtime_only_validation_notes: item.runtime_only_validation_notes,
      })),
    (item) => item.operation_id,
  ).sort((a, b) => a.operation_id.localeCompare(b.operation_id));
}

function describeOperation(operationId: string, clusterId: string): string {
  if (operationId === 'replace_body' && clusterId === 'generic_ops') {
    return 'Replace a BlueprintHelper-owned event, function, macro, graph, or block body; use external_body for non-owned user graph bodies.';
  }
  if (operationId === 'replace_body' && clusterId === 'external_body') {
    return 'Replace a non-BlueprintHelper-owned event or function body using read_context adapter_boundary.body_entry and body_fingerprint evidence.';
  }
  return GRAPH_WRITE_OPERATION_DESCRIPTIONS[operationId] ?? `GraphWrite ${operationId} operation templates.`;
}

export function listGraphWriteTemplateQuickAccess(input?: {
  cluster?: string;
  operation?: string;
  writeMode?: string;
}): TaskSpecTemplateQuickAccessItem[] {
  const allItems = listGraphWriteTemplateQuickAccessItems();
  const filteredItems = allItems
    .filter((item) => !input?.cluster || item.cluster_id === input.cluster)
    .filter((item) => !input?.operation || item.operation_id === input.operation)
    .filter((item) => !input?.writeMode || item.write_mode === input.writeMode);

  return uniqueBy([
    ...filteredItems,
    ...listRouteChildQuickAccessItems(filteredItems, allItems, input),
  ], (item) => `${item.template_id}:${item.write_mode}:${item.source_slot_id}:${item.cluster_id}:${item.operation_id}`)
    .sort((a, b) => a.template_id.localeCompare(b.template_id));
}

function listGraphWriteTemplateQuickAccessItems(): TaskSpecTemplateQuickAccessItem[] {
  const routeById = visibleRouteById();
  return [
    ...listGraphWriteRouteQuickAccessItems(routeById),
    ...getAllGraphWriteSlotDescriptors()
    .filter((slot) => isTemplateVisible(slot, routeById))
    .flatMap((slot) => toQuickAccessItems(slot, routeById)),
  ];
}

function listGraphWriteRouteQuickAccessItems(
  routeById: Map<string, GraphWriteRouteDescriptor>,
): TaskSpecTemplateQuickAccessItem[] {
  return [...routeById.values()]
    .filter((route) => route.quick_access !== undefined)
    .map((route) => {
      const quickAccess = route.quick_access;
      if (!quickAccess || !route.template_path) {
        throw new Error(`GraphWrite route quick-access is missing a route template: ${route.route_id}`);
      }
      return {
        template_id: quickAccess.template_id,
        family: quickAccess.family,
        write_mode: route.write_mode,
        cluster_id: quickAccess.cluster_id,
        operation_id: quickAccess.operation_id,
        quick_access_id: quickAccess.quick_access_id,
        source_slot_id: route.route_id,
        slot_type: 'route' as const,
        arg_slots: [...(quickAccess.arg_slots ?? ['body(statement[])'])],
        template_path: route.template_path,
        insert_paths: [...route.insert_paths],
        unsupported_write_modes: [],
        validation_classification: route.validation_classification ?? 'shared_policy',
        runtime_only_validation_notes: route.runtime_only_validation_notes,
      };
    });
}

function toQuickAccessItems(
  slot: GraphWriteSlotDescriptor,
  routeById: Map<string, GraphWriteRouteDescriptor>,
): TaskSpecTemplateQuickAccessItem[] {
  const quickAccess = slot.quick_access;
  const routeInsertions = uniqueBy(slot.supported_routes
    .map((routeId) => routeById.get(routeId))
    .filter((route): route is GraphWriteRouteDescriptor => route !== undefined)
    .filter((route) => !quickAccess.unsupported_write_modes?.includes(route.write_mode))
    .flatMap((route) => route.insert_paths.map((insertPath) => ({
      route,
      insertPath,
    }))), (entry) => `${entry.route.write_mode}:${entry.insertPath}`);
  const templateIds = [quickAccess.template_id, ...(slot.aliases ?? [])];
  return routeInsertions.flatMap((entry) => templateIds.map((templateId) => ({
    template_id: templateId,
    family: quickAccess.family,
    write_mode: entry.route.write_mode,
    cluster_id: quickAccess.cluster_id,
    operation_id: quickAccess.operation_id,
    quick_access_id: quickAccess.quick_access_id,
    source_slot_id: slot.slot_id,
    slot_type: slot.slot_type,
    arg_slots: formatArgSlots(slot),
    template_path: slot.template_path,
    insert_paths: slot.slot_type === 'expression' ? [...slot.insert_paths] : [entry.insertPath],
    unsupported_write_modes: [...(quickAccess.unsupported_write_modes ?? [])],
    validation_classification: entry.route.validation_classification ?? 'shared_policy',
    runtime_only_validation_notes: entry.route.runtime_only_validation_notes,
  })));
}

function formatArgSlots(slot: GraphWriteSlotDescriptor): string[] {
  return [...slot.input_slots]
    .sort((a, b) => a.index - b.index)
    .map((input) => {
      const typeHint = formatTypeHint(input.type_hint);
      return typeHint ? `${input.name}(${typeHint})` : input.name;
    });
}

function formatTypeHint(typeHint: string | undefined): string {
  if (!typeHint) {
    return '';
  }
  if (typeHint === '*') {
    return '*';
  }
  const placeholderMatch = /^__REQUIRED_(.+)__$/.exec(typeHint);
  if (!placeholderMatch) {
    return typeHint;
  }
  return placeholderMatch[1]
    .toLowerCase()
    .split('_')
    .filter((part) => part.length > 0)
    .map((part) => part[0]?.toUpperCase() + part.slice(1))
    .join('');
}

function supportsWriteMode(item: TaskSpecTemplateQuickAccessItem, writeMode: string): boolean {
  return item.write_mode === writeMode
    && !item.unsupported_write_modes.includes(writeMode as GraphWriteTemplateWriteMode)
    && item.insert_paths.length > 0;
}

function listRouteChildQuickAccessItems(
  filteredItems: TaskSpecTemplateQuickAccessItem[],
  allItems: TaskSpecTemplateQuickAccessItem[],
  input: { operation?: string; writeMode?: string } | undefined,
): TaskSpecTemplateQuickAccessItem[] {
  if (!input?.operation || !input.writeMode) {
    return [];
  }

  const children: TaskSpecTemplateQuickAccessItem[] = [];
  for (const routeItem of filteredItems) {
    if (routeItem.slot_type !== 'route' || !routeItem.arg_slots.some((slot) => slot.includes('statement[]'))) {
      continue;
    }
    const route = getGraphWriteRouteById(routeItem.source_slot_id);
    if (!route) {
      continue;
    }
    for (const item of allItems) {
      if (item.slot_type !== 'statement'
        || !supportsWriteMode(item, input.writeMode)
        || !isSlotAllowedForRoute(item.source_slot_id, route.allowed_slot_ids)) {
        continue;
      }
      children.push({
        ...item,
        cluster_id: routeItem.cluster_id,
        operation_id: routeItem.operation_id,
        quick_access_id: `${routeItem.quick_access_id}.${item.quick_access_id}`,
      });
    }
  }
  return children;
}

function isSlotAllowedForRoute(slotId: string, allowedSlotIds: readonly string[]): boolean {
  return allowedSlotIds.includes(slotId)
    || (allowedSlotIds.includes('graph.body.*') && slotId.startsWith('graph.statement.'));
}

function isTemplateVisible(
  slot: GraphWriteSlotDescriptor,
  routeById: Map<string, GraphWriteRouteDescriptor>,
): boolean {
  return slot.status === 'active'
    && slot.template_path.length > 0
    && slot.quick_access.family === 'graph_write'
    && slot.supported_routes.some((routeId) => routeById.has(routeId));
}

function visibleRouteById(): Map<string, GraphWriteRouteDescriptor> {
  return new Map(getAgentVisibleGraphWriteRoutes().map((route) => [route.route_id, route]));
}

function isGraphWriteTemplateWriteMode(value: unknown): value is GraphWriteTemplateWriteMode {
  return typeof value === 'string' && Object.hasOwn(CANONICAL_ROUTE_BY_WRITE_MODE, value);
}

function getBaseTemplatePathForWriteMode(writeMode: GraphWriteTemplateWriteMode): string {
  const canonicalRoute = getGraphWriteRouteById(CANONICAL_ROUTE_BY_WRITE_MODE[writeMode]);
  if (canonicalRoute?.template_path) {
    return canonicalRoute.template_path;
  }
  throw new Error(`GraphWrite canonical write mode route has no template path: ${writeMode}`);
}

function uniqueBy<T>(items: T[], keyOf: (item: T) => string): T[] {
  return [...new Map(items.map((item) => [keyOf(item), item])).values()];
}
