/**
 * MCP Tools 注册
 *
 * 将 42 个 Bridge 命令映射为 MCP 工具，另提供 2 个本地生命周期工具。
 * Phase 1-3: 8 个蓝图操作/逻辑读取工具
 * Phase 4:   5 个资产浏览工具
 * Phase 5:   9 个蓝图结构操作工具
 * Phase 6:   6 个 UMG Widget 操作工具
 * Phase 7:   6 个 DataAsset & DataTable 操作工具
 * Phase 8:   6 个编辑器命令工具
 */

import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { BridgeClient, BridgeResponse } from './bridge-client.js';
import { execFile, spawn } from 'node:child_process';
import * as path from 'node:path';

/** 编辑器/引擎路径配置 */
export interface EditorConfig {
  ueEngineDir: string;
  ueProjectFile: string;
}

/** 将 Bridge 响应转换为 MCP tool result */
function toToolResult(resp: BridgeResponse, isError = false) {
  return {
    content: [{ type: 'text' as const, text: JSON.stringify(resp, null, 2) }],
    isError: isError || !resp.success,
  };
}

const SAFETY_RESULT_FIELDS = [
  'effective_scope',
  'status',
  'operations_applied',
  'nodes_created',
  'links_connected',
  'warnings',
  'errors',
  'rolled_back',
] as const;

type SafetyResultField = typeof SAFETY_RESULT_FIELDS[number];

function collectSafetyResultFields(resp: BridgeResponse) {
  const safetyFields: Partial<Record<SafetyResultField, unknown>> = {};
  const result = resp.result;

  for (const field of SAFETY_RESULT_FIELDS) {
    if (result?.[field] !== undefined) {
      safetyFields[field] = result[field];
    }
  }

  const responseRecord = resp as unknown as Record<string, unknown>;
  for (const field of SAFETY_RESULT_FIELDS) {
    if (safetyFields[field] === undefined && responseRecord[field] !== undefined) {
      safetyFields[field] = responseRecord[field];
    }
  }

  if (Object.keys(safetyFields).length === 0) {
    return undefined;
  }

  return {
    success: resp.success,
    result: safetyFields,
  };
}

function toMarkdownToolResult(resp: BridgeResponse, markdown: string) {
  const content = [{ type: 'text' as const, text: markdown }];
  const safetyFields = collectSafetyResultFields(resp);
  if (safetyFields) {
    content.push({ type: 'text' as const, text: JSON.stringify(safetyFields, null, 2) });
  }
  return { content };
}

/** 将 Bridge 错误转换为 MCP tool error result */
function toErrorResult(err: unknown) {
  const message = err instanceof Error ? err.message : String(err);
  return {
    content: [{ type: 'text' as const, text: `Bridge error: ${message}` }],
    isError: true,
  };
}

export function registerTools(server: McpServer, bridge: BridgeClient, config: EditorConfig): void {
  // ─── 1. get_rule_markdown ───
  server.registerTool(
    'blueprint_get_rule_markdown',
    {
      description: 'Get the JSON-to-Blueprint conversion rule document in Markdown format.before you get rule md,you should read AGENT.md first',
      inputSchema: z.object({}),
    },
    async () => {
      try {
        const resp = await bridge.sendCommand('get_rule_markdown');
        if (resp.success && resp.result?.['markdown']) {
          return {
            content: [{ type: 'text', text: resp.result['markdown'] as string }],
          };
        }
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 2. get_editor_context ───
  server.registerTool(
    'blueprint_get_editor_context',
    {
      description: 'Get the current Unreal Editor context: active blueprint, graph, node count, compile status.',
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

  // ─── 3. validate_json ───
  server.registerTool(
    'blueprint_validate_json',
    {
      description: 'Pre-validate a JSON string against BlueprintHelper import rules before importing.',
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

  // ─── 4. export_to_json ───
  server.registerTool(
    'blueprint_export_to_json',
    {
      description: 'Export a blueprint graph to raw BlueprintHelper JSON that can be written back or replayed with blueprint_import_json_to_graph. Use blueprint_get_logic for read-only logic summaries.',
      inputSchema: z.object({
        target_blueprint: z.string().optional()
          .describe('Blueprint asset path, e.g. /Game/BP/BP_Test.BP_Test. Omit to use the active blueprint.'),
        target_graph: z.string().optional()
          .describe('Graph name to export. Omit to use the active graph.'),
        scope: z.enum(['graph', 'blueprint', 'selection', 'full_graph', 'full_blueprint']).optional().default('graph')
          .describe('Export scope: graph, blueprint, or selection. Legacy full_graph/full_blueprint are accepted.'),
      }),
    },
    async ({ target_blueprint, target_graph, scope }) => {
      try {
        const payload: Record<string, unknown> = {};
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        if (target_graph) payload.target_graph = target_graph;
        if (scope) payload.scope = scope;
        const resp = await bridge.sendCommand('export_to_json', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 5. get_logic ───
  server.registerTool(
    'blueprint_get_logic',
    {
      description: 'Read the current Blueprint logic as Agent-friendly Markdown. This is for understanding and review only; it is not importable raw BlueprintHelper JSON.',
      inputSchema: z.object({
        target_blueprint: z.string().optional()
          .describe('Blueprint asset path, e.g. /Game/BP/BP_Test.BP_Test. Omit to use the active blueprint.'),
        target_graph: z.string().optional()
          .describe('Graph name to inspect. Omit to use the active graph.'),
        scope: z.enum(['single_graph', 'full_blueprint']).optional().default('single_graph')
          .describe('Logic read scope: single_graph or full_blueprint'),
        detail: z.enum(['brief', 'normal', 'debug']).optional().default('normal')
          .describe('Detail level for the logic summary'),
        include_data_dependencies: z.boolean().optional().default(true)
          .describe('Whether to include data dependencies in the logic summary'),
        include_orphans: z.boolean().optional().default(true)
          .describe('Whether to include orphan nodes in the logic summary'),
      }),
    },
    async ({
      target_blueprint,
      target_graph,
      scope,
      detail,
      include_data_dependencies,
      include_orphans,
    }) => {
      try {
        const payload: Record<string, unknown> = {
          format: 'logic_md',
          scope: scope ?? 'single_graph',
          detail: detail ?? 'normal',
          include_data_dependencies: include_data_dependencies ?? true,
          include_orphans: include_orphans ?? true,
        };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        if (target_graph) payload.target_graph = target_graph;

        const resp = await bridge.sendCommand('export_logic', payload);
        const markdown = resp.result?.['markdown'];
        if (resp.success && typeof markdown === 'string') {
          return toMarkdownToolResult(resp, markdown);
        }
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 6. get_logic_json ───
  server.registerTool(
    'blueprint_get_logic_json',
    {
      description: 'Read the current Blueprint logic as structured logic JSON for analysis. This is not raw BlueprintHelper JSON and must not be passed directly to blueprint_import_json_to_graph.',
      inputSchema: z.object({
        target_blueprint: z.string().optional()
          .describe('Blueprint asset path, e.g. /Game/BP/BP_Test.BP_Test. Omit to use the active blueprint.'),
        target_graph: z.string().optional()
          .describe('Graph name to inspect. Omit to use the active graph.'),
        scope: z.enum(['single_graph', 'full_blueprint']).optional().default('single_graph')
          .describe('Logic read scope: single_graph or full_blueprint'),
        detail: z.enum(['brief', 'normal', 'debug']).optional().default('normal')
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
      }),
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
    }) => {
      try {
        const payload: Record<string, unknown> = {
          format: 'logic_json',
          scope: scope ?? 'single_graph',
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
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 7. import_json_to_graph ───
  server.registerTool(
    'blueprint_import_json_to_graph',
    {
      description: 'Import JSON into a blueprint graph, creating nodes and connections. This modifies the blueprint.',
      inputSchema: z.object({
        json: z.string().describe('The BlueprintHelper JSON to import'),
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

  // ─── 8. import_agent_graph ───
  const agentImportNodeSchema = z.object({
    id: z.string().describe('Local semantic node id'),
    kind: z.enum([
      'event',
      'custom_event',
      'call',
      'get',
      'set',
      'branch',
      'sequence',
      'comment',
    ]).describe('Semantic node kind supported by AgentImportGraph v1'),
  }).passthrough();

  const agentImportLinkSchema = z.object({
    kind: z.enum(['exec', 'data']).describe('Link kind: exec or data'),
    from: z.string().optional().describe('Shorthand source endpoint, e.g. begin_play.then'),
    to: z.string().optional().describe('Shorthand target endpoint, e.g. print.execute'),
    from_node: z.string().optional().describe('Structured source node id'),
    from_pin: z.string().optional().describe('Structured source pin name'),
    to_node: z.string().optional().describe('Structured target node id'),
    to_pin: z.string().optional().describe('Structured target pin name'),
  }).passthrough();

  server.registerTool(
    'blueprint_import_agent_graph',
    {
      description:
        'Import an Agent-facing semantic BlueprintHelper.AgentImportGraph object. This creates Blueprint nodes from intent-level event/call/get/set/branch/sequence/comment nodes and auto-generates layout. It does not replace blueprint_import_json_to_graph for raw JSON replay.',
      inputSchema: z.object({
        schema: z.literal('BlueprintHelper.AgentImportGraph').default('BlueprintHelper.AgentImportGraph'),
        version: z.literal('1.0').default('1.0'),
        target_blueprint: z.string()
          .describe('Required Blueprint asset path, e.g. /Game/BP/BP_Player.BP_Player'),
        target_graph: z.string()
          .describe('Required graph name, e.g. EventGraph'),
        mode: z.literal('append').default('append'),
        layout: z.enum(['auto', 'append_right']).optional().default('auto'),
        declarations: z.object({
          variables: z.array(z.object({
            name: z.string(),
            type: z.string().describe('Pin type category, e.g. bool, int, float, string'),
            default: z.union([z.string(), z.number(), z.boolean()]).optional(),
            default_value: z.string().optional(),
            editable: z.boolean().optional(),
            category: z.string().optional(),
          }).passthrough()).optional(),
        }).passthrough().optional(),
        nodes: z.array(agentImportNodeSchema),
        links: z.array(agentImportLinkSchema).optional().default([]),
        options: z.object({
          compile: z.boolean().optional().default(true),
          save: z.boolean().optional().default(false),
          strict: z.boolean().optional().default(true),
          dry_run: z.boolean().optional().default(false),
          create_missing_variables: z.boolean().optional().default(true),
          reconstruct_existing_nodes: z.boolean().optional().default(false),
        }).passthrough().optional(),
      }).passthrough(),
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

  // ─── 9. compile_blueprint ───
  server.registerTool(
    'blueprint_compile_blueprint',
    {
      description: 'Trigger compilation of a blueprint asset and return diagnostics.',
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

  // ═══════════════════════════════════════════════════════════
  // Phase 4 — 资产浏览工具
  // ═══════════════════════════════════════════════════════════

  // ─── 9. open_asset ───
  server.registerTool(
    'blueprint_open_asset',
    {
      description: 'Open any Unreal asset in its default editor by asset path.',
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

  // ─── 10. list_assets ───
  server.registerTool(
    'blueprint_list_assets',
    {
      description: 'List assets in a Content Browser directory with optional class and name filters.',
      inputSchema: z.object({
        path: z.string().optional().default('/Game')
          .describe('Content directory path, e.g. /Game/Blueprints'),
        class_filter: z.string().optional()
          .describe('Filter by asset class name, e.g. Blueprint, DataTable, WidgetBlueprint'),
        name_filter: z.string().optional()
          .describe('Filter by asset name substring'),
        recursive: z.boolean().optional().default(true)
          .describe('Whether to search subdirectories recursively'),
        max_results: z.number().optional().default(200)
          .describe('Maximum number of results to return (0 = unlimited)'),
      }),
    },
    async ({ path, class_filter, name_filter, recursive, max_results }) => {
      try {
        const payload: Record<string, unknown> = {};
        if (path) payload.path = path;
        if (class_filter) payload.class_filter = class_filter;
        if (name_filter) payload.name_filter = name_filter;
        if (recursive !== undefined) payload.recursive = recursive;
        if (max_results !== undefined) payload.max_results = max_results;
        const resp = await bridge.sendCommand('list_assets', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 11. search_assets ───
  server.registerTool(
    'blueprint_search_assets',
    {
      description: 'Search for assets by keyword across the project content. Always searches recursively.',
      inputSchema: z.object({
        query: z.string().describe('Search keyword (matches asset name substring)'),
        path: z.string().optional()
          .describe('Limit search to this content path, e.g. /Game/Blueprints'),
        class_filter: z.string().optional()
          .describe('Filter by asset class name'),
        max_results: z.number().optional().default(50)
          .describe('Maximum number of results to return'),
      }),
    },
    async ({ query, path, class_filter, max_results }) => {
      try {
        const payload: Record<string, unknown> = { query };
        if (path) payload.path = path;
        if (class_filter) payload.class_filter = class_filter;
        if (max_results !== undefined) payload.max_results = max_results;
        const resp = await bridge.sendCommand('search_assets', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 12. save_asset ───
  server.registerTool(
    'blueprint_save_asset',
    {
      description: 'Save a specific asset to disk.',
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

  // ─── 13. get_asset_info ───
  server.registerTool(
    'blueprint_get_asset_info',
    {
      description: 'Get detailed information about an asset: class, parent class, disk size.',
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

  // ═══════════════════════════════════════════════════════════
  // Phase 5 — 蓝图结构查询与操作工具
  // ═══════════════════════════════════════════════════════════

  const targetSchemaFields = {
    target_blueprint: z.string().optional()
      .describe('Blueprint asset path. Omit to use the active blueprint.'),
    target_graph: z.string().optional()
      .describe('Graph name. Omit to use the active/default graph.'),
  };

  // ─── 14. list_graphs ───
  server.registerTool(
    'blueprint_list_graphs',
    {
      description: 'List all graphs (EventGraph, functions, macros) in a blueprint with their types and node counts.',
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

  // ─── 15. list_variables ───
  server.registerTool(
    'blueprint_list_variables',
    {
      description: 'List all member variables in a blueprint with type info, defaults, and categories.',
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

  // ─── 16. list_event_dispatchers ───
  server.registerTool(
    'blueprint_list_event_dispatchers',
    {
      description: 'List all event dispatchers in a blueprint with their parameter signatures.',
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

  // ─── 17. add_variable ───
  server.registerTool(
    'blueprint_add_variable',
    {
      description: 'Add a member variable to a blueprint. Supports type, default value, category, and flags.',
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

  // ─── 18. remove_variable ───
  server.registerTool(
    'blueprint_remove_variable',
    {
      description: 'Remove a member variable from a blueprint by name. Idempotent — succeeds if variable does not exist.',
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

  // ─── 19. add_graph ───
  server.registerTool(
    'blueprint_add_graph',
    {
      description: 'Add a new function or macro graph to a blueprint. Supports inputs, outputs, and pure flag.',
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

  // ─── 20. remove_graph ───
  server.registerTool(
    'blueprint_remove_graph',
    {
      description: 'Remove a function or macro graph from a blueprint. Cannot remove EventGraph. Idempotent.',
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

  // ─── 21. add_event_dispatcher ───
  server.registerTool(
    'blueprint_add_event_dispatcher',
    {
      description: 'Add an event dispatcher to a blueprint with optional typed parameters.',
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

  // ─── 22. delete_nodes ───
  server.registerTool(
    'blueprint_delete_nodes',
    {
      description: 'Delete specific nodes from a blueprint graph by their IDs (Node_0, Node_1, etc.). Cannot delete FunctionEntry/FunctionResult nodes.',
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

  // ═══════════════════════════════════════════════════════════
  // Phase 6 — UMG Widget 操作工具
  // ═══════════════════════════════════════════════════════════

  const widgetAssetPath = z.string()
    .describe('WidgetBlueprint asset path, e.g. /Game/UI/WBP_Main.WBP_Main');

  // ─── 23. get_widget_tree ───
  server.registerTool(
    'blueprint_get_widget_tree',
    {
      description: 'Get the full widget tree of a WidgetBlueprint, showing hierarchy, types, and slot info.',
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

  // ─── 24. add_widget ───
  server.registerTool(
    'blueprint_add_widget',
    {
      description: 'Add a new widget to a WidgetBlueprint. Specify parent panel and widget class (e.g. TextBlock, Button, CanvasPanel).',
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

  // ─── 25. remove_widget ───
  server.registerTool(
    'blueprint_remove_widget',
    {
      description: 'Remove a widget (and its subtree) from a WidgetBlueprint by name.',
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

  // ─── 26. move_widget ───
  server.registerTool(
    'blueprint_move_widget',
    {
      description: 'Move a widget to a different parent panel within the same WidgetBlueprint.',
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

  // ─── 27. get_widget_properties ───
  server.registerTool(
    'blueprint_get_widget_properties',
    {
      description: 'Get all editable properties of a widget with their current values and types.',
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

  // ─── 28. set_widget_property ───
  server.registerTool(
    'blueprint_set_widget_property',
    {
      description: 'Set a property value on a widget using text import format. Use get_widget_properties to discover available properties first.',
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

  // ═══════════════════════════════════════════════════════════
  // Phase 7 — DataAsset & DataTable 操作 (Tools 29–34)
  // ═══════════════════════════════════════════════════════════

  const objectAssetPath = z.string()
    .describe('Asset path to any UObject (DataAsset, DataTable, etc.), e.g. /Game/Data/DA_Example.DA_Example');

  const dataTableAssetPath = z.string()
    .describe('DataTable asset path, e.g. /Game/Data/DT_Items.DT_Items');

  // ─── 29. get_object_properties ───
  server.registerTool(
    'blueprint_get_object_properties',
    {
      description: 'Get all editable UPROPERTY fields of any asset (DataAsset, etc.) with their current values, types, and categories via FProperty reflection.',
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

  // ─── 30. set_object_property ───
  server.registerTool(
    'blueprint_set_object_property',
    {
      description: 'Set a single UPROPERTY value on any asset using UE text import format. Use get_object_properties to discover available properties first.',
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

  // ─── 31. get_datatable_rows ───
  server.registerTool(
    'blueprint_get_datatable_rows',
    {
      description: 'Get all rows (or specific rows) from a DataTable with column schema and field values.',
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

  // ─── 32. add_datatable_row ───
  server.registerTool(
    'blueprint_add_datatable_row',
    {
      description: 'Add a new row to a DataTable. Fields are key-value pairs matching the row struct properties.',
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

  // ─── 33. update_datatable_row ───
  server.registerTool(
    'blueprint_update_datatable_row',
    {
      description: 'Update fields on an existing DataTable row. Only specified fields are modified.',
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

  // ─── 34. delete_datatable_row ───
  server.registerTool(
    'blueprint_delete_datatable_row',
    {
      description: 'Delete a row from a DataTable by name.',
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

  // ═══════════════════════════════════════════════
  // Phase 8: 编辑器命令 (6 tools)
  // ═══════════════════════════════════════════════

  // ─── 35. undo ───
  server.registerTool(
    'blueprint_undo',
    {
      description: 'Undo the last editor action.',
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

  // ─── 36. redo ───
  server.registerTool(
    'blueprint_redo',
    {
      description: 'Redo the last undone editor action.',
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

  // ─── 37. play_in_editor ───
  server.registerTool(
    'blueprint_play_in_editor',
    {
      description: 'Start a Play In Editor (PIE) session.',
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

  // ─── 38. stop_pie ───
  server.registerTool(
    'blueprint_stop_pie',
    {
      description: 'Stop the currently running PIE session.',
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

  // ─── 39. create_blueprint ───
  server.registerTool(
    'blueprint_create_blueprint',
    {
      description: 'Create a new Blueprint asset in the project.',
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

  // ─── 40. exec_console_command ───
  server.registerTool(
    'blueprint_exec_console_command',
    {
      description: 'Execute an Unreal Editor console command and return its output.',
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

  // ═══════════════════════════════════════════════════════════
  // Editor Lifecycle Tools (41-43)
  // ═══════════════════════════════════════════════════════════

  // ─── 41. close_editor ───
  server.registerTool(
    'blueprint_close_editor',
    {
      description:
        'Save all dirty assets and close the Unreal Editor. The editor will exit on the next frame after the response is sent. Use this before building to avoid LiveCoding duplicate-class issues.',
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

  // ─── 42. build_project ───
  server.registerTool(
    'blueprint_build_project',
    {
      description:
        'Build the Unreal project using UnrealBuildTool. The editor must be closed first. Returns build output. Requires UE_ENGINE_DIR and UE_PROJECT_FILE env vars.',
      inputSchema: z.object({
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
    async ({ target, configuration, platform }) => {
      if (!config.ueEngineDir || !config.ueProjectFile) {
        return toErrorResult(
          new Error(
            'UE_ENGINE_DIR and UE_PROJECT_FILE environment variables must be set for build_project.',
          ),
        );
      }

      const buildBat = path.join(config.ueEngineDir, 'Engine', 'Build', 'BatchFiles', 'Build.bat');
      const projectName = path.basename(config.ueProjectFile, '.uproject');
      const buildTarget = target ?? `${projectName}Editor`;
      const buildConfig = configuration ?? 'Development';
      const buildPlatform = platform ?? 'Win64';

      return new Promise((resolve) => {
        console.error(
          `[BlueprintHelper MCP] Building: ${buildBat} ${buildTarget} ${buildPlatform} ${buildConfig} "${config.ueProjectFile}" -WaitMutex`,
        );

        execFile(
          buildBat,
          [buildTarget, buildPlatform, buildConfig, config.ueProjectFile, '-WaitMutex'],
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

  // ─── 43. open_editor ───
  server.registerTool(
  'blueprint_open_editor',
  {
    description:
      'Launch Unreal Editor for the current project by opening its .uproject file, then wait for the BlueprintHelper Bridge server to become available. Requires UE_ENGINE_DIR and UE_PROJECT_FILE env vars. UE_PROJECT_FILE must point to the current project .uproject file, not only a project directory.',
    inputSchema: z.object({
      wait_timeout_ms: z
        .number()
        .optional()
        .describe('Max time in ms to wait for the editor Bridge to become available (default 120000)'),
    }),
  },
  async ({ wait_timeout_ms }) => {
    if (!config.ueEngineDir || !config.ueProjectFile) {
      return toErrorResult(
        new Error(
          [
            'UE_ENGINE_DIR and UE_PROJECT_FILE environment variables must be set for blueprint_open_editor.',
            'UE_PROJECT_FILE must point to the current Unreal project .uproject file.',
            'Agent instruction: resolve the current project path, locate the .uproject file, set UE_PROJECT_FILE to that absolute path, then call blueprint_open_editor again.',
          ].join('\n'),
        ),
      );
    }

    const uprojectFile = path.resolve(config.ueProjectFile);

    if (path.extname(uprojectFile).toLowerCase() !== '.uproject') {
      return {
        content: [
          {
            type: 'text' as const,
            text: JSON.stringify(
              {
                success: false,
                code: 'UE_PROJECT_FILE_NOT_UPROJECT',
                message:
                  'UE_PROJECT_FILE must point to a .uproject file for the current Unreal project.',
                received_ue_project_file: config.ueProjectFile,
                resolved_project_file: uprojectFile,
                agent_instruction:
                  'Resolve the current project path, find the matching .uproject file, set UE_PROJECT_FILE to that absolute .uproject path, then call blueprint_open_editor again.',
              },
              null,
              2,
            ),
          },
        ],
        isError: true,
      };
    }

    const editorExe = path.join(
      config.ueEngineDir,
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
      // 启动当前项目的 .uproject，而不是打开裸 Unreal Editor。
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
                  'Verify UE_ENGINE_DIR and UE_PROJECT_FILE. UE_PROJECT_FILE must be the absolute path to the current project .uproject.',
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

    // 轮询 Bridge 直到可用。
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
