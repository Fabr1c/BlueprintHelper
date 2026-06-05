import { z } from 'zod';

import type { BlueprintHelperToolContext } from '../types.js';
import type { ToolSource } from './tool-source.js';

export class ToolExecutorRegistry {
  private readonly sources: ToolSource[] = [];

  register(source: ToolSource): this {
    this.sources.push(source);
    return this;
  }

  canHandle(toolName: string): boolean {
    return this.sources.some((source) => source.canHandle(toolName));
  }

  requireSource(toolName: string): ToolSource {
    const source = this.sources.find((candidate) => candidate.canHandle(toolName));
    if (!source) {
      throw new Error(`Tool is registered without a handler: ${toolName}`);
    }
    return source;
  }

  resolveInputSchema(toolName: string) {
    return this.sources.find((source) => source.canHandle(toolName))?.getInputSchema(toolName)
      ?? z.record(z.unknown());
  }

  async execute(toolName: string, input: Record<string, unknown>, context: BlueprintHelperToolContext) {
    return await this.requireSource(toolName).execute(toolName, input, context);
  }
}
