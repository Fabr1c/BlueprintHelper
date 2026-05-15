import type { z } from 'zod';
import type { ToolResultBase } from '../../result/tool-result.js';
import type { BlueprintHelperToolContext } from '../types.js';

export interface ToolSource {
  readonly id: string;
  canHandle(toolName: string): boolean;
  getInputSchema(toolName: string): z.ZodTypeAny | undefined;
  execute(
    toolName: string,
    input: Record<string, unknown>,
    context: BlueprintHelperToolContext,
  ): Promise<ToolResultBase>;
}
