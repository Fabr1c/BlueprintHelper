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

const MANIFEST_HELP_OVERRIDES: Record<string, ManifestHelpOverride> = {};

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
    key: 'bridge ping',
    summary: 'Ping the running Editor Bridge.',
    usage: ['bh bridge ping --select status,summary'],
    input: ['No JSON input.'],
    notes: ['Use diagnostics commands for richer setup or runtime checks.'],
  },
  {
    key: 'bridge call',
    summary: 'Expert/debug-only raw Bridge command tunnel for a narrow allowlist.',
    usage: ['bh bridge call --command <read_only_command> --expert --select status,artifacts.full_result'],
    input: ['No JSON payload. Pass the Bridge command with --command.'],
    notes: ['Ordinary Agent workflows must use grouped commands, named tools, and templates instead of bridge call.'],
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
  return MANIFEST_HELP_OVERRIDES[manifest.tool_name]?.usage ?? [...manifest.help_usage];
}

export function manifestSpecificNotes(manifest: ToolCommandManifest): string[] {
  return MANIFEST_HELP_OVERRIDES[manifest.tool_name]?.notes ?? [...manifest.help_notes];
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
