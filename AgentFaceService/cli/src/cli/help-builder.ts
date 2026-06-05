import {
  buildReadonlyToolCommandManifestRegistry,
  createToolsTemplateBuilder,
  getBlueprintHelperTool,
  type ToolCommandManifest,
  type ToolCommandManifestRegistry,
  type ToolTemplateDispatchResult,
} from '@blueprinthelper/task-core/tool-surface/tool-registry';

export interface HelpBuilder {
  build(target?: string[]): string;
}

type StaticHelpEntry = {
  summary: string;
  usage: string[];
  input: string[];
  templates?: string[];
  notes?: string[];
  commonOptions?: string[];
};

export function createHelpBuilder(
  registry: ToolCommandManifestRegistry = buildReadonlyToolCommandManifestRegistry(),
): HelpBuilder {
  return {
    build(target: string[] = []) {
      const normalizedTarget = normalizeHelpTarget(target);
      if (normalizedTarget.length === 0) {
        return globalHelpText(registry);
      }

      const key = normalizedTarget.join(' ');
      const staticEntry = resolveStaticHelpEntry(key);
      if (staticEntry) {
        return formatEntry(resolveStaticHelpTitle(key) ?? key, staticEntry);
      }

      const manifest = registry.get(resolveLifecycleAlias(key) ?? key);
      if (manifest) {
        const templateBuilder = createToolsTemplateBuilder(registry);
        const manifests = resolveDisplayManifests(registry, key, manifest);
        const dispatches = manifests.map((entry) => templateBuilder.getTemplateDispatch(entry.tool_id));
        return formatManifestEntry(key, manifest, manifests, dispatches);
      }

      return [
        `No tool-specific help is registered for: ${key}`,
        '',
        globalHelpText(registry),
      ].join('\n');
    },
  };
}

function globalHelpText(registry: ToolCommandManifestRegistry): string {
  const defaultTools = uniqueBy(registry.list(), (manifest) => manifest.tool_name)
    .filter((manifest) => manifest.audience !== 'expert')
    .map((manifest) => manifest.tool_name)
    .sort();

  return [
    'BlueprintHelper CLI',
    '',
    'Usage:',
    '  bh <tool_name> [--file params.json | --json json | --stdin] [--format summary|json|full] [--fields path[,path...]] [--omit path[,path...]]',
    '  bh <tool_name> --help',
    '  bh task preview --file <task-spec.json> [--develop] [--format summary|json|full]',
    '  bh task execute --file <task-spec.json> [--preview-token <32-hex>] [--develop] [--format summary|json|full]',
    '  bh task result --id <task_run_id>',
    '  bh context read --file <read-spec.json>',
    '  bh bridge ping',
    '  bh bridge call --command <read_only_command>',
    '  bh tools domains --format json',
    '  bh tools list <domain> <kind> --format json',
    '  bh tools templates <tool_id> --format json',
    '  bh tools templates <tool_id> --route <route_id> --slot [--kind statement|expression|target|view|patch|merge] --format json',
    '  bh metrics report --window 7d --format json',
    '  bh metrics top-errors --window 7d --format markdown',
    '  bh metrics tool-usage --window 30d --limit 50 --format json',
    '  bh metrics task-health --window all --limit 20 --format markdown',
    '',
    'Tool and template selection:',
    '  Start with: bh tools domains --format json',
    '  Filter with: bh tools list <domain> <kind> --format json',
    '  Fetch templates with: bh tools templates <tool_id> --format json',
    '  Fill the returned template path, then run the returned recommended_invocation.',
    '  Pick a returned route_id, then fetch route slots with: bh tools templates <tool_id> --route <route_id> --slot --format json',
    '  For route-first tools, fill the returned template or slot path before running the returned recommended_invocation.',
    '',
    'Default tool names:',
    ...defaultTools.map((toolName) => {
      const tool = getBlueprintHelperTool(toolName);
      return `  ${toolName}${tool ? ` - ${tool.description}` : ''}`;
    }),
    '',
    'Editor lifecycle:',
    '  Use global MCP lifecycle tools for Agent-owned open/close:',
    '    mcp__blueprint_helper__blueprint_open_editor',
    '    mcp__blueprint_helper__blueprint_close_editor',
    '  Do not use CLI lifecycle aliases as Agent compatibility paths.',
    '  If lifecycle MCP is unavailable, report lifecycle_mcp_unavailable instead of starting or closing the editor through CLI.',
    '',
    'Notes:',
    '  --develop enables data.timing for command results.',
    '  Long UE Bridge waits emit progress hints to stderr. Stdout remains final JSON.',
    '  In PowerShell, prefer --file or --stdin for generated JSON; inline --json may lose quotes.',
  ].join('\n');
}

function formatManifestEntry(
  title: string,
  manifest: ToolCommandManifest,
  manifests: ToolCommandManifest[],
  dispatches: ToolTemplateDispatchResult[],
): string {
  return formatEntry(title, {
    summary: manifestPurpose(manifest),
    usage: uniqueStrings([
      ...formatUsage(manifest),
      ...dispatches.map((dispatch) => dispatch.recommended_invocation),
    ]),
    input: formatInputShapes(uniqueStrings(manifests.flatMap((entry) => entry.input_shapes))),
    templates: formatTemplateNavigation(manifests, dispatches),
    notes: formatManifestNotes(manifest),
  });
}

function formatEntry(name: string, entry: StaticHelpEntry): string {
  return [
    `BlueprintHelper CLI help: ${name}`,
    '',
    'Purpose:',
    `  ${entry.summary}`,
    '',
    'Usage:',
    ...entry.usage.map((line) => `  ${line}`),
    '',
    'Input:',
    ...entry.input.map((line) => `  ${line}`),
    ...formatTemplateSection(entry.templates),
    ...formatOptionalSection('Notes:', entry.notes),
    '',
    'Common options:',
    ...(entry.commonOptions ?? defaultCommonOptions()),
  ].join('\n');
}

function manifestPurpose(manifest: ToolCommandManifest): string {
  return getBlueprintHelperTool(manifest.tool_name)?.description
    ?? `${manifest.domain} ${manifest.kind} tool.`;
}

function formatInputShapes(inputShapes: ToolCommandManifest['input_shapes']): string[] {
  if (inputShapes.length === 0) {
    return ['Root JSON: tool payload object.'];
  }
  return inputShapes.map((shape) => {
    switch (shape) {
      case 'empty_object':
        return 'Root JSON: {}';
      case 'bare_taskspec':
        return 'Input file root: bare BlueprintHelper.TaskSpec.v1. Grouped command input: bare BlueprintHelper.TaskSpec.v1 file. Do not wrap it in { "task_spec": ... }.';
      case 'wrapped_taskspec_preview':
        return 'Direct tool input: { "task_spec": { "schema": "BlueprintHelper.TaskSpec.v1", ... } }.';
      case 'wrapped_taskspec_execute':
        return 'Direct tool input: { "task_spec": { "schema": "BlueprintHelper.TaskSpec.v1", ... } }.';
      case 'readspec':
        return 'Root JSON: bare BlueprintHelper.ReadSpec.v1. Do not wrap the input in args.';
      case 'bridge_payload':
        return 'Root JSON: Bridge tool payload object. Use a template path before calling the tool.';
      case 'tool_payload':
        return 'Root JSON: tool payload object. Use a template path before calling the tool.';
    }
  });
}

function formatTemplateNavigation(
  manifests: ToolCommandManifest[],
  dispatches: ToolTemplateDispatchResult[],
): string[] {
  const templatePaths = [
    ...manifests.map((manifest) => `bh tools templates ${manifest.tool_id} --format json`),
    ...dispatches.flatMap((dispatch) => dispatch.cli_invocation_templates.map((template) => template.path)),
    ...dispatches.flatMap((dispatch) => dispatch.routes.flatMap((route) => route.template_paths)),
  ];
  return uniqueStrings(templatePaths);
}

function formatManifestNotes(manifest: ToolCommandManifest): string[] {
  const notes = [
    `Capability id: ${manifest.tool_id}`,
    `Risk: ${manifest.risk}`,
  ];
  if (manifest.requires_bridge) {
    notes.push('Requires a running Editor Bridge.');
  }
  if (manifest.requires_write_session) {
    notes.push('Requires write-session authorization before write-side execution.');
  }
  if (manifest.stop_conditions.some((condition) => condition.startsWith('source_control') || condition === 'not_editable')) {
    notes.push('Stop on source-control/editability preflight failures.');
  }
  if (manifest.stop_conditions.length > 0) {
    notes.push(`Stop conditions: ${manifest.stop_conditions.join(', ')}`);
  }
  notes.push(...toolSpecificNotes(manifest.tool_name));
  return notes;
}

function resolveDisplayManifests(
  registry: ToolCommandManifestRegistry,
  target: string,
  manifest: ToolCommandManifest,
): ToolCommandManifest[] {
  if (target === manifest.tool_id || manifest.aliases.includes(target)) {
    return [manifest];
  }

  const related = registry.list()
    .filter((entry) => entry.tool_name === manifest.tool_name)
    .filter((entry) => entry.audience !== 'expert');
  return related.length > 0 ? related : [manifest];
}

function formatUsage(manifest: ToolCommandManifest): string[] {
  switch (manifest.tool_name) {
    case 'blueprinthelper_find_assets':
      return ['bh blueprinthelper_find_assets --file <find-assets.json> --select status,artifacts.full_result'];
    case 'blueprinthelper_capture_screenshot':
      return ['bh blueprinthelper_capture_screenshot --file <capture-screenshot.json> --select status,artifacts.full_result'];
    case 'blueprinthelper_read_context':
      return [
        'bh blueprinthelper_read_context --file <read-spec.json> --select status,artifacts.full_result',
        '$json | bh blueprinthelper_read_context --stdin --format full',
        'bh context read --file <read-spec.json> --select status,artifacts.full_result',
      ];
    default:
      return [...manifest.recommended_invocations];
  }
}

function toolSpecificNotes(toolName: string): string[] {
  if (toolName === 'blueprinthelper_capture_screenshot') {
    return [
      'graph_name is required when block_ref or node_ref is provided.',
      'Use capture_target:auto for graph_name/block_ref/node_ref requests; graph targets capture Graph-only PNGs.',
    ];
  }
  if (toolName === 'blueprinthelper_find_assets') {
    return [
      'Resolve one explicit Unreal asset_path before preview_task or any write request.',
      'Do not infer Unreal asset_path values from filesystem .uasset paths.',
    ];
  }
  return [];
}

function resolveStaticHelpEntry(key: string): StaticHelpEntry | undefined {
  if (key === 'blueprint_open_editor' || key === 'open_editor') {
    return lifecycleHelp('open');
  }
  if (key === 'blueprint_close_editor' || key === 'close_editor') {
    return lifecycleHelp('close');
  }
  return STATIC_GROUP_HELP.get(key);
}

function resolveStaticHelpTitle(key: string): string | undefined {
  if (key === 'open_editor') {
    return 'blueprint_open_editor';
  }
  if (key === 'close_editor') {
    return 'blueprint_close_editor';
  }
  return undefined;
}

function resolveLifecycleAlias(key: string): string | undefined {
  if (key === 'open_editor') {
    return 'blueprint_open_editor';
  }
  if (key === 'close_editor') {
    return 'blueprint_close_editor';
  }
  return undefined;
}

const STATIC_GROUP_HELP = new Map<string, StaticHelpEntry>([
  ['metrics', {
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
    commonOptions: metricsCommonOptions(),
  }],
  ['metrics report', metricsHelpEntry('report', 'Build the full metrics report with all sections.')],
  ['metrics top-errors', metricsHelpEntry('top-errors', 'Build a metrics report focused on top error rows.')],
  ['metrics tool-usage', metricsHelpEntry('tool-usage', 'Build a metrics report focused on tool usage rows.')],
  ['metrics task-health', metricsHelpEntry('task-health', 'Build a metrics report focused on task health rows.')],
  ['bridge ping', {
    summary: 'Ping the running Editor Bridge.',
    usage: ['bh bridge ping --select status,summary'],
    input: ['No JSON input.'],
    notes: ['Use diagnostics commands for richer setup or runtime checks.'],
  }],
  ['bridge call', {
    summary: 'Call a narrow allowlist of read-only Bridge commands.',
    usage: ['bh bridge call --command <read_only_command> --select status,artifacts.full_result'],
    input: ['No JSON payload. Pass the Bridge command with --command.'],
    notes: ['Ordinary Agent workflows should prefer named tools and templates over bridge call.'],
  }],
]);

function lifecycleHelp(action: 'open' | 'close'): StaticHelpEntry {
  const mcpTool = action === 'open'
    ? 'mcp__blueprint_helper__blueprint_open_editor'
    : 'mcp__blueprint_helper__blueprint_close_editor';
  return {
    summary: `Agent-owned Editor ${action} is a global MCP lifecycle operation, not a normal CLI asset workflow.`,
    usage: [mcpTool],
    input: ['Use the global MCP tool schema exposed by the host Agent environment.'],
    notes: [
      'Ordinary reads, writes, diagnostics, preview, execute, and result lookup stay on the CLI.',
      'Do not use CLI lifecycle aliases as Agent compatibility paths.',
      'CLI lifecycle invocation is blocked for Agents; report lifecycle_mcp_unavailable when the global MCP lifecycle tools are unavailable.',
    ],
  };
}

function metricsHelpEntry(
  kind: 'report' | 'top-errors' | 'tool-usage' | 'task-health',
  summary: string,
): StaticHelpEntry {
  return {
    summary,
    usage: [`bh metrics ${kind} --window 7d --limit 20 --format json`],
    input: ['No JSON payload. Metrics root resolves from BPH_METRICS_DIR or nearest .uproject/Saved/BlueprintHelper/Metrics.'],
    notes: [
      'Markdown writes the rendered report under metricsRoot/reports.',
      'Use --format json for machine-readable stdout.',
    ],
    commonOptions: metricsCommonOptions(),
  };
}

function formatOptionalSection(title: string, lines: string[] | undefined): string[] {
  if (!lines || lines.length === 0) {
    return [];
  }
  return [
    '',
    title,
    ...lines.map((line) => `  ${line}`),
  ];
}

function formatTemplateSection(templates: string[] | undefined): string[] {
  if (!templates || templates.length === 0) {
    return [];
  }
  return [
    '',
    'Template navigation:',
    ...templates.map((line) => `  ${line}`),
    '  Copy a matching template, replace placeholders, then pass it with --file.',
  ];
}

function normalizeHelpTarget(target: string[]): string[] {
  return target
    .map((part) => part.trim())
    .filter((part) => part.length > 0)
    .map((part) => part === 'blueprinthelper-cli' ? 'bh' : part)
    .filter((part) => part !== 'bh');
}

function defaultCommonOptions(): string[] {
  return [
    '  --format summary|json|full',
    '  --fields path[,path...] or --select path[,path...]',
    '  --omit path[,path...]',
    '  --artifact-dir <dir> (overrides BPH_CLI_ARTIFACT_DIR and cli.artifacts.default_output_dir)',
    '  --max-bytes <bytes>',
  ];
}

function metricsCommonOptions(): string[] {
  return [
    '  --format json|markdown',
    '  --window 1d|7d|30d|all',
    '  --limit <N>',
  ];
}

function uniqueBy<T>(items: T[], keyOf: (item: T) => string): T[] {
  const seen = new Set<string>();
  const result: T[] = [];
  for (const item of items) {
    const key = keyOf(item);
    if (!seen.has(key)) {
      seen.add(key);
      result.push(item);
    }
  }
  return result;
}

function uniqueStrings<T extends string>(items: T[]): T[] {
  return [...new Set(items)];
}
