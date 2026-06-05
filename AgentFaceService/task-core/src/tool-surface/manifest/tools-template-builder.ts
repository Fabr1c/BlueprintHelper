import type {
  GetToolTemplateDispatchOptions,
  ToolTemplateDispatchResult,
} from '../catalog/tool-capability-types.js';
import { getRawToolTemplateDispatch } from '../catalog/tool-capability-catalog.js';
import {
  buildReadonlyToolCommandManifestRegistry,
} from './tool-command-manifest-builder.js';
import type { ToolCommandManifestRegistry } from './tool-command-manifest-registry.js';
import {
  createToolsTemplateBuilderCore,
  type ToolsTemplateBuilder,
} from './tools-template-builder-core.js';

export function createToolsTemplateBuilder(
  registry: ToolCommandManifestRegistry = buildReadonlyToolCommandManifestRegistry(),
): ToolsTemplateBuilder {
  return createToolsTemplateBuilderCore(registry, {
    getRawTemplateDispatch: getRawToolTemplateDispatch,
  });
}

export type { ToolsTemplateBuilder };
