import {
  listGraphWriteTemplateClusters,
  listGraphWriteTemplateOperations,
  listGraphWriteTemplateQuickAccess,
  listGraphWriteTemplateWriteModes,
} from './graphwrite-template-metadata.js';
import {
  getNonGraphWriteTemplateFamily,
  getSupportedNonGraphWriteTemplateFamilies,
  listNonGraphWriteTemplateClusters,
  listNonGraphWriteTemplateOperations,
  listNonGraphWriteTemplateQuickAccess,
} from './non-graphwrite-template-metadata.js';
import type {
  TaskSpecTemplateClustersResult,
  TaskSpecTemplateFamiliesResult,
  TaskSpecTemplateOperationsResult,
  TaskSpecTemplateQuickAccessResult,
  TaskSpecTemplateWriteModesResult,
} from './taskspec-template-types.js';

const GRAPH_WRITE_NAVIGATION = {
  levels: ['write_mode', 'cluster', 'operation', 'quick_access', 'leaf_template'],
  next_command: 'bh tools templates write-modes --family graph_write --format json',
  compose_command: 'bh tools templates compose --family graph_write --write-mode <mode> --templates <slot_expr[,slot_expr...]> --out <task-spec.json> --format json',
  requires_write_mode: true,
} as const;

export function listTaskSpecTemplateFamilies(input: {
  workflow?: 'preview_execute' | string;
} = {}): TaskSpecTemplateFamiliesResult {
  if (input.workflow !== undefined && input.workflow !== 'preview_execute') {
    throw new Error(`Unsupported TaskSpec template workflow: ${input.workflow}`);
  }
  return {
    schema: 'BlueprintHelper.TaskSpecTemplateFamilies.v1',
    workflow: 'preview_execute',
    guidance: 'Pick a write family first, then follow the family navigation descriptor. GraphWrite declares write-modes; non-GraphWrite families compose leaf templates with bh tools templates compose --template <leaf_template_id> --out <task-spec.json> --format json. Do not hand-scan template files or report capability_missing before completing indexed discovery.',
    items: [
      {
        family: 'graph_write',
        task_type: 'edit_blueprint_graph',
        description: 'Create or modify Blueprint graph bodies through GraphWrite TaskSpec routes.',
        status: 'supported',
        navigation: GRAPH_WRITE_NAVIGATION,
      },
      ...getSupportedNonGraphWriteTemplateFamilies().map((entry) => ({
        family: entry.family,
        task_type: entry.task_type,
        description: entry.description,
        status: 'supported' as const,
        navigation: entry.navigation,
      })),
    ],
  };
}

export function listTaskSpecTemplateWriteModes(input: {
  family: string;
}): TaskSpecTemplateWriteModesResult {
  if (input.family === 'graph_write') {
    return {
      schema: 'BlueprintHelper.TaskSpecTemplateWriteModes.v1',
      family: input.family,
      status: 'ok',
      guidance: 'Pick a write_mode for the selected family, then run bh tools templates clusters --family <family> --format json. write_mode selects mutation policy; it is not a template id.',
      items: listGraphWriteTemplateWriteModes(),
    };
  }

  const supported = getSupportedNonGraphWriteTemplateFamilies().find((entry) => entry.family === input.family);
  return {
    schema: 'BlueprintHelper.TaskSpecTemplateWriteModes.v1',
    family: input.family,
    status: 'failed',
    guidance: supported
      ? `${supported.family} does not declare a write-mode navigation level. Use its family-defined next command instead.`
      : 'Unknown TaskSpec template family. Run bh tools templates families --workflow preview_execute --format json.',
    items: [],
    diagnostics: [{
      code: supported ? 'navigation_level_not_supported' : 'unsupported_family',
      family: input.family,
      message: supported
        ? `family ${supported.family} does not declare write-mode level`
        : `Unsupported TaskSpec template family: ${input.family}`,
      safe_next_action: supported ? 'use_family_navigation_next_command' : 'run_template_families',
      suggested_route: supported?.navigation.next_command ?? 'bh tools templates families --workflow preview_execute --format json',
    }],
  };
}

export function listTaskSpecTemplateClusters(input: {
  family: string;
}): TaskSpecTemplateClustersResult {
  return {
    schema: 'BlueprintHelper.TaskSpecTemplateClusters.v1',
    family: input.family,
    guidance: 'Pick a cluster only when the selected family declares a cluster navigation level, then run bh tools templates operations --family <family> --cluster <cluster> --format json. GraphWrite also requires --write-mode <mode>. A cluster is a navigation group, not an executable TaskSpec.',
    items: input.family === 'graph_write'
      ? listGraphWriteTemplateClusters()
      : listNonGraphWriteTemplateClusters(input),
  };
}

export function listTaskSpecTemplateOperations(input: {
  family: string;
  cluster?: string;
  writeMode?: string;
}): TaskSpecTemplateOperationsResult {
  const family = getNonGraphWriteTemplateFamily(input.family);
  const cluster = input.cluster ?? '';
  const writeMode = input.writeMode ?? '';
  return {
    schema: 'BlueprintHelper.TaskSpecTemplateOperations.v1',
    family: input.family,
    cluster_id: cluster.length > 0 ? cluster : undefined,
    write_mode: input.family === 'graph_write' && writeMode.length > 0 ? writeMode : undefined,
    guidance: operationGuidance(input.family, family?.navigation.next_command),
    items: input.family === 'graph_write'
      ? listGraphWriteTemplateOperations({ cluster, writeMode })
      : listNonGraphWriteTemplateOperations({ family: input.family, cluster }),
  };
}

export function listTaskSpecTemplateQuickAccess(input: {
  family: string;
  cluster?: string;
  operation?: string;
  writeMode?: string;
}): TaskSpecTemplateQuickAccessResult {
  const cluster = input.cluster ?? '';
  const operation = input.operation ?? '';
  const writeMode = input.writeMode ?? '';
  return {
    schema: 'BlueprintHelper.TaskSpecTemplateQuickAccess.v1',
    family: input.family,
    cluster_id: cluster.length > 0 ? cluster : undefined,
    operation_id: operation.length > 0 ? operation : undefined,
    write_mode: input.family === 'graph_write' && writeMode.length > 0 ? writeMode : undefined,
    items: input.family === 'graph_write'
      ? listGraphWriteTemplateQuickAccess({ cluster, operation, writeMode })
      : listNonGraphWriteTemplateQuickAccess({ family: input.family, cluster, operation }),
  };
}

function operationGuidance(family: string, nextCommand?: string): string {
  if (family === 'graph_write') {
    return 'Pick an operation, then run bh tools templates quick-access --family graph_write --cluster <cluster> --operation <operation> --write-mode <mode> --format json. quick-access leaf entries provide compose slot ids and arguments.';
  }
  return `Pick an operation, then run bh tools templates quick-access --family <family> --operation <operation> --format json. Compose non-GraphWrite scaffolds with bh tools templates compose --template <leaf_template_id> --out <task-spec.json> --format json. Family next command: ${nextCommand ?? 'bh tools templates families --workflow preview_execute --format json'}`;
}
