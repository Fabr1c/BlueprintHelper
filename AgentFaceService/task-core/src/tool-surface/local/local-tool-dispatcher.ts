import { failureResult, type ToolResultBase } from '../../result/tool-result.js';
import type { BlueprintHelperToolContext } from '../types.js';
import { readAgentGuide } from './agent-guide-handler.js';
import { buildProject } from './build-project-handler.js';
import { readStaticDiagnostics } from './diagnostics-handler.js';
import { closeEditor } from './editor/close-editor-handler.js';
import { openEditor } from './editor/open-editor-handler.js';
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
      return openEditor(input, context);
    case 'blueprint_close_editor':
      return closeEditor(input, context);
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
