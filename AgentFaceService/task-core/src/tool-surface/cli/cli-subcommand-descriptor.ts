import type { ToolInputShapeId } from '../manifest/tool-command-manifest.js';

export type CliSubcommandGroup = 'tools.templates' | 'tools.read_templates';

export interface CliSubcommandDescriptor {
  readonly group: CliSubcommandGroup;
  readonly subcommand: string;
  readonly kind: string;
  readonly usage: string;
  readonly defaults?: Record<string, unknown>;
  readonly option_map?: Record<string, string>;
  readonly array_option_map?: Record<string, string>;
}

export interface RouteCliSubcommandInput<TBase extends Record<string, unknown>> {
  readonly group: CliSubcommandGroup;
  readonly positionals: readonly string[];
  readonly options: Record<string, unknown>;
  readonly base: TBase;
}

export type RouteCliSubcommandResult<TBase extends Record<string, unknown>> =
  | { ok: true; command: TBase & { kind: string } & Record<string, unknown> }
  | { ok: false; message: string };

const TASKSPEC_TEMPLATE_SUBCOMMANDS: readonly CliSubcommandDescriptor[] = [
  {
    group: 'tools.templates',
    subcommand: 'families',
    kind: 'tools.templates.families',
    usage: 'bh tools templates families --workflow preview_execute --format json',
    defaults: { workflow: 'preview_execute' },
    option_map: { workflow: 'workflow' },
  },
  {
    group: 'tools.templates',
    subcommand: 'write-modes',
    kind: 'tools.templates.write_modes',
    usage: 'bh tools templates write-modes --family <family> --format json',
    option_map: { family: 'family' },
  },
  {
    group: 'tools.templates',
    subcommand: 'clusters',
    kind: 'tools.templates.clusters',
    usage: 'bh tools templates clusters --family <family> --format json',
    option_map: { family: 'family' },
  },
  {
    group: 'tools.templates',
    subcommand: 'operations',
    kind: 'tools.templates.operations',
    usage: 'bh tools templates operations --family <family> --cluster <cluster> --write-mode <mode> --format json',
    option_map: { family: 'family', cluster: 'cluster', writeMode: 'writeMode' },
  },
  {
    group: 'tools.templates',
    subcommand: 'quick-access',
    kind: 'tools.templates.quick_access',
    usage: 'bh tools templates quick-access --family <family> --cluster <cluster> --operation <operation> --write-mode <mode> --format json',
    option_map: { family: 'family', cluster: 'cluster', operation: 'operation', writeMode: 'writeMode' },
  },
  {
    group: 'tools.templates',
    subcommand: 'compose',
    kind: 'tools.templates.compose',
    usage: 'bh tools templates compose --family <family> --write-mode <mode> --templates <slot_expr[,slot_expr...]> --out <task-spec.json> --format json',
    option_map: { family: 'family', writeMode: 'writeMode', outputPath: 'out' },
    array_option_map: { templateIds: 'templates' },
  },
];

const READ_CONTEXT_TEMPLATE_SUBCOMMANDS: readonly CliSubcommandDescriptor[] = [
  {
    group: 'tools.read_templates',
    subcommand: 'domains',
    kind: 'tools.read_templates.domains',
    usage: 'bh tools read-templates domains --format json',
  },
  {
    group: 'tools.read_templates',
    subcommand: 'clusters',
    kind: 'tools.read_templates.clusters',
    usage: 'bh tools read-templates clusters --domain <domain> --format json',
    option_map: { domain: 'domain' },
  },
  {
    group: 'tools.read_templates',
    subcommand: 'targets',
    kind: 'tools.read_templates.targets',
    usage: 'bh tools read-templates targets --domain <domain> --read-cluster <cluster> --format json',
    option_map: { domain: 'domain', readCluster: 'readCluster' },
  },
  {
    group: 'tools.read_templates',
    subcommand: 'views',
    kind: 'tools.read_templates.views',
    usage: 'bh tools read-templates views --domain <domain> --read-cluster <cluster> --target-kind <target> --format json',
    option_map: { domain: 'domain', readCluster: 'readCluster', targetKind: 'targetKind' },
  },
  {
    group: 'tools.read_templates',
    subcommand: 'quick-access',
    kind: 'tools.read_templates.quick_access',
    usage: 'bh tools read-templates quick-access --domain <domain> --read-cluster <cluster> --target-kind <target> --view-template <view> --format json',
    option_map: {
      domain: 'domain',
      readCluster: 'readCluster',
      targetKind: 'targetKind',
      viewTemplate: 'viewTemplate',
    },
  },
  {
    group: 'tools.read_templates',
    subcommand: 'compose',
    kind: 'tools.read_templates.compose',
    usage: 'bh tools read-templates compose --domain <domain> --read-cluster <cluster> --target-kind <target> --view-template <view> --out <read-spec.json> --format json',
    option_map: {
      domain: 'domain',
      readCluster: 'readCluster',
      targetKind: 'targetKind',
      viewTemplate: 'viewTemplate',
      outputPath: 'out',
    },
    array_option_map: { templateIds: 'templates' },
  },
];

const CLI_SUBCOMMAND_DESCRIPTORS: readonly CliSubcommandDescriptor[] = [
  ...TASKSPEC_TEMPLATE_SUBCOMMANDS,
  ...READ_CONTEXT_TEMPLATE_SUBCOMMANDS,
];

export function listCliSubcommandDescriptors(group?: CliSubcommandGroup): CliSubcommandDescriptor[] {
  return CLI_SUBCOMMAND_DESCRIPTORS.filter((descriptor) => group === undefined || descriptor.group === group)
    .map((descriptor) => ({ ...descriptor }));
}

export function listCliSubcommandUsageLines(group?: CliSubcommandGroup): string[] {
  return listCliSubcommandDescriptors(group).map((descriptor) => descriptor.usage);
}

export function templateNavigationUsageLinesForInputShapes(inputShapes: readonly ToolInputShapeId[]): string[] {
  if (inputShapes.includes('readspec')) {
    return listCliSubcommandUsageLines('tools.read_templates');
  }
  if (
    inputShapes.includes('bare_taskspec')
    || inputShapes.includes('wrapped_taskspec_preview')
    || inputShapes.includes('wrapped_taskspec_execute')
  ) {
    return listCliSubcommandUsageLines('tools.templates');
  }
  return [];
}

export function routeCliSubcommand<TBase extends Record<string, unknown>>(
  input: RouteCliSubcommandInput<TBase>,
): RouteCliSubcommandResult<TBase> {
  const subcommand = input.positionals[2];
  if (input.positionals.length !== 3 || !subcommand) {
    return reject(input.group, subcommand);
  }

  const descriptor = CLI_SUBCOMMAND_DESCRIPTORS.find((entry) =>
    entry.group === input.group && entry.subcommand === subcommand);
  if (!descriptor) {
    return reject(input.group, subcommand);
  }

  const command: Record<string, unknown> = {
    ...input.base,
    ...(descriptor.defaults ?? {}),
    kind: descriptor.kind,
  };

  for (const [targetField, optionField] of Object.entries(descriptor.option_map ?? {})) {
    command[targetField] = input.options[optionField];
  }
  for (const [targetField, optionField] of Object.entries(descriptor.array_option_map ?? {})) {
    command[targetField] = input.options[optionField] ?? [];
  }

  return { ok: true, command: command as TBase & { kind: string } & Record<string, unknown> };
}

function reject<TBase extends Record<string, unknown>>(
  group: CliSubcommandGroup,
  subcommand: string | undefined,
): RouteCliSubcommandResult<TBase> {
  return {
    ok: false,
    message: `Unsupported BlueprintHelper CLI subcommand: ${group} ${subcommand ?? ''}`.trim(),
  };
}
