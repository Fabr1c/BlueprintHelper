import {
  ExecuteTaskInputSchema,
  PreviewTaskInputSchema,
  TaskSpecSchema,
} from '../../task/schema/task-schemas.js';
import {
  InputShapeAdapterError,
  InputShapeAdapterRegistry,
  type InputShapeAdapter,
} from './input-shape-adapter.js';

const bareTaskSpecAdapter: InputShapeAdapter<{ task_spec: unknown }> = {
  id: 'bare_taskspec',
  inputSchema: TaskSpecSchema,
  adapt(input) {
    if (input && typeof input === 'object' && !Array.isArray(input) && 'preview_token' in input) {
      throw new InputShapeAdapterError(
        'preview_token_requires_task_spec_wrapper',
        'execute_task preview_token is only accepted on the wrapped input shape: { task_spec, preview_token }.',
        'preview_token',
      );
    }
    return { task_spec: TaskSpecSchema.parse(normalizeAgentFacingTaskSpec(input)) };
  },
};

const wrappedPreviewTaskSpecAdapter: InputShapeAdapter<{ task_spec: unknown }> = {
  id: 'wrapped_taskspec_preview',
  inputSchema: PreviewTaskInputSchema,
  adapt(input) {
    return PreviewTaskInputSchema.parse(normalizeWrappedTaskSpecInput(input)) as { task_spec: unknown };
  },
};

const wrappedExecuteTaskSpecAdapter: InputShapeAdapter<{ task_spec: unknown; preview_token?: string }> = {
  id: 'wrapped_taskspec_execute',
  inputSchema: ExecuteTaskInputSchema,
  adapt(input) {
    return ExecuteTaskInputSchema.parse(normalizeWrappedTaskSpecInput(input)) as { task_spec: unknown; preview_token?: string };
  },
};

const EXTERNAL_MUTATIONS_BY_GRAPH_STRATEGY: Record<string, string[]> = {
  merge_external_flow: ['exec_boundary_link'],
  patch_external_graph: ['pin_default', 'node_comment', 'node_property'],
  patch_external_links: ['link_connect', 'link_disconnect', 'link_replace'],
  replace_external_body: ['body_replace'],
};

function normalizeWrappedTaskSpecInput(input: unknown): unknown {
  if (!isRecord(input) || !Object.hasOwn(input, 'task_spec')) {
    return input;
  }
  return {
    ...input,
    task_spec: normalizeAgentFacingTaskSpec(input['task_spec']),
  };
}

function normalizeAgentFacingTaskSpec(input: unknown): unknown {
  if (!isRecord(input) || input['task_type'] !== 'edit_blueprint_graph') {
    return input;
  }
  const behavior = asRecord(input['behavior']);
  if (!behavior) {
    return input;
  }
  const strategy = typeof behavior?.['graph_strategy'] === 'string'
    ? behavior['graph_strategy']
    : undefined;
  if (!strategy) {
    return input;
  }

  const output: Record<string, unknown> = { ...input };
  const mutations = EXTERNAL_MUTATIONS_BY_GRAPH_STRATEGY[strategy];
  if (mutations) {
    output['scope_policy'] = normalizeExternalScopePolicy(input, strategy, mutations, behavior);
  }
  return output;
}

function normalizeExternalScopePolicy(
  taskSpec: Record<string, unknown>,
  strategy: string,
  mutations: string[],
  behavior: Record<string, unknown>,
): Record<string, unknown> {
  const current = asRecord(taskSpec['scope_policy']) ?? {};
  const graphName = typeof current['graph_name'] === 'string' && current['graph_name'].trim().length > 0
    ? current['graph_name']
    : inferExternalGraphName(behavior);
  return {
    ...current,
    ...(graphName ? { graph_name: graphName } : {}),
    allow_modify_user_nodes: current['allow_modify_user_nodes'] ?? false,
    external_mutation_policy: current['external_mutation_policy'] ?? {
      strategy,
      allowed_mutations: mutations,
    },
  };
}

function inferExternalGraphName(behavior: Record<string, unknown>): string | undefined {
  const externalReplace = asRecord(behavior['external_replace']);
  const externalReplaceGraph = graphNameFromAnchor(asRecord(externalReplace?.['anchor']));
  if (externalReplaceGraph) return externalReplaceGraph;

  for (const field of ['external_merges', 'external_patches', 'external_link_patches']) {
    const items = Array.isArray(behavior[field]) ? behavior[field] : [];
    for (const item of items) {
      const anchor = asRecord(asRecord(item)?.['anchor']);
      const graphName = graphNameFromAnchor(anchor);
      if (graphName) return graphName;
    }
  }
  return undefined;
}

function graphNameFromAnchor(anchor: Record<string, unknown> | undefined): string | undefined {
  const value = typeof anchor?.['graph_name'] === 'string'
    ? anchor['graph_name']
    : typeof anchor?.['graph'] === 'string'
      ? anchor['graph']
      : undefined;
  return value && value.trim().length > 0 ? value.trim() : undefined;
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return isRecord(value) ? value : undefined;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

export function createTaskSpecInputShapeAdapterRegistry(): InputShapeAdapterRegistry {
  return new InputShapeAdapterRegistry()
    .register(bareTaskSpecAdapter)
    .register(wrappedPreviewTaskSpecAdapter)
    .register(wrappedExecuteTaskSpecAdapter);
}
