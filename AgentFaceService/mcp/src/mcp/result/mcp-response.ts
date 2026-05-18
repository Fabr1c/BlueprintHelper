export type BlueprintViewFormat = 'logic_md' | 'logic_json' | 'raw_json';

export type McpResponseMode =
  | 'summary_text'
  | 'structured_json'
  | 'resource_ref';

export type BlueprintResourceView =
  | 'logic-md'
  | 'logic-json'
  | 'raw-json'
  | 'diff'
  | 'compile-result';

export interface BlueprintDiagnostic {
  severity: 'info' | 'warning' | 'error';
  code: string;
  message: string;
  nodeId?: string;
  pin?: string;
}

export interface BlueprintResultMeta {
  format: BlueprintViewFormat | 'raw_json_ref';
  schema: string;
  assetPath: string;
  graph?: string;
  importable: boolean;
  stats?: Record<string, unknown>;
  diagnostics?: BlueprintDiagnostic[] | unknown[];
}

export interface BuildToolResultOptions {
  mode: McpResponseMode;
  summary?: string;
  structured?: Record<string, unknown>;
  markdown?: string;
  resourceLinks?: Array<{
    uri: string;
    name: string;
    description?: string;
    mimeType: string;
  }>;
}

export type BlueprintToolContent =
  | { type: 'text'; text: string }
  | {
      type: 'resource_link';
      uri: string;
      name: string;
      description?: string;
      mimeType: string;
    };

export interface BlueprintToolResult {
  [key: string]: unknown;
  content: BlueprintToolContent[];
  isError: boolean;
  structuredContent?: Record<string, unknown>;
}

export function normalizeBridgeResult(value: unknown): unknown {
  if (typeof value !== 'string') {
    return value;
  }

  const trimmed = value.trim();
  const looksLikeJson =
    (trimmed.startsWith('{') && trimmed.endsWith('}')) ||
    (trimmed.startsWith('[') && trimmed.endsWith(']'));

  if (!looksLikeJson) {
    return value;
  }

  try {
    return JSON.parse(trimmed) as unknown;
  } catch {
    return value;
  }
}

export function normalizeBlueprintPayload(result: unknown): unknown {
  const normalized = normalizeBridgeResult(result);

  if (!isRecord(normalized)) {
    return normalized;
  }

  const out = { ...normalized };

  if (typeof out['payload'] === 'string') {
    out['payload'] = normalizeBridgeResult(out['payload']);
  }

  return out;
}

export function buildBlueprintToolResult(options: BuildToolResultOptions): BlueprintToolResult {
  const content: BlueprintToolContent[] = [];

  if (options.markdown) {
    content.push({
      type: 'text',
      text: options.markdown,
    });
  } else {
    content.push({
      type: 'text',
      text: options.summary ?? 'BlueprintHelper operation completed.',
    });
  }

  for (const link of options.resourceLinks ?? []) {
    content.push({
      type: 'resource_link',
      uri: link.uri,
      name: link.name,
      description: link.description,
      mimeType: link.mimeType,
    });
  }

  const result: BlueprintToolResult = {
    content,
    isError: false,
  };

  if (options.structured) {
    result.structuredContent = options.structured;
  }

  return result;
}

export function resolveResponseMode(
  requested: McpResponseMode | undefined,
  fallback: McpResponseMode,
): McpResponseMode {
  return requested ?? fallback;
}

export function makeBlueprintResourceUri(input: {
  assetPath: string;
  graph?: string;
  view: BlueprintResourceView;
  rev?: string | number;
}) {
  const params = new URLSearchParams();
  params.set('view', input.view);

  if (input.graph) {
    params.set('graph', input.graph);
  }

  if (input.rev !== undefined) {
    params.set('rev', String(input.rev));
  }

  const normalizedAssetPath = input.assetPath.replace(/^\/+/, '');
  return `blueprint://asset/${encodeURIComponent(normalizedAssetPath)}?${params.toString()}`;
}

export function parseBlueprintResourceUri(uri: string) {
  const parsed = new URL(uri);

  if (parsed.protocol !== 'blueprint:') {
    throw new Error('Invalid blueprint resource protocol.');
  }

  if (parsed.hostname !== 'asset') {
    throw new Error('Invalid blueprint resource host.');
  }

  const view = parsed.searchParams.get('view');
  if (!isBlueprintResourceView(view)) {
    throw new Error(`Unsupported blueprint resource view: ${view}`);
  }

  const assetPath = `/${decodeURIComponent(parsed.pathname.replace(/^\/+/, ''))}`;
  if (!isAllowedUnrealAssetPath(assetPath)) {
    throw new Error('Only Unreal asset paths are allowed.');
  }

  return {
    assetPath,
    graph: parsed.searchParams.get('graph') ?? undefined,
    view,
    rev: parsed.searchParams.get('rev') ?? undefined,
  };
}

export function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

export function getStringField(record: Record<string, unknown> | undefined, field: string) {
  const value = record?.[field];
  return typeof value === 'string' ? value : undefined;
}

export function getRecordField(record: Record<string, unknown> | undefined, field: string) {
  const value = record?.[field];
  return isRecord(value) ? value : undefined;
}

/**
 * Extract the payload body from a normalized Bridge result, preferring:
 * 1. `payload` (object-first)
 * 2. `json` (object alias)
 * 3. The result itself as fallback
 */
export function getBlueprintPayloadBody(result: unknown): unknown {
  const normalized = normalizeBlueprintPayload(result);
  if (!isRecord(normalized)) return normalized;
  if (normalized['payload'] !== undefined) return normalized['payload'];
  if (normalized['json'] !== undefined) return normalized['json'];
  return normalized;
}

function isBlueprintResourceView(value: string | null): value is BlueprintResourceView {
  return (
    value === 'logic-md' ||
    value === 'logic-json' ||
    value === 'raw-json' ||
    value === 'diff' ||
    value === 'compile-result'
  );
}

function isAllowedUnrealAssetPath(assetPath: string) {
  if (!(assetPath.startsWith('/Game/') || assetPath.startsWith('/Plugin/'))) {
    return false;
  }

  return !assetPath.includes('..') && !/^[A-Za-z]:/.test(assetPath);
}
