/**
 * MCP Tools 娉ㄥ唽
 *
 * 锟?42 锟?Bridge 鍛戒护鏄犲皠锟?MCP 宸ュ叿锛屽彟鎻愪緵 2 涓湰鍦扮敓鍛藉懆鏈熷伐鍏凤拷?
 * Phase 1-3: 8 涓摑鍥炬搷锟?閫昏緫璇诲彇宸ュ叿
 * Phase 4:   5 涓祫浜ф祻瑙堝伐锟?
 * Phase 5:   9 涓摑鍥剧粨鏋勬搷浣滃伐锟?
 * Phase 6:   6 锟?UMG Widget 鎿嶄綔宸ュ叿
 * Phase 7:   6 锟?DataAsset & DataTable 鎿嶄綔宸ュ叿
 * Phase 8:   6 涓紪杈戝櫒鍛戒护宸ュ叿
 */

import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { BridgeClient, BridgeResponse } from '@blueprinthelper/task-core/bridge/bridge-client';
import {
  McpResponseMode,
  buildBlueprintToolResult,
  getRecordField,
  getStringField,
  isRecord,
  makeBlueprintResourceUri,
  normalizeBlueprintPayload,
  resolveResponseMode,
} from '../result/mcp-response.js';
import {
  normalizeToolResult,
  toMcpResult,
  buildDiagnosticsMarkdown,
  buildDiagnosticsData,
  type ToolResultBase,
  type DiagnosticsMarkdownReport,
} from '../result/tool-result.js';
import type { TaskToolsConfig } from './task-tools.js';
import { registerSharedRegistryTools } from './shared-registry-adapter.js';
import { resolveProjectEngineDir } from '../../project-profile/agent-profile.js';
import { resolveExplicitProjectFile } from '../../project-profile/editor-paths.js';
import { execFile, spawn } from 'node:child_process';
import * as path from 'node:path';
import * as fs from 'node:fs';
import { fileURLToPath } from 'node:url';
import { registerEditorLifecycleTools } from './editor-lifecycle-tools.js';

/** 缂栬緫锟?寮曟搸璺緞閰嶇疆 */
export interface EditorConfig {
  ueEngineDir: string;
  taskCompiler?: TaskToolsConfig['taskCompiler'];
}

const rawJsonInputSchema = z.record(z.unknown())
  .describe('The BlueprintHelper RawJson object (nodes, links, version, schema) to import.');

const LEGACY_TOOL_GUIDANCE =
  'Normal Agents should prefer blueprinthelper_read_agent_guide, blueprinthelper_read_context, blueprinthelper_preview_task, and blueprinthelper_execute_task.';
const FROZEN_TOOL_PREFIX =
  'FROZEN / Expert-only / Normal agents must not call directly.';

const AGENT_GUIDE_INDEX_FILE_NAME = '00_Agent_Onboarding_Index.md';
const AGENT_GUIDE_INDEX_RELATIVE_PATH = path.join(
  'AgentFaceService',
  'agent-guide',
  AGENT_GUIDE_INDEX_FILE_NAME,
);
const AGENT_GUIDE_LOCAL_INDEX_RELATIVE_PATH = path.join(
  'agent-guide',
  AGENT_GUIDE_INDEX_FILE_NAME,
);
const AGENT_GUIDE_MODULE_INDEX_PATH = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  '..',
  '..',
  '..',
  '..',
  'agent-guide',
  AGENT_GUIDE_INDEX_FILE_NAME,
);

function legacyDebugExpertDescription(description: string): string {
  return `${FROZEN_TOOL_PREFIX} [Legacy/internal/debug/expert] ${description} ${LEGACY_TOOL_GUIDANCE}`;
}

function legacyWriteExpertDescription(description: string): string {
  return `${FROZEN_TOOL_PREFIX} [Legacy/internal/debug/expert write] ${description} ${LEGACY_TOOL_GUIDANCE}`;
}

function preflightOnlyDescription(description: string): string {
  return `Preflight only. ${description} Use this only to start the explicit target editor before TaskSpec-first work. ${LEGACY_TOOL_GUIDANCE}`;
}

type RegisterToolConfig = { description?: string };
type RegisterToolHandler = (...args: unknown[]) => unknown;
type RegisterToolWrapper = (
  name: string,
  toolConfig: RegisterToolConfig,
  handler: RegisterToolHandler,
) => unknown;

function unregisterFrozenToolRegistrations(server: McpServer): void {
  const mutableServer = server as unknown as { registerTool: RegisterToolWrapper };
  const registerTool = mutableServer.registerTool.bind(server);

  mutableServer.registerTool = (name, toolConfig, handler) => {
    if (toolConfig.description?.startsWith(FROZEN_TOOL_PREFIX)) {
      return undefined;
    }

    return registerTool(name, toolConfig, handler);
  };
}

/** 锟?Bridge 鍝嶅簲杞崲锟?MCP tool result */
function toToolResult(resp: BridgeResponse, isError = false) {
  return {
    content: [{ type: 'text' as const, text: JSON.stringify(resp, null, 2) }],
    isError: isError || !resp.success,
  };
}

const responseModeSchema = z.enum([
  'summary_text',
  'structured_json',
  'resource_ref',
]).optional();

const debugCaseInputSchema = z.object({
  debug_case_id: z.string().min(1).describe('DebugCase id returned in ToolResultBase.debug_case_ids[]'),
});

const readContextInputSchema = z.object({
  schema: z.literal('BlueprintHelper.ReadSpec.v1'),
  read_type: z.enum([
    'asset_context',
    'blueprint_logic',
    'component_context',
    'variable_context',
    'graph_context',
    'widget_context',
    'data_table_context',
    'object_property_context',
  ]),
  target: z.object({
    asset_path: z.string(),
    asset_type: z.string().optional(),
    target_type: z.enum([
      'asset',
      'blueprint',
      'graph',
      'function',
      'event',
      'custom_event',
      'component',
      'member_variable',
      'event_dispatcher',
      'widget',
      'data_table_row',
      'block',
    ]).optional().default('blueprint'),
    target_name: z.string().optional(),
    block_id: z.string().optional(),
  }),
  view: z.object({
    format: z.enum(['logic_md', 'logic_json', 'summary', 'schema']).optional().default('logic_md'),
    max_items: z.number().int().positive().optional(),
    detail: z.enum(['brief', 'normal', 'full', 'debug']).optional(),
  }).optional().default({ format: 'logic_md' }),
  context: z.object({
    context_id: z.string().optional(),
    task_run_id: z.string().optional(),
  }).optional(),
});

const BlueprintLogicMdOutputSchema = z.object({
  format: z.literal('logic_md'),
  schema: z.literal('BlueprintHelper.LogicMd.v1'),
  assetPath: z.string(),
  graph: z.string().optional(),
  importable: z.boolean(),
  stats: z.record(z.unknown()).optional(),
  diagnostics: z.array(z.record(z.unknown())).optional(),
});

const BlueprintLogicJsonOutputSchema = z.object({
  format: z.literal('logic_json'),
  schema: z.literal('BlueprintHelper.LogicJson.v1'),
  assetPath: z.string(),
  graph: z.string().optional(),
  importable: z.boolean(),
  logic: z.unknown(),
  stats: z.record(z.unknown()).optional(),
  diagnostics: z.array(z.record(z.unknown())).optional(),
});

const BlueprintRawJsonRefOutputSchema = z.object({
  format: z.literal('raw_json_ref'),
  schema: z.literal('BlueprintHelper.RawJsonRef.v1'),
  assetPath: z.string(),
  graph: z.string().optional(),
  importable: z.boolean(),
  rawUri: z.string(),
  stats: z.record(z.unknown()).optional(),
});

const BlueprintRawJsonExportOutputSchema = BlueprintRawJsonRefOutputSchema
  .partial()
  .passthrough();

function makeLogicMdStructured(
  payload: Record<string, unknown>,
  targetBlueprint?: string,
  targetGraph?: string,
) {
  const assetPath = getStringField(payload, 'assetPath') ?? targetBlueprint;
  if (!assetPath) {
    return undefined;
  }

  return omitUndefined({
    format: 'logic_md',
    schema: 'BlueprintHelper.LogicMd.v1',
    assetPath,
    graph: getStringField(payload, 'graph') ?? targetGraph,
    importable: false,
    stats: getRecordField(payload, 'stats'),
    diagnostics: Array.isArray(payload['diagnostics']) ? payload['diagnostics'] : undefined,
  });
}

function makeLogicJsonStructured(
  payload: Record<string, unknown>,
  targetBlueprint?: string,
  targetGraph?: string,
) {
  const assetPath = getStringField(payload, 'assetPath') ?? targetBlueprint;
  if (!assetPath || payload['logic'] === undefined) {
    return undefined;
  }

  return omitUndefined({
    format: 'logic_json',
    schema: 'BlueprintHelper.LogicJson.v1',
    assetPath,
    graph: getStringField(payload, 'graph') ?? targetGraph,
    importable: false,
    logic: payload['logic'],
    stats: getRecordField(payload, 'stats'),
    diagnostics: Array.isArray(payload['diagnostics']) ? payload['diagnostics'] : undefined,
  });
}

function getRawJsonPayload(normalizedPayload: unknown) {
  if (!isRecord(normalizedPayload)) return normalizedPayload;
  // payload 鏄富瑕佸瓧娈碉紙object-first锛夛紝json 鏄吋瀹瑰洖閫€
  if (normalizedPayload['payload'] !== undefined) return normalizedPayload['payload'];
  if (normalizedPayload['json'] !== undefined) return normalizedPayload['json'];
  return normalizedPayload;
}

function omitUndefined(input: Record<string, unknown>) {
  return Object.fromEntries(
    Object.entries(input).filter(([, value]) => value !== undefined),
  );
}

function toMarkdownToolResult(
  resp: BridgeResponse,
  markdown: string,
  structured?: Record<string, unknown>,
  mode: McpResponseMode = 'summary_text',
) {
  return buildBlueprintToolResult({
    mode,
    markdown,
    structured,
  });
}

/** 锟?Bridge 閿欒杞崲锟?MCP tool error result */
function toErrorResult(err: unknown) {
  const message = err instanceof Error ? err.message : String(err);
  return {
    content: [{ type: 'text' as const, text: `Bridge error: ${message}` }],
    isError: true,
  };
}

function resolveAgentGuideIndexPath(): string {
  const cwd = process.cwd();
  const candidates = [
    path.resolve(cwd, AGENT_GUIDE_INDEX_RELATIVE_PATH),
    path.resolve(cwd, AGENT_GUIDE_LOCAL_INDEX_RELATIVE_PATH),
    path.resolve(cwd, '..', AGENT_GUIDE_INDEX_RELATIVE_PATH),
    path.resolve(cwd, '..', AGENT_GUIDE_LOCAL_INDEX_RELATIVE_PATH),
    path.resolve(cwd, '..', '..', AGENT_GUIDE_INDEX_RELATIVE_PATH),
  ];

  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) {
      return candidate;
    }
  }

  if (fs.existsSync(AGENT_GUIDE_MODULE_INDEX_PATH)) {
    return AGENT_GUIDE_MODULE_INDEX_PATH;
  }

  throw new Error(`Unable to find ${AGENT_GUIDE_INDEX_RELATIVE_PATH} from ${cwd}`);
}

function readAgentGuideIndexMarkdown(): string {
  return fs.readFileSync(resolveAgentGuideIndexPath(), 'utf8');
}

function makeReadTraceId(): string {
  return `trace_read_context_${Date.now()}`;
}

function stripBlueprintHelperPrefix(schema: unknown): unknown {
  if (typeof schema !== 'string') return schema;
  return schema.startsWith('BlueprintHelper.') ? schema.slice('BlueprintHelper.'.length) : schema;
}

function normalizeReadPayloadSchema(payload: Record<string, unknown>): Record<string, unknown> {
  return {
    ...payload,
    schema: stripBlueprintHelperPrefix(payload['schema']),
  };
}

function makeLogicSummaryPayload(
  payload: Record<string, unknown>,
  target: z.infer<typeof readContextInputSchema>['target'],
): Record<string, unknown> {
  const logic = getRecordField(payload, 'logic');
  const stats = getRecordField(payload, 'stats') ?? {};
  const entry = getRecordField(logic, 'entry');
  const nodes = Array.isArray(logic?.['nodes']) ? logic['nodes'] : undefined;
  const targetType = target.target_type ?? 'blueprint';
  const isWholeGraphTarget = ['asset', 'blueprint', 'graph'].includes(targetType);
  const groups = Array.isArray(logic?.['groups'])
    ? logic['groups'].filter(isRecord).map((group) => omitUndefined({
      group_type: getStringField(group, 'group_type'),
      block_id: getStringField(group, 'block_id'),
      group_entry_node_path: getStringField(group, 'group_entry_node_path'),
      name: getStringField(group, 'name'),
      entry: getRecordField(group, 'entry'),
      nodes: Array.isArray(group['nodes']) ? group['nodes'].length : undefined,
    }))
    : undefined;
  const targetFound = Boolean(entry || (groups && groups.length > 0) || (isWholeGraphTarget && nodes && nodes.length > 0));

  return omitUndefined({
    schema: 'LogicSummary.v1',
    asset_path: getStringField(logic, 'asset_path') ?? target.asset_path,
    graph: getStringField(logic, 'graph'),
    target_type: target.target_type,
    target_name: target.target_name,
    block_id: target.block_id,
    scope: getStringField(payload, 'scope'),
    target_found: targetFound,
    stats,
    entry,
    groups,
    node_count: typeof stats['nodes'] === 'number' ? stats['nodes'] : nodes?.length,
    group_count: groups?.length,
  });
}

function isTargetEntryLogicRead(target: z.infer<typeof readContextInputSchema>['target']) {
  return target.target_type === 'function'
    || target.target_type === 'event'
    || target.target_type === 'custom_event';
}

function getRecordArray(record: Record<string, unknown> | undefined, field: string): Record<string, unknown>[] {
  const value = record?.[field];
  return Array.isArray(value) ? value.filter(isRecord) : [];
}

function collectLogicNodes(logic: Record<string, unknown> | undefined): Record<string, unknown>[] {
  const nodes = getRecordArray(logic, 'nodes');
  if (nodes.length > 0) {
    return nodes;
  }

  const groups = getRecordArray(logic, 'groups');
  return groups.flatMap((group) => getRecordArray(group, 'nodes'));
}

function countLogicLinks(nodes: Record<string, unknown>[], type: string): number {
  return nodes.reduce((count, node) => count + getRecordArray(node, 'links')
    .filter((link) => getStringField(link, 'type') === type).length, 0);
}

function countLogicOrphans(nodes: Record<string, unknown>[], entryNodeRef?: string): number {
  const linkedNodeRefs = new Set<string>();
  for (const node of nodes) {
    const nodeRef = getStringField(node, 'node_ref');
    if (nodeRef) {
      for (const link of getRecordArray(node, 'links')) {
        linkedNodeRefs.add(nodeRef);
        const toNode = getStringField(link, 'to_node');
        if (toNode) {
          linkedNodeRefs.add(toNode);
        }
      }
    }
  }

  return nodes.filter((node) => {
    const nodeRef = getStringField(node, 'node_ref');
    return nodeRef && nodeRef !== entryNodeRef && !linkedNodeRefs.has(nodeRef);
  }).length;
}

function formatLogicNodeLine(node: Record<string, unknown>): string {
  const nodeRef = getStringField(node, 'node_ref') ?? '<node>';
  const kind = getStringField(node, 'kind') ?? 'unknown';
  const name = getStringField(node, 'name') ?? 'Unnamed';
  const owner = getStringField(node, 'owner');
  return owner
    ? `- ${nodeRef} [${kind}] ${name} owner=${owner}`
    : `- ${nodeRef} [${kind}] ${name}`;
}

function makeLogicMdPayloadFromLogicJson(
  payload: Record<string, unknown>,
  target: z.infer<typeof readContextInputSchema>['target'],
): Record<string, unknown> {
  const logic = getRecordField(payload, 'logic');
  const entry = getRecordField(logic, 'entry');
  const nodes = collectLogicNodes(logic);
  const entryNodeRef = getStringField(entry, 'node_ref');
  const execLinks = countLogicLinks(nodes, 'exec');
  const dataLinks = countLogicLinks(nodes, 'data');
  const orphanNodes = countLogicOrphans(nodes, entryNodeRef);
  const targetType = target.target_type ?? 'blueprint';
  const titleByType: Record<string, string> = {
    function: 'Function',
    event: 'Event',
    custom_event: 'Custom Event',
  };
  const title = titleByType[targetType] ?? 'Target';
  const targetName = target.target_name
    ?? getStringField(logic, 'function')
    ?? getStringField(logic, 'event')
    ?? getStringField(entry, 'name')
    ?? '<unnamed>';
  const graph = getStringField(logic, 'graph');

  const lines = [
    '# Logic Graph',
    '',
    `${title}: ${targetName}`,
  ];
  if (graph) {
    lines.push(`Graph: ${graph}`);
  }
  lines.push(`Nodes: ${nodes.length} | Exec Links: ${execLinks} | Data Links: ${dataLinks} | Orphans: ${orphanNodes}`);
  lines.push('');
  lines.push('## Entry');
  lines.push(entry ? formatLogicNodeLine(entry) : '- <missing>');
  lines.push('');
  lines.push('## Nodes');
  if (nodes.length === 0) {
    lines.push('- None');
  } else {
    lines.push(...nodes.map(formatLogicNodeLine));
  }

  const allLinks = nodes.flatMap((node) => getRecordArray(node, 'links').map((link) => ({
    source: getStringField(node, 'node_ref') ?? '<node>',
    link,
  })));
  lines.push('');
  lines.push('## Execution');
  const executionLinks = allLinks.filter(({ link }) => getStringField(link, 'type') === 'exec');
  if (executionLinks.length === 0) {
    lines.push('- None');
  } else {
    for (const { source, link } of executionLinks) {
      lines.push(`- ${source}.${getStringField(link, 'from_pin') ?? '<pin>'} -> ${getStringField(link, 'to_node') ?? '<node>'}.${getStringField(link, 'to_pin') ?? '<pin>'}`);
    }
  }

  const dataDependencyLinks = allLinks.filter(({ link }) => getStringField(link, 'type') === 'data');
  if (dataDependencyLinks.length > 0) {
    lines.push('');
    lines.push('## Data Dependencies');
    for (const { source, link } of dataDependencyLinks) {
      lines.push(`- ${source}.${getStringField(link, 'from_pin') ?? '<pin>'} -> ${getStringField(link, 'to_node') ?? '<node>'}.${getStringField(link, 'to_pin') ?? '<pin>'}`);
    }
  }

  return {
    schema: 'LogicMd.v1',
    format: 'logic_md',
    importable: false,
    scope: getStringField(payload, 'scope') ?? inferBlueprintLogicScope(targetType),
    markdown: lines.join('\n'),
    stats: {
      nodes: nodes.length,
      exec_links: execLinks,
      data_links: dataLinks,
      orphan_nodes: orphanNodes,
    },
  };
}

function applyMaxItemsToLogicPayload(
  payload: Record<string, unknown>,
  maxItems?: number,
): { payload: Record<string, unknown>; truncated: boolean } {
  if (!maxItems) {
    return { payload, truncated: false };
  }

  const logic = getRecordField(payload, 'logic');
  if (!logic) {
    return { payload, truncated: false };
  }

  let remaining = maxItems;
  let nodesTotal = 0;
  let nodesReturned = 0;
  let truncated = false;
  const logicCopy: Record<string, unknown> = { ...logic };

  const topLevelNodes = logic['nodes'];
  if (Array.isArray(topLevelNodes)) {
    nodesTotal += topLevelNodes.length;
    const keptNodes = topLevelNodes.slice(0, Math.max(remaining, 0));
    logicCopy['nodes'] = keptNodes;
    nodesReturned += keptNodes.length;
    remaining -= keptNodes.length;
    truncated ||= keptNodes.length < topLevelNodes.length;
  }

  const groups = logic['groups'];
  if (Array.isArray(groups)) {
    logicCopy['groups'] = groups.map((group) => {
      if (!isRecord(group) || !Array.isArray(group['nodes'])) {
        return group;
      }

      const groupNodes = group['nodes'];
      nodesTotal += groupNodes.length;
      const keptNodes = groupNodes.slice(0, Math.max(remaining, 0));
      nodesReturned += keptNodes.length;
      remaining -= keptNodes.length;
      truncated ||= keptNodes.length < groupNodes.length;
      return {
        ...group,
        nodes: keptNodes,
        nodes_total: groupNodes.length,
        nodes_returned: keptNodes.length,
      };
    });
  }

  if (!truncated) {
    return { payload, truncated: false };
  }

  return {
    payload: {
      ...payload,
      logic: logicCopy,
      truncation: {
        nodes_total: nodesTotal,
        nodes_returned: nodesReturned,
      },
    },
    truncated: true,
  };
}

function prepareReadPayloadForView(
  payload: Record<string, unknown>,
  target: z.infer<typeof readContextInputSchema>['target'],
  format: string,
  maxItems?: number,
): { payload: Record<string, unknown>; truncated: boolean } {
  if (format === 'summary') {
    return {
      payload: makeLogicSummaryPayload(payload, target),
      truncated: false,
    };
  }

  if (format === 'logic_md' && isTargetEntryLogicRead(target)) {
    return {
      payload: makeLogicMdPayloadFromLogicJson(payload, target),
      truncated: false,
    };
  }

  if (format === 'logic_json') {
    return applyMaxItemsToLogicPayload(payload, maxItems);
  }

  return { payload, truncated: false };
}

function buildReadContextTarget(target: z.infer<typeof readContextInputSchema>['target']) {
  return omitUndefined({
    asset_path: target.asset_path,
    asset_type: target.asset_type,
    target_type: target.target_type,
    target_name: target.target_name,
    block_id: target.block_id,
  });
}

function inferBlueprintLogicScope(targetType: string): string {
  if (targetType === 'blueprint' || targetType === 'asset') return 'blueprint';
  if (targetType === 'function') return 'target_function';
  if (targetType === 'event') return 'target_event';
  if (targetType === 'custom_event') return 'target_custom_event';
  return 'target_graph';
}

function buildBlueprintLogicReadPayload(input: z.infer<typeof readContextInputSchema>) {
  const payload: Record<string, unknown> = {
    asset_path: input.target.asset_path,
  };

  const targetName = input.target.target_name;
  switch (input.target.target_type) {
    case 'graph':
      if (targetName) payload['graph'] = targetName;
      break;
    case 'function':
      if (targetName) payload['function'] = targetName;
      break;
    case 'event':
    case 'custom_event':
      if (targetName) payload['event'] = targetName;
      break;
    case 'block':
      if (input.target.block_id ?? targetName) payload['block_id'] = input.target.block_id ?? targetName;
      break;
  }

  payload['scope'] = inferBlueprintLogicScope(input.target.target_type);
  return payload;
}

function makeBlueprintLogicSchemaPayload() {
  return {
    schema: 'BlueprintLogicReadSchema.v1',
    read_type: 'blueprint_logic',
    target_types: ['blueprint', 'graph', 'function', 'event', 'custom_event', 'block'],
    formats: ['logic_md', 'logic_json', 'summary', 'schema'],
    target_name_semantics: {
      graph: 'Graph name',
      function: 'Function name',
      event: 'Event name',
      custom_event: 'Custom Event name',
    },
  };
}

function toReadContextMcpResult(input: {
  target: z.infer<typeof readContextInputSchema>['target'];
  readType: string;
  format: string;
  payload: Record<string, unknown>;
  maxItems?: number;
  status?: string;
  ok?: boolean;
}) {
  const ok = input.ok ?? true;
  const normalizedPayload = normalizeReadPayloadSchema(input.payload);
  const prepared = prepareReadPayloadForView(
    normalizedPayload,
    input.target,
    input.format,
    input.maxItems,
  );
  const payload = prepared.payload;
  const scope = typeof payload['scope'] === 'string'
    ? payload['scope']
    : inferBlueprintLogicScope(input.target.target_type);
  const structured = {
    ok,
    schema: 'BlueprintHelper.McpToolResult.v1',
    operation: 'read_context',
    trace_id: makeReadTraceId(),
    status: input.status ?? (ok ? 'completed' : 'failed'),
    modified: false,
    target: buildReadContextTarget(input.target),
    data: {
      schema: 'ReadContextPack.v1',
      read_type: input.readType,
      format: input.format,
      scope,
      payload,
      stats: isRecord(payload['stats']) ? payload['stats'] : {},
      truncated: prepared.truncated,
    },
  };

  return {
    content: [
      {
        type: 'text' as const,
        text: `read_context ${structured.status}: ${input.target.asset_path}, read_type=${input.readType}, format=${input.format}.`,
      },
    ],
    isError: !ok,
    structuredContent: structured,
  };
}

function extractReadPayloadFromBridgeResult(result: unknown): {
  ok: boolean;
  status?: string;
  payload: Record<string, unknown>;
} {
  const normalized = normalizeBlueprintPayload(result);
  const raw = isRecord(normalized) ? normalized : {};
  const ok = typeof raw['ok'] === 'boolean' ? raw['ok'] : true;
  const data = isRecord(raw['data']) ? raw['data'] : raw;
  return {
    ok,
    status: typeof raw['status'] === 'string' ? raw['status'] : undefined,
    payload: data,
  };
}

export function registerTools(server: McpServer, bridge: BridgeClient, config: EditorConfig): void {
  registerEditorLifecycleTools(server, bridge, config);
  if (isEditorLifecycleOnlyMcpSurface()) {
    return;
  }

  registerSharedRegistryTools(server, bridge, {
    cwd: process.cwd(),
    ueEngineDir: config.ueEngineDir,
    taskCompiler: config.taskCompiler,
    toolNames: new Set([
      'blueprinthelper_read_reference_context',
      'blueprinthelper_preview_task',
      'blueprinthelper_execute_task',
      'blueprinthelper_get_task_result',
    ]),
  });
  unregisterFrozenToolRegistrations(server);

  // 鈹€鈹€鈹€ 1. read_agent_guide 鈹€鈹€鈹€
  server.registerTool(
    'blueprinthelper_read_agent_guide',
    {
      description: 'Read the BlueprintHelper AgentGuide onboarding index Markdown. Use this as the Agent-facing entry for supported capability surface and schema guide paths.',
      inputSchema: z.object({}),
    },
    async () => {
      try {
        return {
          content: [{ type: 'text' as const, text: readAgentGuideIndexMarkdown() }],
        };
      } catch (err) {
        const message = err instanceof Error ? err.message : String(err);
        return {
          content: [{ type: 'text' as const, text: `AgentGuide error: ${message}` }],
          isError: true,
        };
      }
    },
  );

  server.registerTool(
    'blueprinthelper_get_debug_case',
    {
      description: 'Read a summary-only DebugCase by id. Returns DebugCase metadata and event summary only; never returns DebugBundle artifact content, local bundle paths, raw JSON payloads, tokens, or source file content.',
      inputSchema: debugCaseInputSchema,
    },
    async ({ debug_case_id }) => {
      try {
        const resp = await bridge.sendCommand('get_debug_case', { debug_case_id });
        if (isRecord(resp.result) && resp.result['schema'] === 'BlueprintHelper.McpToolResult.v1') {
          return toMcpResult(resp.result as unknown as ToolResultBase);
        }

        return toMcpResult(normalizeToolResult(resp, 'get_debug_case', {
          data: isRecord(resp.result) ? resp.result : { debug_case_id },
          modified: false,
        }));
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  server.registerTool(
    'blueprinthelper_read_context',
    {
      description: 'Read UE asset context through BlueprintHelper.ReadSpec.v1. First slice supports blueprint_logic with logic_md, logic_json, summary, and schema formats.',
      inputSchema: readContextInputSchema,
    },
    async (input) => {
      try {
        if (input.read_type !== 'blueprint_logic') {
          return toMcpResult(normalizeToolResult(
            {
              request_id: 'read_context',
              success: false,
              error_code: 'unsupported_read_type',
              message: `read_context currently supports blueprint_logic only, got ${input.read_type}.`,
            },
            'read_context',
          ));
        }

        const format = input.view?.format ?? 'logic_md';
        if (format === 'schema') {
          return toReadContextMcpResult({
            target: input.target,
            readType: input.read_type,
            format,
            payload: makeBlueprintLogicSchemaPayload(),
          });
        }

        const bridgeFormat = format === 'logic_json' || format === 'summary' || (format === 'logic_md' && isTargetEntryLogicRead(input.target))
          ? 'logic_json'
          : 'logic_md';
        const command = bridgeFormat === 'logic_json'
          ? 'read_blueprint_logic_json'
          : 'read_blueprint_logic_md';
        const payload = buildBlueprintLogicReadPayload(input);

        const resp = await bridge.sendCommand(command, payload);
        if (!resp.success) {
          return toMcpResult(normalizeToolResult(resp, 'read_context', {
            target: buildReadContextTarget(input.target) as never,
            modified: false,
          }));
        }

        const readPayload = extractReadPayloadFromBridgeResult(resp.result);
        return toReadContextMcpResult({
          target: input.target,
          readType: input.read_type,
          format,
          payload: readPayload.payload,
          maxItems: input.view?.max_items,
          status: readPayload.status,
          ok: readPayload.ok,
        });
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 2. get_editor_context 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_get_editor_context',
    {
      description: legacyDebugExpertDescription('Get the current Unreal Editor context: active blueprint, graph, node count, compile status.'),
      inputSchema: z.object({}),
    },
    async () => {
      try {
        const resp = await bridge.sendCommand('get_editor_context');
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 2.5. get_runtime_profile 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_get_runtime_profile',
    {
      description:
        'Get the current BlueprintHelper runtime profile: version, Bridge status, config status, write permission, risk command status, security profile, and unavailable capabilities.',
      inputSchema: z.object({}),
    },
    async () => {
      try {
        const resp = await bridge.sendCommand('get_runtime_profile');
        if (!resp.success) {
          return toToolResult(resp);
        }

        // 灏濊瘯瑙ｆ瀽 UE 渚у凡搴忓垪鍖栫殑 ToolResultBase
        const raw = resp.result as Record<string, unknown> | undefined;
        if (raw && typeof raw['ok'] === 'boolean') {
          const data = raw['data'] as Record<string, unknown> | undefined;
          const version = typeof data?.['version'] === 'string' ? data['version'] : 'unknown';
          // UE 渚у凡缁忚繑鍥炰簡 ToolResultBase JSON锛岀洿鎺ヤ娇锟?
          return {
            content: [{ type: 'text' as const, text: `get_runtime_profile ${raw['status'] ?? 'completed'}: version=${version}, modified=${raw['modified'] ?? false}.` }],
            isError: !raw['ok'],
            structuredContent: raw as Record<string, unknown>,
          };
        }

        // 鍥為€€锛氱敤 normalizeToolResult 鏍囧噯锟?
        const result = normalizeToolResult(resp, 'get_runtime_profile');
        return toMcpResult(result);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 2.6. diagnostics (Static) 鈹€鈹€鈹€
  server.registerTool(
    'blueprinthelper_request_write_session',
    {
      description:
        'Request a short-lived BlueprintHelper write session from the running Unreal Editor. The Editor must ask the user to approve the session; the approval applies to the running Editor/Bridge for the approved scope and lifetime, and the raw session id is not returned to the Agent.',
      inputSchema: z.object({
        reason: z.string().min(1).describe('Human-readable reason shown in the Editor approval prompt.'),
        scope: z.enum(['project', 'asset_list']).optional().describe('Requested write scope. Default is project.'),
        ttl_seconds: z.number().int().positive().max(3600).optional().describe('Requested session lifetime in seconds. Default is 900.'),
        asset_paths: z.array(z.string()).optional().describe('Optional explicit asset paths for asset_list scope.'),
      }),
    },
    async ({ reason, scope, ttl_seconds, asset_paths }) => {
      try {
        const payload: Record<string, unknown> = { reason };
        if (scope) payload['scope'] = scope;
        if (ttl_seconds !== undefined) payload['ttl_seconds'] = ttl_seconds;
        if (asset_paths) payload['asset_paths'] = asset_paths;

        const resp = await bridge.sendCommand('request_write_session', payload);
        if (!resp.success) {
          return toToolResult(resp);
        }

        const writeSession = isRecord(resp.result?.['write_session'])
          ? resp.result['write_session']
          : undefined;
        const sessionId = typeof writeSession?.['session_id'] === 'string'
          ? writeSession['session_id']
          : '';

        if (!sessionId) {
          return toErrorResult(new Error('Bridge approved a write session but did not return write_session.session_id.'));
        }

        bridge.setWriteSessionId(sessionId);

        const sanitizedSession: Record<string, unknown> = {
          scope: writeSession?.['scope'] ?? scope ?? 'project',
          expires_at_utc: writeSession?.['expires_at_utc'],
        };
        if (Array.isArray(writeSession?.['asset_paths'])) {
          sanitizedSession['asset_paths'] = writeSession['asset_paths'];
        }

        return {
          content: [
            {
              type: 'text' as const,
              text: `write session approved: scope=${sanitizedSession['scope']}, expires_at_utc=${sanitizedSession['expires_at_utc'] ?? 'unknown'}.`,
            },
          ],
          isError: false,
          structuredContent: {
            ok: true,
            schema: 'BlueprintHelper.McpToolResult.v1',
            operation: 'request_write_session',
            status: 'completed',
            modified: false,
            data: {
              schema: 'WriteSession.v1',
              write_session: sanitizedSession,
            },
          },
        };
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  server.registerTool(
    'blueprinthelper_diagnostics',
    {
      description:
        'Run static diagnostics without requiring the UE Editor. Checks .blueprinthelper/agent-profile.json, CLAUDE.md managed block, Skill entry, project structure, and install/configuration state. Returns a Markdown diagnostic report.',
      inputSchema: z.object({}),
    },
    async () => {
      try {
        const report: DiagnosticsMarkdownReport = {
          blocking: [],
          warnings: [],
          info: [],
        };

        // Static checks.
        const projectDir = process.cwd();
        const agentProfilePath = path.join(
          projectDir,
          '.blueprinthelper',
          'agent-profile.json',
        );
        if (fs.existsSync(agentProfilePath)) {
          try {
            const agentProfile = JSON.parse(fs.readFileSync(agentProfilePath, 'utf8'));
            const environment = isRecord(agentProfile)
              ? getRecordField(agentProfile, 'environment')
              : undefined;
            const engineDir =
              getStringField(environment, 'ue_engine_dir') ??
              getStringField(environment, 'UE_ENGINE_DIR');

            if (engineDir?.trim()) {
              report.info.push({ code: 'agent_profile.valid' });
              report.info.push({ code: 'agent_profile.ue_engine_dir.present' });
            } else {
              report.blocking.push({
                code: 'agent_profile.ue_engine_dir.missing',
                extra: 'reason: .blueprinthelper/agent-profile.json must define environment.ue_engine_dir',
              });
            }
          } catch (err) {
            report.blocking.push({
              code: 'agent_profile.invalid_json',
              extra: `reason: ${err instanceof Error ? err.message : String(err)}`,
            });
          }
        } else {
          report.blocking.push({
            code: 'agent_profile.unavailable',
            extra: 'reason: .blueprinthelper/agent-profile.json not found',
          });
        }

        // 妫€锟?CLAUDE.md锛圙lobal guidance锟?
        const globalClaudePath = path.join(
          process.env['USERPROFILE'] ?? process.env['HOME'] ?? '',
          '.claude',
          'CLAUDE.md',
        );
        if (fs.existsSync(globalClaudePath)) {
          report.info.push({ code: 'global_guidance.present' });
        } else {
          report.blocking.push({ code: 'global_guidance.missing' });
        }

        // 妫€锟?Skill 鍏ュ彛
        const skillPath = path.join(
          process.env['USERPROFILE'] ?? process.env['HOME'] ?? '',
          '.claude',
          'skills',
          'blueprinthelper',
          'SKILL.md',
        );
        if (fs.existsSync(skillPath)) {
          report.info.push({ code: 'skill_entry.valid' });
        } else {
          report.warnings.push({ code: 'skill_entry.invalid' });
        }

        // 妫€鏌ラ」鐩洰褰曠粨鏋勶紙鏄惁锟?UE 椤圭洰锟?
        const uprojectFiles = fs
          .readdirSync(projectDir)
          .filter((f) => f.endsWith('.uproject'));
        if (uprojectFiles.length > 0) {
          report.info.push({ code: 'project_structure.valid' });
          // 妫€锟?Project Marker
          const projectClaudePath = path.join(projectDir, '.claude', 'CLAUDE.md');
          const projectAgentsPath = path.join(projectDir, 'AGENTS.md');
          if (
            fs.existsSync(projectClaudePath) ||
            fs.existsSync(projectAgentsPath) ||
            fs.existsSync(agentProfilePath)
          ) {
            report.info.push({ code: 'project_marker.present' });
          } else {
            report.warnings.push({ code: 'project_marker.missing' });
          }
        } else {
          report.warnings.push({
            code: 'project_structure.invalid',
            extra: 'reason: no .uproject found in current directory',
          });
        }

        // 妫€鏌ョ増锟?
        const packageJsonPath = path.join(
          process.cwd(),
          'plugins',
          'BlueprintHelper_MCP_Server',
          'package.json',
        );
        let mcpVersion = 'unknown';
        if (fs.existsSync(packageJsonPath)) {
          try {
            const pkg = JSON.parse(fs.readFileSync(packageJsonPath, 'utf-8'));
            mcpVersion = pkg.version ?? 'unknown';
            report.info.push({ code: 'version.match' });
          } catch {
            report.warnings.push({ code: 'version.invalid' });
          }
        } else {
          report.warnings.push({ code: 'version.invalid' });
        }

        const markdown = buildDiagnosticsMarkdown(report);
        const data = buildDiagnosticsData('static', markdown);

        return {
          content: [
            {
              type: 'text' as const,
              text: `diagnostics static: blocking=${report.blocking.length}, warnings=${report.warnings.length}, info=${report.info.length}.`,
            },
          ],
          isError: false,
          structuredContent: {
            ok: true,
            schema: 'BlueprintHelper.McpToolResult.v1',
            operation: 'diagnostics',
            trace_id: `trace_diag_static_${Date.now()}`,
            status: 'completed',
            modified: false,
            data,
          },
        };
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 2.7. diagnostics_runtime 鈹€鈹€鈹€
  server.registerTool(
    'blueprinthelper_diagnostics_runtime',
    {
      description:
        'Run BlueprintHelper runtime diagnostics when the UE Editor Bridge is reachable. Checks editor status, Bridge connectivity, runtime profile, write permission, risk-command state, and project markers. Read-only Agent-facing diagnostic tool.',
      inputSchema: z.object({}),
    },
    async () => {
      try {
        const resp = await bridge.sendCommand('diagnostics_runtime');
        if (!resp.success) {
          return toToolResult(resp);
        }

        // 灏濊瘯瑙ｆ瀽 UE 渚у凡搴忓垪鍖栫殑 ToolResultBase
        const raw = resp.result as Record<string, unknown> | undefined;
        if (raw && typeof raw['ok'] === 'boolean') {
          const data = raw['data'] as Record<string, unknown> | undefined;
          const mode = data?.['mode'] ?? 'runtime';
          return {
            content: [
              {
                type: 'text' as const,
                text: `diagnostics ${mode}: ok=${raw['ok']}, status=${raw['status'] ?? 'completed'}.`,
              },
            ],
            isError: !raw['ok'],
            structuredContent: raw as Record<string, unknown>,
          };
        }

        // 鍥為€€锛氱敤 normalizeToolResult 鏍囧噯锟?
        const result = normalizeToolResult(resp, 'diagnostics_runtime');
        return toMcpResult(result);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 2.8. get_logic_md 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_get_logic_md',
    {
      description:
        legacyDebugExpertDescription('Read blueprint logic as Markdown. Legacy direct read tool.'),
      inputSchema: z.object({
        asset_path: z.string()
          .describe('Blueprint asset path, e.g. /Game/BP/BP_Player.BP_Player'),
        graph: z.string().optional()
          .describe('Target graph name'),
        function: z.string().optional()
          .describe('Target function name'),
        event: z.string().optional()
          .describe('Target event name'),
        block_id: z.string().optional()
          .describe('BlueprintHelper-owned block id'),
        scope: z.enum(['blueprint', 'target_graph', 'target_function', 'target_event']).optional()
          .describe('Optional explicit read scope'),
      }),
    },
    async ({ asset_path, graph, function: func, event, block_id, scope }) => {
      try {
        const payload: Record<string, unknown> = { asset_path };
        if (graph) payload['graph'] = graph;
        if (func) payload['function'] = func;
        if (event) payload['event'] = event;
        if (block_id) payload['block_id'] = block_id;
        if (scope) payload['scope'] = scope;

        const resp = await bridge.sendCommand('read_blueprint_logic_md', payload);
        if (!resp.success) {
          return toToolResult(resp);
        }

        // 灏濊瘯瑙ｆ瀽 UE 渚у凡搴忓垪鍖栫殑 ToolResultBase
        const raw = resp.result as Record<string, unknown> | undefined;
        if (raw && typeof raw['ok'] === 'boolean') {
          const data = raw['data'] as Record<string, unknown> | undefined;
          const targetInfo = raw['target'] as Record<string, unknown> | undefined;
          const assetInfo = targetInfo?.['asset_path'] ?? asset_path;
          const graphInfo = targetInfo?.['graph'] ?? graph ?? '';
          return {
            content: [
              {
                type: 'text' as const,
                text: `read_blueprint_logic_md ${raw['status'] ?? 'completed'}: ${assetInfo}${graphInfo ? '.' + graphInfo : ''}, scope=${data?.['scope'] ?? 'target_graph'}.`,
              },
            ],
            isError: !raw['ok'],
            structuredContent: raw as Record<string, unknown>,
          };
        }

        // 鍥為€€
        const result = normalizeToolResult(resp, 'read_blueprint_logic_md_by_target');
        return toMcpResult(result);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 2.10. create_asset 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_create_asset',
    {
      description:
        legacyWriteExpertDescription('Create a UE asset such as a Blueprint class, input asset, DataAsset, or DataTable.'),
      inputSchema: z.object({
        asset_path: z.string()
          .describe('Asset path'),
        asset_type: z.enum([
          'blueprint_class',
          'blueprint_interface',
          'structure',
          'input_action',
          'input_mapping_context',
          'data_asset',
        ]).describe('Asset type to create'),
        parent_class: z.string().optional()
          .describe('Parent class name for blueprint_class assets'),
        value_type: z.string().optional()
          .describe('Input Action value type'),
        collision: z.enum(['fail_if_exists', 'reuse_if_exists']).optional()
          .describe('Asset collision policy'),
      }),
    },
    async ({ asset_path, asset_type, parent_class, value_type, collision }) => {
      try {
        const payload: Record<string, unknown> = { asset_path, asset_type };
        if (parent_class) payload['parent_class'] = parent_class;
        if (value_type) payload['value_type'] = value_type;
        if (collision) payload['collision'] = collision;

        const resp = await bridge.sendCommand('create_asset', payload);

        // 灏濊瘯瑙ｆ瀽 UE 渚у凡搴忓垪鍖栫殑 ToolResultBase
        const raw = resp.result as Record<string, unknown> | undefined;
        if (raw && typeof raw['ok'] === 'boolean') {
          const targetInfo = raw['target'] as Record<string, unknown> | undefined;
          const data = raw['data'] as Record<string, unknown> | undefined;
          const asset = data?.['asset'] as Record<string, unknown> | undefined;
          const created = asset?.['created'] ?? false;
          const existed = asset?.['already_existed'] ?? false;
          return {
            content: [
              {
                type: 'text' as const,
                text: `create_asset ${raw['status'] ?? 'applied'}: ${targetInfo?.['asset_path'] ?? asset_path}, type=${asset_type}, created=${created}, already_existed=${existed}.`,
              },
            ],
            isError: !raw['ok'],
            structuredContent: raw as Record<string, unknown>,
          };
        }

        if (!resp.success) {
          return toToolResult(resp);
        }

        const result = normalizeToolResult(resp, 'create_asset');
        return toMcpResult(result);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 2.11. read_components 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_read_components',
    {
      description: legacyDebugExpertDescription('Read the Blueprint component tree.'),
      inputSchema: z.object({
        asset_path: z.string().describe('Blueprint asset path'),
      }),
    },
    async ({ asset_path }) => {
      try {
        const resp = await bridge.sendCommand('read_components', { asset_path });
        const raw = resp.result as Record<string, unknown> | undefined;
        if (raw && typeof raw['ok'] === 'boolean') {
          return { content: [{ type: 'text' as const, text: `read_components ${raw['status'] ?? 'completed'}: ${asset_path}, components=${((raw['data'] as Record<string, unknown>|undefined)?.['stats'] as Record<string, unknown>|undefined)?.['components'] ?? '?'}.` }], isError: !raw['ok'], structuredContent: raw as Record<string, unknown> };
        }
        if (!resp.success) return toToolResult(resp);
        const result = normalizeToolResult(resp, 'read_components');
        return toMcpResult(result);
      } catch (err) { return toErrorResult(err); }
    },
  );

  // 鈹€鈹€鈹€ 2.12. add_component 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_add_component',
    {
      description: legacyWriteExpertDescription('Add a component to a Blueprint.'),
      inputSchema: z.object({
        asset_path: z.string().describe('Blueprint asset path'),
        component_name: z.string().describe('Component name'),
        component_class: z.string().describe('Component class'),
        parent_component: z.string().optional().describe('Parent component name'),
        socket_name: z.string().optional().describe('Attach socket name'),
        attach_rule: z.enum(['keep_relative', 'snap_to_target']).optional().describe('Attach rule'),
        name_collision_policy: z.enum(['fail_if_exists', 'reuse_if_exists']).optional().describe('Name collision policy'),
      }),
    },
    async ({ asset_path, component_name, component_class, parent_component, socket_name, attach_rule, name_collision_policy }) => {
      try {
        const payload: Record<string, unknown> = { asset_path, component_name, component_class };
        if (parent_component) payload['parent_component'] = parent_component;
        if (socket_name) payload['socket_name'] = socket_name;
        if (attach_rule) payload['attach_rule'] = attach_rule;
        if (name_collision_policy) payload['name_collision_policy'] = name_collision_policy;
        const resp = await bridge.sendCommand('add_component', payload);
        const raw = resp.result as Record<string, unknown> | undefined;
        if (raw && typeof raw['ok'] === 'boolean') {
          const comp = (raw['data'] as Record<string, unknown>|undefined)?.['component'] as Record<string, unknown>|undefined;
          return { content: [{ type: 'text' as const, text: `add_component ${raw['status'] ?? 'applied'}: ${component_name}(${component_class}), parent=${parent_component ?? 'root'}, created=${comp?.['created'] ?? false}.` }], isError: !raw['ok'], structuredContent: raw as Record<string, unknown> };
        }
        if (!resp.success) return toToolResult(resp);
        const result = normalizeToolResult(resp, 'add_component');
        return toMcpResult(result);
      } catch (err) { return toErrorResult(err); }
    },
  );

  // 鈹€鈹€鈹€ 2.13. set_component_property 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_set_component_property',
    {
      description: legacyWriteExpertDescription('Set a single Blueprint component property.'),
      inputSchema: z.object({
        asset_path: z.string().describe('Blueprint asset path'),
        component_name: z.string().describe('Component name'),
        property_path: z.string().describe('Property path'),
        value: z.union([z.string(), z.boolean(), z.number()]).describe('Property value'),
      }),
    },
    async ({ asset_path, component_name, property_path, value }) => {
      try {
        const resp = await bridge.sendCommand('set_component_property', { asset_path, component_name, property_path, value });
        const raw = resp.result as Record<string, unknown> | undefined;
        if (raw && typeof raw['ok'] === 'boolean') {
          const pr = (raw['data'] as Record<string, unknown>|undefined)?.['property_result'] as Record<string, unknown>|undefined;
          return { content: [{ type: 'text' as const, text: `set_component_property ${raw['status'] ?? 'applied'}: ${component_name}.${property_path}, applied=${pr?.['applied_count'] ?? 0}, changed=${pr?.['changed_count'] ?? 0}.` }], isError: !raw['ok'], structuredContent: raw as Record<string, unknown> };
        }
        if (!resp.success) return toToolResult(resp);
        const result = normalizeToolResult(resp, 'set_component_property');
        return toMcpResult(result);
      } catch (err) { return toErrorResult(err); }
    },
  );

  // 鈹€鈹€鈹€ 2.14. set_component_properties 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_set_component_properties',
    {
      description: legacyWriteExpertDescription('Set multiple Blueprint component properties transactionally.'),
      inputSchema: z.object({
        asset_path: z.string().describe('Blueprint asset path'),
        component_name: z.string().describe('Component name'),
        settings: z.array(z.object({
          property_path: z.string().describe('Property path'),
          value: z.union([z.string(), z.boolean(), z.number()]).describe('Property value'),
        })).describe('Property settings'),
      }),
    },
    async ({ asset_path, component_name, settings }) => {
      try {
        const resp = await bridge.sendCommand('set_component_properties', { asset_path, component_name, settings });
        const raw = resp.result as Record<string, unknown> | undefined;
        if (raw && typeof raw['ok'] === 'boolean') {
          const pr = (raw['data'] as Record<string, unknown>|undefined)?.['property_result'] as Record<string, unknown>|undefined;
          return { content: [{ type: 'text' as const, text: `set_component_properties ${raw['status'] ?? 'applied'}: ${component_name}, requested=${pr?.['requested_count'] ?? 0}, applied=${pr?.['applied_count'] ?? 0}, changed=${pr?.['changed_count'] ?? 0}, invalid=${(pr?.['invalid_settings'] as unknown[])?.length ?? 0}.` }], isError: !raw['ok'], structuredContent: raw as Record<string, unknown> };
        }
        if (!resp.success) return toToolResult(resp);
        const result = normalizeToolResult(resp, 'set_component_properties');
        return toMcpResult(result);
      } catch (err) { return toErrorResult(err); }
    },
  );

  // 鈹€鈹€鈹€ 2.15. remove_component 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_remove_component',
    {
      description: legacyWriteExpertDescription('Remove a component from a Blueprint.'),
      inputSchema: z.object({
        asset_path: z.string().describe('Blueprint asset path'),
        component_name: z.string().describe('Component name to remove'),
      }),
    },
    async ({ asset_path, component_name }) => {
      try {
        const resp = await bridge.sendCommand('remove_component', { asset_path, component_name });
        const raw = resp.result as Record<string, unknown> | undefined;
        if (raw && typeof raw['ok'] === 'boolean') {
          const comp = (raw['data'] as Record<string, unknown>|undefined)?.['component'] as Record<string, unknown>|undefined;
          return { content: [{ type: 'text' as const, text: `remove_component ${raw['status'] ?? 'applied'}: ${component_name}, removed=${comp?.['removed'] ?? false}.` }], isError: !raw['ok'], structuredContent: raw as Record<string, unknown> };
        }
        if (!resp.success) return toToolResult(resp);
        const result = normalizeToolResult(resp, 'remove_component');
        return toMcpResult(result);
      } catch (err) { return toErrorResult(err); }
    },
  );

  // 鈹€鈹€鈹€ 3. validate_json 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_validate_json',
    {
      description: legacyDebugExpertDescription('Pre-validate a JSON string against BlueprintHelper import rules before importing.'),
      inputSchema: z.object({
        json: z.string().describe('The JSON string to validate'),
      }),
    },
    async ({ json }) => {
      try {
        const resp = await bridge.sendCommand('validate_json', { json });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 4. export_to_json 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_export_to_json',
    {
      description: legacyDebugExpertDescription('Export a blueprint graph to raw BlueprintHelper JSON that can be written back or replayed with blueprint_import_json_to_graph. Use blueprint_get_logic for read-only logic summaries.'),
      inputSchema: z.object({
        target_blueprint: z.string().optional()
          .describe('Blueprint asset path, e.g. /Game/BP/BP_Test.BP_Test. Omit to use the active blueprint.'),
        target_graph: z.string().optional()
          .describe('Graph name to export. Omit to use the active graph.'),
        scope: z.enum(['graph', 'blueprint', 'selection']).optional().default('graph')
          .describe('Export scope: graph, blueprint, or selection.'),
        response_mode: responseModeSchema
          .describe('MCP response mode. Default returns a RawJson resource link.'),
      }),
      outputSchema: BlueprintRawJsonExportOutputSchema,
    },
    async ({ target_blueprint, target_graph, scope, response_mode }) => {
      try {
        const payload: Record<string, unknown> = {};
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        if (target_graph) payload.target_graph = target_graph;
        if (scope) payload.scope = scope;
        const resp = await bridge.sendCommand('export_to_json', payload);
        if (!resp.success) {
          return toToolResult(resp);
        }

        const normalizedPayload = normalizeBlueprintPayload(resp.result);
        const rawJsonPayload = getRawJsonPayload(normalizedPayload);
        const normalizedRecord = isRecord(normalizedPayload) ? normalizedPayload : undefined;
        const rawRecord = isRecord(rawJsonPayload) ? rawJsonPayload : undefined;
        const assetPath =
          target_blueprint ??
          getStringField(normalizedRecord, 'assetPath') ??
          getStringField(normalizedRecord, 'target_blueprint');
        const graph = target_graph ?? getStringField(normalizedRecord, 'graph');
        const mode = resolveResponseMode(response_mode, assetPath ? 'resource_ref' : 'structured_json');

        if (!assetPath) {
          return buildBlueprintToolResult({
            mode: 'structured_json',
            summary: 'RawJson export completed without an asset path.',
            structured: {
              format: 'raw_json',
              schema: 'BlueprintHelper.RawJsonRef.v1',
              importable: true,
              json: rawJsonPayload,
            },
          });
        }

        const rawUri = makeBlueprintResourceUri({
          assetPath,
          graph,
          view: 'raw-json',
        });
        return buildBlueprintToolResult({
          mode,
          summary: 'RawJson is available as a resource. Use only for debugging, compatibility, or replay.',
          structured: omitUndefined({
            format: 'raw_json_ref',
            schema: 'BlueprintHelper.RawJsonRef.v1',
            assetPath,
            graph,
            importable: true,
            rawUri,
            stats: getRecordField(rawRecord, 'stats') ?? getRecordField(normalizedRecord, 'stats'),
            json: isRecord(rawJsonPayload) ? rawJsonPayload : rawJsonPayload,
          }),
          resourceLinks: [
            {
              uri: rawUri,
              name: `${assetPath} RawJson`,
              description: 'Full raw BlueprintHelper JSON export.',
              mimeType: 'application/json',
            },
          ],
        });
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 5. get_logic 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_get_logic',
    {
      description: legacyDebugExpertDescription('Read the current Blueprint logic as Markdown. This is for understanding and review only; it is not importable raw BlueprintHelper JSON.'),
      inputSchema: z.object({
        target_blueprint: z.string().optional()
          .describe('Blueprint asset path, e.g. /Game/BP/BP_Test.BP_Test. Omit to use the active blueprint.'),
        target_graph: z.string().optional()
          .describe('Graph name to inspect. Omit to use the active graph.'),
        scope: z.enum(['graph', 'blueprint']).optional().default('graph')
          .describe('Logic read scope: graph or blueprint'),
        detail: z.enum(['brief', 'normal', 'full', 'debug']).optional().default('normal')
          .describe('Detail level for the logic summary'),
        include_data_dependencies: z.boolean().optional().default(true)
          .describe('Whether to include data dependencies in the logic summary'),
        include_orphans: z.boolean().optional().default(true)
          .describe('Whether to include orphan nodes in the logic summary'),
        response_mode: responseModeSchema
          .describe('MCP response mode. Default returns Markdown text and structured metadata.'),
      }),
      outputSchema: BlueprintLogicMdOutputSchema,
    },
    async ({
      target_blueprint,
      target_graph,
      scope,
      detail,
      include_data_dependencies,
      include_orphans,
      response_mode,
    }) => {
      try {
        const payload: Record<string, unknown> = {
          format: 'logic_md',
          scope: scope ?? 'graph',
          detail: detail ?? 'normal',
          include_data_dependencies: include_data_dependencies ?? true,
          include_orphans: include_orphans ?? true,
        };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        if (target_graph) payload.target_graph = target_graph;

        const resp = await bridge.sendCommand('export_logic', payload);
        const normalizedPayload = normalizeBlueprintPayload(resp.result);
        const payloadRecord = isRecord(normalizedPayload) ? normalizedPayload : undefined;
        const markdown = getStringField(payloadRecord, 'markdown');
        if (resp.success && typeof markdown === 'string' && payloadRecord) {
          const mode = resolveResponseMode(response_mode, 'summary_text');
          return toMarkdownToolResult(
            resp,
            markdown,
            makeLogicMdStructured(payloadRecord, target_blueprint, target_graph),
            mode,
          );
        }
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 6. get_logic_json 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_get_logic_json',
    {
      description: legacyDebugExpertDescription('Read the current Blueprint logic as structured logic JSON for analysis. This is not raw BlueprintHelper JSON and must not be passed directly to blueprint_import_json_to_graph.'),
      inputSchema: z.object({
        target_blueprint: z.string().optional()
          .describe('Blueprint asset path, e.g. /Game/BP/BP_Test.BP_Test. Omit to use the active blueprint.'),
        target_graph: z.string().optional()
          .describe('Graph name to inspect. Omit to use the active graph.'),
        scope: z.enum(['graph', 'blueprint']).optional().default('graph')
          .describe('Logic read scope: graph or blueprint'),
        detail: z.enum(['brief', 'normal', 'full', 'debug']).optional().default('normal')
          .describe('Detail level for the logic JSON'),
        include_data_dependencies: z.boolean().optional().default(true)
          .describe('Whether to include data dependencies in the logic JSON'),
        include_orphans: z.boolean().optional().default(true)
          .describe('Whether to include orphan nodes in the logic JSON'),
        include_node_ids: z.boolean().optional().default(false)
          .describe('Whether to include stable node identifiers where available'),
        include_positions: z.boolean().optional().default(false)
          .describe('Whether to include graph node positions'),
        include_raw_node_types: z.boolean().optional().default(false)
          .describe('Whether to include raw Unreal node type names'),
        response_mode: responseModeSchema
          .describe('MCP response mode. Default returns LogicJson in structuredContent with text summary.'),
      }),
      outputSchema: BlueprintLogicJsonOutputSchema,
    },
    async ({
      target_blueprint,
      target_graph,
      scope,
      detail,
      include_data_dependencies,
      include_orphans,
      include_node_ids,
      include_positions,
      include_raw_node_types,
      response_mode,
    }) => {
      try {
        const payload: Record<string, unknown> = {
          format: 'logic_json',
          scope: scope ?? 'graph',
          detail: detail ?? 'normal',
          include_data_dependencies: include_data_dependencies ?? true,
          include_orphans: include_orphans ?? true,
          include_node_ids: include_node_ids ?? false,
          include_positions: include_positions ?? false,
          include_raw_node_types: include_raw_node_types ?? false,
        };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        if (target_graph) payload.target_graph = target_graph;

        const resp = await bridge.sendCommand('export_logic', payload);
        if (!resp.success) {
          return toToolResult(resp);
        }

        const normalizedPayload = normalizeBlueprintPayload(resp.result);
        const payloadRecord = isRecord(normalizedPayload) ? normalizedPayload : undefined;
        if (!payloadRecord) {
          return toToolResult(resp);
        }

        const structured = makeLogicJsonStructured(payloadRecord, target_blueprint, target_graph);
        if (!structured) {
          return toToolResult(resp);
        }

        const stats = getRecordField(structured, 'stats');
        const nodes = stats?.['nodes'] ?? 'unknown';
        const assetPath = getStringField(structured, 'assetPath') ?? target_blueprint ?? 'unknown asset';
        const graphName = getStringField(structured, 'graph') ?? target_graph;
        const suffix = graphName ? `.${graphName}` : '';
        const mode = resolveResponseMode(response_mode, 'structured_json');

        return buildBlueprintToolResult({
          mode,
          summary: `Exported LogicJson: ${assetPath}${suffix}, nodes=${nodes}.`,
          structured,
        });
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 7. import_json_to_graph 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_import_json_to_graph',
    {
      description: legacyWriteExpertDescription('Import structured RawJson into a blueprint graph, creating nodes and connections. Rejects legacy string JSON and LogicJson/LogicMD read-only views.'),
      inputSchema: z.object({
        json: rawJsonInputSchema,
        target_blueprint: z.string().optional()
          .describe('Target blueprint asset path. Omit to use the active blueprint.'),
        target_graph: z.string().optional()
          .describe('Target graph name. Omit to use the active graph.'),
        compile_after_import: z.boolean().optional().default(true)
          .describe('Whether to compile the blueprint after import'),
        strict: z.boolean().optional().default(true)
          .describe('When true, any import error, no-op, partial node generation, or link failure rolls back the transaction'),
        allow_partial: z.boolean().optional().default(false)
          .describe('Only meaningful with strict=false; explicitly allows partial success to remain applied'),
      }),
    },
    async ({ json, target_blueprint, target_graph, compile_after_import, strict, allow_partial }) => {
      try {
        // Pre-check: reject read-only views before Bridge call
        if (isRecord(json)) {
          const schema = typeof json['schema'] === 'string' ? json['schema'] : '';
          if (schema.startsWith('BlueprintHelper.Logic')) {
            return {
              content: [{ type: 'text' as const, text: 'LogicJson/LogicMD are read-only views and cannot be imported as RawJson.' }],
              isError: true,
            };
          }
          if (json['importable'] === false) {
            return {
              content: [{ type: 'text' as const, text: 'This JSON view is marked importable=false and cannot be imported.' }],
              isError: true,
            };
          }
        }

        const payload: Record<string, unknown> = {
          json,
          compile_after_import,
          strict: strict ?? true,
          allow_partial: allow_partial ?? false,
        };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        if (target_graph) payload.target_graph = target_graph;
        const resp = await bridge.sendCommand('import_json', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 8. import_agent_graph 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_import_agent_graph',
    {
      description:
        legacyWriteExpertDescription('Import a BlueprintHelper.AgentImportGraph logic_spec object. Old semantic nodes/links/declarations/layout payloads are no longer accepted.'),
      inputSchema: z.object({
        schema: z.literal('BlueprintHelper.AgentImportGraph').default('BlueprintHelper.AgentImportGraph'),
        version: z.literal('1.0').default('1.0'),
        target_blueprint: z.string()
          .describe('Required Blueprint asset path, e.g. /Game/BP/BP_Player.BP_Player'),
        target_graph: z.string()
          .describe('Required graph name, e.g. EventGraph'),
        logic_spec: z.record(z.unknown()).describe('BlueprintLogicSpec/SemanticIR statement tree.'),
        options: z.object({
          compile: z.boolean().optional().default(true),
          save: z.boolean().optional().default(false),
          strict: z.boolean().optional().default(true),
          dry_run: z.boolean().optional().default(false),
          create_missing_variables: z.boolean().optional().default(true),
          reconstruct_existing_nodes: z.boolean().optional().default(false),
        }).strict().optional(),
      }).strict(),
    },
    async (payload) => {
      try {
        const resp = await bridge.sendCommand('import_agent_graph', payload);
        return toToolResult(resp, resp.result?.['success'] === false);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 9. compile_blueprint 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_compile_blueprint',
    {
      description: legacyWriteExpertDescription('Trigger compilation of a blueprint asset and return diagnostics.'),
      inputSchema: z.object({
        target_blueprint: z.string().optional()
          .describe('Blueprint asset path to compile. Omit to use the active blueprint.'),
      }),
    },
    async ({ target_blueprint }) => {
      try {
        const payload: Record<string, unknown> = {};
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        const resp = await bridge.sendCommand('compile_blueprint', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲锟?
  // Phase 4 锟?璧勪骇娴忚宸ュ叿
  // 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲锟?

  // 鈹€鈹€鈹€ 9. open_asset 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_open_asset',
    {
      description: legacyWriteExpertDescription('Open any Unreal asset in its default editor by asset path.'),
      inputSchema: z.object({
        asset_path: z.string().describe('Full asset path, e.g. /Game/Blueprints/BP_Player.BP_Player'),
      }),
    },
    async ({ asset_path }) => {
      try {
        const resp = await bridge.sendCommand('open_asset', { asset_path });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 12. save_asset 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_save_asset',
    {
      description: legacyWriteExpertDescription('Save a specific asset to disk.'),
      inputSchema: z.object({
        asset_path: z.string().describe('Full asset path to save, e.g. /Game/Blueprints/BP_Player.BP_Player'),
      }),
    },
    async ({ asset_path }) => {
      try {
        const resp = await bridge.sendCommand('save_asset', { asset_path });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 13. get_asset_info 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_get_asset_info',
    {
      description: legacyDebugExpertDescription('Get detailed information about an asset: class, parent class, disk size.'),
      inputSchema: z.object({
        asset_path: z.string().describe('Full asset path, e.g. /Game/Blueprints/BP_Player.BP_Player'),
      }),
    },
    async ({ asset_path }) => {
      try {
        const resp = await bridge.sendCommand('get_asset_info', { asset_path });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲锟?
  // Phase 5 锟?钃濆浘缁撴瀯鏌ヨ涓庢搷浣滃伐锟?
  // 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲锟?

  const targetSchemaFields = {
    target_blueprint: z.string().optional()
      .describe('Blueprint asset path. Omit to use the active blueprint.'),
    target_graph: z.string().optional()
      .describe('Graph name. Omit to use the active/default graph.'),
  };

  // 鈹€鈹€鈹€ 14. list_graphs 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_list_graphs',
    {
      description: legacyDebugExpertDescription('List all graphs (EventGraph, functions, macros) in a blueprint with their types and node counts.'),
      inputSchema: z.object({
        target_blueprint: targetSchemaFields.target_blueprint,
      }),
    },
    async ({ target_blueprint }) => {
      try {
        const payload: Record<string, unknown> = {};
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        const resp = await bridge.sendCommand('list_graphs', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 15. list_variables 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_list_variables',
    {
      description: legacyDebugExpertDescription('List all member variables in a blueprint with type info, defaults, and categories.'),
      inputSchema: z.object({
        target_blueprint: targetSchemaFields.target_blueprint,
      }),
    },
    async ({ target_blueprint }) => {
      try {
        const payload: Record<string, unknown> = {};
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        const resp = await bridge.sendCommand('list_variables', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 16. list_event_dispatchers 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_list_event_dispatchers',
    {
      description: legacyDebugExpertDescription('List all event dispatchers in a blueprint with their parameter signatures.'),
      inputSchema: z.object({
        target_blueprint: targetSchemaFields.target_blueprint,
      }),
    },
    async ({ target_blueprint }) => {
      try {
        const payload: Record<string, unknown> = {};
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        const resp = await bridge.sendCommand('list_event_dispatchers', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 17. add_variable 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_add_variable',
    {
      description: legacyWriteExpertDescription('Add a member variable to a blueprint. Supports type, default value, category, and flags.'),
      inputSchema: z.object({
        target_blueprint: targetSchemaFields.target_blueprint,
        name: z.string().describe('Variable name'),
        pin_type: z.object({
          category: z.string().describe('Pin category: bool, int, float, real, byte, name, string, text, object, class, struct, interface, softobject, softclass, enum'),
          sub_category: z.string().optional().describe('Pin sub-category'),
          object_path: z.string().optional().describe('Object path for object/struct/enum types'),
          container_type: z.string().optional().describe('Container type: None, Array, Set, Map'),
        }).optional().describe('Variable type. Defaults to bool if omitted.'),
        default_value: z.string().optional().describe('Default value as string'),
        category: z.string().optional().describe('Variable category for organization'),
        flags: z.object({
          blueprint_read_only: z.boolean().optional(),
          expose_on_spawn: z.boolean().optional(),
        }).optional().describe('Variable flags'),
      }),
    },
    async ({ target_blueprint, name, pin_type, default_value, category, flags }) => {
      try {
        const payload: Record<string, unknown> = { name };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        if (pin_type) payload.pin_type = pin_type;
        if (default_value !== undefined) payload.default_value = default_value;
        if (category) payload.category = category;
        if (flags) payload.flags = flags;
        const resp = await bridge.sendCommand('add_variable', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 18. remove_variable 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_remove_variable',
    {
      description: legacyWriteExpertDescription('Remove a member variable from a blueprint by name. Idempotent 锟?succeeds if variable does not exist.'),
      inputSchema: z.object({
        target_blueprint: targetSchemaFields.target_blueprint,
        name: z.string().describe('Variable name to remove'),
      }),
    },
    async ({ target_blueprint, name }) => {
      try {
        const payload: Record<string, unknown> = { name };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        const resp = await bridge.sendCommand('remove_variable', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 19. add_graph 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_add_graph',
    {
      description: legacyWriteExpertDescription('Add a new function or macro graph to a blueprint. Supports inputs, outputs, and pure flag.'),
      inputSchema: z.object({
        target_blueprint: targetSchemaFields.target_blueprint,
        name: z.string().describe('Graph name'),
        graph_type: z.enum(['Function', 'Macro']).optional().default('Function')
          .describe('Graph type: Function or Macro'),
        inputs: z.array(z.object({
          name: z.string(),
          pin_type: z.object({
            category: z.string(),
            sub_category: z.string().optional(),
            object_path: z.string().optional(),
            container_type: z.string().optional(),
          }),
        })).optional().describe('Function input parameters'),
        outputs: z.array(z.object({
          name: z.string(),
          pin_type: z.object({
            category: z.string(),
            sub_category: z.string().optional(),
            object_path: z.string().optional(),
            container_type: z.string().optional(),
          }),
        })).optional().describe('Function output/return parameters'),
        is_pure: z.boolean().optional().default(false)
          .describe('Whether the function is pure (no execution pins)'),
      }),
    },
    async ({ target_blueprint, name, graph_type, inputs, outputs, is_pure }) => {
      try {
        const payload: Record<string, unknown> = { name };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        if (graph_type) payload.graph_type = graph_type;
        if (inputs) payload.inputs = inputs;
        if (outputs) payload.outputs = outputs;
        if (is_pure !== undefined) payload.is_pure = is_pure;
        const resp = await bridge.sendCommand('add_graph', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 20. remove_graph 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_remove_graph',
    {
      description: legacyWriteExpertDescription('Remove a function or macro graph from a blueprint. Cannot remove EventGraph. Idempotent.'),
      inputSchema: z.object({
        target_blueprint: targetSchemaFields.target_blueprint,
        name: z.string().describe('Graph name to remove'),
      }),
    },
    async ({ target_blueprint, name }) => {
      try {
        const payload: Record<string, unknown> = { name };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        const resp = await bridge.sendCommand('remove_graph', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 21. add_event_dispatcher 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_add_event_dispatcher',
    {
      description: legacyWriteExpertDescription('Add an event dispatcher to a blueprint with optional typed parameters.'),
      inputSchema: z.object({
        target_blueprint: targetSchemaFields.target_blueprint,
        name: z.string().describe('Event dispatcher name'),
        params: z.array(z.object({
          name: z.string().describe('Parameter name'),
          pin_type: z.object({
            category: z.string(),
            sub_category: z.string().optional(),
            object_path: z.string().optional(),
            container_type: z.string().optional(),
          }).optional().describe('Parameter type. Defaults to bool.'),
        })).optional().describe('Dispatcher parameters'),
      }),
    },
    async ({ target_blueprint, name, params }) => {
      try {
        const payload: Record<string, unknown> = { name };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        if (params) payload.params = params;
        const resp = await bridge.sendCommand('add_event_dispatcher', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 22. delete_nodes 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_delete_nodes',
    {
      description: legacyWriteExpertDescription('Delete specific nodes from a blueprint graph by their IDs (Node_0, Node_1, etc.). Cannot delete FunctionEntry/FunctionResult nodes.'),
      inputSchema: z.object({
        ...targetSchemaFields,
        node_ids: z.array(z.string()).describe('Array of node IDs to delete, e.g. ["Node_0", "Node_3"]'),
      }),
    },
    async ({ target_blueprint, target_graph, node_ids }) => {
      try {
        const payload: Record<string, unknown> = { node_ids };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        if (target_graph) payload.target_graph = target_graph;
        const resp = await bridge.sendCommand('delete_nodes', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲锟?
  // Phase 6 锟?UMG Widget 鎿嶄綔宸ュ叿
  // 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲锟?

  const widgetAssetPath = z.string()
    .describe('WidgetBlueprint asset path, e.g. /Game/UI/WBP_Main.WBP_Main');

  // 鈹€鈹€鈹€ 23. get_widget_tree 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_get_widget_tree',
    {
      description: legacyDebugExpertDescription('Get the full widget tree of a WidgetBlueprint, showing hierarchy, types, and slot info.'),
      inputSchema: z.object({
        asset_path: widgetAssetPath,
      }),
    },
    async ({ asset_path }) => {
      try {
        const resp = await bridge.sendCommand('get_widget_tree', { asset_path });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 24. add_widget 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_add_widget',
    {
      description: legacyWriteExpertDescription('Add a new widget to a WidgetBlueprint. Specify parent panel and widget class (e.g. TextBlock, Button, CanvasPanel).'),
      inputSchema: z.object({
        asset_path: widgetAssetPath,
        widget_class: z.string()
          .describe('Widget class name without U prefix, e.g. TextBlock, Button, CanvasPanel, VerticalBox, Image'),
        parent_name: z.string().optional()
          .describe('Parent panel widget name. Omit to add to root panel.'),
        widget_name: z.string().optional()
          .describe('Name for the new widget. Auto-generated if omitted.'),
      }),
    },
    async ({ asset_path, widget_class, parent_name, widget_name }) => {
      try {
        const payload: Record<string, unknown> = { asset_path, widget_class };
        if (parent_name) payload.parent_name = parent_name;
        if (widget_name) payload.widget_name = widget_name;
        const resp = await bridge.sendCommand('add_widget', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 25. remove_widget 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_remove_widget',
    {
      description: legacyWriteExpertDescription('Remove a widget (and its subtree) from a WidgetBlueprint by name.'),
      inputSchema: z.object({
        asset_path: widgetAssetPath,
        widget_name: z.string().describe('Name of the widget to remove'),
      }),
    },
    async ({ asset_path, widget_name }) => {
      try {
        const resp = await bridge.sendCommand('remove_widget', { asset_path, widget_name });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 26. move_widget 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_move_widget',
    {
      description: legacyWriteExpertDescription('Move a widget to a different parent panel within the same WidgetBlueprint.'),
      inputSchema: z.object({
        asset_path: widgetAssetPath,
        widget_name: z.string().describe('Name of the widget to move'),
        new_parent: z.string().describe('Name of the new parent panel widget'),
        insert_index: z.number().optional()
          .describe('Position to insert at (-1 or omit for end)'),
      }),
    },
    async ({ asset_path, widget_name, new_parent, insert_index }) => {
      try {
        const payload: Record<string, unknown> = { asset_path, widget_name, new_parent };
        if (insert_index !== undefined) payload.insert_index = insert_index;
        const resp = await bridge.sendCommand('move_widget', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 27. get_widget_properties 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_get_widget_properties',
    {
      description: legacyDebugExpertDescription('Get all editable properties of a widget with their current values and types.'),
      inputSchema: z.object({
        asset_path: widgetAssetPath,
        widget_name: z.string().describe('Name of the widget to inspect'),
      }),
    },
    async ({ asset_path, widget_name }) => {
      try {
        const resp = await bridge.sendCommand('get_widget_properties', { asset_path, widget_name });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 28. set_widget_property 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_set_widget_property',
    {
      description: legacyWriteExpertDescription('Set a property value on a widget using text import format. Use get_widget_properties to discover available properties first.'),
      inputSchema: z.object({
        asset_path: widgetAssetPath,
        widget_name: z.string().describe('Name of the widget to modify'),
        property_name: z.string().describe('Property name, e.g. Text, ColorAndOpacity, Visibility'),
        value: z.string().describe('Property value in UE text import format'),
      }),
    },
    async ({ asset_path, widget_name, property_name, value }) => {
      try {
        const resp = await bridge.sendCommand('set_widget_property', {
          asset_path, widget_name, property_name, value,
        });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲锟?
  // Phase 7 锟?DataAsset & DataTable 鎿嶄綔 (Tools 29锟?4)
  // 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲锟?

  const objectAssetPath = z.string()
    .describe('Asset path to any UObject (DataAsset, DataTable, etc.), e.g. /Game/Data/DA_Example.DA_Example');

  const dataTableAssetPath = z.string()
    .describe('DataTable asset path, e.g. /Game/Data/DT_Items.DT_Items');

  // 鈹€鈹€鈹€ 29. get_object_properties 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_get_object_properties',
    {
      description: legacyDebugExpertDescription('Get all editable UPROPERTY fields of any asset (DataAsset, etc.) with their current values, types, and categories via FProperty reflection.'),
      inputSchema: z.object({
        asset_path: objectAssetPath,
      }),
    },
    async ({ asset_path }) => {
      try {
        const resp = await bridge.sendCommand('get_object_properties', { asset_path });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 30. set_object_property 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_set_object_property',
    {
      description: legacyWriteExpertDescription('Set a single UPROPERTY value on any asset using UE text import format. Use get_object_properties to discover available properties first.'),
      inputSchema: z.object({
        asset_path: objectAssetPath,
        property_name: z.string().describe('Property name to set'),
        value: z.string().describe('New value in UE text import format'),
      }),
    },
    async ({ asset_path, property_name, value }) => {
      try {
        const resp = await bridge.sendCommand('set_object_property', {
          asset_path, property_name, value,
        });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 31. get_datatable_rows 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_get_datatable_rows',
    {
      description: legacyDebugExpertDescription('Get all rows (or specific rows) from a DataTable with column schema and field values.'),
      inputSchema: z.object({
        asset_path: dataTableAssetPath,
        row_names: z.array(z.string()).optional()
          .describe('Optional list of row names to filter. Omit to get all rows.'),
      }),
    },
    async ({ asset_path, row_names }) => {
      try {
        const payload: Record<string, unknown> = { asset_path };
        if (row_names && row_names.length > 0) payload.row_names = row_names;
        const resp = await bridge.sendCommand('get_datatable_rows', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 32. add_datatable_row 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_add_datatable_row',
    {
      description: legacyWriteExpertDescription('Add a new row to a DataTable. Fields are key-value pairs matching the row struct properties.'),
      inputSchema: z.object({
        asset_path: dataTableAssetPath,
        row_name: z.string().describe('Name for the new row'),
        fields: z.record(z.string()).optional()
          .describe('Object of field_name: value_string pairs. Values use UE text import format.'),
      }),
    },
    async ({ asset_path, row_name, fields }) => {
      try {
        const payload: Record<string, unknown> = { asset_path, row_name };
        if (fields) payload.fields = fields;
        const resp = await bridge.sendCommand('add_datatable_row', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 33. update_datatable_row 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_update_datatable_row',
    {
      description: legacyWriteExpertDescription('Update fields on an existing DataTable row. Only specified fields are modified.'),
      inputSchema: z.object({
        asset_path: dataTableAssetPath,
        row_name: z.string().describe('Name of the row to update'),
        fields: z.record(z.string())
          .describe('Object of field_name: new_value_string pairs. Values use UE text import format.'),
      }),
    },
    async ({ asset_path, row_name, fields }) => {
      try {
        const resp = await bridge.sendCommand('update_datatable_row', {
          asset_path, row_name, fields,
        });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 34. delete_datatable_row 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_delete_datatable_row',
    {
      description: legacyWriteExpertDescription('Delete a row from a DataTable by name.'),
      inputSchema: z.object({
        asset_path: dataTableAssetPath,
        row_name: z.string().describe('Name of the row to delete'),
      }),
    },
    async ({ asset_path, row_name }) => {
      try {
        const resp = await bridge.sendCommand('delete_datatable_row', { asset_path, row_name });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲锟?
  // Phase 8: 缂栬緫鍣ㄥ懡锟?(6 tools)
  // 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲锟?

  // 鈹€鈹€鈹€ 35. undo 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_undo',
    {
      description: legacyWriteExpertDescription('Undo the last editor action.'),
      inputSchema: z.object({}),
    },
    async () => {
      try {
        const resp = await bridge.sendCommand('undo', {});
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 36. redo 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_redo',
    {
      description: legacyWriteExpertDescription('Redo the last undone editor action.'),
      inputSchema: z.object({}),
    },
    async () => {
      try {
        const resp = await bridge.sendCommand('redo', {});
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 37. play_in_editor 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_play_in_editor',
    {
      description: legacyWriteExpertDescription('Start a Play In Editor (PIE) session.'),
      inputSchema: z.object({}),
    },
    async () => {
      try {
        const resp = await bridge.sendCommand('play_in_editor', {});
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 38. stop_pie 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_stop_pie',
    {
      description: legacyWriteExpertDescription('Stop the currently running PIE session.'),
      inputSchema: z.object({}),
    },
    async () => {
      try {
        const resp = await bridge.sendCommand('stop_pie', {});
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 39. create_blueprint 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_create_blueprint',
    {
      description: legacyWriteExpertDescription('Create a new Blueprint asset in the project.'),
      inputSchema: z.object({
        asset_path: z.string().describe('Full asset path, e.g. /Game/Blueprints/BP_MyActor'),
        parent_class: z.string().optional().default('Actor').describe('Parent class name (Actor, Pawn, Character, etc.)'),
      }),
    },
    async ({ asset_path, parent_class }) => {
      try {
        const resp = await bridge.sendCommand('create_blueprint', { asset_path, parent_class });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 40. exec_console_command 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_exec_console_command',
    {
      description: legacyWriteExpertDescription('Execute an Unreal Editor console command and return its output.'),
      inputSchema: z.object({
        command: z.string().describe('The console command to execute'),
      }),
    },
    async ({ command }) => {
      try {
        const resp = await bridge.sendCommand('exec_console_command', { command });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲锟?
  // Editor Lifecycle Tools (41-43)
  // 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲锟?

  // 鈹€鈹€鈹€ 41. close_editor 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_close_editor',
    {
      description:
        legacyWriteExpertDescription('Save all dirty assets and close the Unreal Editor. The editor will exit on the next frame after the response is sent. Use this before building to avoid LiveCoding duplicate-class issues.'),
      inputSchema: z.object({
        save_all: z
          .boolean()
          .optional()
          .describe('Whether to save all dirty packages before closing (default true)'),
      }),
    },
    async ({ save_all }) => {
      try {
        const payload: Record<string, unknown> = {};
        if (save_all !== undefined) payload['save_all'] = save_all;
        const resp = await bridge.sendCommand('close_editor', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // 鈹€鈹€鈹€ 42. build_project 鈹€鈹€鈹€
  server.registerTool(
    'blueprint_build_project',
    {
      description:
        legacyWriteExpertDescription('Build the Unreal project using UnrealBuildTool. The editor must be closed first. Returns build output. Reads the UE engine root from the project .blueprinthelper/agent-profile.json environment.ue_engine_dir and requires an explicit project_file tool argument.'),
      inputSchema: z.object({
        project_file: z
          .string()
          .min(1)
          .describe('Absolute path to the target .uproject file. Agents should discover it from the current workspace before calling this tool.'),
        target: z
          .string()
          .optional()
          .describe('Build target name (default: derived from project filename + "Editor", e.g. MrStoneEditor)'),
        configuration: z
          .enum(['Development', 'DebugGame', 'Debug', 'Shipping', 'Test'])
          .optional()
          .describe('Build configuration (default: Development)'),
        platform: z.string().optional().describe('Target platform (default: Win64)'),
      }),
    },
    async ({ project_file, target, configuration, platform }) => {
      let uprojectFile: string;
      try {
        uprojectFile = resolveExplicitProjectFile(project_file);
      } catch (err) {
        return toErrorResult(err);
      }

      let ueEngineDir: string;
      try {
        ueEngineDir = resolveProjectEngineDir(uprojectFile, config);
      } catch (err) {
        return toErrorResult(err);
      }

      const buildBat = path.join(ueEngineDir, 'Engine', 'Build', 'BatchFiles', 'Build.bat');
      const projectName = path.basename(uprojectFile, '.uproject');
      const buildTarget = target ?? `${projectName}Editor`;
      const buildConfig = configuration ?? 'Development';
      const buildPlatform = platform ?? 'Win64';

      return new Promise((resolve) => {
        console.error(
          `[BlueprintHelper MCP] Building: ${buildBat} ${buildTarget} ${buildPlatform} ${buildConfig} "${uprojectFile}" -WaitMutex`,
        );

        execFile(
          buildBat,
          [buildTarget, buildPlatform, buildConfig, uprojectFile, '-WaitMutex'],
          { maxBuffer: 10 * 1024 * 1024, timeout: 600_000 },
          (error, stdout, stderr) => {
            const output = (stdout || '') + (stderr ? `\n--- stderr ---\n${stderr}` : '');
            if (error) {
              resolve({
                content: [
                  {
                    type: 'text' as const,
                    text: JSON.stringify(
                      {
                        success: false,
                        exit_code: error.code ?? -1,
                        message: `Build failed: ${error.message}`,
                        output: output.slice(-8000),
                      },
                      null,
                      2,
                    ),
                  },
                ],
                isError: true,
              });
            } else {
              resolve({
                content: [
                  {
                    type: 'text' as const,
                    text: JSON.stringify(
                      {
                        success: true,
                        message: 'Build succeeded.',
                        output: output.slice(-4000),
                      },
                      null,
                      2,
                    ),
                  },
                ],
                isError: false,
              });
            }
          },
        );
      });
    },
  );

  // 鈹€鈹€鈹€ 43. open_editor 鈹€鈹€鈹€
  server.registerTool(
  'blueprint_open_editor',
  {
    description:
      preflightOnlyDescription('Launch Unreal Editor by opening the explicit project_file .uproject, then wait for the BlueprintHelper Bridge server to become available. Reads the UE engine root from the project .blueprinthelper/agent-profile.json environment.ue_engine_dir. Agents should discover the .uproject from the current workspace before calling this tool.'),
    inputSchema: z.object({
      project_file: z
        .string()
        .min(1)
        .describe('Absolute path to the target .uproject file. Agents should discover it from the current workspace before calling this tool.'),
      wait_timeout_ms: z
        .number()
        .optional()
        .describe('Max time in ms to wait for the editor Bridge to become available (default 120000)'),
    }),
  },
  async ({ project_file, wait_timeout_ms }) => {
    let uprojectFile: string;
    try {
      uprojectFile = resolveExplicitProjectFile(project_file);
    } catch (err) {
      return toErrorResult(err);
    }

    let ueEngineDir: string;
    try {
      ueEngineDir = resolveProjectEngineDir(uprojectFile, config);
    } catch (err) {
      return toErrorResult(err);
    }

    const editorExe = path.join(
      ueEngineDir,
      'Engine',
      'Binaries',
      'Win64',
      'UnrealEditor.exe',
    );

    const timeoutMs = wait_timeout_ms ?? 120_000;

    const launchCommand = `"${editorExe}" "${uprojectFile}"`;

    console.error(`[BlueprintHelper MCP] Launching editor with project file: ${launchCommand}`);

    let child;
    try {
      // 鍚姩褰撳墠椤圭洰锟?.uproject锛岃€屼笉鏄墦寮€锟?Unreal Editor锟?
      child = spawn(editorExe, [uprojectFile], {
        detached: true,
        stdio: 'ignore',
      });

      child.unref();
    } catch (err) {
      return {
        content: [
          {
            type: 'text' as const,
            text: JSON.stringify(
              {
                success: false,
                code: 'EDITOR_LAUNCH_FAILED',
                message: 'Failed to launch Unreal Editor with the current project .uproject.',
                editor_exe: editorExe,
                uproject_path: uprojectFile,
                launch_command: launchCommand,
                agent_instruction:
                  'Verify environment.ue_engine_dir in the project agent-profile and project_file. project_file must be the absolute path to the target .uproject.',
                error: err instanceof Error ? err.message : String(err),
              },
              null,
              2,
            ),
          },
        ],
        isError: true,
      };
    }

    // 杞 Bridge 鐩村埌鍙敤锟?
    const startTime = Date.now();
    const pollIntervalMs = 3000;

    while (Date.now() - startTime < timeoutMs) {
      await new Promise((r) => setTimeout(r, pollIntervalMs));

      const alive = await bridge.ping();
      if (alive) {
        return {
          content: [
            {
              type: 'text' as const,
              text: JSON.stringify(
                {
                  success: true,
                  code: 'EDITOR_BRIDGE_AVAILABLE',
                  message:
                    'Unreal Editor was launched with the current project .uproject and BlueprintHelper Bridge is available.',
                  editor_exe: editorExe,
                  uproject_path: uprojectFile,
                  launch_command: launchCommand,
                  elapsed_ms: Date.now() - startTime,
                  agent_instruction:
                    'The editor and Bridge are ready. Editor-bound BlueprintHelper MCP tools may now be used. Prefer explicit asset_path and target_graph arguments for write operations.',
                },
                null,
                2,
              ),
            },
          ],
          isError: false,
        };
      }
    }

    return {
      content: [
        {
          type: 'text' as const,
          text: JSON.stringify(
            {
              success: false,
              code: 'EDITOR_STARTED_BRIDGE_TIMEOUT',
              message:
                `Unreal Editor was started with the current project .uproject, but BlueprintHelper Bridge did not become available within ${timeoutMs}ms. The editor may still be loading.`,
              editor_exe: editorExe,
              uproject_path: uprojectFile,
              launch_command: launchCommand,
              elapsed_ms: Date.now() - startTime,
              agent_instruction:
                'Do not use editor-bound BlueprintHelper MCP tools yet. Wait for Unreal Editor to finish loading, then check Bridge/status or call blueprint_open_editor again with a longer wait_timeout_ms.',
            },
            null,
            2,
          ),
        },
      ],
      isError: true,
    };
  },
);
}

function isEditorLifecycleOnlyMcpSurface(): boolean {
  return true;
}
