import type { ToolInputShapeId } from '../manifest/tool-command-manifest.js';

export type CliSubcommandGroup = 'tools.templates' | 'tools.read_templates';

export type CliTemplateIndexCommand =
  | 'bh tools templates families --workflow preview_execute --format json'
  | 'bh tools read-templates domains --format json';

type CliSubcommandCapabilityKind = 'discover' | 'read' | 'plan' | 'write' | 'diagnose';

export interface CliSubcommandGroupDescriptor {
  readonly group: CliSubcommandGroup;
  readonly command_prefix: readonly string[];
  readonly input_shapes: readonly ToolInputShapeId[];
  readonly capability_kinds: readonly CliSubcommandCapabilityKind[];
  readonly template_index_command: CliTemplateIndexCommand;
}

export interface CliSubcommandDescriptor {
  readonly group: CliSubcommandGroup;
  readonly positionals: readonly string[];
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

type CliSubcommandDefinition = Omit<CliSubcommandDescriptor, 'group' | 'positionals'>;

const TASKSPEC_TEMPLATE_GROUP_DESCRIPTOR: CliSubcommandGroupDescriptor = {
  group: 'tools.templates',
  command_prefix: ['tools', 'templates'],
  input_shapes: ['bare_taskspec', 'wrapped_taskspec_preview', 'wrapped_taskspec_execute'],
  capability_kinds: ['discover', 'plan', 'write', 'diagnose'],
  template_index_command: 'bh tools templates families --workflow preview_execute --format json',
};

const READ_CONTEXT_TEMPLATE_GROUP_DESCRIPTOR: CliSubcommandGroupDescriptor = {
  group: 'tools.read_templates',
  command_prefix: ['tools', 'read-templates'],
  input_shapes: ['readspec'],
  capability_kinds: ['read'],
  template_index_command: 'bh tools read-templates domains --format json',
};

const CLI_SUBCOMMAND_GROUP_DESCRIPTORS: readonly CliSubcommandGroupDescriptor[] = [
  TASKSPEC_TEMPLATE_GROUP_DESCRIPTOR,
  READ_CONTEXT_TEMPLATE_GROUP_DESCRIPTOR,
];

const TASKSPEC_TEMPLATE_SUBCOMMANDS: readonly CliSubcommandDefinition[] = [
  {
    subcommand: 'families',
    kind: 'tools.templates.families',
    usage: 'bh tools templates families --workflow preview_execute --format json',
    defaults: { workflow: 'preview_execute' },
    option_map: { workflow: 'workflow' },
  },
  {
    subcommand: 'write-modes',
    kind: 'tools.templates.write_modes',
    usage: 'bh tools templates write-modes --family <family> --format json',
    option_map: { family: 'family' },
  },
  {
    subcommand: 'clusters',
    kind: 'tools.templates.clusters',
    usage: 'bh tools templates clusters --family <family> --format json',
    option_map: { family: 'family' },
  },
  {
    subcommand: 'operations',
    kind: 'tools.templates.operations',
    usage: 'bh tools templates operations --family <family> --cluster <cluster> --write-mode <mode> --format json',
    option_map: { family: 'family', cluster: 'cluster', writeMode: 'writeMode' },
  },
  {
    subcommand: 'quick-access',
    kind: 'tools.templates.quick_access',
    usage: 'bh tools templates quick-access --family <family> --cluster <cluster> --operation <operation> --write-mode <mode> --format json',
    option_map: { family: 'family', cluster: 'cluster', operation: 'operation', writeMode: 'writeMode' },
  },
  {
    subcommand: 'compose',
    kind: 'tools.templates.compose',
    usage: 'bh tools templates compose --family <family> --write-mode <mode> --templates <slot_expr[,slot_expr...]> --out <task-spec.json> --format json',
    option_map: { family: 'family', writeMode: 'writeMode', outputPath: 'out' },
    array_option_map: { templateIds: 'templates' },
  },
];

const READ_CONTEXT_TEMPLATE_SUBCOMMANDS: readonly CliSubcommandDefinition[] = [
  {
    subcommand: 'domains',
    kind: 'tools.read_templates.domains',
    usage: 'bh tools read-templates domains --format json',
  },
  {
    subcommand: 'clusters',
    kind: 'tools.read_templates.clusters',
    usage: 'bh tools read-templates clusters --domain <domain> --format json',
    option_map: { domain: 'domain' },
  },
  {
    subcommand: 'targets',
    kind: 'tools.read_templates.targets',
    usage: 'bh tools read-templates targets --domain <domain> --read-cluster <cluster> --format json',
    option_map: { domain: 'domain', readCluster: 'readCluster' },
  },
  {
    subcommand: 'views',
    kind: 'tools.read_templates.views',
    usage: 'bh tools read-templates views --domain <domain> --read-cluster <cluster> --target-kind <target> --format json',
    option_map: { domain: 'domain', readCluster: 'readCluster', targetKind: 'targetKind' },
  },
  {
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
  ...buildCliSubcommandDescriptors(TASKSPEC_TEMPLATE_GROUP_DESCRIPTOR, TASKSPEC_TEMPLATE_SUBCOMMANDS),
  ...buildCliSubcommandDescriptors(READ_CONTEXT_TEMPLATE_GROUP_DESCRIPTOR, READ_CONTEXT_TEMPLATE_SUBCOMMANDS),
];

export function listCliSubcommandGroupDescriptors(): CliSubcommandGroupDescriptor[] {
  return CLI_SUBCOMMAND_GROUP_DESCRIPTORS.map(cloneCliSubcommandGroupDescriptor);
}

export function getCliSubcommandGroupDescriptor(group: CliSubcommandGroup): CliSubcommandGroupDescriptor {
  const descriptor = CLI_SUBCOMMAND_GROUP_DESCRIPTORS.find((entry) => entry.group === group);
  if (!descriptor) {
    throw new Error(`Unknown BlueprintHelper CLI subcommand group: ${group}`);
  }
  return cloneCliSubcommandGroupDescriptor(descriptor);
}

export function resolveCliSubcommandGroupFromPositionals(
  positionals: readonly string[],
): CliSubcommandGroupDescriptor | undefined {
  const descriptor = CLI_SUBCOMMAND_GROUP_DESCRIPTORS.find((entry) =>
    entry.command_prefix.every((token, index) => positionals[index] === token));
  return descriptor ? cloneCliSubcommandGroupDescriptor(descriptor) : undefined;
}

export function listCliSubcommandDescriptors(group?: CliSubcommandGroup): CliSubcommandDescriptor[] {
  return CLI_SUBCOMMAND_DESCRIPTORS.filter((descriptor) => group === undefined || descriptor.group === group)
    .map(cloneCliSubcommandDescriptor);
}

export function listCliSubcommandUsageLines(group?: CliSubcommandGroup): string[] {
  return listCliSubcommandDescriptors(group).map((descriptor) => descriptor.usage);
}

export function templateNavigationUsageLinesForInputShapes(inputShapes: readonly ToolInputShapeId[]): string[] {
  const inputShapeSet = new Set(inputShapes);
  return CLI_SUBCOMMAND_GROUP_DESCRIPTORS
    .filter((groupDescriptor) => groupDescriptor.input_shapes.some((shape) => inputShapeSet.has(shape)))
    .flatMap((groupDescriptor) => listCliSubcommandUsageLines(groupDescriptor.group));
}

export function templateIndexCommandForCapabilityKind(
  capabilityKind: string,
): CliTemplateIndexCommand | undefined {
  return CLI_SUBCOMMAND_GROUP_DESCRIPTORS.find((groupDescriptor) =>
    groupDescriptor.capability_kinds.some((kind) => kind === capabilityKind))?.template_index_command;
}

export function routeCliSubcommand<TBase extends Record<string, unknown>>(
  input: RouteCliSubcommandInput<TBase>,
): RouteCliSubcommandResult<TBase> {
  const groupDescriptor = getCliSubcommandGroupDescriptor(input.group);
  const subcommand = input.positionals[groupDescriptor.command_prefix.length];
  const hasExpectedPrefix = groupDescriptor.command_prefix.every((token, index) => input.positionals[index] === token);
  if (input.positionals.length !== groupDescriptor.command_prefix.length + 1 || !subcommand || !hasExpectedPrefix) {
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

function buildCliSubcommandDescriptors(
  groupDescriptor: CliSubcommandGroupDescriptor,
  definitions: readonly CliSubcommandDefinition[],
): CliSubcommandDescriptor[] {
  return definitions.map((definition) => ({
    ...definition,
    group: groupDescriptor.group,
    positionals: [...groupDescriptor.command_prefix, definition.subcommand],
  }));
}

function cloneCliSubcommandDescriptor(descriptor: CliSubcommandDescriptor): CliSubcommandDescriptor {
  return {
    ...descriptor,
    positionals: [...descriptor.positionals],
  };
}

function cloneCliSubcommandGroupDescriptor(
  descriptor: CliSubcommandGroupDescriptor,
): CliSubcommandGroupDescriptor {
  return {
    ...descriptor,
    command_prefix: [...descriptor.command_prefix],
    input_shapes: [...descriptor.input_shapes],
    capability_kinds: [...descriptor.capability_kinds],
  };
}
