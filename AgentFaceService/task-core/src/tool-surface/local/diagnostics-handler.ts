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
  const projectProfilePath = path.join(context.cwd, '.blueprinthelper', 'project-profile.json');
  const legacyAgentProfilePath = path.join(context.cwd, '.blueprinthelper', 'agent-profile.json');
  const agentWorkflowPath = path.join(context.cwd, '.blueprinthelper', 'AgentWorkFlow.md');
  const agentsPath = path.join(context.cwd, 'AGENTS.md');
  const claudePath = path.join(context.cwd, 'CLAUDE.md');
  const hasProjectProfile = fs.existsSync(projectProfilePath);
  const hasLegacyAgentProfile = fs.existsSync(legacyAgentProfilePath);
  const report: DiagnosticsMarkdownReport = {
    blocking: hasProjectProfile || hasLegacyAgentProfile ? [] : [{ code: 'project_profile_missing' }],
    warnings: [
      ...(hasProjectProfile ? [] : [{ code: 'project_profile_missing_using_legacy' }]),
      ...(hasLegacyAgentProfile ? [{ code: 'legacy_agent_profile.present' }] : []),
      ...(fs.existsSync(agentWorkflowPath) ? [] : [{ code: 'agent_workflow.missing' }]),
      ...(fileContains(agentsPath, 'BEGIN BLUEPRINTHELPER CODEX') ? [] : [{ code: 'project_agents_marker.missing' }]),
      ...(fileContains(claudePath, 'BEGIN BLUEPRINTHELPER CLAUDE') ? [] : [{ code: 'project_claude_marker.missing' }]),
    ],
    info: [
      { code: 'cwd', extra: context.cwd },
      ...(hasProjectProfile ? [{ code: 'project_profile.present' }] : []),
      ...(fs.existsSync(agentWorkflowPath) ? [{ code: 'agent_workflow.present' }] : []),
      ...(fileContains(agentsPath, 'BEGIN BLUEPRINTHELPER CODEX') ? [{ code: 'project_agents_marker.present' }] : []),
      ...(fileContains(claudePath, 'BEGIN BLUEPRINTHELPER CLAUDE') ? [{ code: 'project_claude_marker.present' }] : []),
    ],
  };
  const markdown = buildDiagnosticsMarkdown(report);
  return successRead(
    'blueprinthelper_diagnostics',
    { target_type: 'asset' },
    buildDiagnosticsData('static', markdown),
  ) as ToolResultBase;
}

function fileContains(filePath: string, pattern: string): boolean {
  if (!fs.existsSync(filePath)) {
    return false;
  }
  return fs.readFileSync(filePath, 'utf8').includes(pattern);
}
