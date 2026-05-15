import { z } from 'zod';
import {
  bridgeCommandByToolName,
  bridgeToolSchemas,
  executeBridgeTool,
} from './bridge-tool-handlers.js';
import { executeLocalTool, localToolNames } from './local-tool-handlers.js';
import { executeTaskTool, taskToolSchemas } from './task-tool-handlers.js';
import type {
  BlueprintHelperToolDefinition,
  ToolAudience,
  ToolRisk,
} from './types.js';

type ToolMeta = {
  name: string;
  description: string;
  audience: ToolAudience;
  risk: ToolRisk;
  requiresExpert?: boolean;
};

const toolMetas: ToolMeta[] = [
  { name: 'blueprinthelper_read_task_context', description: 'Read a compact TaskContextPack.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_read_reference_context', description: 'Read compact reference impact context.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_preview_task', description: 'Validate and preview a BlueprintHelper.TaskSpec.v1.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_execute_task', description: 'Execute a BlueprintHelper.TaskSpec.v1 after preview.', audience: 'default', risk: 'high' },
  { name: 'blueprinthelper_get_task_result', description: 'Read a TaskRunJournal by task_run_id.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_read_agent_guide', description: 'Read the AgentGuide onboarding index.', audience: 'default', risk: 'none' },
  { name: 'blueprinthelper_get_debug_case', description: 'Read a summary-only DebugCase by id.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_list_debug_cases', description: 'List summary-only DebugCases.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_export_debug_bundle', description: 'Export a local DebugBundle manifest for a DebugCase.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_query_review_records', description: 'Query summary ReviewRecords by asset, task run, or pending state.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_read_context', description: 'Read UE asset context through ReadSpec.', audience: 'default', risk: 'low' },
  { name: 'blueprint_get_runtime_profile', description: 'Read the BlueprintHelper runtime profile.', audience: 'default', risk: 'low' },
  { name: 'blueprinthelper_request_write_session', description: 'Request Editor-approved write permission.', audience: 'default', risk: 'medium' },
  { name: 'blueprinthelper_diagnostics', description: 'Run static diagnostics.', audience: 'default', risk: 'none' },
  { name: 'blueprinthelper_diagnostics_runtime', description: 'Run runtime diagnostics through the Bridge.', audience: 'default', risk: 'low' },
  { name: 'blueprint_open_editor', description: 'Open the editor from the current project directory or an explicit project_file.', audience: 'compat', risk: 'medium' },
  { name: 'blueprint_close_editor', description: 'Close the current editor process and return lifecycle status.', audience: 'compat', risk: 'medium' },
];

export function getBlueprintHelperToolRegistry(): BlueprintHelperToolDefinition[] {
  return toolMetas.map((meta) => {
    const taskSchema = taskToolSchemas[meta.name as keyof typeof taskToolSchemas];
    const bridgeSchema = bridgeToolSchemas[meta.name];
    const isBridgeTool = meta.name in bridgeCommandByToolName || bridgeSchema !== undefined;
    const isLocalTool = localToolNames.has(meta.name);
    return {
      ...meta,
      inputSchema: taskSchema ?? bridgeSchema ?? z.record(z.unknown()),
      execute: async (input, context) => {
        if (taskSchema) {
          return executeTaskTool(meta.name as keyof typeof taskToolSchemas, input, context);
        }
        if (isLocalTool) {
          return executeLocalTool(meta.name, input, context);
        }
        if (isBridgeTool) {
          return executeBridgeTool(meta.name, input, context);
        }
        throw new Error(`Tool is registered without a handler: ${meta.name}`);
      },
    };
  });
}

export function getBlueprintHelperTool(name: string): BlueprintHelperToolDefinition | undefined {
  return getBlueprintHelperToolRegistry().find((tool) => tool.name === name);
}
