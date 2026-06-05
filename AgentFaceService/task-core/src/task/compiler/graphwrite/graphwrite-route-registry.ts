import { TaskSpecCompileError } from '../task-compiler-errors.js';
import {
  GRAPHWRITE_ROUTE_MANIFEST,
} from './generated/graphwrite-route-manifest.generated.js';
import {
  isAgentVisibleGraphWriteRoute,
  makeGraphWriteRouteKey,
  type GraphWriteRouteDescriptor,
  type GraphWriteSelectorDescriptor,
} from './graphwrite-route-descriptor.js';

const ALL_GRAPHWRITE_ROUTES: readonly GraphWriteRouteDescriptor[] = GRAPHWRITE_ROUTE_MANIFEST;
const ROUTES_BY_ID = new Map(ALL_GRAPHWRITE_ROUTES.map((route) => [route.route_id, route]));
const ROUTES_BY_SCOPE = new Map(
  ALL_GRAPHWRITE_ROUTES.map((route) => [makeGraphWriteRouteKey(route.graph_strategy, route.public_scope), route]),
);

export function getAllGraphWriteRoutes(): readonly GraphWriteRouteDescriptor[] {
  return ALL_GRAPHWRITE_ROUTES;
}

export function getAgentVisibleGraphWriteRoutes(): readonly GraphWriteRouteDescriptor[] {
  return ALL_GRAPHWRITE_ROUTES.filter(isAgentVisibleGraphWriteRoute);
}

export function getGraphWriteRoutesForTemplateDiscovery(): readonly GraphWriteRouteDescriptor[] {
  return getAgentVisibleGraphWriteRoutes();
}

export function getGraphWriteRouteById(routeId: string): GraphWriteRouteDescriptor | undefined {
  return ROUTES_BY_ID.get(routeId);
}

export function getGraphWriteRouteByScope(
  graphStrategy: string,
  publicScope: string,
): GraphWriteRouteDescriptor | undefined {
  return ROUTES_BY_SCOPE.get(makeGraphWriteRouteKey(graphStrategy, publicScope));
}

export function requireGraphWriteRouteByScope(
  graphStrategy: string,
  publicScope: string,
): GraphWriteRouteDescriptor {
  const route = getGraphWriteRouteByScope(graphStrategy, publicScope);
  if (!route || route.status === 'planned') {
    throw new TaskSpecCompileError('unsupported_graphwrite_route', 'Unsupported GraphWrite route scope.', [
      {
        code: 'unsupported_graphwrite_route',
        path: 'behavior.graph_strategy',
        message: `No active GraphWrite descriptor route for ${graphStrategy}:${publicScope}.`,
      },
    ]);
  }
  return route;
}

export function getGraphWriteCompilerIdForStrategy(graphStrategy: string): string | undefined {
  return ALL_GRAPHWRITE_ROUTES.find((route) => (
    route.graph_strategy === graphStrategy
    && route.status !== 'planned'
  ))?.compiler_id;
}

export function getGraphWriteRequiredFieldByStrategy(): Record<string, string> {
  const requiredFieldByStrategy: Record<string, string> = {};
  for (const route of ALL_GRAPHWRITE_ROUTES) {
    if (route.status === 'planned') continue;
    const existingField = requiredFieldByStrategy[route.graph_strategy];
    if (existingField && existingField !== route.behavior_field) {
      throw new TaskSpecCompileError('graphwrite_descriptor_invalid', 'GraphWrite route descriptor has inconsistent behavior fields.', [
        {
          code: 'graphwrite_descriptor_invalid',
          path: 'graphwrite_route_source',
          message: `graph_strategy="${route.graph_strategy}" maps to both "${existingField}" and "${route.behavior_field}".`,
        },
      ]);
    }
    requiredFieldByStrategy[route.graph_strategy] = route.behavior_field;
  }
  return requiredFieldByStrategy;
}

export function getSupportedGraphWriteStrategies(): string[] {
  return Object.keys(getGraphWriteRequiredFieldByStrategy()).sort();
}

export function normalizeSelectorWithDescriptor(
  descriptor: GraphWriteSelectorDescriptor,
  selector: Record<string, unknown>,
  path: string,
  replaceScope: string,
): Record<string, unknown> {
  const kind = getRequiredString(selector, 'kind', `${path}.kind`);
  if (kind !== descriptor.expected_kind) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `replace selector kind must match ${replaceScope}.`, [
      {
        code: 'replace_selector_scope_mismatch',
        path: `${path}.kind`,
        message: `${replaceScope} requires selector.kind="${descriptor.expected_kind}".`,
      },
    ]);
  }

  const out: Record<string, unknown> = {};
  for (const field of descriptor.passthrough_fields ?? []) {
    const value = selector[field];
    if (typeof value === 'string' && value.length > 0) {
      out[field] = value;
    }
  }
  for (const field of descriptor.required_fields) {
    getRequiredString(selector, field, `${path}.${field}`);
  }
  for (const [sourceField, outputField] of Object.entries(descriptor.output_fields)) {
    out[outputField] = getRequiredString(selector, sourceField, `${path}.${sourceField}`);
  }
  return out;
}

function getRequiredString(record: Record<string, unknown>, field: string, path: string): string {
  const value = record[field];
  if (typeof value === 'string' && value.trim().length > 0) {
    return value;
  }

  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be a non-empty string.`, [
    {
      code: 'missing_required_string',
      path,
      message: `${path} must be a non-empty string.`,
    },
  ]);
}
