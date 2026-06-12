import { buildReadonlyToolCommandManifestRegistry } from '../manifest/tool-command-manifest-builder.js';
import type { ToolInputShapeId } from '../manifest/tool-command-manifest.js';
import type { ToolCommandManifestRegistry } from '../manifest/tool-command-manifest-registry.js';
import {
  adaptToolInput,
  type InputShapeId,
  type InputShapeAdapterRegistry,
} from './input-shape-adapter.js';
import { registerReadSpecInputShapeAdapters } from './readspec-input-adapters.js';
import { createTaskSpecInputShapeAdapterRegistry } from './taskspec-input-adapters.js';

export function createDefaultInputShapeAdapterRegistry(): InputShapeAdapterRegistry {
  return registerReadSpecInputShapeAdapters(createTaskSpecInputShapeAdapterRegistry());
}

export function normalizeToolInputForManifest(input: {
  readonly toolName: string;
  readonly value: Record<string, unknown>;
  readonly manifestRegistry?: ToolCommandManifestRegistry;
  readonly inputShapeAdapters?: InputShapeAdapterRegistry;
  readonly requireManifest?: boolean;
}): Record<string, unknown> {
  const manifestRegistry = input.manifestRegistry ?? buildReadonlyToolCommandManifestRegistry();
  const manifest = manifestRegistry.get(input.toolName);
  if (!manifest) {
    if (input.requireManifest === true) {
      throw new Error(`ToolCommandManifest is required before input normalization: ${input.toolName}`);
    }
    return input.value;
  }

  const adapterInputShapes = manifest.input_shapes.filter(isAdapterInputShape);
  if (adapterInputShapes.length === 0) {
    return input.value;
  }

  return adaptToolInput(
    input.inputShapeAdapters ?? createDefaultInputShapeAdapterRegistry(),
    adapterInputShapes,
    input.value,
  );
}

function isAdapterInputShape(shape: ToolInputShapeId): shape is InputShapeId {
  return shape !== 'cli_options';
}
