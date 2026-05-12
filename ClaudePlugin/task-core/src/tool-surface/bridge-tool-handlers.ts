import { z } from 'zod';
import {
  failureResult,
  normalizeToolResult,
  successRead,
  type ToolResultBase,
} from '../result/tool-result.js';
import type { BridgeResponse } from '../bridge/bridge-client.js';
import type { BlueprintHelperToolContext } from './types.js';

const ReadContextInputSchema = z.object({
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

type ReadContextInput = z.infer<typeof ReadContextInputSchema>;

export const bridgeCommandByToolName: Record<string, string> = {
  blueprinthelper_get_debug_case: 'get_debug_case',
  blueprint_get_runtime_profile: 'get_runtime_profile',
  blueprinthelper_request_write_session: 'request_write_session',
  blueprinthelper_diagnostics_runtime: 'diagnostics_runtime',
  blueprint_get_editor_context: 'get_editor_context',
  blueprint_get_logic_md: 'read_blueprint_logic_md',
  blueprint_create_asset: 'create_asset',
  blueprint_read_components: 'read_components',
  blueprint_add_component: 'add_component',
  blueprint_set_component_property: 'set_component_property',
  blueprint_set_component_properties: 'set_component_properties',
  blueprint_remove_component: 'remove_component',
  blueprint_validate_json: 'validate_json',
  blueprint_export_to_json: 'export_to_json',
  blueprint_get_logic: 'export_logic',
  blueprint_get_logic_json: 'export_logic',
  blueprint_import_json_to_graph: 'import_json',
  blueprint_import_agent_graph: 'import_agent_graph',
  blueprint_compile_blueprint: 'compile_blueprint',
  blueprint_open_asset: 'open_asset',
  blueprint_list_assets: 'list_assets',
  blueprint_search_assets: 'search_assets',
  blueprint_save_asset: 'save_asset',
  blueprint_get_asset_info: 'get_asset_info',
  blueprint_list_graphs: 'list_graphs',
  blueprint_list_variables: 'list_variables',
  blueprint_list_event_dispatchers: 'list_event_dispatchers',
  blueprint_add_variable: 'add_variable',
  blueprint_remove_variable: 'remove_variable',
  blueprint_add_graph: 'add_graph',
  blueprint_remove_graph: 'remove_graph',
  blueprint_add_event_dispatcher: 'add_event_dispatcher',
  blueprint_delete_nodes: 'delete_nodes',
  blueprint_get_widget_tree: 'get_widget_tree',
  blueprint_add_widget: 'add_widget',
  blueprint_remove_widget: 'remove_widget',
  blueprint_move_widget: 'move_widget',
  blueprint_get_widget_properties: 'get_widget_properties',
  blueprint_set_widget_property: 'set_widget_property',
  blueprint_get_object_properties: 'get_object_properties',
  blueprint_set_object_property: 'set_object_property',
  blueprint_get_datatable_rows: 'get_datatable_rows',
  blueprint_add_datatable_row: 'add_datatable_row',
  blueprint_update_datatable_row: 'update_datatable_row',
  blueprint_delete_datatable_row: 'delete_datatable_row',
  blueprint_undo: 'undo',
  blueprint_redo: 'redo',
  blueprint_play_in_editor: 'play_in_editor',
  blueprint_stop_pie: 'stop_pie',
  blueprint_create_blueprint: 'create_blueprint',
  blueprint_exec_console_command: 'exec_console_command',
  blueprint_close_editor: 'close_editor',
};

export const bridgeToolSchemas: Record<string, z.ZodTypeAny> = {
  blueprinthelper_read_context: ReadContextInputSchema,
};

export async function executeBridgeTool(
  toolName: string,
  input: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Promise<ToolResultBase> {
  if (toolName === 'blueprinthelper_read_context') {
    return executeReadContext(input, context);
  }

  const bridgeCommand = bridgeCommandByToolName[toolName];
  if (!bridgeCommand) {
    return failureResult(toolName, {
      code: 'bridge_tool_not_mapped',
      stage: 'parse_input',
      message: `No Bridge command mapping for ${toolName}.`,
      retryable: false,
      rollback_result: 'not_needed',
    });
  }

  const payload = normalizeBridgePayload(toolName, input);
  const response = await context.bridge.sendCommand(bridgeCommand, payload);
  if (toolName === 'blueprinthelper_request_write_session' && response.success) {
    const sessionId = extractWriteSessionId(response);
    if (sessionId) {
      context.bridge.setWriteSessionId(sessionId);
    }
    return sanitizedWriteSessionResult(response, payload);
  }
  return normalizeBridgeToolResult(toolName, response);
}

async function executeReadContext(
  rawInput: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Promise<ToolResultBase> {
  const input = ReadContextInputSchema.parse(rawInput);
  if (input.read_type !== 'blueprint_logic') {
    return failureResult('read_context', {
      code: 'unsupported_read_type',
      stage: 'parse_input',
      message: `read_context currently supports blueprint_logic only, got ${input.read_type}.`,
      retryable: true,
      rollback_result: 'not_needed',
    });
  }

  const format = input.view?.format ?? 'logic_md';
  if (format === 'schema') {
    return successRead('read_context', buildReadContextTarget(input), {
      schema: 'ReadContextPack.v1',
      read_type: input.read_type,
      format,
      payload: {
        schema: 'BlueprintLogicReadSchema.v1',
        read_type: 'blueprint_logic',
        target_types: ['blueprint', 'graph', 'function', 'event', 'custom_event', 'block'],
        formats: ['logic_md', 'logic_json', 'summary', 'schema'],
      },
      stats: {},
      truncated: false,
    }) as ToolResultBase;
  }

  const bridgeFormat = format === 'logic_json' || format === 'summary' || isTargetEntryLogicRead(input)
    ? 'logic_json'
    : 'logic_md';
  const response = await context.bridge.sendCommand(
    bridgeFormat === 'logic_json' ? 'read_blueprint_logic_json' : 'read_blueprint_logic_md',
    buildBlueprintLogicReadPayload(input),
  );
  if (!response.success) {
    return normalizeBridgeToolResult('read_context', response);
  }

  const payload = extractBridgePayload(response.result);
  return successRead('read_context', buildReadContextTarget(input), {
    schema: 'ReadContextPack.v1',
    read_type: input.read_type,
    format,
    scope: payload['scope'] ?? inferBlueprintLogicScope(input.target.target_type),
    payload,
    stats: isRecord(payload['stats']) ? payload['stats'] : {},
    truncated: false,
  }) as ToolResultBase;
}

function normalizeBridgePayload(toolName: string, input: Record<string, unknown>): Record<string, unknown> {
  if (toolName === 'blueprint_get_logic') {
    return { format: 'logic_md', ...input };
  }
  if (toolName === 'blueprint_get_logic_json') {
    return { format: 'logic_json', ...input };
  }
  return input;
}

function normalizeBridgeToolResult(toolName: string, response: BridgeResponse): ToolResultBase {
  if (isToolResultBase(response.result)) {
    return response.result;
  }
  return normalizeToolResult(response, toolName);
}

function extractWriteSessionId(response: BridgeResponse): string | undefined {
  const result = isRecord(response.result) ? response.result : undefined;
  const writeSession = isRecord(result?.['write_session']) ? result['write_session'] : undefined;
  const sessionId = writeSession?.['session_id'];
  return typeof sessionId === 'string' && sessionId.length > 0 ? sessionId : undefined;
}

function sanitizedWriteSessionResult(
  response: BridgeResponse,
  payload: Record<string, unknown>,
): ToolResultBase {
  if (!response.success) {
    return normalizeBridgeToolResult('blueprinthelper_request_write_session', response);
  }
  const result = isRecord(response.result) ? response.result : {};
  const writeSession = isRecord(result['write_session']) ? result['write_session'] : {};
  const sanitizedSession: Record<string, unknown> = {
    scope: writeSession['scope'] ?? payload['scope'] ?? 'project',
    expires_at_utc: writeSession['expires_at_utc'],
  };
  if (Array.isArray(writeSession['asset_paths'])) {
    sanitizedSession['asset_paths'] = writeSession['asset_paths'];
  }
  return successRead('blueprinthelper_request_write_session', { target_type: 'asset' }, {
    schema: 'WriteSession.v1',
    write_session: sanitizedSession,
  }) as ToolResultBase;
}

function isToolResultBase(value: unknown): value is ToolResultBase {
  return isRecord(value)
    && typeof value['ok'] === 'boolean'
    && typeof value['schema'] === 'string'
    && typeof value['operation'] === 'string'
    && typeof value['status'] === 'string';
}

function isTargetEntryLogicRead(input: ReadContextInput): boolean {
  const targetType = input.target.target_type;
  return targetType === 'function' || targetType === 'event' || targetType === 'custom_event';
}

function buildReadContextTarget(input: ReadContextInput) {
  return omitUndefined({
    asset_path: input.target.asset_path,
    asset_type: input.target.asset_type,
    target_type: input.target.target_type,
    target_name: input.target.target_name,
    block_id: input.target.block_id,
  }) as never;
}

function buildBlueprintLogicReadPayload(input: ReadContextInput): Record<string, unknown> {
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

function inferBlueprintLogicScope(targetType: string): string {
  if (targetType === 'blueprint' || targetType === 'asset') return 'blueprint';
  if (targetType === 'function') return 'target_function';
  if (targetType === 'event') return 'target_event';
  if (targetType === 'custom_event') return 'target_custom_event';
  return 'target_graph';
}

function extractBridgePayload(result: unknown): Record<string, unknown> {
  if (!isRecord(result)) {
    return {};
  }
  const normalized = isRecord(result['data']) ? result['data'] : result;
  return isRecord(normalized) ? normalized : {};
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function omitUndefined(record: Record<string, unknown>): Record<string, unknown> {
  return Object.fromEntries(Object.entries(record).filter(([, value]) => value !== undefined));
}
