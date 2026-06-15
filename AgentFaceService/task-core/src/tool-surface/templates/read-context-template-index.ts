import { getActiveReadContextRouteDescriptors } from './read-context-template-registry.js';
import type {
  ReadContextRouteDescriptor,
  ReadContextTemplateClustersResult,
  ReadContextTemplateDescriptor,
  ReadContextTemplateFamiliesResult,
  ReadContextTemplatesResult,
} from './read-context-template-types.js';

export function listReadContextTemplateFamilies(): ReadContextTemplateFamiliesResult {
  return {
    schema: 'BlueprintHelper.ReadContextTemplateFamilies.v1',
    workflow: 'read_context',
    items: uniqueSorted(
      getActiveReadContextRouteDescriptors(),
      (route) => route.family,
    ).map((family) => ({
      family,
      description: describeFamily(family),
      status: 'supported' as const,
    })),
  };
}

export function listReadContextTemplateClusters(input: {
  family: string;
}): ReadContextTemplateClustersResult {
  return {
    schema: 'BlueprintHelper.ReadContextTemplateClusters.v1',
    family: input.family,
    items: uniqueSorted(
      matchingRoutes({ family: input.family }),
      (route) => route.cluster,
    ).map((cluster) => ({
      family: input.family,
      cluster,
      description: describeCluster(input.family, cluster),
    })),
  };
}

export function listReadContextTemplates(input: {
  family: string;
  cluster: string;
}): ReadContextTemplatesResult {
  return {
    schema: 'BlueprintHelper.ReadContextTemplates.v1',
    family: input.family,
    cluster: input.cluster,
    items: matchingRoutes(input)
      .map(toTemplateDescriptor)
      .sort((left, right) => left.template_id.localeCompare(right.template_id)),
  };
}

function matchingRoutes(input: {
  family: string;
  cluster?: string;
}): readonly ReadContextRouteDescriptor[] {
  return getActiveReadContextRouteDescriptors().filter((route) =>
    route.family === input.family
    && (input.cluster === undefined || route.cluster === input.cluster));
}

function toTemplateDescriptor(route: ReadContextRouteDescriptor): ReadContextTemplateDescriptor {
  return {
    template_id: route.template_id,
    family: route.family,
    cluster: route.cluster,
    description: route.description,
    template_path: route.template_path,
    read_spec: route.read_spec,
    required_fields: [...route.required_fields],
    optional_fields: [...route.optional_fields],
    context_evidence: { ...route.context_evidence },
    output_schema: route.output_schema,
    recommended_invocation: route.recommended_invocation,
    allowed_tools: [...route.allowed_tools] as ReadContextTemplateDescriptor['allowed_tools'],
    stop_conditions: [...route.stop_conditions],
  };
}

function describeFamily(family: string): string {
  const descriptions: Readonly<Record<string, string>> = {
    blueprint: 'Read Blueprint asset, logic, component, variable, and property context.',
    widget_blueprint: 'Read Widget Blueprint tree, widget property, and widget logic context.',
    data_table: 'Read DataTable schema and row context.',
    data_asset: 'Read DataAsset object property context.',
    material: 'Read Material graph expressions, parameters, connections, outputs, and owned anchors.',
  };
  return descriptions[family] ?? `ReadContext templates for ${family} assets.`;
}

function describeCluster(family: string, cluster: string): string {
  const familySpecificDescriptions: Readonly<Record<string, Readonly<Record<string, string>>>> = {
    material: {
      logic: 'Read Material graph expressions, parameters, connections, outputs, and owned anchors.',
    },
  };
  const familyDescription = familySpecificDescriptions[family]?.[cluster];
  if (familyDescription) {
    return familyDescription;
  }

  const descriptions: Readonly<Record<string, string>> = {
    asset: 'Read asset-level diagnostics and identity facts.',
    logic: 'Read Blueprint graph, event, function, custom event, owned block, or Material graph logic.',
    properties: 'Read UObject or widget property values and property metadata.',
    schema: 'Read schema-like data such as rows, fields, variables, or dispatchers.',
    structure: 'Read component or Widget Blueprint hierarchy and structure facts.',
  };
  return descriptions[cluster] ?? `Read ${cluster} context for ${family}.`;
}

function uniqueSorted<T>(items: readonly T[], keyOf: (item: T) => string): string[] {
  return [...new Set(items.map(keyOf))].sort((left, right) => left.localeCompare(right));
}
