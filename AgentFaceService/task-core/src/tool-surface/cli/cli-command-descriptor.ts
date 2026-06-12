import { listCliSubcommandDescriptors } from './cli-subcommand-descriptor.js';
import type { BuiltinResultProjectionPolicyId } from '../result/result-projection-registry.js';

export type CliCommandDescriptorToken = string;

export interface CliCommandRequiredOption {
  readonly option: string;
  readonly message: string;
}

export interface CliCommandRequiredOneOfOptions {
  readonly options: readonly string[];
  readonly message: string;
}

export interface CliCommandDescriptor {
  readonly id: string;
  readonly kind: string;
  readonly executor_id: string;
  readonly result_policy_id?: BuiltinResultProjectionPolicyId;
  readonly status_policy_id?: CliCommandStatusPolicyId;
  readonly run_id_policy_id?: CliCommandRunIdPolicyId;
  readonly output_data_policy_id?: CliCommandOutputDataPolicyId;
  readonly manifest_lookup_id?: string;
  readonly metrics_tool_name?: string;
  readonly metrics_lookup_id?: string;
  readonly input_io_kind?: CliCommandInputIoKind;
  readonly positionals: readonly CliCommandDescriptorToken[];
  readonly defaults?: Record<string, unknown>;
  readonly option_map?: Record<string, string>;
  readonly array_option_map?: Record<string, string>;
  readonly param_option_map?: Record<string, string>;
  readonly required_options?: readonly CliCommandRequiredOption[];
  readonly required_one_of_options?: readonly CliCommandRequiredOneOfOptions[];
  readonly allowed_formats?: readonly string[];
}

export type CliCommandStatusPolicyId =
  | 'tool.result_status'
  | 'task.preview_status'
  | 'task.execute_status'
  | 'task.result_status'
  | 'bridge.ping_status'
  | 'metrics.report_status';

export type CliCommandRunIdPolicyId =
  | 'tool.result_task_or_cli'
  | 'task.preview_run_id'
  | 'metrics.report_run_id';

export type CliCommandOutputDataPolicyId =
  | 'tool.result_data'
  | 'metrics.report_data';

export type CliCommandInputIoKind =
  | 'file'
  | 'task_file';

const BASE_CLI_COMMAND_DESCRIPTORS: readonly CliCommandDescriptor[] = [
  {
    id: 'tools.domains',
    kind: 'tools.domains',
    executor_id: 'tools.registry',
    positionals: ['tools', 'domains'],
    defaults: { audience: 'default' },
    option_map: { includeReserved: 'includeReserved', audience: 'audience' },
  },
  {
    id: 'tools.list',
    kind: 'tools.list',
    executor_id: 'tools.registry',
    positionals: ['tools', 'list', ':toolDomain', ':toolCatalogKind'],
    defaults: { audience: 'default' },
    option_map: {
      audience: 'audience',
      requiresBridge: 'requiresBridge',
      risks: 'risks',
      expert: 'expert',
    },
  },
  {
    id: 'task.preview',
    kind: 'task.preview',
    executor_id: 'task.preview',
    result_policy_id: 'task.preview.default',
    status_policy_id: 'task.preview_status',
    run_id_policy_id: 'task.preview_run_id',
    metrics_tool_name: 'blueprinthelper_preview_task',
    metrics_lookup_id: 'blueprint.plan.taskspec.preview',
    input_io_kind: 'task_file',
    positionals: ['task', 'preview'],
    option_map: { file: 'file', compileOnly: 'compileOnly' },
    required_options: [{ option: 'file', message: 'Missing --file for bh task preview.' }],
  },
  {
    id: 'task.execute',
    kind: 'task.execute',
    executor_id: 'task.execute',
    result_policy_id: 'task.execute.default',
    status_policy_id: 'task.execute_status',
    run_id_policy_id: 'tool.result_task_or_cli',
    metrics_tool_name: 'blueprinthelper_execute_task',
    metrics_lookup_id: 'blueprint.write.taskspec.execute',
    input_io_kind: 'task_file',
    positionals: ['task', 'execute'],
    option_map: { file: 'file', previewToken: 'previewToken' },
    required_options: [{ option: 'file', message: 'Missing --file for bh task execute.' }],
  },
  {
    id: 'task.result',
    kind: 'task.result',
    executor_id: 'task.result',
    result_policy_id: 'task.result.default',
    status_policy_id: 'task.result_status',
    run_id_policy_id: 'tool.result_task_or_cli',
    metrics_tool_name: 'blueprinthelper_get_task_result',
    metrics_lookup_id: 'project.read.task_result',
    positionals: ['task', 'result'],
    option_map: { taskRunId: 'id' },
    required_options: [{ option: 'id', message: 'Missing --id for bh task result.' }],
  },
  {
    id: 'bridge.ping',
    kind: 'bridge.ping',
    executor_id: 'bridge.ping',
    result_policy_id: 'bridge.default',
    status_policy_id: 'bridge.ping_status',
    run_id_policy_id: 'tool.result_task_or_cli',
    positionals: ['bridge', 'ping'],
  },
  {
    id: 'bridge.call',
    kind: 'bridge.call',
    executor_id: 'bridge.call',
    result_policy_id: 'bridge.default',
    status_policy_id: 'tool.result_status',
    run_id_policy_id: 'tool.result_task_or_cli',
    positionals: ['bridge', 'call'],
    option_map: { bridgeCommand: 'command' },
    required_options: [{ option: 'command', message: 'Missing --command for bh bridge call.' }],
  },
  {
    id: 'context.read',
    kind: 'context.read',
    executor_id: 'context.read',
    result_policy_id: 'context.read.default',
    status_policy_id: 'tool.result_status',
    run_id_policy_id: 'tool.result_task_or_cli',
    positionals: ['context', 'read'],
    defaults: { toolName: 'blueprinthelper_read_context' },
    manifest_lookup_id: 'blueprint.read.context.logic_flow',
    metrics_lookup_id: 'blueprint.read.context.logic_flow',
    option_map: { file: 'file', json: 'json', stdin: 'stdin' },
    required_one_of_options: [{
      options: ['file', 'json', 'stdin'],
      message: 'Missing input for bh context read: choose --file, --json, or --stdin.',
    }],
  },
  {
    id: 'metrics.report',
    kind: 'metrics.report',
    executor_id: 'metrics.report',
    result_policy_id: 'metrics.report.default',
    status_policy_id: 'metrics.report_status',
    run_id_policy_id: 'metrics.report_run_id',
    output_data_policy_id: 'metrics.report_data',
    positionals: ['metrics', 'report'],
    defaults: { format: 'json', metricsKind: 'report', window: '7d', limit: 20 },
    option_map: {
      format: 'format',
      window: 'window',
      limit: 'limit',
      artifactDir: 'artifactDir',
      maxBytes: 'maxBytes',
      fields: 'fields',
      omitFields: 'omitFields',
      develop: 'develop',
    },
    allowed_formats: ['json', 'markdown'],
  },
  {
    id: 'metrics.top-errors',
    kind: 'metrics.report',
    executor_id: 'metrics.report',
    result_policy_id: 'metrics.report.default',
    status_policy_id: 'metrics.report_status',
    run_id_policy_id: 'metrics.report_run_id',
    output_data_policy_id: 'metrics.report_data',
    positionals: ['metrics', 'top-errors'],
    defaults: { format: 'json', metricsKind: 'top-errors', window: '7d', limit: 20 },
    option_map: {
      format: 'format',
      window: 'window',
      limit: 'limit',
      artifactDir: 'artifactDir',
      maxBytes: 'maxBytes',
      fields: 'fields',
      omitFields: 'omitFields',
      develop: 'develop',
    },
    allowed_formats: ['json', 'markdown'],
  },
  {
    id: 'metrics.tool-usage',
    kind: 'metrics.report',
    executor_id: 'metrics.report',
    result_policy_id: 'metrics.report.default',
    status_policy_id: 'metrics.report_status',
    run_id_policy_id: 'metrics.report_run_id',
    output_data_policy_id: 'metrics.report_data',
    positionals: ['metrics', 'tool-usage'],
    defaults: { format: 'json', metricsKind: 'tool-usage', window: '7d', limit: 20 },
    option_map: {
      format: 'format',
      window: 'window',
      limit: 'limit',
      artifactDir: 'artifactDir',
      maxBytes: 'maxBytes',
      fields: 'fields',
      omitFields: 'omitFields',
      develop: 'develop',
    },
    allowed_formats: ['json', 'markdown'],
  },
  {
    id: 'metrics.task-health',
    kind: 'metrics.report',
    executor_id: 'metrics.report',
    result_policy_id: 'metrics.report.default',
    status_policy_id: 'metrics.report_status',
    run_id_policy_id: 'metrics.report_run_id',
    output_data_policy_id: 'metrics.report_data',
    positionals: ['metrics', 'task-health'],
    defaults: { format: 'json', metricsKind: 'task-health', window: '7d', limit: 20 },
    option_map: {
      format: 'format',
      window: 'window',
      limit: 'limit',
      artifactDir: 'artifactDir',
      maxBytes: 'maxBytes',
      fields: 'fields',
      omitFields: 'omitFields',
      develop: 'develop',
    },
    allowed_formats: ['json', 'markdown'],
  },
];

export function listCliCommandDescriptors(): CliCommandDescriptor[] {
  return [
    ...BASE_CLI_COMMAND_DESCRIPTORS,
    ...listCliSubcommandDescriptors().map((descriptor): CliCommandDescriptor => ({
      id: descriptor.kind,
      kind: descriptor.kind,
      executor_id: 'tools.registry',
      result_policy_id: 'local.default',
      status_policy_id: 'tool.result_status',
      run_id_policy_id: 'tool.result_task_or_cli',
      positionals: descriptor.positionals,
      defaults: descriptor.defaults,
      option_map: descriptor.option_map,
      array_option_map: descriptor.array_option_map,
    })),
  ].map((descriptor) => ({
    ...descriptor,
    positionals: [...descriptor.positionals],
    required_options: descriptor.required_options ? [...descriptor.required_options] : undefined,
    required_one_of_options: descriptor.required_one_of_options
      ? descriptor.required_one_of_options.map((entry) => ({ ...entry, options: [...entry.options] }))
      : undefined,
    allowed_formats: descriptor.allowed_formats ? [...descriptor.allowed_formats] : undefined,
  }));
}
