import type {
  GetToolTemplateDispatchOptions,
  ToolTemplateDispatchResult,
} from '../catalog/tool-capability-types.js';
import type { ToolCommandManifestRegistry } from './tool-command-manifest-registry.js';

export interface ToolsTemplateBuilder {
  getTemplateDispatch(toolIdOrAlias: string, options?: GetToolTemplateDispatchOptions): ToolTemplateDispatchResult;
}

export interface ToolTemplateDispatchProvider {
  getRawTemplateDispatch(toolId: string, options?: GetToolTemplateDispatchOptions): ToolTemplateDispatchResult;
}

export function createToolsTemplateBuilderCore(
  registry: ToolCommandManifestRegistry,
  provider: ToolTemplateDispatchProvider,
): ToolsTemplateBuilder {
  return {
    getTemplateDispatch(toolIdOrAlias: string, options: GetToolTemplateDispatchOptions = {}) {
      const manifest = registry.get(toolIdOrAlias);
      if (manifest) {
        return provider.getRawTemplateDispatch(manifest.tool_id, options);
      }

      preserveAmbiguousLookupErrors(registry, toolIdOrAlias);
      return provider.getRawTemplateDispatch(toolIdOrAlias, options);
    },
  };
}

function preserveAmbiguousLookupErrors(
  registry: ToolCommandManifestRegistry,
  toolIdOrAlias: string,
): void {
  try {
    registry.require(toolIdOrAlias);
  } catch (error) {
    if (error instanceof Error && error.message.startsWith('Ambiguous BlueprintHelper tool command manifest lookup:')) {
      throw error;
    }
  }
}
