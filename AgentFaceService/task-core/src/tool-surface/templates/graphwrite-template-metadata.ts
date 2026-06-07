import {
  getAgentVisibleGraphWriteRoutes,
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

const BASE_TEMPLATE_BY_WRITE_MODE: Record<GraphWriteTemplateWriteMode, string> = {
  'graph.append': 'AgentFaceService/agent-guide/Templates/write/taskspec/graph_append_template.json',
  'graph.replace': 'AgentFaceService/agent-guide/Templates/write/taskspec/graph_replace_template.json',
  'graph.merge': 'AgentFaceService/agent-guide/Templates/write/taskspec/graph_merge_external_flow_template.json',
  'graph.patch': 'AgentFaceService/agent-guide/Templates/write/taskspec/graph_patch_owned_template.json',
};

export function listGraphWriteTemplateWriteModes(): TaskSpecTemplateWriteModeItem[] {
  return uniqueBy(
    getGraphWriteRoutesForTemplateDiscovery()
      .map((route) => ({
        family: 'graph_write' as const,
        write_mode: route.write_mode,
        base_template_path: BASE_TEMPLATE_BY_WRITE_MODE[route.write_mode],
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
      })),
    (item) => item.operation_id,
  ).sort((a, b) => a.operation_id.localeCompare(b.operation_id));
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
  return getAllGraphWriteSlotDescriptors()
    .filter((slot) => isTemplateVisible(slot, routeById))
    .flatMap((slot) => toQuickAccessItems(slot, routeById));
}

function toQuickAccessItems(
  slot: GraphWriteSlotDescriptor,
  routeById: Map<string, GraphWriteRouteDescriptor>,
): TaskSpecTemplateQuickAccessItem[] {
  const quickAccess = slot.quick_access;
  return uniqueBy(slot.supported_routes
    .map((routeId) => routeById.get(routeId))
    .filter((route): route is GraphWriteRouteDescriptor => route !== undefined)
    .filter((route) => !quickAccess.unsupported_write_modes?.includes(route.write_mode))
    .flatMap((route) => route.insert_paths.map((insertPath) => ({
      route,
      insertPath,
    }))), (entry) => `${entry.route.write_mode}:${entry.insertPath}`)
    .map((entry) => ({
    template_id: quickAccess.template_id,
    family: quickAccess.family,
    write_mode: entry.route.write_mode,
    cluster_id: quickAccess.cluster_id,
    operation_id: quickAccess.operation_id,
    quick_access_id: quickAccess.quick_access_id,
    source_slot_id: slot.slot_id,
    template_path: slot.template_path,
    insert_paths: [entry.insertPath],
    unsupported_write_modes: [...(quickAccess.unsupported_write_modes ?? [])],
  }));
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
  return typeof value === 'string' && Object.hasOwn(BASE_TEMPLATE_BY_WRITE_MODE, value);
}

function uniqueBy<T>(items: T[], keyOf: (item: T) => string): T[] {
  return [...new Map(items.map((item) => [keyOf(item), item])).values()];
}
