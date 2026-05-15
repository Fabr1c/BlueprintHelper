import * as fs from 'node:fs';
import * as path from 'node:path';
import {
  buildDiagnosticsData,
  buildDiagnosticsMarkdown,
  successRead,
  type DiagnosticsMarkdownReport,
  type ToolResultBase,
} from '../../result/tool-result.js';
import type { BlueprintHelperToolContext } from '../types.js';

export function readStaticDiagnostics(context: BlueprintHelperToolContext): ToolResultBase {
  const agentProfilePath = path.join(context.cwd, '.blueprinthelper', 'agent-profile.json');
  const report: DiagnosticsMarkdownReport = {
    blocking: fs.existsSync(agentProfilePath) ? [] : [{ code: 'agent_profile_missing' }],
    warnings: [],
    info: [{ code: 'cwd', extra: context.cwd }],
  };
  const markdown = buildDiagnosticsMarkdown(report);
  return successRead(
    'blueprinthelper_diagnostics',
    { target_type: 'asset' },
    buildDiagnosticsData('static', markdown),
  ) as ToolResultBase;
}
