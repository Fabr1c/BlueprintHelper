import { failureResult, type ToolResultBase } from '../../result/tool-result.js';
import type { BlueprintHelperToolContext } from '../types.js';
import { readAgentGuide } from './agent-guide-handler.js';
import { buildProject } from './build-project-handler.js';
import { readStaticDiagnostics } from './diagnostics-handler.js';
import type { LocalToolName } from './local-tool-names.js';

export async function executeLocalTool(
  name: string,
  input: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Promise<ToolResultBase> {
  switch (name as LocalToolName) {
    case 'blueprinthelper_read_agent_guide':
      return readAgentGuide(context);
    case 'blueprinthelper_diagnostics':
      return readStaticDiagnostics(context);
    case 'blueprint_build_project':
      return buildProject(input, context);
    case 'blueprint_open_editor':
      return lifecycleMcpRequired(name, 'mcp__blueprint_helper__blueprint_open_editor');
    case 'blueprint_close_editor':
      return lifecycleMcpRequired(name, 'mcp__blueprint_helper__blueprint_close_editor');
    default:
      return failureResult(name, {
        code: 'local_tool_not_mapped',
        stage: 'parse_input',
        message: `No local tool mapping for ${name}.`,
        retryable: false,
        rollback_result: 'not_needed',
      });
  }
}

function lifecycleMcpRequired(name: string, mcpToolName: string): ToolResultBase {
  return failureResult(name, {
    code: 'lifecycle_mcp_required',
    stage: 'parse_input',
    message: `Editor lifecycle is MCP-only for Agents. Use ${mcpToolName}; do not run bh open_editor, bh close_editor, blueprint_open_editor, or blueprint_close_editor through the CLI.`,
    retryable: false,
    rollback_result: 'not_needed',
  }, { target_type: 'asset' });
}
