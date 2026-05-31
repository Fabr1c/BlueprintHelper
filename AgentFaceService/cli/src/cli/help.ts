import { getBlueprintHelperTool } from '@blueprinthelper/task-core/tool-surface/tool-registry';

const TEMPLATE_ROOT = 'AgentFaceService/agent-guide/Templates';
const READ_INDEX = `${TEMPLATE_ROOT}/read/SEMANTIC_INDEX.md`;
const WRITE_INDEX = `${TEMPLATE_ROOT}/write/SEMANTIC_INDEX.md`;
const ROOT_INDEX = `${TEMPLATE_ROOT}/SEMANTIC_INDEX.md`;

type HelpEntry = {
  summary: string;
  usage: string[];
  input: string[];
  templates?: string[];
  examples?: string[];
  notes?: string[];
};

const helpEntries: Record<string, HelpEntry> = {
  blueprint_get_runtime_profile: {
    summary: 'Read the current BlueprintHelper runtime profile from the running Editor/Bridge.',
    usage: [
      'bh blueprint_get_runtime_profile --json "{}" --select status,summary',
      'bh blueprint_get_runtime_profile --file <params.json> --format summary',
    ],
    input: ['Root JSON: {}'],
    templates: [
      ROOT_INDEX,
      `${TEMPLATE_ROOT}/blueprint_get_runtime_profile_template.json`,
    ],
  },
  blueprinthelper_diagnostics: {
    summary: 'Run local static installation/configuration diagnostics without calling UE Bridge.',
    usage: ['bh blueprinthelper_diagnostics --json "{}" --select status,summary'],
    input: ['Root JSON: {}'],
    templates: [
      ROOT_INDEX,
      `${TEMPLATE_ROOT}/blueprinthelper_diagnostics_template.json`,
    ],
  },
  blueprinthelper_diagnostics_runtime: {
    summary: 'Run runtime diagnostics through the running Editor/Bridge.',
    usage: ['bh blueprinthelper_diagnostics_runtime --json "{}" --select status,summary,artifacts.full_result'],
    input: ['Root JSON: {}'],
    templates: [
      ROOT_INDEX,
      `${TEMPLATE_ROOT}/blueprinthelper_diagnostics_runtime_template.json`,
    ],
  },
  blueprinthelper_read_agent_guide: {
    summary: 'Read the AgentGuide onboarding entry.',
    usage: ['bh blueprinthelper_read_agent_guide --json "{}" --format summary'],
    input: ['Root JSON: {}'],
    templates: [
      ROOT_INDEX,
      `${TEMPLATE_ROOT}/blueprinthelper_read_agent_guide_template.json`,
      'AgentFaceService/agent-guide/00_Agent_Onboarding_Index.md',
    ],
  },
  blueprinthelper_read_context: {
    summary: 'Read UE asset context through BlueprintHelper.ReadSpec.v1.',
    usage: [
      'bh blueprinthelper_read_context --file <read-spec.json> --select status,artifacts.full_result',
      '$json | bh blueprinthelper_read_context --stdin --format full',
      'bh context read --file <read-spec.json> --select status,artifacts.full_result',
    ],
    input: [
      'Root JSON: bare BlueprintHelper.ReadSpec.v1.',
      'Do not wrap the input in args or { "task_spec": ... }.',
    ],
    templates: [
      READ_INDEX,
      `${TEMPLATE_ROOT}/read/read_context_asset_summary_template.json`,
      `${TEMPLATE_ROOT}/read/read_context_function_logic_flow_template.json`,
      `${TEMPLATE_ROOT}/read/read_context_graph_logic_json_template.json`,
      `${TEMPLATE_ROOT}/read/read_context_components_template.json`,
    ],
    notes: [
      'Use read templates before authoring or repairing TaskSpecs.',
      'Use logic_flow for simple function/event reads, logic_md for larger entry reads, and logic_json when stable write anchors are needed.',
    ],
  },
  blueprinthelper_read_context_capabilities: {
    summary: 'Read the compact ReadContext capability matrix without touching UE assets.',
    usage: ['bh blueprinthelper_read_context_capabilities --json "{}" --select status,artifacts.full_result'],
    input: ['Root JSON: {}'],
    templates: [
      READ_INDEX,
      `${TEMPLATE_ROOT}/read/blueprinthelper_read_context_capabilities_template.json`,
    ],
  },
  blueprinthelper_read_reference_context: {
    summary: 'Read compact ReferenceContextPack.v1 impact, dependency, and reference context.',
    usage: [
      'bh blueprinthelper_read_reference_context --file <reference-context.json> --select status,artifacts.full_result',
    ],
    input: ['Root JSON: BlueprintHelper reference-context request object.'],
    templates: [
      READ_INDEX,
      `${TEMPLATE_ROOT}/read/blueprinthelper_read_reference_context_safety_template.json`,
      `${TEMPLATE_ROOT}/read/blueprinthelper_read_reference_context_dependencies_template.json`,
      `${TEMPLATE_ROOT}/read/blueprinthelper_read_reference_context_function_template.json`,
      `${TEMPLATE_ROOT}/read/blueprinthelper_read_reference_context_member_variable_template.json`,
    ],
    notes: ['Use before deletes, renames, signature changes, or edits with dependency risk.'],
  },
  blueprinthelper_read_function_chain_context: {
    summary: 'Trace project-authored function/event/custom-event calls reachable from one Blueprint entry.',
    usage: [
      'bh blueprinthelper_read_function_chain_context --file <function-chain.json> --select status,artifacts.full_result',
    ],
    input: ['Root JSON: function-chain context request object.'],
    templates: [
      READ_INDEX,
      `${TEMPLATE_ROOT}/read/blueprinthelper_read_function_chain_context_template.json`,
    ],
  },
  blueprinthelper_find_assets: {
    summary: 'Find Unreal assets through AssetRegistry before a target asset_path is known.',
    usage: [
      'bh blueprinthelper_find_assets --file <find-assets.json> --select status,artifacts.full_result',
      'bh blueprinthelper_find_assets --json "{\\"schema\\":\\"BlueprintHelper.FindAssetsRequest.v1\\",\\"query\\":\\"Player\\",\\"path_prefixes\\":[\\"/Game\\"],\\"asset_types\\":[\\"blueprint\\"],\\"limit\\":25}" --format summary',
    ],
    input: [
      'Root JSON: BlueprintHelper.FindAssetsRequest.v1.',
      'Use this before blueprinthelper_read_context when the Unreal asset_path is unknown.',
    ],
    templates: [
      ROOT_INDEX,
      `${TEMPLATE_ROOT}/blueprinthelper_find_assets_template.json`,
    ],
    notes: [
      'Resolve one explicit Unreal asset_path before preview_task or any write request.',
      'If multiple candidates are returned, narrow the query or ask for confirmation before writes.',
      'Do not infer Unreal asset_path values from filesystem .uasset paths.',
    ],
  },
  blueprinthelper_preview_task: {
    summary: 'Validate and preview a BlueprintHelper.TaskSpec.v1 before execute.',
    usage: [
      'bh blueprinthelper_preview_task --file <preview-wrapper.json> --select status,preview_id,summary,artifacts.full_result',
      'bh task preview --file <bare-task-spec.json> --format summary',
    ],
    input: [
      'Direct tool input: { "task_spec": { "schema": "BlueprintHelper.TaskSpec.v1", ... } }.',
      'Grouped command input: bare BlueprintHelper.TaskSpec.v1 file.',
    ],
    templates: [
      WRITE_INDEX,
      `${TEMPLATE_ROOT}/write/blueprinthelper_preview_task_wrapper_template.json`,
      `${TEMPLATE_ROOT}/write/task_preview_bare_taskspec_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_create_blueprint_feature_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_graph_append_container_action_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_graph_append_event_delegate_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_graph_append_generic_schedule_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_graph_append_generic_ops_template.json`,
    ],
    notes: ['Preview is the write gate. If preview is blocked, repair the TaskSpec or stop and report.'],
  },
  blueprinthelper_execute_task: {
    summary: 'Execute a BlueprintHelper.TaskSpec.v1 after a successful preview.',
    usage: [
      'bh blueprinthelper_execute_task --file <execute-wrapper.json> --select status,task_run_id,summary',
      'bh task execute --file <bare-task-spec.json> --format summary',
    ],
    input: [
      'Direct tool input: { "task_spec": { "schema": "BlueprintHelper.TaskSpec.v1", ... } }.',
      'Grouped command input: bare BlueprintHelper.TaskSpec.v1 file.',
    ],
    templates: [
      WRITE_INDEX,
      `${TEMPLATE_ROOT}/write/blueprinthelper_execute_task_wrapper_template.json`,
      `${TEMPLATE_ROOT}/write/task_execute_bare_taskspec_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_create_blueprint_feature_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_graph_append_container_action_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_graph_append_event_delegate_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_graph_append_generic_schedule_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_graph_append_generic_ops_template.json`,
    ],
    notes: ['Execute only after preview succeeds and write permission is approved when required.'],
  },
  blueprinthelper_get_task_result: {
    summary: 'Read a completed task result or TaskRunJournal by task_run_id.',
    usage: [
      'bh blueprinthelper_get_task_result --id <task_run_id> --select status,artifacts.full_result',
      'bh task result --id <task_run_id> --select status,artifacts.full_result',
    ],
    input: ['Direct tool input: { "task_run_id": "task_..." }, or pass --id.'],
    templates: [
      ROOT_INDEX,
      `${TEMPLATE_ROOT}/blueprinthelper_get_task_result_template.json`,
    ],
  },
  blueprinthelper_request_write_session: {
    summary: 'Request Editor-approved write permission after preview reports it is required.',
    usage: [
      'bh blueprinthelper_request_write_session --file <write-session-request.json> --select status,summary',
    ],
    input: ['Root JSON: project-scoped or asset-list-scoped write-session request.'],
    templates: [
      ROOT_INDEX,
      `${TEMPLATE_ROOT}/blueprinthelper_request_write_session_project_template.json`,
      `${TEMPLATE_ROOT}/blueprinthelper_request_write_session_assets_template.json`,
    ],
    notes: ['Request only after preview succeeds and reports write_permission disabled.'],
  },
  blueprinthelper_get_debug_case: {
    summary: 'Read one summary-only DebugCase by id.',
    usage: ['bh blueprinthelper_get_debug_case --file <debug-case.json> --select status,artifacts.full_result'],
    input: ['Root JSON: { "debug_case_id": "..." }'],
    templates: [
      ROOT_INDEX,
      `${TEMPLATE_ROOT}/blueprinthelper_get_debug_case_template.json`,
    ],
  },
  blueprinthelper_list_debug_cases: {
    summary: 'List summary-only DebugCases.',
    usage: ['bh blueprinthelper_list_debug_cases --json "{}" --select status,artifacts.full_result'],
    input: ['Root JSON: {}, optionally { "limit": 20 }.'],
    templates: [
      ROOT_INDEX,
      `${TEMPLATE_ROOT}/blueprinthelper_list_debug_cases_template.json`,
    ],
  },
  blueprinthelper_export_debug_bundle: {
    summary: 'Export a local DebugBundle manifest for a DebugCase.',
    usage: ['bh blueprinthelper_export_debug_bundle --file <debug-case.json> --select status,artifacts.full_result'],
    input: ['Root JSON: { "debug_case_id": "..." }'],
    templates: [
      ROOT_INDEX,
      `${TEMPLATE_ROOT}/blueprinthelper_export_debug_bundle_template.json`,
    ],
  },
  blueprinthelper_query_review_records: {
    summary: 'Query summary ReviewRecords by asset, task run, archive session, or pending state.',
    usage: ['bh blueprinthelper_query_review_records --file <review-query.json> --select status,artifacts.full_result'],
    input: ['Root JSON: Review record query object.'],
    templates: [
      ROOT_INDEX,
      `${TEMPLATE_ROOT}/blueprinthelper_query_review_records_template.json`,
    ],
  },
  blueprinthelper_apply_review_action: {
    summary: 'Expert/internal Review action tool for accepting or rejecting ReviewRecord targets.',
    usage: ['bh blueprinthelper_apply_review_action --file <review-action.json> --expert --select status,summary'],
    input: ['Root JSON: { "review_record_id": "...", "action": "accept|reject", "target_keys": [...] }.'],
    notes: ['This is plugin-development/internal. Ordinary Agents should not use it.'],
  },
  blueprint_open_editor: lifecycleHelp('open'),
  blueprint_close_editor: lifecycleHelp('close'),
};

const groupHelpEntries: Record<string, HelpEntry> = {
  'task preview': {
    summary: 'Preview a bare BlueprintHelper.TaskSpec.v1 file through the grouped CLI command.',
    usage: ['bh task preview --file <bare-task-spec.json> --format summary'],
    input: ['Input file root: bare BlueprintHelper.TaskSpec.v1. Do not wrap it in { "task_spec": ... }.'],
    templates: [
      WRITE_INDEX,
      `${TEMPLATE_ROOT}/write/task_preview_bare_taskspec_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_create_blueprint_feature_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_graph_append_container_action_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_graph_append_event_delegate_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_graph_append_generic_schedule_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_graph_append_generic_ops_template.json`,
    ],
  },
  'task execute': {
    summary: 'Execute a bare BlueprintHelper.TaskSpec.v1 file through the grouped CLI command.',
    usage: ['bh task execute --file <bare-task-spec.json> --format summary'],
    input: ['Input file root: bare BlueprintHelper.TaskSpec.v1. Do not wrap it in { "task_spec": ... }.'],
    templates: [
      WRITE_INDEX,
      `${TEMPLATE_ROOT}/write/task_execute_bare_taskspec_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_create_blueprint_feature_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_graph_append_container_action_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_graph_append_event_delegate_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_graph_append_generic_schedule_template.json`,
      `${TEMPLATE_ROOT}/write/taskspec_graph_append_generic_ops_template.json`,
    ],
  },
  'task result': {
    summary: 'Read a task result by task_run_id through the grouped CLI command.',
    usage: ['bh task result --id <task_run_id> --select status,artifacts.full_result'],
    input: ['Pass the task id with --id.'],
    templates: [
      ROOT_INDEX,
      `${TEMPLATE_ROOT}/blueprinthelper_get_task_result_template.json`,
    ],
  },
  'context read': {
    summary: 'Read UE asset context through the grouped CLI command.',
    usage: ['bh context read --file <read-spec.json> --select status,artifacts.full_result'],
    input: ['Input file root: bare BlueprintHelper.ReadSpec.v1.'],
    templates: [
      READ_INDEX,
      `${TEMPLATE_ROOT}/read/read_context_asset_summary_template.json`,
      `${TEMPLATE_ROOT}/read/read_context_function_logic_flow_template.json`,
      `${TEMPLATE_ROOT}/read/read_context_graph_logic_json_template.json`,
    ],
  },
  'bridge ping': {
    summary: 'Ping the running Editor Bridge.',
    usage: ['bh bridge ping --select status,summary'],
    input: ['No JSON input.'],
    notes: ['Use diagnostics commands for richer setup or runtime checks.'],
  },
  'bridge call': {
    summary: 'Call a narrow allowlist of read-only Bridge commands.',
    usage: ['bh bridge call --command <read_only_command> --select status,artifacts.full_result'],
    input: ['No JSON payload. Pass the Bridge command with --command.'],
    notes: ['Ordinary Agent workflows should prefer named tools and templates over bridge call.'],
  },
};

export function buildHelpText(target: string[] = []): string {
  const normalizedTarget = normalizeHelpTarget(target);
  if (normalizedTarget.length === 0) {
    return globalHelpText();
  }

  const key = normalizedTarget.join(' ');
  const groupEntry = groupHelpEntries[key];
  if (groupEntry) {
    return formatEntry(key, groupEntry);
  }

  const toolName = resolveToolAlias(key);
  const toolEntry = helpEntries[toolName];
  if (toolEntry) {
    return formatEntry(toolName, toolEntry);
  }

  return [
    `No tool-specific help is registered for: ${key}`,
    '',
    globalHelpText(),
  ].join('\n');
}

function globalHelpText(): string {
  const defaultTools = [
    'blueprint_get_runtime_profile',
    'blueprinthelper_diagnostics',
    'blueprinthelper_diagnostics_runtime',
    'blueprinthelper_read_agent_guide',
    'blueprinthelper_read_context',
    'blueprinthelper_read_context_capabilities',
    'blueprinthelper_read_reference_context',
    'blueprinthelper_read_function_chain_context',
    'blueprinthelper_find_assets',
    'blueprinthelper_preview_task',
    'blueprinthelper_request_write_session',
    'blueprinthelper_execute_task',
    'blueprinthelper_get_task_result',
    'blueprinthelper_get_debug_case',
    'blueprinthelper_list_debug_cases',
    'blueprinthelper_export_debug_bundle',
    'blueprinthelper_query_review_records',
  ];

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
    '',
    'Template-first input:',
    `  Start at ${TEMPLATE_ROOT}/INDEX.md`,
    `  Support templates: ${ROOT_INDEX}`,
    `  Read templates:    ${READ_INDEX}`,
    `  Write templates:   ${WRITE_INDEX}`,
    '  Copy a matching template, replace placeholders, then pass it with --file.',
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

function formatEntry(name: string, entry: HelpEntry): string {
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
    ...formatOptionalSection('Examples:', entry.examples),
    ...formatOptionalSection('Notes:', entry.notes),
    '',
    'Common options:',
    '  --format summary|json|full',
    '  --fields path[,path...] or --select path[,path...]',
    '  --omit path[,path...]',
    '  --artifact-dir <dir>',
    '  --max-bytes <bytes>',
  ].join('\n');
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

function resolveToolAlias(key: string): string {
  switch (key) {
    case 'open_editor':
      return 'blueprint_open_editor';
    case 'close_editor':
      return 'blueprint_close_editor';
    default:
      return key;
  }
}

function lifecycleHelp(action: 'open' | 'close'): HelpEntry {
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
