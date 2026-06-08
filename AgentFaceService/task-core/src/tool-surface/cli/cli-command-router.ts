import { listCliCommandDescriptors, type CliCommandDescriptor } from './cli-command-descriptor.js';
import { resolveCliSubcommandGroupFromPositionals } from './cli-subcommand-descriptor.js';

export interface RouteCliCommandInput<TBase extends Record<string, unknown>> {
  readonly positionals: readonly string[];
  readonly options: Record<string, unknown>;
  readonly base: TBase;
}

export type RouteCliCommandResult<TBase extends Record<string, unknown>> =
  | { ok: true; command: TBase & { kind: string } & Record<string, unknown> }
  | { ok: false; message: string };

export function routeCliCommand<TBase extends Record<string, unknown>>(
  input: RouteCliCommandInput<TBase>,
): RouteCliCommandResult<TBase> {
  const match = findMatchingDescriptor(input.positionals);
  if (!match) {
    const unsupportedSubcommand = describeUnsupportedTemplateSubcommand(input.positionals);
    if (unsupportedSubcommand) {
      return {
        ok: false,
        message: `Unsupported BlueprintHelper CLI subcommand: ${unsupportedSubcommand}`.trim(),
      };
    }
    return {
      ok: false,
      message: `Unsupported BlueprintHelper CLI command: ${input.positionals.join(' ')}`.trim(),
    };
  }

  for (const requiredOption of match.descriptor.required_options ?? []) {
    if (!hasOptionValue(input.options[requiredOption.option])) {
      return { ok: false, message: requiredOption.message };
    }
  }

  const command: Record<string, unknown> = {
    ...input.base,
    ...(match.descriptor.defaults ?? {}),
    kind: match.descriptor.kind,
    resultPolicyId: match.descriptor.result_policy_id,
    statusPolicyId: match.descriptor.status_policy_id,
    runIdPolicyId: match.descriptor.run_id_policy_id,
    outputDataPolicyId: match.descriptor.output_data_policy_id,
    metricsToolName: match.descriptor.metrics_tool_name,
    inputIoKind: match.descriptor.input_io_kind,
    ...match.captures,
  };

  for (const [targetField, optionField] of Object.entries(match.descriptor.option_map ?? {})) {
    const value = input.options[optionField];
    if (value !== undefined) {
      command[targetField] = value;
    }
  }
  for (const [targetField, optionField] of Object.entries(match.descriptor.array_option_map ?? {})) {
    const value = input.options[optionField];
    command[targetField] = Array.isArray(value) ? value : [];
  }
  for (const [paramField, optionField] of Object.entries(match.descriptor.param_option_map ?? {})) {
    const value = input.options[optionField];
    if (value !== undefined) {
      const params = isRecord(command['params']) ? { ...command['params'] } : {};
      params[paramField] = value;
      command['params'] = params;
    }
  }

  const format = command['format'];
  if (
    match.descriptor.allowed_formats
    && typeof format === 'string'
    && !match.descriptor.allowed_formats.includes(format)
  ) {
    return {
      ok: false,
      message: `Unsupported --format value for bh ${input.positionals.join(' ')}: ${format}`,
    };
  }

  return { ok: true, command: command as TBase & { kind: string } & Record<string, unknown> };
}

function describeUnsupportedTemplateSubcommand(positionals: readonly string[]): string | undefined {
  const groupDescriptor = resolveCliSubcommandGroupFromPositionals(positionals);
  return groupDescriptor
    ? `${groupDescriptor.group} ${positionals[groupDescriptor.command_prefix.length] ?? ''}`
    : undefined;
}

function findMatchingDescriptor(positionals: readonly string[]): {
  descriptor: CliCommandDescriptor;
  captures: Record<string, string>;
} | undefined {
  for (const descriptor of listCliCommandDescriptors()) {
    const captures = matchPositionals(descriptor, positionals);
    if (captures) {
      return { descriptor, captures };
    }
  }
  return undefined;
}

function matchPositionals(
  descriptor: CliCommandDescriptor,
  positionals: readonly string[],
): Record<string, string> | undefined {
  if (descriptor.positionals.length !== positionals.length) {
    return undefined;
  }

  const captures: Record<string, string> = {};
  for (let index = 0; index < descriptor.positionals.length; index += 1) {
    const expected = descriptor.positionals[index];
    const actual = positionals[index];
    if (expected.startsWith(':')) {
      const field = expected.slice(1);
      if (!actual) {
        return undefined;
      }
      captures[field] = actual;
      continue;
    }
    if (expected !== actual) {
      return undefined;
    }
  }
  return captures;
}

function hasOptionValue(value: unknown): boolean {
  if (Array.isArray(value)) {
    return value.length > 0;
  }
  return value !== undefined && value !== null && value !== '';
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}
