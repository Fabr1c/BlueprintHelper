import { z } from 'zod';

import type { BlueprintHelperToolContext } from '../types.js';
import {
  adaptToolInput,
  type InputShapeAdapterRegistry,
} from '../input/input-shape-adapter.js';
import { registerReadSpecInputShapeAdapters } from '../input/readspec-input-adapters.js';
import { createTaskSpecInputShapeAdapterRegistry } from '../input/taskspec-input-adapters.js';
import { buildReadonlyToolCommandManifestRegistry } from '../manifest/tool-command-manifest-builder.js';
import type { ToolCommandManifestRegistry } from '../manifest/tool-command-manifest-registry.js';
import type { ToolSource } from './tool-source.js';

export class ToolExecutorRegistry {
  private readonly sources: ToolSource[] = [];

  constructor(
    private readonly manifestRegistry: ToolCommandManifestRegistry = buildReadonlyToolCommandManifestRegistry(),
    private readonly inputShapeAdapters: InputShapeAdapterRegistry = registerReadSpecInputShapeAdapters(createTaskSpecInputShapeAdapterRegistry()),
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
    const manifest = this.manifestRegistry.get(toolName);
    const normalizedInput = manifest
      ? adaptToolInput(this.inputShapeAdapters, manifest.input_shapes, input)
      : input;
    return await source.execute(toolName, normalizedInput, context);
  }
}
