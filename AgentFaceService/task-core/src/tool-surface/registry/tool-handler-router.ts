import { sanitizeAgentFacingToolResult } from '../../result/tool-result.js';
import type { BlueprintHelperToolContext } from '../types.js';
import type { ToolMeta } from './tool-metas.js';
import { ToolExecutorRegistry } from './tool-executor-registry.js';
import { toolSources } from './tool-sources.js';

const defaultToolExecutorRegistry = toolSources.reduce(
  (registry, source) => registry.register(source),
  new ToolExecutorRegistry(),
);

export function resolveToolInputSchema(name: string) {
  return defaultToolExecutorRegistry.resolveInputSchema(name);
}

export function createToolExecutor(meta: ToolMeta) {
  return async (input: Record<string, unknown>, context: BlueprintHelperToolContext) => {
    return sanitizeAgentFacingToolResult(await defaultToolExecutorRegistry.execute(meta.name, input, context), {
      preserveDebug: context.expert === true,
    });
  };
}
