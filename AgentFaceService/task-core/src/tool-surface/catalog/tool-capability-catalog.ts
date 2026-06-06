import { toolMetas } from '../registry/tool-metas.js';
import { createToolsTemplateBuilderCore } from '../manifest/tools-template-builder-core.js';
import { buildReadonlyToolCommandManifestRegistry } from '../manifest/tool-command-manifest-builder.js';
import { getGraphWriteRoutesForTemplateDiscovery } from '../../task/compiler/graphwrite/graphwrite-route-registry.js';
import { getGraphWriteSlotsForTemplateDiscovery } from '../../task/compiler/graphwrite/graphwrite-slot-registry.js';
import {
  buildDescriptorRecommendedInvocation,
  createToolCapabilityDescriptorRegistry,
} from './tool-capability-descriptor-registry.js';
import type { ToolCapabilityDescriptor } from './tool-capability-descriptor-registry.js';
import type { ToolAudience, ToolRisk } from '../types.js';
import type {
  CliInvocationTemplateRef,
  GetToolTemplateDispatchOptions,
  ListToolCapabilitiesOptions,
  ListToolDomainsOptions,
  ToolCapabilityDomain,
  ToolCapabilityItem,
  ToolCapabilityKind,
  ToolCapabilityListResult,
  ToolDomainCatalogItem,
  ToolDomainListResult,
  ToolTemplateDispatchResult,
  ToolTemplateRouteKind,
  ToolTemplateRouteRef,
  ToolTemplateSlotKind,
  ToolTemplateSlotRef,
} from './tool-capability-types.js';

const TEMPLATE_ROOT = 'AgentFaceService/agent-guide/Templates';
const READ_ROUTE_TEMPLATE_ROOT = `${TEMPLATE_ROOT}/read/routes`;
const WRITE_ROUTE_TEMPLATE_ROOT = `${TEMPLATE_ROOT}/write/routes`;

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
  capability('blueprint.read.context.logic_flow', 'blueprint', 'read', 'blueprinthelper_read_context', 'Read compact execution/data flow for a known function, event, or custom event.', 'blueprint-explorer', 'low', true, false, ['read_context_function_logic_flow', 'read_context_event_logic_flow', 'read_context_custom_event_logic_flow']),
  capability('blueprint.read.context.logic_json', 'blueprint', 'read', 'blueprinthelper_read_context', 'Read stable LogicJson anchors for a known graph or block.', 'blueprint-explorer', 'low', true, false, ['read_context_graph_logic_json', 'read_context_block_logic_json']),
  capability('blueprint.read.context.components', 'blueprint', 'read', 'blueprinthelper_read_context', 'Read Blueprint component facts and property metadata.', 'blueprint-explorer', 'low', true, false, ['read_context_components']),
  capability('blueprint.read.context.variables', 'blueprint', 'read', 'blueprinthelper_read_context', 'Read Blueprint variable metadata and defaults.', 'blueprint-explorer', 'low', true, false, ['read_context_variables']),
  capability('blueprint.read.reference.dependencies', 'blueprint', 'read', 'blueprinthelper_read_reference_context', 'Read dependency ReferenceContextPack before risky edits.', 'blueprint-explorer', 'low', true, false, ['blueprinthelper_read_reference_context_dependencies']),
  capability('blueprint.read.function_chain', 'blueprint', 'read', 'blueprinthelper_read_function_chain_context', 'Trace project-authored function/event/custom-event calls.', 'blueprint-explorer', 'low', true, false, ['blueprinthelper_read_function_chain_context']),
  capability('blueprint.plan.taskspec.preview', 'blueprint', 'plan', 'blueprinthelper_preview_task', 'Validate and preview a BlueprintHelper.TaskSpec.v1 before execute.', 'task-worker', 'low', false, false, ['blueprinthelper_preview_task_wrapper', 'task_preview_bare_taskspec']),
  capability('blueprint.write.taskspec.execute', 'blueprint', 'write', 'blueprinthelper_execute_task', 'Execute a BlueprintHelper.TaskSpec.v1 after preview and write permission.', 'task-worker', 'high', false, true, ['blueprinthelper_execute_task_wrapper', 'task_execute_bare_taskspec']),
  capability('blueprint.diagnose.compile', 'blueprint', 'diagnose', 'blueprint_compile_blueprint', 'Compile an explicit Blueprint asset through the running Editor Bridge for validation.', 'task-worker', 'medium', true, true, ['blueprint_compile_blueprint']),

  capability('umg.read.widget_tree', 'umg', 'read', 'blueprinthelper_read_context', 'Read Widget Blueprint tree context.', 'blueprint-explorer', 'low', true, false, ['read_context_widget_tree']),
  capability('umg.read.widget_property', 'umg', 'read', 'blueprinthelper_read_context', 'Read Widget Blueprint property context.', 'blueprint-explorer', 'low', true, false, ['read_context_widget_property']),
  capability('umg.plan.taskspec.preview', 'umg', 'plan', 'blueprinthelper_preview_task', 'Preview UMG TaskSpec changes.', 'task-worker', 'low', false, false, ['blueprinthelper_preview_task_wrapper', 'task_preview_bare_taskspec']),
  capability('umg.write.taskspec.execute', 'umg', 'write', 'blueprinthelper_execute_task', 'Execute UMG TaskSpec changes after preview.', 'task-worker', 'high', false, true, ['blueprinthelper_execute_task_wrapper', 'task_execute_bare_taskspec']),

  capability('data.read.data_asset', 'data', 'read', 'blueprinthelper_read_context', 'Read DataAsset object property context.', 'blueprint-explorer', 'low', true, false, ['read_context_data_asset']),
  capability('data.read.data_table', 'data', 'read', 'blueprinthelper_read_context', 'Read DataTable or DataTable row context.', 'blueprint-explorer', 'low', true, false, ['read_context_data_table', 'read_context_data_table_row']),
  capability('data.plan.taskspec.preview', 'data', 'plan', 'blueprinthelper_preview_task', 'Preview DataAsset, DataTable, or object-property TaskSpec changes.', 'task-worker', 'low', false, false, ['blueprinthelper_preview_task_wrapper', 'task_preview_bare_taskspec']),
  capability('data.write.taskspec.execute', 'data', 'write', 'blueprinthelper_execute_task', 'Execute DataAsset, DataTable, or object-property TaskSpec changes.', 'task-worker', 'high', false, true, ['blueprinthelper_execute_task_wrapper', 'task_execute_bare_taskspec']),

  capability('editor.read.runtime_profile', 'editor', 'read', 'blueprint_get_runtime_profile', 'Read BlueprintHelper runtime profile from the running Editor Bridge.', 'blueprint-explorer', 'low', true, false, ['blueprint_get_runtime_profile']),
  capability('editor.read.screenshot', 'editor', 'read', 'blueprinthelper_capture_screenshot', 'Capture screenshot evidence for an asset, graph, block, or node.', 'blueprint-explorer', 'low', true, false, ['blueprinthelper_capture_screenshot']),
  capability('editor.read.source_control.status', 'editor', 'read', 'blueprinthelper_source_control_status', 'Read source-control checkout and lock state for assets or files before a write.', 'task-worker', 'low', true, false, ['blueprinthelper_source_control_status']),
  capability('editor.write.source_control.checkout', 'editor', 'write', 'blueprinthelper_source_control_checkout', 'Check out source-controlled assets or files before editing under Perforce/source control.', 'task-worker', 'medium', true, false, ['blueprinthelper_source_control_checkout']),
  capability('editor.write.asset.save', 'editor', 'write', 'blueprint_save_asset', 'Persist an explicit Unreal asset package after write-session and source-control checks.', 'task-worker', 'high', true, true, ['blueprint_save_asset']),
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
  cliTemplate('blueprint_compile_blueprint', `${TEMPLATE_ROOT}/blueprint_compile_blueprint_template.json`, 'Explicit Blueprint compile validation request.', '{ "target_blueprint": "/Game/Path/BP_Target.BP_Target" }'),
  cliTemplate('blueprint_save_asset', `${TEMPLATE_ROOT}/blueprint_save_asset_template.json`, 'Explicit asset save request.', '{ "asset_path": "/Game/Path/BP_Target.BP_Target" }'),
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

  cliTemplate('read_context_function_logic_flow', `${READ_ROUTE_TEMPLATE_ROOT}/blueprint_logic_function_logic_flow_template.json`, 'Function logic_flow ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_event_logic_flow', `${READ_ROUTE_TEMPLATE_ROOT}/blueprint_logic_event_logic_flow_template.json`, 'Event logic_flow ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_custom_event_logic_flow', `${READ_ROUTE_TEMPLATE_ROOT}/blueprint_logic_custom_event_logic_flow_template.json`, 'Custom event logic_flow ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_graph_logic_json', `${READ_ROUTE_TEMPLATE_ROOT}/blueprint_logic_graph_logic_json_template.json`, 'Graph logic_json ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_block_logic_json', `${READ_ROUTE_TEMPLATE_ROOT}/blueprint_logic_block_logic_json_template.json`, 'Block logic_json ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_components', `${READ_ROUTE_TEMPLATE_ROOT}/blueprint_components_template.json`, 'Component ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_variables', `${READ_ROUTE_TEMPLATE_ROOT}/blueprint_variables_template.json`, 'Variable ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_widget_tree', `${READ_ROUTE_TEMPLATE_ROOT}/widget_tree_template.json`, 'Widget tree ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_widget_property', `${READ_ROUTE_TEMPLATE_ROOT}/widget_property_template.json`, 'Widget property ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_data_asset', `${READ_ROUTE_TEMPLATE_ROOT}/data_asset_object_template.json`, 'DataAsset ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_data_table', `${READ_ROUTE_TEMPLATE_ROOT}/data_table_template.json`, 'DataTable ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('read_context_data_table_row', `${READ_ROUTE_TEMPLATE_ROOT}/data_table_row_template.json`, 'DataTable row ReadSpec.', 'BlueprintHelper.ReadSpec.v1'),
  cliTemplate('blueprinthelper_read_reference_context_dependencies', `${TEMPLATE_ROOT}/read/blueprinthelper_read_reference_context_dependencies_template.json`, 'ReferenceContext dependency request.', 'BlueprintHelper.ReferenceContextRequest.v1'),
  cliTemplate('blueprinthelper_read_function_chain_context', `${TEMPLATE_ROOT}/read/blueprinthelper_read_function_chain_context_template.json`, 'Function-chain context request.', 'BlueprintHelper.FunctionChainContextRequest.v1'),

  cliTemplate('blueprinthelper_preview_task_wrapper', `${TEMPLATE_ROOT}/write/blueprinthelper_preview_task_wrapper_template.json`, 'Wrapped preview tool request.', '{ "task_spec": BlueprintHelper.TaskSpec.v1 }'),
  cliTemplate('blueprinthelper_execute_task_wrapper', `${TEMPLATE_ROOT}/write/blueprinthelper_execute_task_wrapper_template.json`, 'Wrapped execute tool request.', '{ "task_spec": BlueprintHelper.TaskSpec.v1 }'),
  cliTemplate('task_preview_bare_taskspec', `${TEMPLATE_ROOT}/write/task_preview_bare_taskspec_template.json`, 'Bare TaskSpec preview file.', 'BlueprintHelper.TaskSpec.v1'),
  cliTemplate('task_execute_bare_taskspec', `${TEMPLATE_ROOT}/write/task_execute_bare_taskspec_template.json`, 'Bare TaskSpec execute file.', 'BlueprintHelper.TaskSpec.v1'),
]);

function route(
  routeId: string,
  routeKind: ToolTemplateRouteKind,
  purpose: string,
  templatePaths: string[],
  requiredFields: string[],
  optionalFields: string[],
  insertPaths: string[],
  whenToUse?: string,
  whenNotToUse?: string,
): ToolTemplateRouteRef {
  return {
    route_id: routeId,
    route_kind: routeKind,
    purpose,
    template_paths: templatePaths,
    required_fields: requiredFields,
    optional_fields: optionalFields,
    insert_paths: insertPaths,
    when_to_use: whenToUse,
    when_not_to_use: whenNotToUse,
  };
}

function slot(
  slotId: string,
  slotType: ToolTemplateSlotKind,
  path: string,
  appliesToRoutes: string[],
  insertPath: string,
  keywords: string[],
  whenToUse: string,
  whenNotToUse?: string,
): ToolTemplateSlotRef {
  return {
    slot_id: slotId,
    slot_type: slotType,
    path,
    applies_to_routes: appliesToRoutes,
    insert_path: insertPath,
    keywords,
    when_to_use: whenToUse,
    when_not_to_use: whenNotToUse,
  };
}

const GRAPHWRITE_TEMPLATE_DISCOVERY_ROUTES = getGraphWriteRoutesForTemplateDiscovery();
const GRAPHWRITE_ROUTES: readonly ToolTemplateRouteRef[] = GRAPHWRITE_TEMPLATE_DISCOVERY_ROUTES.map((descriptor) => route(
  descriptor.route_id,
  'graph_write',
  descriptor.purpose,
  descriptor.template_path ? [descriptor.template_path] : [],
  [...descriptor.required_fields],
  [...descriptor.optional_fields],
  [...descriptor.insert_paths],
  descriptor.when_to_use,
  descriptor.when_not_to_use,
));

const BLUEPRINT_TASKSPEC_ROUTES: readonly ToolTemplateRouteRef[] = [
  route(
    'blueprint.create_feature',
    'taskspec',
    'Create or extend a Blueprint feature through the composite TaskSpec surface.',
    [`${WRITE_ROUTE_TEMPLATE_ROOT}/blueprint_create_feature_template.json`],
    ['schema=BlueprintHelper.TaskSpec.v1', 'task_type=create_blueprint_feature', 'target.asset_path', 'behavior'],
    [],
    ['behavior'],
    'Use for composite Blueprint feature creation when one TaskSpec should coordinate signatures, variables, and graph writes.',
  ),
  ...GRAPHWRITE_ROUTES,
  route(
    'blueprint.variables.edit',
    'taskspec',
    'Edit Blueprint variables through the variable TaskSpec surface.',
    [`${WRITE_ROUTE_TEMPLATE_ROOT}/blueprint_edit_variables_template.json`],
    ['schema=BlueprintHelper.TaskSpec.v1', 'task_type=edit_blueprint_variables', 'target.asset_path', 'behavior.changes[]'],
    [],
    ['behavior.changes[]'],
    'Use for member variable, local variable, default, and replication edits.',
  ),
];

const UMG_TASKSPEC_ROUTES: readonly ToolTemplateRouteRef[] = [
  route(
    'umg.widget.edit',
    'taskspec',
    'Edit Widget Blueprint tree or properties through the UMG TaskSpec surface.',
    [`${WRITE_ROUTE_TEMPLATE_ROOT}/umg_widget_edit_template.json`],
    ['schema=BlueprintHelper.TaskSpec.v1', 'task_type=edit_umg_widget', 'target.asset_path', 'behavior.changes[]'],
    [],
    ['behavior.changes[]'],
    'Use for Widget Blueprint create, update, delete, or property changes.',
  ),
];

const DATA_TASKSPEC_ROUTES: readonly ToolTemplateRouteRef[] = [
  route(
    'data.object_properties.edit',
    'taskspec',
    'Edit object or DataAsset properties through the object-property TaskSpec surface.',
    [`${WRITE_ROUTE_TEMPLATE_ROOT}/data_object_properties_edit_template.json`],
    ['schema=BlueprintHelper.TaskSpec.v1', 'task_type=edit_object_properties', 'target.asset_path', 'behavior.changes[]'],
    [],
    ['behavior.changes[]'],
    'Use for DataAsset or UObject property edits.',
  ),
  route(
    'data.data_table.rows.edit',
    'taskspec',
    'Edit DataTable rows through the row TaskSpec surface.',
    [`${WRITE_ROUTE_TEMPLATE_ROOT}/data_table_rows_edit_template.json`],
    ['schema=BlueprintHelper.TaskSpec.v1', 'task_type=edit_data_table', 'target.asset_path', 'behavior.rows[]'],
    [],
    ['behavior.rows[]'],
    'Use for adding, updating, or deleting DataTable rows.',
  ),
  route(
    'data.asset.data_asset.create',
    'taskspec',
    'Create a DataAsset asset through the asset factory TaskSpec surface.',
    [`${WRITE_ROUTE_TEMPLATE_ROOT}/data_asset_create_template.json`],
    ['schema=BlueprintHelper.TaskSpec.v1', 'task_type=create_asset', 'target.asset_path', 'behavior.asset_class'],
    [],
    ['behavior'],
    'Use for creating a DataAsset when the class is known.',
  ),
  route(
    'data.asset.data_table.create',
    'taskspec',
    'Create a DataTable asset through the asset factory TaskSpec surface.',
    [`${WRITE_ROUTE_TEMPLATE_ROOT}/data_table_create_template.json`],
    ['schema=BlueprintHelper.TaskSpec.v1', 'task_type=create_asset', 'target.asset_path', 'behavior.row_struct'],
    [],
    ['behavior'],
    'Use for creating a DataTable when the row struct is known.',
  ),
];

const READ_CONTEXT_ROUTES: readonly ToolTemplateRouteRef[] = [
  route(
    'read.blueprint.logic.function.logic_flow',
    'read_context',
    'Read compact function execution/data flow.',
    [`${READ_ROUTE_TEMPLATE_ROOT}/blueprint_logic_function_logic_flow_template.json`],
    ['schema=BlueprintHelper.ReadSpec.v1', 'read_type=blueprint_logic', 'target.asset_path', 'target.target_type=function', 'target.target_name', 'view.format=logic_flow'],
    ['view.max_items', 'view.detail'],
    ['target', 'view'],
    'Use before editing or replacing a known function body.',
  ),
  route(
    'read.blueprint.logic.event.logic_flow',
    'read_context',
    'Read compact event execution/data flow.',
    [`${READ_ROUTE_TEMPLATE_ROOT}/blueprint_logic_event_logic_flow_template.json`],
    ['schema=BlueprintHelper.ReadSpec.v1', 'read_type=blueprint_logic', 'target.asset_path', 'target.target_type=event', 'target.target_name', 'view.format=logic_flow'],
    ['view.max_items', 'view.detail'],
    ['target', 'view'],
    'Use before editing or replacing a known event body.',
  ),
  route(
    'read.blueprint.logic.custom_event.logic_flow',
    'read_context',
    'Read compact custom event execution/data flow.',
    [`${READ_ROUTE_TEMPLATE_ROOT}/blueprint_logic_custom_event_logic_flow_template.json`],
    ['schema=BlueprintHelper.ReadSpec.v1', 'read_type=blueprint_logic', 'target.asset_path', 'target.target_type=custom_event', 'target.target_name', 'view.format=logic_flow'],
    ['view.max_items', 'view.detail'],
    ['target', 'view'],
    'Use before editing or replacing a known custom event body.',
  ),
  route(
    'read.blueprint.logic.graph.logic_json',
    'read_context',
    'Read graph LogicJson anchors and references.',
    [`${READ_ROUTE_TEMPLATE_ROOT}/blueprint_logic_graph_logic_json_template.json`],
    ['schema=BlueprintHelper.ReadSpec.v1', 'read_type=blueprint_logic', 'target.asset_path', 'target.target_type=graph', 'target.target_name', 'view.format=logic_json'],
    ['view.max_items', 'view.detail'],
    ['target', 'view'],
    'Use when a write needs stable graph anchors or node/link refs.',
  ),
  route(
    'read.blueprint.logic.block.logic_json',
    'read_context',
    'Read block LogicJson anchors and references.',
    [`${READ_ROUTE_TEMPLATE_ROOT}/blueprint_logic_block_logic_json_template.json`],
    ['schema=BlueprintHelper.ReadSpec.v1', 'read_type=blueprint_logic', 'target.asset_path', 'target.target_type=block', 'target.target_name', 'view.format=logic_json'],
    ['view.max_items', 'view.detail'],
    ['target', 'view'],
    'Use when a write needs stable anchors or pin refs for one owned block.',
  ),
  route(
    'read.blueprint.variables',
    'read_context',
    'Read Blueprint variable and dispatcher context.',
    [`${READ_ROUTE_TEMPLATE_ROOT}/blueprint_variables_template.json`],
    ['schema=BlueprintHelper.ReadSpec.v1', 'read_type=variable_context', 'target.asset_path', 'target.target_type=member_variable'],
    ['target.target_name'],
    ['target'],
    'Use before variable reads, writes, or default edits.',
  ),
  route(
    'read.blueprint.components',
    'read_context',
    'Read Blueprint component facts.',
    [`${READ_ROUTE_TEMPLATE_ROOT}/blueprint_components_template.json`],
    ['schema=BlueprintHelper.ReadSpec.v1', 'read_type=component_context', 'target.asset_path'],
    ['target.target_type', 'target.target_name'],
    ['target'],
    'Use before component refs, component property reads, or component edits.',
  ),
  route(
    'read.widget.tree',
    'read_context',
    'Read Widget Blueprint tree.',
    [`${READ_ROUTE_TEMPLATE_ROOT}/widget_tree_template.json`],
    ['schema=BlueprintHelper.ReadSpec.v1', 'read_type=widget_context', 'target.asset_path', 'target.target_type=blueprint'],
    ['view.detail'],
    ['target', 'view'],
    'Use before UMG tree edits.',
  ),
  route(
    'read.widget.property',
    'read_context',
    'Read one Widget Blueprint widget property context.',
    [`${READ_ROUTE_TEMPLATE_ROOT}/widget_property_template.json`],
    ['schema=BlueprintHelper.ReadSpec.v1', 'read_type=widget_context', 'target.asset_path', 'target.target_type=widget', 'target.target_name'],
    ['view.detail'],
    ['target', 'view'],
    'Use before UMG widget property edits.',
  ),
  route(
    'read.data_asset.object',
    'read_context',
    'Read DataAsset object property context.',
    [`${READ_ROUTE_TEMPLATE_ROOT}/data_asset_object_template.json`],
    ['schema=BlueprintHelper.ReadSpec.v1', 'read_type=data_asset_context', 'target.asset_path', 'target.target_type=data_asset'],
    ['view.detail'],
    ['target', 'view'],
    'Use before DataAsset object property edits.',
  ),
  route(
    'read.data_table.table',
    'read_context',
    'Read DataTable rows and schema.',
    [`${READ_ROUTE_TEMPLATE_ROOT}/data_table_template.json`],
    ['schema=BlueprintHelper.ReadSpec.v1', 'read_type=data_table_context', 'target.asset_path'],
    ['target.row_name', 'view.detail'],
    ['target', 'view'],
    'Use before DataTable row edits.',
  ),
  route(
    'read.data_table.row',
    'read_context',
    'Read a specific DataTable row.',
    [`${READ_ROUTE_TEMPLATE_ROOT}/data_table_row_template.json`],
    ['schema=BlueprintHelper.ReadSpec.v1', 'read_type=data_table_context', 'target.asset_path', 'target.row_name'],
    ['view.detail'],
    ['target', 'view'],
    'Use when the target row is known.',
  ),
];

const GRAPHWRITE_SLOT_TEMPLATE_REFS: readonly ToolTemplateSlotRef[] = uniqueGraphWriteSlotTemplateRefs(
  GRAPHWRITE_TEMPLATE_DISCOVERY_ROUTES.flatMap((descriptor) =>
    getGraphWriteSlotsForTemplateDiscovery(descriptor.route_id)),
);

function uniqueGraphWriteSlotTemplateRefs(slotRefs: readonly ToolTemplateSlotRef[]): ToolTemplateSlotRef[] {
  return [...new Map(slotRefs.map((slotRef) => [slotRef.slot_id, slotRef])).values()];
}

const READ_CONTEXT_SLOTS: readonly ToolTemplateSlotRef[] = [
  slot(
    'read.target.asset',
    'target',
    `${TEMPLATE_ROOT}/read/slots/read_target_asset_template.json`,
    READ_CONTEXT_ROUTES.map((entry) => entry.route_id),
    'target',
    ['target', 'asset_path'],
    'Use to fill the asset path for any ReadContext route.',
  ),
  slot(
    'read.target.function',
    'target',
    `${TEMPLATE_ROOT}/read/slots/read_target_function_template.json`,
    ['read.blueprint.logic.function.logic_flow'],
    'target',
    ['target', 'function', 'target_name'],
    'Use when reading a known function graph.',
  ),
  slot(
    'read.target.named_logic',
    'target',
    `${TEMPLATE_ROOT}/read/slots/read_target_named_logic_template.json`,
    [
      'read.blueprint.logic.function.logic_flow',
      'read.blueprint.logic.event.logic_flow',
      'read.blueprint.logic.custom_event.logic_flow',
      'read.blueprint.logic.graph.logic_json',
      'read.blueprint.logic.block.logic_json',
    ],
    'target',
    ['target', 'logic', 'target_name'],
    'Use when reading a named function, event, custom event, graph, or block.',
  ),
  slot(
    'read.target.widget',
    'target',
    `${TEMPLATE_ROOT}/read/slots/read_target_widget_template.json`,
    ['read.widget.tree', 'read.widget.property'],
    'target',
    ['target', 'widget'],
    'Use when reading Widget Blueprint tree or a named widget property.',
  ),
  slot(
    'read.target.data_table_row',
    'target',
    `${TEMPLATE_ROOT}/read/slots/read_target_data_table_row_template.json`,
    ['read.data_table.row'],
    'target',
    ['target', 'data_table', 'row_name'],
    'Use when reading a specific DataTable row.',
  ),
  slot(
    'read.view.logic_flow',
    'view',
    `${TEMPLATE_ROOT}/read/slots/read_view_logic_flow_template.json`,
    ['read.blueprint.logic.function.logic_flow', 'read.blueprint.logic.event.logic_flow', 'read.blueprint.logic.custom_event.logic_flow'],
    'view',
    ['view', 'logic_flow'],
    'Use for compact flow-oriented Blueprint logic reads.',
  ),
  slot(
    'read.view.logic_json',
    'view',
    `${TEMPLATE_ROOT}/read/slots/read_view_logic_json_template.json`,
    ['read.blueprint.logic.graph.logic_json', 'read.blueprint.logic.block.logic_json'],
    'view',
    ['view', 'logic_json', 'anchors'],
    'Use when a later write needs stable anchors or pin refs.',
  ),
];

interface ToolCapabilityDescriptorOptions {
  readonly route_refs?: readonly ToolTemplateRouteRef[];
  readonly slot_refs?: readonly ToolTemplateSlotRef[];
  readonly stop_conditions?: readonly string[];
  readonly recommended_invocations?: readonly string[];
}

const DEFAULT_LOCAL_STOP_CONDITIONS = ['tool_unavailable'] as const;
const DEFAULT_BRIDGE_STOP_CONDITIONS = ['tool_unavailable', 'bridge_unavailable'] as const;
const FIND_ASSETS_STOP_CONDITIONS = ['tool_unavailable', 'bridge_unavailable', 'target asset not found'] as const;
const READ_CONTEXT_STOP_CONDITIONS = ['tool_unavailable', 'bridge_unavailable', 'target not found', 'target asset not found', 'target graph not found'] as const;
const READ_REFERENCE_STOP_CONDITIONS = ['tool_unavailable', 'bridge_unavailable', 'reference target not found'] as const;
const FUNCTION_CHAIN_STOP_CONDITIONS = ['tool_unavailable', 'bridge_unavailable', 'entry function not found'] as const;
const PREVIEW_STOP_CONDITIONS = ['tool_unavailable', 'write_session_required', 'taskspec_template_unavailable', 'preview_blocked'] as const;
const EXECUTE_STOP_CONDITIONS = ['tool_unavailable', 'write_session_required', 'preview_required', 'execute_failed'] as const;
const WRITE_SESSION_STOP_CONDITIONS = ['tool_unavailable', 'preview_required', 'write_permission_denied'] as const;
const DIAGNOSTICS_STOP_CONDITIONS = ['tool_unavailable', 'diagnostics_failed'] as const;
const RUNTIME_DIAGNOSTICS_STOP_CONDITIONS = ['tool_unavailable', 'bridge_unavailable', 'diagnostics_failed'] as const;
const COMPILE_STOP_CONDITIONS = ['tool_unavailable', 'bridge_unavailable', 'write_session_required', 'compile_failed', 'target_asset_not_found', 'tool_failed'] as const;
const SAVE_STOP_CONDITIONS = ['tool_unavailable', 'bridge_unavailable', 'write_session_required', 'source_control_unavailable', 'checked_out_by_other', 'source_control_conflicted', 'checkout_required', 'not_editable', 'save_failed', 'target_asset_not_found', 'tool_failed'] as const;
const SOURCE_CONTROL_STATUS_STOP_CONDITIONS = ['tool_unavailable', 'bridge_unavailable', 'source_control_unavailable', 'checked_out_by_other', 'source_control_conflicted', 'not_editable'] as const;
const SOURCE_CONTROL_CHECKOUT_STOP_CONDITIONS = ['tool_unavailable', 'bridge_unavailable', 'source_control_unavailable', 'checked_out_by_other', 'source_control_conflicted', 'checkout_failed', 'not_editable'] as const;
const TASK_PREVIEW_INVOCATION = ['bh task preview --file <filled_taskspec.json> --format summary'] as const;
const TASK_EXECUTE_INVOCATION = ['bh task execute --file <filled_taskspec.json> --preview-token <preview_token> --format summary'] as const;

function createDescriptorRegistry() {
  const descriptorOptions = buildDescriptorOptionsByCapabilityId();
  return createToolCapabilityDescriptorRegistry({
    descriptors: CAPABILITIES.map((capabilityItem) => toToolCapabilityDescriptor(
      capabilityItem,
      descriptorOptions.get(capabilityItem.id),
    )),
  });
}

function buildDescriptorOptionsByCapabilityId(): Map<string, ToolCapabilityDescriptorOptions> {
  return new Map<string, ToolCapabilityDescriptorOptions>([
    ['blueprint.discover.assets', { stop_conditions: FIND_ASSETS_STOP_CONDITIONS }],
    ['blueprint.read.context.logic_flow', {
      route_refs: readRoutesById([
        'read.blueprint.logic.function.logic_flow',
        'read.blueprint.logic.event.logic_flow',
        'read.blueprint.logic.custom_event.logic_flow',
      ]),
      slot_refs: READ_CONTEXT_SLOTS,
      stop_conditions: READ_CONTEXT_STOP_CONDITIONS,
    }],
    ['blueprint.read.context.logic_json', {
      route_refs: readRoutesById([
        'read.blueprint.logic.graph.logic_json',
        'read.blueprint.logic.block.logic_json',
      ]),
      slot_refs: READ_CONTEXT_SLOTS,
      stop_conditions: READ_CONTEXT_STOP_CONDITIONS,
    }],
    ['blueprint.read.context.components', {
      route_refs: readRoutesById(['read.blueprint.components']),
      slot_refs: READ_CONTEXT_SLOTS,
      stop_conditions: READ_CONTEXT_STOP_CONDITIONS,
    }],
    ['blueprint.read.context.variables', {
      route_refs: readRoutesById(['read.blueprint.variables']),
      slot_refs: READ_CONTEXT_SLOTS,
      stop_conditions: READ_CONTEXT_STOP_CONDITIONS,
    }],
    ['blueprint.read.reference.dependencies', { stop_conditions: READ_REFERENCE_STOP_CONDITIONS }],
    ['blueprint.read.function_chain', { stop_conditions: FUNCTION_CHAIN_STOP_CONDITIONS }],
    ['blueprint.plan.taskspec.preview', {
      route_refs: BLUEPRINT_TASKSPEC_ROUTES,
      slot_refs: GRAPHWRITE_SLOT_TEMPLATE_REFS,
      stop_conditions: PREVIEW_STOP_CONDITIONS,
      recommended_invocations: TASK_PREVIEW_INVOCATION,
    }],
    ['blueprint.write.taskspec.execute', {
      route_refs: BLUEPRINT_TASKSPEC_ROUTES,
      slot_refs: GRAPHWRITE_SLOT_TEMPLATE_REFS,
      stop_conditions: EXECUTE_STOP_CONDITIONS,
      recommended_invocations: TASK_EXECUTE_INVOCATION,
    }],
    ['blueprint.diagnose.compile', { stop_conditions: COMPILE_STOP_CONDITIONS }],

    ['umg.read.widget_tree', {
      route_refs: readRoutesById(['read.widget.tree']),
      slot_refs: READ_CONTEXT_SLOTS,
      stop_conditions: READ_CONTEXT_STOP_CONDITIONS,
    }],
    ['umg.read.widget_property', {
      route_refs: readRoutesById(['read.widget.property']),
      slot_refs: READ_CONTEXT_SLOTS,
      stop_conditions: READ_CONTEXT_STOP_CONDITIONS,
    }],
    ['umg.plan.taskspec.preview', {
      route_refs: UMG_TASKSPEC_ROUTES,
      stop_conditions: PREVIEW_STOP_CONDITIONS,
      recommended_invocations: TASK_PREVIEW_INVOCATION,
    }],
    ['umg.write.taskspec.execute', {
      route_refs: UMG_TASKSPEC_ROUTES,
      stop_conditions: EXECUTE_STOP_CONDITIONS,
      recommended_invocations: TASK_EXECUTE_INVOCATION,
    }],

    ['data.read.data_asset', {
      route_refs: readRoutesById(['read.data_asset.object']),
      slot_refs: READ_CONTEXT_SLOTS,
      stop_conditions: READ_CONTEXT_STOP_CONDITIONS,
    }],
    ['data.read.data_table', {
      route_refs: readRoutesById(['read.data_table.table', 'read.data_table.row']),
      slot_refs: READ_CONTEXT_SLOTS,
      stop_conditions: READ_CONTEXT_STOP_CONDITIONS,
    }],
    ['data.plan.taskspec.preview', {
      route_refs: DATA_TASKSPEC_ROUTES,
      stop_conditions: PREVIEW_STOP_CONDITIONS,
      recommended_invocations: TASK_PREVIEW_INVOCATION,
    }],
    ['data.write.taskspec.execute', {
      route_refs: DATA_TASKSPEC_ROUTES,
      stop_conditions: EXECUTE_STOP_CONDITIONS,
      recommended_invocations: TASK_EXECUTE_INVOCATION,
    }],

    ['editor.write.asset.save', { stop_conditions: SAVE_STOP_CONDITIONS }],
    ['editor.write.source_control.checkout', { stop_conditions: SOURCE_CONTROL_CHECKOUT_STOP_CONDITIONS }],
    ['editor.read.source_control.status', { stop_conditions: SOURCE_CONTROL_STATUS_STOP_CONDITIONS }],
    ['editor.diagnose.static', { stop_conditions: DIAGNOSTICS_STOP_CONDITIONS }],
    ['editor.diagnose.runtime', { stop_conditions: RUNTIME_DIAGNOSTICS_STOP_CONDITIONS }],
    ['project.write.write_session', { stop_conditions: WRITE_SESSION_STOP_CONDITIONS }],
  ]);
}

function readRoutesById(routeIds: readonly string[]): ToolTemplateRouteRef[] {
  const routesById = new Map(READ_CONTEXT_ROUTES.map((routeEntry) => [routeEntry.route_id, routeEntry]));
  return routeIds.map((routeId) => {
    const routeEntry = routesById.get(routeId);
    if (!routeEntry) {
      throw new Error(`Unknown ReadContext route descriptor id: ${routeId}`);
    }
    return routeEntry;
  });
}

function toToolCapabilityDescriptor(
  capabilityItem: ToolCapabilityItem,
  options: ToolCapabilityDescriptorOptions = {},
): ToolCapabilityDescriptor {
  return {
    tool_id: capabilityItem.id,
    tool_name: capabilityItem.tool_name,
    route_refs: [...(options.route_refs ?? [])],
    slot_refs: [...(options.slot_refs ?? [])],
    stop_conditions: [...(options.stop_conditions ?? defaultStopConditions(capabilityItem))],
    recommended_invocations: [...(options.recommended_invocations ?? defaultRecommendedInvocations(capabilityItem.tool_name))],
    metrics_identity: {
      capability: `${capabilityItem.domain}.${capabilityItem.kind}`,
      semantic_operation: capabilityItem.id,
    },
  };
}

function defaultStopConditions(capabilityItem: ToolCapabilityItem): readonly string[] {
  return capabilityItem.requires_bridge ? DEFAULT_BRIDGE_STOP_CONDITIONS : DEFAULT_LOCAL_STOP_CONDITIONS;
}

function defaultRecommendedInvocations(toolName: string): readonly string[] {
  return [`bh ${toolName} --file <filled-template.json> --select status,artifacts.full_result`];
}

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

export function getToolTemplateDispatch(toolId: string, options: GetToolTemplateDispatchOptions = {}): ToolTemplateDispatchResult {
  return createToolsTemplateBuilderCore(
    buildReadonlyToolCommandManifestRegistry(),
    { getRawTemplateDispatch: getRawToolTemplateDispatch },
  ).getTemplateDispatch(toolId, options);
}

export function getToolCapabilityDescriptor(toolId: string) {
  return createDescriptorRegistry().get(toolId);
}

export function getRawToolTemplateDispatch(toolId: string, options: GetToolTemplateDispatchOptions = {}): ToolTemplateDispatchResult {
  const capabilityItem = CAPABILITIES.find((entry) => entry.id === toolId);
  if (!capabilityItem) {
    throw new Error(`Unknown BlueprintHelper tool capability id: ${toolId}`);
  }

  const descriptorRegistry = createDescriptorRegistry();
  const descriptor = descriptorRegistry.require(toolId);
  const cliInvocationTemplates = capabilityItem.cli_template_ids.map(resolveCliTemplate);
  const inputShape = cliInvocationTemplates.find((template) => template.input_shape)?.input_shape;
  const routes = [...descriptor.route_refs];
  const requestedRoute = options.route ? normalizeTemplateRouteId(options.route) : undefined;
  const selectedRoute = options.route
    ? routes.find((entry) => entry.route_id === requestedRoute)
    : undefined;
  if (options.route && !selectedRoute) {
    throw new Error(`Unknown BlueprintHelper template route for ${toolId}: ${options.route}`);
  }
  const slotTemplates = options.slot === true
    ? descriptorRegistry.filterSlotTemplates(toolId, options, selectedRoute)
    : [];

  return {
    schema: 'BlueprintHelper.ToolTemplateSelection.v1',
    tool_id: capabilityItem.id,
    tool_name: capabilityItem.tool_name,
    cli_invocation_templates: cliInvocationTemplates,
    routes,
    selected_route: selectedRoute,
    slot_templates: slotTemplates,
    input_shape: inputShape,
    recommended_invocation: descriptor.recommended_invocations[0]
      ?? buildDescriptorRecommendedInvocation(descriptor, cliInvocationTemplates),
    allowed_tools: [capabilityItem.tool_name],
    stop_conditions: [...descriptor.stop_conditions],
    next: buildTemplateDispatchNext(capabilityItem.id, routes),
  };
}

function normalizeTemplateRouteId(routeId: string): string {
  return READ_CONTEXT_ROUTE_ALIASES.get(routeId) ?? routeId;
}

const READ_CONTEXT_ROUTE_ALIASES = new Map<string, string>([
  ['read_context.function.logic_flow', 'read.blueprint.logic.function.logic_flow'],
  ['read_context.event.logic_flow', 'read.blueprint.logic.event.logic_flow'],
  ['read_context.custom_event.logic_flow', 'read.blueprint.logic.custom_event.logic_flow'],
  ['read_context.graph.logic_json', 'read.blueprint.logic.graph.logic_json'],
  ['read_context.block.logic_json', 'read.blueprint.logic.block.logic_json'],
]);

function buildTemplateDispatchNext(toolId: string, routes: readonly ToolTemplateRouteRef[]): ToolTemplateDispatchResult['next'] {
  if (routes.length === 0) {
    return {};
  }
  return {
    route_command: `bh tools templates ${toolId} --route <route_id> --format json`,
    slot_command: `bh tools templates ${toolId} --route <route_id> --slot --format json`,
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

function resolveCliTemplate(templateId: string): CliInvocationTemplateRef {
  const template = CLI_TEMPLATES.get(templateId);
  if (!template) {
    throw new Error(`Unknown BlueprintHelper CLI invocation template id: ${templateId}`);
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
