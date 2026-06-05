import { toolMetas } from '../registry/tool-metas.js';
import type { ToolAudience, ToolRisk } from '../types.js';
import type {
  CliInvocationTemplateRef,
  ListToolCapabilitiesOptions,
  ListToolDomainsOptions,
  TaskSpecSemanticTemplateRef,
  ToolCapabilityDomain,
  ToolCapabilityItem,
  ToolCapabilityKind,
  ToolCapabilityListResult,
  ToolDomainCatalogItem,
  ToolDomainListResult,
  ToolTemplateDispatchResult,
} from './tool-capability-types.js';

const TEMPLATE_ROOT = 'AgentFaceService/agent-guide/Templates';

const toolMetaByName = new Map(toolMetas.map((meta) => [meta.name, meta]));

const DOMAINS: readonly ToolDomainCatalogItem[] = [
  {
    id: 'blueprint',
    label: 'Blueprint',
    status: 'active',
    default_kinds: ['discover', 'read', 'plan', 'write', 'diagnose'],
    purpose: 'Blueprint graph, function, event, variable, component, and class-setting workflows.',
    available_by_default: true,
  },
  {
    id: 'umg',
    label: 'UMG',
    status: 'active',
    default_kinds: ['read', 'plan', 'write'],
    purpose: 'Widget Blueprint tree and property workflows.',
    available_by_default: true,
  },
  {
    id: 'data',
    label: 'Data',
    status: 'active',
    default_kinds: ['read', 'plan', 'write'],
    purpose: 'DataAsset, DataTable, and UObject property workflows.',
    available_by_default: true,
  },
  {
    id: 'editor',
    label: 'Editor',
    status: 'active',
    default_kinds: ['discover', 'read', 'write', 'diagnose'],
    purpose: 'Runtime profile, diagnostics, screenshot, source-control, and editor evidence workflows.',
    available_by_default: true,
  },
  {
    id: 'project',
    label: 'Project',
    status: 'active',
    default_kinds: ['discover', 'write'],
    purpose: 'AgentGuide, project profile, and write-session workflows.',
    available_by_default: true,
  },
  {
    id: 'debug',
    label: 'Debug',
    status: 'active',
    default_kinds: ['diagnose'],
    purpose: 'Summary-only DebugCase and DebugBundle manifest workflows.',
    available_by_default: true,
  },
  {
    id: 'review',
    label: 'Review',
    status: 'active',
    default_kinds: ['diagnose', 'write'],
    purpose: 'ReviewRecord query workflows; write actions require expert access.',
    available_by_default: true,
  },
  {
    id: 'animation',
    label: 'Animation',
    status: 'reserved',
    default_kinds: [],
    purpose: 'Reserved for future Animation Blueprint tool catalog entries.',
    available_by_default: false,
    reason: 'No Agent-facing tool catalog entry yet.',
  },
  {
    id: 'material',
    label: 'Material',
    status: 'reserved',
    default_kinds: [],
    purpose: 'Reserved for future Material graph tool catalog entries.',
    available_by_default: false,
    reason: 'No Agent-facing tool catalog entry yet.',
  },
];

const CAPABILITIES: readonly ToolCapabilityItem[] = [
  capability('blueprint.discover.assets', 'blueprint', 'discover', 'blueprinthelper_find_assets', 'Resolve unknown Unreal asset paths before reads or writes.', 'blueprint-explorer', 'low', true, false, ['blueprinthelper_find_assets']),
  capability('blueprint.read.context.logic_flow', 'blueprint', 'read', 'blueprinthelper_read_context', 'Read compact execution/data flow for a known function, event, or custom event.', 'blueprint-explorer', 'low', true, false, ['read_context_function_logic_flow']),
  capability('blueprint.read.context.logic_json', 'blueprint', 'read', 'blueprinthelper_read_context', 'Read stable LogicJson anchors for a known graph or block.', 'blueprint-explorer', 'low', true, false, ['read_context_graph_logic_json']),
  capability('blueprint.read.context.components', 'blueprint', 'read', 'blueprinthelper_read_context', 'Read Blueprint component facts and property metadata.', 'blueprint-explorer', 'low', true, false, ['read_context_components']),
  capability('blueprint.read.context.variables', 'blueprint', 'read', 'blueprinthelper_read_context', 'Read Blueprint variable metadata and defaults.', 'blueprint-explorer', 'low', true, false, ['read_context_variables']),
  capability('blueprint.read.reference.dependencies', 'blueprint', 'read', 'blueprinthelper_read_reference_context', 'Read dependency ReferenceContextPack before risky edits.', 'blueprint-explorer', 'low', true, false, ['blueprinthelper_read_reference_context_dependencies']),
  capability('blueprint.read.function_chain', 'blueprint', 'read', 'blueprinthelper_read_function_chain_context', 'Trace project-authored function/event/custom-event calls.', 'blueprint-explorer', 'low', true, false, ['blueprinthelper_read_function_chain_context']),
  capability('blueprint.plan.taskspec.preview', 'blueprint', 'plan', 'blueprinthelper_preview_task', 'Validate and preview a BlueprintHelper.TaskSpec.v1 before execute.', 'task-worker', 'low', false, false, ['blueprinthelper_preview_task_wrapper', 'task_preview_bare_taskspec'], ['taskspec_create_blueprint_feature', 'taskspec_graph_merge_external_flow', 'taskspec_edit_blueprint_variables']),
  capability('blueprint.write.taskspec.execute', 'blueprint', 'write', 'blueprinthelper_execute_task', 'Execute a BlueprintHelper.TaskSpec.v1 after preview and write permission.', 'task-worker', 'high', false, true, ['blueprinthelper_execute_task_wrapper', 'task_execute_bare_taskspec'], ['taskspec_create_blueprint_feature', 'taskspec_graph_merge_external_flow', 'taskspec_edit_blueprint_variables']),

  capability('umg.read.widget_tree', 'umg', 'read', 'blueprinthelper_read_context', 'Read Widget Blueprint tree context.', 'blueprint-explorer', 'low', true, false, ['read_context_widget_tree']),
  capability('umg.read.widget_property', 'umg', 'read', 'blueprinthelper_read_context', 'Read Widget Blueprint property context.', 'blueprint-explorer', 'low', true, false, ['read_context_widget_property']),
  capability('umg.plan.taskspec.preview', 'umg', 'plan', 'blueprinthelper_preview_task', 'Preview UMG TaskSpec changes.', 'task-worker', 'low', false, false, ['blueprinthelper_preview_task_wrapper', 'task_preview_bare_taskspec'], ['taskspec_edit_umg_widget']),
  capability('umg.write.taskspec.execute', 'umg', 'write', 'blueprinthelper_execute_task', 'Execute UMG TaskSpec changes after preview.', 'task-worker', 'high', false, true, ['blueprinthelper_execute_task_wrapper', 'task_execute_bare_taskspec'], ['taskspec_edit_umg_widget']),

  capability('data.read.data_asset', 'data', 'read', 'blueprinthelper_read_context', 'Read DataAsset object property context.', 'blueprint-explorer', 'low', true, false, ['read_context_data_asset']),
  capability('data.read.data_table', 'data', 'read', 'blueprinthelper_read_context', 'Read DataTable or DataTable row context.', 'blueprint-explorer', 'low', true, false, ['read_context_data_table', 'read_context_data_table_row']),
  capability('data.plan.taskspec.preview', 'data', 'plan', 'blueprinthelper_preview_task', 'Preview DataAsset, DataTable, or object-property TaskSpec changes.', 'task-worker', 'low', false, false, ['blueprinthelper_preview_task_wrapper', 'task_preview_bare_taskspec'], ['taskspec_edit_object_properties', 'taskspec_edit_data_table_rows', 'taskspec_create_asset_data_asset', 'taskspec_create_asset_data_table']),
  capability('data.write.taskspec.execute', 'data', 'write', 'blueprinthelper_execute_task', 'Execute DataAsset, DataTable, or object-property TaskSpec changes.', 'task-worker', 'high', false, true, ['blueprinthelper_execute_task_wrapper', 'task_execute_bare_taskspec'], ['taskspec_edit_object_properties', 'taskspec_edit_data_table_rows', 'taskspec_create_asset_data_asset', 'taskspec_create_asset_data_table']),

  capability('editor.read.runtime_profile', 'editor', 'read', 'blueprint_get_runtime_profile', 'Read BlueprintHelper runtime profile from the running Editor Bridge.', 'blueprint-explorer', 'low', true, false, ['blueprint_get_runtime_profile']),
  capability('editor.read.screenshot', 'editor', 'read', 'blueprinthelper_capture_screenshot', 'Capture screenshot evidence for an asset, graph, block, or node.', 'blueprint-explorer', 'low', true, false, ['blueprinthelper_capture_screenshot']),
  capability('editor.read.source_control.status', 'editor', 'read', 'blueprinthelper_source_control_status', 'Read source-control checkout and lock state for assets or files before a write.', 'task-worker', 'low', true, false, ['blueprinthelper_source_control_status']),
  capability('editor.write.source_control.checkout', 'editor', 'write', 'blueprinthelper_source_control_checkout', 'Check out source-controlled assets or files before editing under Perforce/source control.', 'task-worker', 'medium', true, false, ['blueprinthelper_source_control_checkout']),
  capability('editor.diagnose.static', 'editor', 'diagnose', 'blueprinthelper_diagnostics', 'Run static installation/configuration diagnostics.', 'main-agent', 'none', false, false, ['blueprinthelper_diagnostics']),
  capability('editor.diagnose.runtime', 'editor', 'diagnose', 'blueprinthelper_diagnostics_runtime', 'Run runtime diagnostics through the running Editor Bridge.', 'blueprint-explorer', 'low', true, false, ['blueprinthelper_diagnostics_runtime']),

  capability('project.discover.agent_guide', 'project', 'discover', 'blueprinthelper_read_agent_guide', 'Read the AgentGuide onboarding entry.', 'main-agent', 'none', false, false, ['blueprinthelper_read_agent_guide']),
  capability('project.write.write_session', 'project', 'write', 'blueprinthelper_request_write_session', 'Request Editor-approved write permission after preview requires it.', 'task-worker', 'medium', false, false, ['blueprinthelper_request_write_session_project', 'blueprinthelper_request_write_session_assets']),
  capability('project.read.task_result', 'project', 'read', 'blueprinthelper_get_task_result', 'Read a TaskRunJournal by task_run_id.', 'task-worker', 'low', false, false, ['blueprinthelper_get_task_result']),

  capability('debug.diagnose.case', 'debug', 'diagnose', 'blueprinthelper_get_debug_case', 'Read one summary-only DebugCase by id.', 'sourcecode-explorer', 'low', true, false, ['blueprinthelper_get_debug_case']),
  capability('debug.diagnose.list', 'debug', 'diagnose', 'blueprinthelper_list_debug_cases', 'List summary-only DebugCases.', 'sourcecode-explorer', 'low', true, false, ['blueprinthelper_list_debug_cases']),
  capability('debug.diagnose.bundle', 'debug', 'diagnose', 'blueprinthelper_export_debug_bundle', 'Export a local DebugBundle manifest for a DebugCase.', 'sourcecode-explorer', 'low', true, false, ['blueprinthelper_export_debug_bundle']),

  capability('review.diagnose.records', 'review', 'diagnose', 'blueprinthelper_query_review_records', 'Query summary ReviewRecords by asset, task run, archive session, or pending state.', 'sourcecode-explorer', 'low', true, false, ['blueprinthelper_query_review_records']),
  capability('review.write.apply_action', 'review', 'write', 'blueprinthelper_apply_review_action', 'Expert/internal Review action tool for accepting or rejecting ReviewRecord targets.', 'task-worker', 'high', true, true, ['blueprinthelper_apply_review_action']),
];

const CLI_TEMPLATES = new Map<string, CliInvocationTemplateRef>([
  cliTemplate('blueprint_get_runtime_profile', `${TEMPLATE_ROOT}/blueprint_get_runtime_profile_template.json`, 'Runtime profile request.', '{}'),
  cliTemplate('blueprinthelper_diagnostics', `${TEMPLATE_ROOT}/blueprinthelper_diagnostics_template.json`, 'Static diagnostics request.', '{}'),
  cliTemplate('blueprinthelper_diagnostics_runtime', `${TEMPLATE_ROOT}/blueprinthelper_diagnostics_runtime_template.json`, 'Runtime diagnostics request.', '{}'),
  cliTemplate('blueprinthelper_read_agent_guide', `${TEMPLATE_ROOT}/blueprinthelper_read_agent_guide_template.json`, 'AgentGuide onboarding request.', '{}'),
  cliTemplate('blueprinthelper_find_assets', `${TEMPLATE_ROOT}/blueprinthelper_find_assets_template.json`, 'FindAssets request.', 'BlueprintHelper.FindAssetsRequest.v1'),
  cliTemplate('blueprinthelper_capture_screenshot', `${TEMPLATE_ROOT}/blueprinthelper_capture_screenshot_template.json`, 'Screenshot evidence request.', 'BlueprintHelper.CaptureScreenshotRequest.v1'),
  cliTemplate('blueprinthelper_source_control_status', `${TEMPLATE_ROOT}/blueprinthelper_source_control_status_template.json`, 'Source-control status request before a write.', 'BlueprintHelper.SourceControlRequest.v1'),
  cliTemplate('blueprinthelper_source_control_checkout', `${TEMPLATE_ROOT}/blueprinthelper_source_control_checkout_template.json`, 'Source-control checkout request before a write.', 'BlueprintHelper.SourceControlRequest.v1'),
  cliTemplate('blueprinthelper_get_task_result', `${TEMPLATE_ROOT}/blueprinthelper_get_task_result_template.json`, 'Task result request.', '{ "task_run_id": "..." }'),
  cliTemplate('blueprinthelper_request_write_session_project', `${TEMPLATE_ROOT}/blueprinthelper_request_write_session_project_template.json`, 'Project write-session request.', 'BlueprintHelper.WriteSessionRequest.v1'),
  cliTemplate('blueprinthelper_request_write_session_assets', `${TEMPLATE_ROOT}/blueprinthelper_request_write_session_assets_template.json`, 'Asset-list write-session request.', 'BlueprintHelper.WriteSessionRequest.v1'),
  cliTemplate('blueprinthelper_get_debug_case', `${TEMPLATE_ROOT}/blueprinthelper_get_debug_case_template.json`, 'DebugCase request.', '{ "debug_case_id": "..." }'),
  cliTemplate('blueprinthelper_list_debug_cases', `${TEMPLATE_ROOT}/blueprinthelper_list_debug_cases_template.json`, 'DebugCase list request.', '{}'),
  cliTemplate('blueprinthelper_export_debug_bundle', `${TEMPLATE_ROOT}/blueprinthelper_export_debug_bundle_template.json`, 'DebugBundle export request.', '{ "debug_case_id": "..." }'),
  cliTemplate('blueprinthelper_query_review_records', `${TEMPLATE_ROOT}/blueprinthelper_query_review_records_template.json`, 'Review record query request.', 'BlueprintHelper.ReviewQueryRequest.v1'),
  cliTemplate('blueprinthelper_apply_review_action', `${TEMPLATE_ROOT}/blueprinthelper_query_review_records_template.json`, 'Review action request; fill the Review action schema manually if unavailable.', 'BlueprintHelper.ReviewActionRequest.v1'),

  cliTemplate('read_context_function_logic_flow', `${TEMPLATE_ROOT}/read/read_context_function_logic_flow_template.json`, 'Function logic_flow ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_graph_logic_json', `${TEMPLATE_ROOT}/read/read_context_graph_logic_json_template.json`, 'Graph logic_json ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_components', `${TEMPLATE_ROOT}/read/read_context_components_template.json`, 'Component ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_variables', `${TEMPLATE_ROOT}/read/read_context_variables_template.json`, 'Variable ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_widget_tree', `${TEMPLATE_ROOT}/read/read_context_widget_tree_template.json`, 'Widget tree ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_widget_property', `${TEMPLATE_ROOT}/read/read_context_widget_property_template.json`, 'Widget property ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_data_asset', `${TEMPLATE_ROOT}/read/read_context_data_asset_template.json`, 'DataAsset ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_data_table', `${TEMPLATE_ROOT}/read/read_context_data_table_template.json`, 'DataTable ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_data_table_row', `${TEMPLATE_ROOT}/read/read_context_data_table_row_template.json`, 'DataTable row ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('blueprinthelper_read_reference_context_dependencies', `${TEMPLATE_ROOT}/read/blueprinthelper_read_reference_context_dependencies_template.json`, 'ReferenceContext dependency request.', 'BlueprintHelper.ReferenceContextRequest.v1'),
  cliTemplate('blueprinthelper_read_function_chain_context', `${TEMPLATE_ROOT}/read/blueprinthelper_read_function_chain_context_template.json`, 'Function-chain context request.', 'BlueprintHelper.FunctionChainContextRequest.v1'),

  cliTemplate('blueprinthelper_preview_task_wrapper', `${TEMPLATE_ROOT}/write/blueprinthelper_preview_task_wrapper_template.json`, 'Wrapped preview tool request.', '{ "task_spec": BlueprintHelper.TaskSpec.v1 }'),
  cliTemplate('blueprinthelper_execute_task_wrapper', `${TEMPLATE_ROOT}/write/blueprinthelper_execute_task_wrapper_template.json`, 'Wrapped execute tool request.', '{ "task_spec": BlueprintHelper.TaskSpec.v1 }'),
  cliTemplate('task_preview_bare_taskspec', `${TEMPLATE_ROOT}/write/task_preview_bare_taskspec_template.json`, 'Bare TaskSpec preview file.', 'BlueprintHelper.TaskSpec.v1'),
  cliTemplate('task_execute_bare_taskspec', `${TEMPLATE_ROOT}/write/task_execute_bare_taskspec_template.json`, 'Bare TaskSpec execute file.', 'BlueprintHelper.TaskSpec.v1'),
]);

const TASKSPEC_TEMPLATES = new Map<string, TaskSpecSemanticTemplateRef>([
  taskSpecTemplate('taskspec_create_blueprint_feature', `${TEMPLATE_ROOT}/write/taskspec_create_blueprint_feature_template.json`, 'create_blueprint_feature'),
  taskSpecTemplate('taskspec_graph_merge_external_flow', `${TEMPLATE_ROOT}/write/taskspec_graph_merge_external_flow_template.json`, 'merge_external_flow'),
  taskSpecTemplate('taskspec_edit_blueprint_variables', `${TEMPLATE_ROOT}/write/taskspec_edit_blueprint_variables_template.json`, 'edit_blueprint_variables'),
  taskSpecTemplate('taskspec_edit_umg_widget', `${TEMPLATE_ROOT}/write/taskspec_edit_umg_widget_template.json`, 'edit_umg_widget'),
  taskSpecTemplate('taskspec_edit_object_properties', `${TEMPLATE_ROOT}/write/taskspec_edit_object_properties_template.json`, 'edit_object_properties'),
  taskSpecTemplate('taskspec_edit_data_table_rows', `${TEMPLATE_ROOT}/write/taskspec_edit_data_table_rows_template.json`, 'edit_data_table_rows'),
  taskSpecTemplate('taskspec_create_asset_data_asset', `${TEMPLATE_ROOT}/write/taskspec_create_asset_data_asset_template.json`, 'create_asset'),
  taskSpecTemplate('taskspec_create_asset_data_table', `${TEMPLATE_ROOT}/write/taskspec_create_asset_data_table_template.json`, 'create_asset'),
]);

const STOP_CONDITIONS = new Map<string, string[]>([
  ['blueprinthelper_find_assets', ['tool_unavailable', 'bridge_unavailable', 'target asset not found']],
  ['blueprinthelper_read_context', ['tool_unavailable', 'bridge_unavailable', 'target function not found']],
  ['blueprinthelper_read_reference_context', ['tool_unavailable', 'bridge_unavailable', 'reference target not found']],
  ['blueprinthelper_read_function_chain_context', ['tool_unavailable', 'bridge_unavailable', 'entry function not found']],
  ['blueprinthelper_preview_task', ['tool_unavailable', 'write_session_required', 'taskspec_template_unavailable', 'preview_blocked']],
  ['blueprinthelper_execute_task', ['tool_unavailable', 'write_session_required', 'preview_required', 'execute_failed']],
  ['blueprinthelper_request_write_session', ['tool_unavailable', 'preview_required', 'write_permission_denied']],
  ['blueprinthelper_diagnostics', ['tool_unavailable', 'diagnostics_failed']],
  ['blueprinthelper_diagnostics_runtime', ['tool_unavailable', 'bridge_unavailable', 'diagnostics_failed']],
  ['blueprinthelper_source_control_status', ['tool_unavailable', 'bridge_unavailable', 'source_control_unavailable', 'checked_out_by_other', 'source_control_conflicted', 'not_editable']],
  ['blueprinthelper_source_control_checkout', ['tool_unavailable', 'bridge_unavailable', 'source_control_unavailable', 'checked_out_by_other', 'source_control_conflicted', 'checkout_failed', 'not_editable']],
]);

export function listToolDomains(options: ListToolDomainsOptions = {}): ToolDomainListResult {
  const audience = options.audience ?? 'default';
  const active = DOMAINS.filter((domain) => domain.status === 'active');
  const reserved = options.includeReserved
    ? DOMAINS.filter((domain) => domain.status === 'reserved')
    : [];

  return {
    schema: 'BlueprintHelper.ToolDomainList.v1',
    audience,
    items: [...active],
    reserved: [...reserved],
    next: {
      list_command: 'bh tools list <domain> <kind> --format json',
    },
  };
}

export function listToolCapabilities(options: ListToolCapabilitiesOptions): ToolCapabilityListResult {
  const audience = options.audience ?? 'default';
  const risks = new Set(options.risks ?? []);
  const items = CAPABILITIES.filter((capabilityItem) => {
    if (capabilityItem.domain !== options.domain || capabilityItem.kind !== options.kind) {
      return false;
    }
    if (!isAudienceVisible(capabilityItem, audience, options.expert === true)) {
      return false;
    }
    if (options.requiresBridge !== undefined && capabilityItem.requires_bridge !== options.requiresBridge) {
      return false;
    }
    if (risks.size > 0 && !risks.has(capabilityItem.risk)) {
      return false;
    }
    return true;
  });

  return {
    schema: 'BlueprintHelper.ToolCapabilityList.v1',
    query: {
      domain: options.domain,
      kind: options.kind,
      audience,
    },
    items,
    next: {
      templates_command: 'bh tools templates <tool_id> --format json',
    },
  };
}

export function getToolTemplateDispatch(toolId: string): ToolTemplateDispatchResult {
  const capabilityItem = CAPABILITIES.find((entry) => entry.id === toolId);
  if (!capabilityItem) {
    throw new Error(`Unknown BlueprintHelper tool capability id: ${toolId}`);
  }

  const cliInvocationTemplates = capabilityItem.cli_template_ids.map(resolveCliTemplate);
  const taskSpecSemanticTemplates = capabilityItem.taskspec_template_ids.map(resolveTaskSpecTemplate);
  const inputShape = cliInvocationTemplates.find((template) => template.input_shape)?.input_shape;

  return {
    schema: 'BlueprintHelper.ToolTemplateSelection.v1',
    tool_id: capabilityItem.id,
    tool_name: capabilityItem.tool_name,
    cli_invocation_templates: cliInvocationTemplates,
    taskspec_semantic_templates: taskSpecSemanticTemplates,
    input_shape: inputShape,
    recommended_invocation: buildRecommendedInvocation(capabilityItem, cliInvocationTemplates),
    allowed_tools: [capabilityItem.tool_name],
    stop_conditions: resolveStopConditions(capabilityItem),
  };
}

export function isToolCapabilityDomain(value: string): value is ToolCapabilityDomain {
  return DOMAINS.some((domain) => domain.id === value);
}

export function isToolCapabilityKind(value: string): value is ToolCapabilityKind {
  return value === 'discover'
    || value === 'read'
    || value === 'plan'
    || value === 'write'
    || value === 'diagnose';
}

function capability(
  id: string,
  domain: ToolCapabilityDomain,
  kind: ToolCapabilityKind,
  toolName: string,
  purpose: string,
  agentRole: ToolCapabilityItem['agent_role'],
  fallbackRisk: ToolRisk,
  requiresBridge: boolean,
  requiresWriteSession: boolean,
  cliTemplateIds: string[],
  taskspecTemplateIds: string[] = [],
): ToolCapabilityItem {
  const meta = toolMetaByName.get(toolName);
  return {
    id,
    domain,
    kind,
    tool_name: toolName,
    purpose,
    agent_role: agentRole,
    audience: meta?.audience ?? 'default',
    risk: meta?.risk ?? fallbackRisk,
    requires_bridge: requiresBridge,
    requires_write_session: requiresWriteSession,
    cli_template_ids: [...cliTemplateIds],
    taskspec_template_ids: [...taskspecTemplateIds],
  };
}

function cliTemplate(
  cliTemplateId: string,
  path: string,
  recommendedFor: string,
  inputShape: string,
): [string, CliInvocationTemplateRef] {
  return [cliTemplateId, {
    cli_template_id: cliTemplateId,
    path,
    template_kind: 'cli_invocation',
    recommended_for: [recommendedFor],
    input_shape: inputShape,
  }];
}

function taskSpecTemplate(
  taskSpecTemplateId: string,
  path: string,
  taskType: string,
): [string, TaskSpecSemanticTemplateRef] {
  return [taskSpecTemplateId, {
    taskspec_template_id: taskSpecTemplateId,
    path,
    template_kind: 'taskspec_semantic',
    recommended_for: [taskType],
    task_type: taskType,
  }];
}

function resolveCliTemplate(templateId: string): CliInvocationTemplateRef {
  const template = CLI_TEMPLATES.get(templateId);
  if (!template) {
    throw new Error(`Unknown BlueprintHelper CLI invocation template id: ${templateId}`);
  }
  return template;
}

function resolveTaskSpecTemplate(templateId: string): TaskSpecSemanticTemplateRef {
  const template = TASKSPEC_TEMPLATES.get(templateId);
  if (!template) {
    throw new Error(`Unknown BlueprintHelper TaskSpec semantic template id: ${templateId}`);
  }
  return template;
}

function isAudienceVisible(
  capabilityItem: ToolCapabilityItem,
  audience: ToolAudience,
  expert: boolean,
): boolean {
  if (capabilityItem.audience === 'expert') {
    return audience === 'expert' && expert;
  }
  if (capabilityItem.audience === 'compat') {
    return audience === 'compat';
  }
  return true;
}

function buildRecommendedInvocation(
  capabilityItem: ToolCapabilityItem,
  templates: CliInvocationTemplateRef[],
): string {
  const templatePath = templates[0]?.path ?? '<filled-template.json>';
  if (capabilityItem.tool_name === 'blueprinthelper_preview_task') {
    return 'bh task preview --file <filled_taskspec.json> --format summary';
  }
  if (capabilityItem.tool_name === 'blueprinthelper_execute_task') {
    return 'bh task execute --file <filled_taskspec.json> --preview-token <preview_token> --format summary';
  }
  return `bh ${capabilityItem.tool_name} --file ${templatePath} --select status,artifacts.full_result`;
}

function resolveStopConditions(capabilityItem: ToolCapabilityItem): string[] {
  const specific = STOP_CONDITIONS.get(capabilityItem.tool_name);
  if (specific) {
    return [...specific];
  }
  return capabilityItem.requires_bridge
    ? ['tool_unavailable', 'bridge_unavailable']
    : ['tool_unavailable'];
}
