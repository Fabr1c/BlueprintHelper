import type { ToolCommandManifest } from './tool-command-manifest.js';

export interface ToolCommandManifestRegistryOptions {
  canonicalAliases?: ReadonlyMap<string, string> | Record<string, string>;
}

export interface ToolCommandManifestRegistry {
  list(): ToolCommandManifest[];
  get(toolIdOrAlias: string): ToolCommandManifest | undefined;
  require(toolIdOrAlias: string): ToolCommandManifest;
  has(toolIdOrAlias: string): boolean;
}

export function createToolCommandManifestRegistry(
  manifests: readonly ToolCommandManifest[],
  options: ToolCommandManifestRegistryOptions = {},
): ToolCommandManifestRegistry {
  const items = [...manifests];
  const manifestsById = new Map<string, ToolCommandManifest>();
  const lookupCandidates = new Map<string, ToolCommandManifest[]>();

  for (const manifest of items) {
    if (manifestsById.has(manifest.tool_id)) {
      throw new Error(`Duplicate BlueprintHelper tool command manifest id: ${manifest.tool_id}`);
    }
    manifestsById.set(manifest.tool_id, manifest);
    addLookupCandidate(lookupCandidates, manifest.tool_name, manifest);
    for (const alias of manifest.aliases) {
      addLookupCandidate(lookupCandidates, alias, manifest);
    }
  }
  const canonicalAliasIndex = buildCanonicalAliasIndex(manifestsById, options.canonicalAliases);

  return {
    list() {
      return [...items];
    },
    get(toolIdOrAlias: string) {
      const direct = manifestsById.get(toolIdOrAlias);
      if (direct) {
        return direct;
      }
      const canonicalAlias = canonicalAliasIndex.get(toolIdOrAlias);
      if (canonicalAlias) {
        return canonicalAlias;
      }
      const candidates = lookupCandidates.get(toolIdOrAlias) ?? [];
      return candidates.length === 1 ? candidates[0] : undefined;
    },
    require(toolIdOrAlias: string) {
      const manifest = this.get(toolIdOrAlias);
      if (!manifest) {
        const candidates = lookupCandidates.get(toolIdOrAlias) ?? [];
        if (candidates.length > 1) {
          throw new Error(`Ambiguous BlueprintHelper tool command manifest lookup: ${toolIdOrAlias} -> ${candidates.map((entry) => entry.tool_id).join(', ')}`);
        }
        throw new Error(`Unknown BlueprintHelper tool command manifest: ${toolIdOrAlias}`);
      }
      return manifest;
    },
    has(toolIdOrAlias: string) {
      return this.get(toolIdOrAlias) !== undefined;
    },
  };
}

function addLookupCandidate(
  lookupCandidates: Map<string, ToolCommandManifest[]>,
  key: string,
  manifest: ToolCommandManifest,
): void {
  const candidates = lookupCandidates.get(key) ?? [];
  if (!candidates.some((entry) => entry.tool_id === manifest.tool_id)) {
    candidates.push(manifest);
  }
  lookupCandidates.set(key, candidates);
}

function buildCanonicalAliasIndex(
  manifestsById: ReadonlyMap<string, ToolCommandManifest>,
  canonicalAliases: ToolCommandManifestRegistryOptions['canonicalAliases'],
): Map<string, ToolCommandManifest> {
  const entries = canonicalAliases instanceof Map
    ? canonicalAliases.entries()
    : Object.entries(canonicalAliases ?? {});
  const index = new Map<string, ToolCommandManifest>();
  for (const [alias, toolId] of entries) {
    const manifest = manifestsById.get(toolId);
    if (!manifest) {
      throw new Error(`Canonical BlueprintHelper tool command alias points to an unknown manifest: ${alias} -> ${toolId}`);
    }
    index.set(alias, manifest);
  }
  return index;
}
