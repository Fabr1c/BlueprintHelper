import { buildReadonlyToolCommandManifestRegistry } from '../manifest/tool-command-manifest-builder.js';
import type { ToolCommandManifestRegistry } from '../manifest/tool-command-manifest-registry.js';
import {
  adaptToolInput,
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
}): Record<string, unknown> {
  const manifestRegistry = input.manifestRegistry ?? buildReadonlyToolCommandManifestRegistry();
  const manifest = manifestRegistry.get(input.toolName);
  if (!manifest) {
    return input.value;
  }

  return adaptToolInput(
    input.inputShapeAdapters ?? createDefaultInputShapeAdapterRegistry(),
    manifest.input_shapes,
    input.value,
  );
}
