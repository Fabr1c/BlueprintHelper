import type { ToolAudience, ToolRisk } from '../types.js';

export type ToolMeta = {
  name: string;
  description: string;
  audience: ToolAudience;
  risk: ToolRisk;
  requiresExpert?: boolean;
};

export const toolMetas: ToolMeta[] = [
  { name: 'blueprinthelper_read_task_context', description: 'Read a compact TaskContextPack.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_read_reference_context', description: 'Read compact ReferenceContextPack.v1 impact context.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_preview_task', description: 'Validate and preview a BlueprintHelper.TaskSpec.v1.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_execute_task', description: 'Execute a BlueprintHelper.TaskSpec.v1 after preview.', audience: 'default', risk: 'high' },
  { name: 'blueprinthelper_get_task_result', description: 'Read a TaskRunJournal by task_run_id.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_read_agent_guide', description: 'Read the AgentGuide onboarding index.', audience: 'default', risk: 'none' },
  { name: 'blueprinthelper_get_debug_case', description: 'Read a summary-only DebugCase by id.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_list_debug_cases', description: 'List summary-only DebugCases.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_export_debug_bundle', description: 'Export a local DebugBundle manifest for a DebugCase.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_query_review_records', description: 'Query summary ReviewRecords by asset, task run, or pending state.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_apply_review_action', description: 'Accept or reject ReviewRecord targets through the Review action service.', audience: 'expert', risk: 'high', requiresExpert: true },
  { name: 'blueprinthelper_read_function_chain_context', description: 'Read compact project custom function/event call-chain references from a Blueprint entry.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_read_context', description: 'Read UE asset context through ReadSpec.', audience: 'default', risk: 'low' },
  { name: 'blueprint_get_runtime_profile', description: 'Read the BlueprintHelper runtime profile.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_request_write_session', description: 'Request Editor-approved write permission.', audience: 'default', risk: 'medium' },
  { name: 'blueprinthelper_diagnostics', description: 'Run static diagnostics.', audience: 'default', risk: 'none' },
  { name: 'blueprinthelper_diagnostics_runtime', description: 'Run runtime diagnostics through the Bridge.', audience: 'default', risk: 'low' },
  { name: 'blueprint_open_editor', description: 'Open the editor from the current project directory or an explicit project_file.', audience: 'compat', risk: 'medium' },
  { name: 'blueprint_close_editor', description: 'Close the current editor process and return lifecycle status.', audience: 'compat', risk: 'medium' },
];
