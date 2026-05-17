import { z } from 'zod';
import { sanitizeAgentFacingToolResult } from '../../result/tool-result.js';
import type { BlueprintHelperToolContext } from '../types.js';
import type { ToolMeta } from './tool-metas.js';
import { toolSources } from './tool-sources.js';

export function resolveToolInputSchema(name: string) {
  return findToolSource(name)?.getInputSchema(name) ?? z.record(z.unknown());
}

export function createToolExecutor(meta: ToolMeta) {
  return async (input: Record<string, unknown>, context: BlueprintHelperToolContext) => {
    const source = findToolSource(meta.name);
    if (!source) {
      throw new Error(`Tool is registered without a handler: ${meta.name}`);
    }
    return sanitizeAgentFacingToolResult(await source.execute(meta.name, input, context));
  };
}

function findToolSource(name: string) {
  return toolSources.find((source) => source.canHandle(name));
}
