import { z } from 'zod';
import { FindAssetsInputSchema } from './asset-discovery-schema.js';
import { ReadFunctionChainContextInputSchema } from './function-chain-context-schema.js';
import { ReadContextCapabilitiesInputSchema } from './read-context/read-context-capabilities.js';
import { ReadContextInputSchema } from './read-context/read-context-schemas.js';
import { CaptureScreenshotInputSchema } from './screenshot/capture-screenshot-schema.js';

const DebugCaseInputSchema = z.object({
  debug_case_id: z.string().min(1),
});

const DebugCaseListInputSchema = z.object({
  limit: z.number().int().positive().max(200).optional(),
});

const ReviewRecordQueryInputSchema = z.object({
  archive_session_id: z.string().min(1).optional(),
  asset_path: z.string().min(1).optional(),
  task_run_id: z.string().min(1).optional(),
  pending_only: z.boolean().optional(),
});

const ReviewActionInputSchema = z.object({
  review_record_id: z.string().min(1),
  action: z.enum(['accept', 'reject']),
  target_keys: z.array(z.string().min(1)).optional(),
});

const SourceControlInputSchema = z.object({
  asset_paths: z.array(z.string().min(1)).optional(),
  package_names: z.array(z.string().min(1)).optional(),
  file_paths: z.array(z.string().min(1)).optional(),
  update_status: z.boolean().optional(),
}).refine(
  (value) =>
    (value.asset_paths?.length ?? 0) > 0 ||
    (value.package_names?.length ?? 0) > 0 ||
    (value.file_paths?.length ?? 0) > 0,
  {
    message: 'At least one of asset_paths, package_names, or file_paths is required.',
  },
);

const CompileBlueprintInputSchema = z.object({
  target_blueprint: z.string().min(1).optional(),
});

const SaveAssetInputSchema = z.object({
  asset_path: z.string().min(1),
});

export type BridgeToolHandlerId =
  | 'generic_bridge'
  | 'read_context'
  | 'read_context_capabilities'
  | 'write_session'
  | 'capture_screenshot';

export interface BridgeToolDescriptor {
  readonly tool_name: string;
  readonly bridge_command?: string;
  readonly schema?: z.ZodTypeAny;
  readonly handler_id: BridgeToolHandlerId;
  readonly allow_cli_bridge_call?: boolean;
}

export interface CliBridgeCallCommandDescriptor {
  readonly bridge_command: string;
  readonly allow_cli_bridge_call: true;
}

export const BRIDGE_TOOL_DESCRIPTORS: readonly BridgeToolDescriptor[] = [
  { tool_name: 'blueprinthelper_read_context', schema: ReadContextInputSchema, handler_id: 'read_context' },
  { tool_name: 'blueprinthelper_read_context_capabilities', schema: ReadContextCapabilitiesInputSchema, handler_id: 'read_context_capabilities' },
  { tool_name: 'blueprinthelper_request_write_session', bridge_command: 'request_write_session', handler_id: 'write_session' },
  { tool_name: 'blueprinthelper_capture_screenshot', schema: CaptureScreenshotInputSchema, handler_id: 'capture_screenshot' },
  { tool_name: 'blueprinthelper_get_debug_case', bridge_command: 'get_debug_case', schema: DebugCaseInputSchema, handler_id: 'generic_bridge', allow_cli_bridge_call: true },
  { tool_name: 'blueprinthelper_list_debug_cases', bridge_command: 'list_debug_cases', schema: DebugCaseListInputSchema, handler_id: 'generic_bridge', allow_cli_bridge_call: true },
  { tool_name: 'blueprinthelper_export_debug_bundle', bridge_command: 'export_debug_bundle', schema: DebugCaseInputSchema, handler_id: 'generic_bridge', allow_cli_bridge_call: true },
  { tool_name: 'blueprinthelper_query_review_records', bridge_command: 'query_review_records', schema: ReviewRecordQueryInputSchema, handler_id: 'generic_bridge' },
  { tool_name: 'blueprinthelper_apply_review_action', bridge_command: 'apply_review_action', schema: ReviewActionInputSchema, handler_id: 'generic_bridge' },
  { tool_name: 'blueprinthelper_read_function_chain_context', bridge_command: 'read_function_chain_context', schema: ReadFunctionChainContextInputSchema, handler_id: 'generic_bridge' },
  { tool_name: 'blueprinthelper_find_assets', bridge_command: 'find_assets', schema: FindAssetsInputSchema, handler_id: 'generic_bridge' },
  { tool_name: 'blueprinthelper_source_control_status', bridge_command: 'source_control_status', schema: SourceControlInputSchema, handler_id: 'generic_bridge' },
  { tool_name: 'blueprinthelper_source_control_checkout', bridge_command: 'source_control_checkout', schema: SourceControlInputSchema, handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_get_runtime_profile', bridge_command: 'get_runtime_profile', handler_id: 'generic_bridge', allow_cli_bridge_call: true },
  { tool_name: 'blueprinthelper_diagnostics_runtime', bridge_command: 'diagnostics_runtime', handler_id: 'generic_bridge', allow_cli_bridge_call: true },
  { tool_name: 'blueprint_get_editor_context', bridge_command: 'get_editor_context', handler_id: 'generic_bridge', allow_cli_bridge_call: true },
  { tool_name: 'blueprint_create_asset', bridge_command: 'create_asset', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_read_components', bridge_command: 'read_components', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_add_component', bridge_command: 'add_component', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_set_component_property', bridge_command: 'set_component_property', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_set_component_properties', bridge_command: 'set_component_properties', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_remove_component', bridge_command: 'remove_component', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_validate_json', bridge_command: 'validate_json', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_export_to_json', bridge_command: 'export_to_json', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_get_logic', bridge_command: 'export_logic', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_get_logic_json', bridge_command: 'export_logic', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_import_json_to_graph', bridge_command: 'import_json', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_import_agent_graph', bridge_command: 'import_agent_graph', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_compile_blueprint', bridge_command: 'compile_blueprint', schema: CompileBlueprintInputSchema, handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_open_asset', bridge_command: 'open_asset', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_save_asset', bridge_command: 'save_asset', schema: SaveAssetInputSchema, handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_get_asset_info', bridge_command: 'get_asset_info', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_list_graphs', bridge_command: 'list_graphs', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_list_variables', bridge_command: 'list_variables', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_list_event_dispatchers', bridge_command: 'list_event_dispatchers', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_add_variable', bridge_command: 'add_variable', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_remove_variable', bridge_command: 'remove_variable', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_add_graph', bridge_command: 'add_graph', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_remove_graph', bridge_command: 'remove_graph', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_add_event_dispatcher', bridge_command: 'add_event_dispatcher', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_delete_nodes', bridge_command: 'delete_nodes', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_get_widget_tree', bridge_command: 'get_widget_tree', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_add_widget', bridge_command: 'add_widget', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_remove_widget', bridge_command: 'remove_widget', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_move_widget', bridge_command: 'move_widget', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_get_widget_properties', bridge_command: 'get_widget_properties', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_set_widget_property', bridge_command: 'set_widget_property', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_get_object_properties', bridge_command: 'get_object_properties', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_set_object_property', bridge_command: 'set_object_property', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_get_datatable_rows', bridge_command: 'get_datatable_rows', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_add_datatable_row', bridge_command: 'add_datatable_row', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_update_datatable_row', bridge_command: 'update_datatable_row', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_delete_datatable_row', bridge_command: 'delete_datatable_row', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_undo', bridge_command: 'undo', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_redo', bridge_command: 'redo', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_play_in_editor', bridge_command: 'play_in_editor', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_stop_pie', bridge_command: 'stop_pie', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_create_blueprint', bridge_command: 'create_blueprint', handler_id: 'generic_bridge' },
  { tool_name: 'blueprint_exec_console_command', bridge_command: 'exec_console_command', handler_id: 'generic_bridge' },
];

export const CLI_BRIDGE_CALL_COMMAND_DESCRIPTORS: readonly CliBridgeCallCommandDescriptor[] = [
  { bridge_command: 'read_reference_context', allow_cli_bridge_call: true },
  { bridge_command: 'get_task_run_journal', allow_cli_bridge_call: true },
];

export function getBridgeToolDescriptor(toolName: string): BridgeToolDescriptor | undefined {
  return BRIDGE_TOOL_DESCRIPTORS.find((descriptor) => descriptor.tool_name === toolName);
}

export function isCliBridgeCallAllowed(bridgeCommand: string): boolean {
  return BRIDGE_TOOL_DESCRIPTORS.some((descriptor) =>
    descriptor.bridge_command === bridgeCommand && descriptor.allow_cli_bridge_call === true)
    || CLI_BRIDGE_CALL_COMMAND_DESCRIPTORS.some((descriptor) =>
      descriptor.bridge_command === bridgeCommand && descriptor.allow_cli_bridge_call === true);
}
