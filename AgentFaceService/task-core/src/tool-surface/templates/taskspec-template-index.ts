import {
  listGraphWriteTemplateClusters,
  listGraphWriteTemplateOperations,
  listGraphWriteTemplateQuickAccess,
  listGraphWriteTemplateWriteModes,
} from './graphwrite-template-metadata.js';
import {
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

export function listTaskSpecTemplateFamilies(input: {
  workflow?: 'preview_execute' | string;
} = {}): TaskSpecTemplateFamiliesResult {
  if (input.workflow !== undefined && input.workflow !== 'preview_execute') {
    throw new Error(`Unsupported TaskSpec template workflow: ${input.workflow}`);
  }
  return {
    schema: 'BlueprintHelper.TaskSpecTemplateFamilies.v1',
    workflow: 'preview_execute',
    guidance: 'Pick a write family first, then run bh tools templates write-modes --family <family> --format json. Do not hand-scan template files or report capability_missing before completing indexed discovery.',
    items: [
      {
        family: 'graph_write',
        task_type: 'edit_blueprint_graph',
        description: 'Create or modify Blueprint graph bodies through GraphWrite TaskSpec routes.',
        status: 'supported',
      },
      ...getSupportedNonGraphWriteTemplateFamilies().map((entry) => ({
        family: entry.family,
        task_type: entry.task_type,
        description: entry.description,
        status: 'supported' as const,
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
      guidance: 'Pick a write_mode for the selected family, then run bh tools templates clusters --family <family> --format json. write_mode selects mutation policy; it is not a template id.',
      items: listGraphWriteTemplateWriteModes(),
    };
  }

  const supported = getSupportedNonGraphWriteTemplateFamilies().find((entry) => entry.family === input.family);
  return {
    schema: 'BlueprintHelper.TaskSpecTemplateWriteModes.v1',
    family: input.family,
    guidance: 'Pick a write_mode for the selected family, then run bh tools templates clusters --family <family> --format json. write_mode selects mutation policy; it is not a template id.',
    items: supported?.write_mode
      ? [{
        family: supported.family,
        write_mode: supported.write_mode,
        description: supported.description,
        base_template_path: supported.base_template_path,
      }]
      : [],
  };
}

export function listTaskSpecTemplateClusters(input: {
  family: string;
}): TaskSpecTemplateClustersResult {
  return {
    schema: 'BlueprintHelper.TaskSpecTemplateClusters.v1',
    family: input.family,
    guidance: 'Pick a cluster for the selected family, then run bh tools templates operations --family <family> --cluster <cluster> --write-mode <mode> --format json. A cluster is a navigation group, not an executable TaskSpec.',
    items: input.family === 'graph_write'
      ? listGraphWriteTemplateClusters()
      : listNonGraphWriteTemplateClusters(input),
  };
}

export function listTaskSpecTemplateOperations(input: {
  family: string;
  cluster: string;
  writeMode: string;
}): TaskSpecTemplateOperationsResult {
  return {
    schema: 'BlueprintHelper.TaskSpecTemplateOperations.v1',
    family: input.family,
    cluster_id: input.cluster,
    write_mode: input.writeMode,
    guidance: 'Pick an operation, then run bh tools templates quick-access --family <family> --cluster <cluster> --operation <operation> --write-mode <mode> --format json. quick-access leaf entries provide compose template ids and slot arguments.',
    items: input.family === 'graph_write'
      ? listGraphWriteTemplateOperations(input)
      : listNonGraphWriteTemplateOperations(input),
  };
}

export function listTaskSpecTemplateQuickAccess(input: {
  family: string;
  cluster: string;
  operation: string;
  writeMode: string;
}): TaskSpecTemplateQuickAccessResult {
  return {
    schema: 'BlueprintHelper.TaskSpecTemplateQuickAccess.v1',
    family: input.family,
    cluster_id: input.cluster,
    operation_id: input.operation,
    write_mode: input.writeMode,
    items: input.family === 'graph_write'
      ? listGraphWriteTemplateQuickAccess(input)
      : listNonGraphWriteTemplateQuickAccess(input),
  };
}
