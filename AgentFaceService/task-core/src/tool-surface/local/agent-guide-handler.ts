import * as fs from 'node:fs';
import {
  failureResult,
  successRead,
  type ToolResultBase,
} from '../../result/tool-result.js';
import type { BlueprintHelperToolContext } from '../types.js';
import { resolveAgentGuidePath } from './agent-guide-path.js';

export function readAgentGuide(context: BlueprintHelperToolContext): ToolResultBase {
  const guidePath = resolveAgentGuidePath(context.cwd);
  if (!guidePath) {
    return failureResult('blueprinthelper_read_agent_guide', {
      code: 'agent_guide_not_found',
      stage: 'execute',
      message: `AgentGuide not found from cwd ${context.cwd}.`,
      retryable: false,
      rollback_result: 'not_needed',
    });
  }

  try {
    const markdown = fs.readFileSync(guidePath, 'utf8');
    return successRead('blueprinthelper_read_agent_guide', { target_type: 'asset' }, {
      schema: 'AgentGuideMarkdown.v1',
      format: 'markdown',
      markdown,
    }) as ToolResultBase;
  } catch (err) {
    return failureResult('blueprinthelper_read_agent_guide', {
      code: 'agent_guide_read_failed',
      stage: 'execute',
      message: err instanceof Error ? err.message : String(err),
      retryable: false,
      rollback_result: 'not_needed',
    });
  }
}
