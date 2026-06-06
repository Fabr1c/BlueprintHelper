import { z } from 'zod';

import type { BlueprintHelperToolContext } from '../types.js';
import type { InputShapeAdapterRegistry } from '../input/input-shape-adapter.js';
import {
  createDefaultInputShapeAdapterRegistry,
  normalizeToolInputForManifest,
} from '../input/default-input-shape-adapters.js';
import { buildReadonlyToolCommandManifestRegistry } from '../manifest/tool-command-manifest-builder.js';
import type { ToolCommandManifestRegistry } from '../manifest/tool-command-manifest-registry.js';
import type { ToolSource } from './tool-source.js';

export class ToolExecutorRegistry {
  private readonly sources: ToolSource[] = [];

  constructor(
    private readonly manifestRegistry: ToolCommandManifestRegistry = buildReadonlyToolCommandManifestRegistry(),
    private readonly inputShapeAdapters: InputShapeAdapterRegistry = createDefaultInputShapeAdapterRegistry(),
  ) {}

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
    const source = this.requireSource(toolName);
    const normalizedInput = normalizeToolInputForManifest({
      toolName,
      value: input,
      manifestRegistry: this.manifestRegistry,
      inputShapeAdapters: this.inputShapeAdapters,
    });
    return await source.execute(toolName, normalizedInput, context);
  }
}
