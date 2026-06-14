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
      description: describeDomain(domain),
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
      description: describeCluster(input.domain, readCluster),
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
        description: describeTarget(input.domain, input.readCluster, targetKind),
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
        description: describeView(route.view_template),
        output_schema: route.output_schema,
      }))
      .sort((left, right) => left.view_template.localeCompare(right.view_template)),
  };
}

function describeDomain(domain: string): string {
  const descriptions: Readonly<Record<string, string>> = {
    blueprint: 'Read Blueprint asset, logic, component, variable, and property context.',
    widget_blueprint: 'Read Widget Blueprint tree, widget property, and widget logic context.',
    data_table: 'Read DataTable schema and row context.',
    data_asset: 'Read DataAsset object property context.',
    material: 'Read Material graph expressions, parameters, connections, outputs, and owned anchors.',
  };
  return descriptions[domain] ?? `ReadContext templates for ${domain} assets.`;
}

function describeCluster(domain: string, readCluster: string): string {
  const domainSpecificDescriptions: Readonly<Record<string, Readonly<Record<string, string>>>> = {
    material: {
      logic: 'Read Material graph expressions, parameters, connections, outputs, and owned anchors.',
    },
  };
  const domainDescription = domainSpecificDescriptions[domain]?.[readCluster];
  if (domainDescription) {
    return domainDescription;
  }

  const descriptions: Readonly<Record<string, string>> = {
    asset: 'Read asset-level diagnostics and identity facts.',
    components: 'Read Blueprint component tree and component metadata facts.',
    logic: 'Read Blueprint graph, event, function, custom event, or owned block logic.',
    properties: 'Read UObject property values and property metadata.',
    schema: 'Read schema-like data such as rows, fields, or object properties.',
    structure_tree: 'Read Widget Blueprint hierarchy and widget structure facts.',
    variables: 'Read Blueprint member variable and event dispatcher metadata.',
  };
  return descriptions[readCluster] ?? `Read ${readCluster} context for ${domain}.`;
}

function describeTarget(domain: string, readCluster: string, targetKind: string): string {
  const domainSpecificDescriptions: Readonly<Record<string, Readonly<Record<string, string>>>> = {
    material: {
      graph: 'Read a Material graph target using asset_path; Material assets do not require target_name for P0 graph reads.',
    },
  };
  const domainDescription = domainSpecificDescriptions[domain]?.[targetKind];
  if (domainDescription) {
    return domainDescription;
  }

  const descriptions: Readonly<Record<string, string>> = {
    asset: 'Read an asset-level target using asset_path.',
    block: 'Read a BlueprintHelper-owned logic block using asset_path and block_id.',
    blueprint: 'Read the whole Blueprint-level target using asset_path.',
    custom_event: 'Read a named custom event graph using asset_path and target_name.',
    data_asset: 'Read a DataAsset object using asset_path; property_json for this target reads the object payload and does not require target_name.',
    data_table: 'Read a DataTable using asset_path.',
    data_table_row: 'Read a named DataTable row using asset_path and target_name.',
    event: 'Read a named Blueprint event graph using asset_path and target_name.',
    event_dispatcher: 'Read Blueprint event dispatcher metadata using asset_path.',
    function: 'Read a named Blueprint function graph using asset_path and target_name.',
    graph: 'Read a named Blueprint graph using asset_path and target_name.',
    property: 'Read a named object property using asset_path and target_name; target_name is the ReadContext property locator/filter.',
    widget: 'Read a named widget using asset_path and target_name; target_name is the ReadContext widget locator/filter used by widget property_json.',
    widget_tree: 'Read the Widget Blueprint tree using asset_path.',
  };
  return descriptions[targetKind] ?? `Read ${targetKind} target context for ${domain}.${readCluster}.`;
}

function describeView(viewTemplate: string): string {
  const descriptions: Readonly<Record<string, string>> = {
    diagnostics_json: 'Asset diagnostic JSON for validation and targeting.',
    logic_flow: 'Compact execution and data-flow view for Agent reasoning.',
    logic_json: 'Structured logic JSON with stable anchors for precise follow-up reads or writes.',
    logic_md: 'Markdown logic summary for Agent reasoning.',
    property_json: 'Structured property JSON for object or widget property reads.',
    schema_json: 'Structured schema JSON for variables, rows, or object fields.',
    tree_json: 'Structured tree JSON for component or widget hierarchy reads.',
  };
  return descriptions[viewTemplate] ?? `ReadContext ${viewTemplate} output.`;
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
