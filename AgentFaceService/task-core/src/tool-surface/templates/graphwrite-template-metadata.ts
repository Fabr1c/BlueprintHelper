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
  'graph.merge': 'graph.merge_external_flow.append_after',
  'graph.patch': 'graph.patch.connect_pins',
};

const GRAPH_WRITE_MODE_DESCRIPTIONS: Readonly<Record<GraphWriteTemplateWriteMode, string>> = {
  'graph.append': 'Create new owned graph content, usually a custom event or owned entry body.',
  'graph.replace': 'Replace an existing event, function, macro, graph, or owned block body.',
  'graph.merge': 'Insert owned logic at an existing stable graph anchor.',
  'graph.patch': 'Edit links, pin defaults, comments, or owned nodes inside BlueprintHelper-owned graph content.',
};

const GRAPH_WRITE_CLUSTER_DESCRIPTIONS: Readonly<Record<string, string>> = {
  container: 'Array, set, and map operation statements.',
  event_delegate: 'Component-bound event and delegate binding statements.',
  external_body: 'External event or function body replacement through adapter-backed body anchors.',
  generic_ops: 'General Blueprint statements and expressions such as call, set, let, branch, return, construct, and literal values.',
  patch: 'Owned graph reference, link, pin default, and node comment patch templates.',
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
  const byCluster = new Map<string, Set<GraphWriteTemplateWriteMode>>();
  for (const item of quickAccessItems) {
    const modes = byCluster.get(item.cluster_id) ?? new Set<GraphWriteTemplateWriteMode>();
    for (const mode of item.unsupported_write_modes) {
      if (isGraphWriteTemplateWriteMode(mode)) {
        modes.add(mode);
      }
    }
    byCluster.set(item.cluster_id, modes);
  }
  return [...byCluster.entries()]
    .map(([clusterId, unsupported]) => ({
      family: 'graph_write' as const,
      cluster_id: clusterId,
      description: GRAPH_WRITE_CLUSTER_DESCRIPTIONS[clusterId] ?? `GraphWrite ${clusterId} templates.`,
      unsupported_write_modes: [...unsupported].sort(),
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
        description: describeOperation(item.operation_id),
      })),
    (item) => item.operation_id,
  ).sort((a, b) => a.operation_id.localeCompare(b.operation_id));
}

function describeOperation(operationId: string): string {
  return GRAPH_WRITE_OPERATION_DESCRIPTIONS[operationId] ?? `GraphWrite ${operationId} operation templates.`;
}

export function listGraphWriteTemplateQuickAccess(input?: {
  cluster?: string;
  operation?: string;
  writeMode?: string;
}): TaskSpecTemplateQuickAccessItem[] {
  return listGraphWriteTemplateQuickAccessItems()
    .filter((item) => !input?.cluster || item.cluster_id === input.cluster)
    .filter((item) => !input?.operation || item.operation_id === input.operation)
    .filter((item) => !input?.writeMode || item.write_mode === input.writeMode)
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
  const fallbackRoute = getGraphWriteRoutesForTemplateDiscovery()
    .find((route) => route.write_mode === writeMode && route.template_path);
  if (fallbackRoute?.template_path) {
    return fallbackRoute.template_path;
  }
  throw new Error(`GraphWrite write mode has no template path: ${writeMode}`);
}

function uniqueBy<T>(items: T[], keyOf: (item: T) => string): T[] {
  return [...new Map(items.map((item) => [keyOf(item), item])).values()];
}
