import type { MetricsOperationIdentity } from './metrics-types.js';
import {
  getAllGraphWriteRoutes,
  getGraphWriteRouteByScope,
} from '../task/compiler/graphwrite/graphwrite-route-registry.js';
import type { GraphWriteRouteDescriptor } from '../task/compiler/graphwrite/graphwrite-route-descriptor.js';

const GRAPH_WRITE_ADAPTER_CAPABILITY = 'graph_write';

const CAPABILITY_BY_ADAPTER_OPERATION = new Map<string, string>([
  ['append_blueprint_graph', GRAPH_WRITE_ADAPTER_CAPABILITY],
  ['replace_blueprint_graph', GRAPH_WRITE_ADAPTER_CAPABILITY],
  ['patch_blueprint_graph', GRAPH_WRITE_ADAPTER_CAPABILITY],
  ['merge_blueprint_graph', GRAPH_WRITE_ADAPTER_CAPABILITY],
  ['merge_external_flow', GRAPH_WRITE_ADAPTER_CAPABILITY],
  ['patch_external_graph', GRAPH_WRITE_ADAPTER_CAPABILITY],
  ['replace_external_body', GRAPH_WRITE_ADAPTER_CAPABILITY],
]);

const GRAPH_STRATEGY_BY_ADAPTER_OPERATION = new Map<string, string>([
  ['append_blueprint_graph', 'append_new_owned_graph'],
  ['replace_blueprint_graph', 'replace_owned_graph'],
  ['patch_blueprint_graph', 'patch_owned_graph'],
  ['merge_blueprint_graph', 'merge_owned_graph'],
  ['merge_external_flow', 'merge_external_flow'],
  ['patch_external_graph', 'patch_external_graph'],
  ['replace_external_body', 'replace_external_body'],
]);

export function extractTaskPlanMetricOperations(taskPlanLike: unknown): MetricsOperationIdentity[] {
  const raw = asRecord(taskPlanLike);
  const steps = Array.isArray(raw?.['steps']) ? raw['steps'] : [];
  const operations: MetricsOperationIdentity[] = [];

  for (const stepValue of steps) {
    const step = asRecord(stepValue);
    if (!step) {
      continue;
    }

    const adapterOperation = readString(step['operation']);
    if (adapterOperation) {
      const routeOperation = resolveAdapterRouteMetricOperation(adapterOperation, step);
      if (routeOperation) {
        operations.push(routeOperation);
        continue;
      }

      operations.push({
        capability: CAPABILITY_BY_ADAPTER_OPERATION.get(adapterOperation) ?? adapterOperation,
        semantic_operation: buildAdapterSemanticOperation(adapterOperation, step),
      });
      continue;
    }

    const capability = readString(step['capability']);
    if (!capability) {
      continue;
    }

    const body = step['write'] ?? step['action'];
    const descriptorOperations = collectDescriptorStepOperations(capability, body);
    if (descriptorOperations.length > 0) {
      operations.push(...descriptorOperations);
      continue;
    }

    const stepOperations = collectStructuredStepOperations(body);
    if (stepOperations.length === 0) {
      operations.push({ capability, semantic_operation: `${capability}.unknown` });
      continue;
    }

    for (const semanticOperation of stepOperations) {
      operations.push({
        capability,
        semantic_operation: semanticOperation,
      });
    }
  }

  return operations;
}

export function extractReadToolOperation(input: unknown): MetricsOperationIdentity {
  const raw = asRecord(input);
  const readType = readString(raw?.['read_type']) ?? 'read';
  const view = asRecord(raw?.['view']);
  const format = readString(view?.['format'])
    ?? readString(raw?.['format'])
    ?? defaultReadContextFormat(readType)
    ?? 'unknown';

  return {
    capability: 'read_context',
    semantic_operation: `${readType}.${format}`,
  };
}

function defaultReadContextFormat(readType: string): string | undefined {
  if (readType === 'graph_context') {
    return 'logic_json';
  }
  if (readType === 'blueprint_logic') {
    return 'logic_flow';
  }

  return undefined;
}

function collectStructuredStepOperations(value: unknown): string[] {
  const config = asRecord(value);
  const ops = Array.isArray(config?.['ops']) ? config['ops'] : [];
  const operationNames = ops
    .map((entry) => readString(asRecord(entry)?.['op']))
    .filter((entry): entry is string => entry !== undefined);

  if (operationNames.length > 0) {
    return operationNames;
  }

  const strategy = readString(config?.['strategy']);
  return strategy ? [strategy] : [];
}

function collectDescriptorStepOperations(capability: string, value: unknown): MetricsOperationIdentity[] {
  if (capability !== GRAPH_WRITE_ADAPTER_CAPABILITY) {
    return [];
  }

  const config = asRecord(value);
  const ops = Array.isArray(config?.['ops']) ? config['ops'] : [];
  const operations: MetricsOperationIdentity[] = [];

  for (const opValue of ops) {
    const op = asRecord(opValue);
    if (!op) {
      continue;
    }

    const route = resolveStructuredRoute(op);
    if (route) {
      operations.push({
        capability: GRAPH_WRITE_ADAPTER_CAPABILITY,
        semantic_operation: route.route_id,
      });
    }
  }

  return operations;
}

function resolveStructuredRoute(op: Record<string, unknown>): GraphWriteRouteDescriptor | undefined {
  const operation = readString(op['op']);
  if (!operation) {
    return undefined;
  }

  const replaceScope = readString(op['replace_scope']);
  if (operation === 'replace_body' && replaceScope) {
    return getGraphWriteRouteByScope('replace_owned_graph', replaceScope);
  }

  return resolveUniqueGraphWriteRoute((route) => route.taskplan_op === operation);
}

function resolveAdapterRouteMetricOperation(
  adapterOperation: string,
  step: Record<string, unknown>,
): MetricsOperationIdentity | undefined {
  const graphStrategy = GRAPH_STRATEGY_BY_ADAPTER_OPERATION.get(adapterOperation);
  if (!graphStrategy) {
    return undefined;
  }

  const route = resolveAdapterRoute(graphStrategy, step);
  if (!route) {
    return undefined;
  }

  return {
    capability: GRAPH_WRITE_ADAPTER_CAPABILITY,
    semantic_operation: route.route_id,
  };
}

function resolveAdapterRoute(
  graphStrategy: string,
  step: Record<string, unknown>,
): GraphWriteRouteDescriptor | undefined {
  const target = asRecord(step['target']);
  const args = asRecord(step['args']);
  const explicitScope = readString(
    target?.['replace_scope']
    ?? target?.['patch_scope']
    ?? target?.['merge_scope'],
  );
  const variant = readString(
    args?.['patch_type']
    ?? args?.['kind']
    ?? args?.['scope']
    ?? args?.['insert_strategy']
    ?? target?.['insert_strategy']
    ?? args?.['strategy'],
  );

  if (explicitScope) {
    const scopedRoute = getGraphWriteRouteByScope(graphStrategy, explicitScope);
    if (scopedRoute) {
      return scopedRoute;
    }
  }

  const candidates = getAllGraphWriteRoutes().filter((route) => route.graph_strategy === graphStrategy);
  const variantRoute = matchSingleRoute(candidates, variant);
  if (variantRoute) {
    return variantRoute;
  }

  return candidates.length === 1 ? candidates[0] : undefined;
}

function matchSingleRoute(
  routes: readonly GraphWriteRouteDescriptor[],
  variant: string | undefined,
): GraphWriteRouteDescriptor | undefined {
  if (!variant) {
    return undefined;
  }

  return resolveUniqueRoute(routes, (route) => (
    route.public_scope === variant
    || route.taskplan_op === variant
  ));
}

function resolveUniqueGraphWriteRoute(
  predicate: (route: GraphWriteRouteDescriptor) => boolean,
): GraphWriteRouteDescriptor | undefined {
  return resolveUniqueRoute(getAllGraphWriteRoutes(), predicate);
}

function resolveUniqueRoute(
  routes: readonly GraphWriteRouteDescriptor[],
  predicate: (route: GraphWriteRouteDescriptor) => boolean,
): GraphWriteRouteDescriptor | undefined {
  const matches = routes.filter(predicate);
  return matches.length === 1 ? matches[0] : undefined;
}

function buildAdapterSemanticOperation(operation: string, step: Record<string, unknown>): string {
  const target = asRecord(step['target']);
  const args = asRecord(step['args']);
  const parts = [operation];

  const scope = readString(
    target?.['patch_scope']
    ?? target?.['replace_scope']
    ?? target?.['merge_scope'],
  );
  const variant = readString(
    args?.['patch_type']
    ?? args?.['kind']
    ?? args?.['insert_strategy']
    ?? args?.['scope']
    ?? target?.['insert_strategy']
    ?? args?.['strategy'],
  );

  if (scope) {
    parts.push(scope);
  }
  if (variant) {
    parts.push(variant);
  }

  return parts.join('.');
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return value !== null && typeof value === 'object' && !Array.isArray(value)
    ? value as Record<string, unknown>
    : undefined;
}

function readString(value: unknown): string | undefined {
  return typeof value === 'string' && value.trim().length > 0 ? value.trim() : undefined;
}
