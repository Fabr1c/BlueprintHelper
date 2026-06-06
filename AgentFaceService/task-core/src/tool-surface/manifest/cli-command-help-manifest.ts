import type { ToolCommandManifest } from './tool-command-manifest.js';

export type CommandHelpEntry = {
  summary: string;
  usage: string[];
  input: string[];
  templates?: string[];
  notes?: string[];
  commonOptions?: string[];
};

export type CliCommandHelpManifest = CommandHelpEntry & {
  key: string;
  title?: string;
  aliases?: string[];
};

type ManifestHelpOverride = {
  usage?: string[];
  notes?: string[];
};

const MANIFEST_HELP_OVERRIDES: Record<string, ManifestHelpOverride> = {
  blueprinthelper_capture_screenshot: {
    usage: ['bh blueprinthelper_capture_screenshot --file <capture-screenshot.json> --select status,artifacts.full_result'],
    notes: [
      'graph_name is required when block_ref or node_ref is provided.',
      'Use capture_target:auto for graph_name/block_ref/node_ref requests; graph targets capture Graph-only PNGs.',
    ],
  },
  blueprinthelper_find_assets: {
    usage: ['bh blueprinthelper_find_assets --file <find-assets.json> --select status,artifacts.full_result'],
    notes: [
      'Resolve one explicit Unreal asset_path before preview_task or any write request.',
      'Do not infer Unreal asset_path values from filesystem .uasset paths.',
    ],
  },
  blueprinthelper_read_context: {
    usage: [
      'bh blueprinthelper_read_context --file <read-spec.json> --select status,artifacts.full_result',
      '$json | bh blueprinthelper_read_context --stdin --format full',
      'bh context read --file <read-spec.json> --select status,artifacts.full_result',
    ],
  },
};

const METRICS_COMMON_OPTIONS = [
  '  --format json|markdown',
  '  --window 1d|7d|30d|all',
  '  --limit <N>',
];

const CLI_COMMAND_HELP_MANIFESTS: readonly CliCommandHelpManifest[] = [
  {
    key: 'metrics',
    summary: 'Build standalone BlueprintHelper metrics reports from the local metrics store.',
    usage: [
      'bh metrics report --window 7d --format json',
      'bh metrics top-errors --window 7d --limit 20 --format markdown',
      'bh metrics tool-usage --window 30d --limit 50 --format json',
      'bh metrics task-health --window all --limit 20 --format markdown',
    ],
    input: ['No JSON payload. Metrics root resolves from BPH_METRICS_DIR or nearest .uproject/Saved/BlueprintHelper/Metrics.'],
    notes: [
      'Markdown reports are written under metricsRoot/reports.',
      'JSON stdout returns report data; markdown stdout returns a compact result plus artifact paths.',
      'Only bh metrics ... commands emit metrics reports.',
    ],
    commonOptions: METRICS_COMMON_OPTIONS,
  },
  metricReportHelp('report', 'Build the full metrics report with all sections.'),
  metricReportHelp('top-errors', 'Build a metrics report focused on top error rows.'),
  metricReportHelp('tool-usage', 'Build a metrics report focused on tool usage rows.'),
  metricReportHelp('task-health', 'Build a metrics report focused on task health rows.'),
  {
    key: 'blueprint_open_editor',
    aliases: ['open_editor'],
    summary: 'Agent-owned Editor open is a global MCP lifecycle operation, not a normal CLI asset workflow.',
    usage: ['mcp__blueprint_helper__blueprint_open_editor'],
    input: ['Use the global MCP tool schema exposed by the host Agent environment.'],
    notes: [
      'Ordinary reads, writes, diagnostics, preview, execute, and result lookup stay on the CLI.',
      'Do not use CLI lifecycle aliases as Agent compatibility paths.',
      'CLI lifecycle invocation is blocked for Agents; report lifecycle_mcp_unavailable when the global MCP lifecycle tools are unavailable.',
    ],
  },
  {
    key: 'blueprint_close_editor',
    aliases: ['close_editor'],
    summary: 'Agent-owned Editor close is a global MCP lifecycle operation, not a normal CLI asset workflow.',
    usage: ['mcp__blueprint_helper__blueprint_close_editor'],
    input: ['Use the global MCP tool schema exposed by the host Agent environment.'],
    notes: [
      'Ordinary reads, writes, diagnostics, preview, execute, and result lookup stay on the CLI.',
      'Do not use CLI lifecycle aliases as Agent compatibility paths.',
      'CLI lifecycle invocation is blocked for Agents; report lifecycle_mcp_unavailable when the global MCP lifecycle tools are unavailable.',
    ],
  },
  {
    key: 'bridge ping',
    summary: 'Ping the running Editor Bridge.',
    usage: ['bh bridge ping --select status,summary'],
    input: ['No JSON input.'],
    notes: ['Use diagnostics commands for richer setup or runtime checks.'],
  },
  {
    key: 'bridge call',
    summary: 'Call a narrow allowlist of read-only Bridge commands.',
    usage: ['bh bridge call --command <read_only_command> --select status,artifacts.full_result'],
    input: ['No JSON payload. Pass the Bridge command with --command.'],
    notes: ['Ordinary Agent workflows should prefer named tools and templates over bridge call.'],
  },
];

export function resolveCliCommandHelpManifest(key: string): CliCommandHelpManifest | undefined {
  return CLI_COMMAND_HELP_MANIFESTS.find((entry) => (
    entry.key === key || entry.aliases?.includes(key)
  ));
}

export function globalCliCommandUsageLines(): string[] {
  return resolveCliCommandHelpManifest('metrics')?.usage ?? [];
}

export function formatManifestUsage(manifest: ToolCommandManifest): string[] {
  return MANIFEST_HELP_OVERRIDES[manifest.tool_name]?.usage ?? [...manifest.recommended_invocations];
}

export function manifestSpecificNotes(manifest: ToolCommandManifest): string[] {
  return MANIFEST_HELP_OVERRIDES[manifest.tool_name]?.notes ?? [];
}

function metricReportHelp(
  kind: 'report' | 'top-errors' | 'tool-usage' | 'task-health',
  summary: string,
): CliCommandHelpManifest {
  return {
    key: `metrics ${kind}`,
    summary,
    usage: [`bh metrics ${kind} --window 7d --limit 20 --format json`],
    input: ['No JSON payload. Metrics root resolves from BPH_METRICS_DIR or nearest .uproject/Saved/BlueprintHelper/Metrics.'],
    notes: [
      'Markdown writes the rendered report under metricsRoot/reports.',
      'Use --format json for machine-readable stdout.',
    ],
    commonOptions: METRICS_COMMON_OPTIONS,
  };
}
