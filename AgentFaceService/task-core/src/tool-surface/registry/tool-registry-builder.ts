import type { BlueprintHelperToolDefinition } from '../types.js';
import { createToolExecutor, resolveToolInputSchema } from './tool-handler-router.js';
import { toolMetas } from './tool-metas.js';

export function getBlueprintHelperToolRegistry(): BlueprintHelperToolDefinition[] {
  return toolMetas.map((meta) => ({
    ...meta,
    inputSchema: resolveToolInputSchema(meta.name),
    execute: createToolExecutor(meta),
  }));
}

export function getBlueprintHelperTool(name: string): BlueprintHelperToolDefinition | undefined {
  return getBlueprintHelperToolRegistry().find((tool) => tool.name === name);
}
