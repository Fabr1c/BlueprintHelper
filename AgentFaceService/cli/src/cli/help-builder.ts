import {
  buildReadonlyToolCommandManifestRegistry,
  EMPTY_OBJECT_INPUT_NOTE,
  formatManifestUsage,
  getBlueprintHelperTool,
  getRemovedDirectCliToolCommand,
  globalCliCommandUsageLines,
  listCliSubcommandUsageLines,
  manifestSpecificNotes,
  resolveCliCommandHelpManifest,
  templateNavigationUsageLinesForInputShapes,
  type CommandHelpEntry,
  type ToolCommandManifest,
  type ToolCommandManifestRegistry,
} from '@blueprinthelper/task-core/tool-surface/tool-registry';

export interface HelpBuilder {
  build(target?: string[]): string;
}

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
      const removedDirectCommand = normalizedTarget.length === 1
        ? getRemovedDirectCliToolCommand(normalizedTarget[0] ?? '')
        : undefined;
      if (removedDirectCommand) {
        return formatEntry(removedDirectCommand.tool_name, {
          summary: removedDirectCommand.reason,
          usage: [removedDirectCommand.replacement_command],
          input: ['Use the grouped command input shape shown above.'],
          notes: ['The direct tool-name command is intentionally not an Agent-facing compatibility path.'],
        });
      }

      const manifest = registry.get(key);
      if (manifest) {
        const manifests = resolveDisplayManifests(registry, key, manifest);
        return formatManifestEntry(key, manifest, manifests);
      }

      const cliCommandEntry = resolveCliCommandHelpManifest(key);
      if (cliCommandEntry) {
        return formatEntry(cliCommandEntry.title ?? cliCommandEntry.key, cliCommandEntry);
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
  return [
    'BlueprintHelper CLI',
    '',
    'Usage:',
    '  bh task preview --file <task-spec.json> [--develop] [--format summary|json|full]',
    '  bh task execute --file <task-spec.json> [--preview-token <32-hex>] [--develop] [--format summary|json|full]',
    '  bh task result --id <task_run_id>',
    '  bh context read (--file <read-spec.json> | --json <json> | --stdin)',
    '  bh bridge ping',
    '  bh tools domains --format json',
    '  bh tools list <domain> <kind> --format json',
    ...listCliSubcommandUsageLines().map((line) => `  ${line}`),
    ...globalCliCommandUsageLines().map((line) => `  ${line}`),
    '',
    'Tool and template selection:',
    '  Start with: bh tools domains --format json',
    '  Filter with: bh tools list <domain> <kind> --format json',
    `  Discover TaskSpec templates with: ${listCliSubcommandUsageLines('tools.templates')[0]}`,
    `  Discover ReadContext templates with: ${listCliSubcommandUsageLines('tools.read_templates')[0]}`,
    '  Compose a temporary TaskSpec or ReadSpec, then run bh task preview, bh task execute, or bh context read.',
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
): string {
  return formatEntry(title, {
    summary: manifestPurpose(manifest),
    usage: uniqueStrings([
      ...formatManifestUsage(manifest),
      ...manifests.flatMap((entry) => entry.recommended_invocations),
    ]),
    input: formatInputShapes(uniqueStrings(manifests.flatMap((entry) => entry.input_shapes))),
    templates: formatTemplateNavigation(manifests),
    notes: formatManifestNotes(manifest),
  });
}

function formatEntry(name: string, entry: CommandHelpEntry): string {
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
        return `Root JSON: {}. ${EMPTY_OBJECT_INPUT_NOTE}`;
      case 'bare_taskspec':
        return 'Input file root: bare BlueprintHelper.TaskSpec.v1. Grouped command input: bare BlueprintHelper.TaskSpec.v1 file. Do not wrap it in { "task_spec": ... }.';
      case 'wrapped_taskspec_preview':
        return 'Direct tool input: { "task_spec": { "schema": "BlueprintHelper.TaskSpec.v1", ... } }.';
      case 'wrapped_taskspec_execute':
        return 'Direct tool input: { "task_spec": { "schema": "BlueprintHelper.TaskSpec.v1", ... } }.';
      case 'readspec':
        return 'Root JSON: bare BlueprintHelper.ReadSpec.v1. Do not wrap the input in args.';
      case 'read_reference_context':
        return 'Root JSON: BlueprintHelper.ReferenceContextRequest.v1 with schema field. Do not wrap the input in args.';
      case 'bridge_payload':
        return 'Root JSON: Bridge tool payload object. Use a template path before calling the tool.';
      case 'bridge_logic_json_payload':
        return 'Root JSON: Bridge logic payload object; format defaults to logic_json.';
      case 'tool_payload':
        return 'Root JSON: tool payload object. Use a template path before calling the tool.';
    }
  });
}

function formatTemplateNavigation(
  manifests: ToolCommandManifest[],
): string[] {
  return templateNavigationUsageLinesForInputShapes(uniqueStrings(
    manifests.flatMap((manifest) => manifest.input_shapes),
  ));
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
  notes.push(...manifestSpecificNotes(manifest));
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
    '  Placeholder style: replace __REQUIRED_NAME__; replace or delete __OPTIONAL_NAME__ fields.',
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

function uniqueStrings<T extends string>(items: T[]): T[] {
  return [...new Set(items)];
}
