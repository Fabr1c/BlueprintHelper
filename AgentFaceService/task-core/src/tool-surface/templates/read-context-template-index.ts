import { getActiveReadContextRouteDescriptors } from './read-context-template-registry.js';
import type {
  ReadContextRouteDescriptor,
  ReadContextTemplateClustersResult,
  ReadContextTemplateDomainsResult,
  ReadContextTemplateQuickAccessItem,
  ReadContextTemplateQuickAccessResult,
  ReadContextTemplateTargetsResult,
  ReadContextTemplateViewsResult,
} from './read-context-template-types.js';

export function listReadContextTemplateDomains(): ReadContextTemplateDomainsResult {
  return {
    schema: 'BlueprintHelper.ReadContextTemplateDomains.v1',
    workflow: 'read_context',
    items: uniqueSorted(
      getActiveReadContextRouteDescriptors(),
      (route) => route.domain,
    ).map((domain) => ({
      domain,
      status: 'supported' as const,
    })),
  };
}

export function listReadContextTemplateClusters(input: {
  domain: string;
}): ReadContextTemplateClustersResult {
  return {
    schema: 'BlueprintHelper.ReadContextTemplateClusters.v1',
    domain: input.domain,
    items: uniqueSorted(
      matchingRoutes({ domain: input.domain }),
      (route) => route.read_cluster,
    ).map((readCluster) => ({
      domain: input.domain,
      read_cluster: readCluster,
    })),
  };
}

export function listReadContextTemplateTargets(input: {
  domain: string;
  readCluster: string;
}): ReadContextTemplateTargetsResult {
  const routes = matchingRoutes({
    domain: input.domain,
    readCluster: input.readCluster,
  });
  return {
    schema: 'BlueprintHelper.ReadContextTemplateTargets.v1',
    domain: input.domain,
    read_cluster: input.readCluster,
    items: uniqueSorted(routes, (route) => route.target_kind).map((targetKind) => {
      const targetRoutes = routes.filter((route) => route.target_kind === targetKind);
      return {
        domain: input.domain,
        read_cluster: input.readCluster,
        target_kind: targetKind,
        required_target_fields: uniqueStringSet(targetRoutes.flatMap((route) => route.required_target_fields)),
      };
    }),
  };
}

export function listReadContextTemplateViews(input: {
  domain: string;
  readCluster: string;
  targetKind: string;
}): ReadContextTemplateViewsResult {
  return {
    schema: 'BlueprintHelper.ReadContextTemplateViews.v1',
    domain: input.domain,
    read_cluster: input.readCluster,
    target_kind: input.targetKind,
    items: matchingRoutes(input)
      .map((route) => ({
        domain: route.domain,
        read_cluster: route.read_cluster,
        target_kind: route.target_kind,
        view_template: route.view_template,
        output_schema: route.output_schema,
      }))
      .sort((left, right) => left.view_template.localeCompare(right.view_template)),
  };
}

export function listReadContextTemplateQuickAccess(input: {
  domain: string;
  readCluster: string;
  targetKind: string;
  viewTemplate: string;
}): ReadContextTemplateQuickAccessResult {
  return {
    schema: 'BlueprintHelper.ReadContextTemplateQuickAccess.v1',
    domain: input.domain,
    read_cluster: input.readCluster,
    target_kind: input.targetKind,
    view_template: input.viewTemplate,
    items: matchingRoutes(input)
      .filter((route) => route.view_template === input.viewTemplate)
      .map(toQuickAccessItem),
  };
}

function matchingRoutes(input: {
  domain: string;
  readCluster?: string;
  targetKind?: string;
}): readonly ReadContextRouteDescriptor[] {
  return getActiveReadContextRouteDescriptors().filter((route) =>
    route.domain === input.domain
    && (input.readCluster === undefined || route.read_cluster === input.readCluster)
    && (input.targetKind === undefined || route.target_kind === input.targetKind));
}

function toQuickAccessItem(route: ReadContextRouteDescriptor): ReadContextTemplateQuickAccessItem {
  return {
    template_id: route.route_id,
    domain: route.domain,
    read_cluster: route.read_cluster,
    target_kind: route.target_kind,
    view_template: route.view_template,
    source_route_id: route.route_id,
    read_type: route.read_type,
    target_type: route.target_type,
    format: route.format,
    template_path: route.base_template_path,
    required_target_fields: [...route.required_target_fields],
    output_schema: route.output_schema,
  };
}

function uniqueSorted<T>(items: readonly T[], keyOf: (item: T) => string): string[] {
  return uniqueStringSet(items.map(keyOf));
}

function uniqueStringSet(items: readonly string[]): string[] {
  return [...new Set(items)].sort((left, right) => left.localeCompare(right));
}
