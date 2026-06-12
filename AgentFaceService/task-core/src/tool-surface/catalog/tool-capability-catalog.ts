import { getGraphWriteRoutesForTemplateDiscovery } from '../../task/compiler/graphwrite/graphwrite-route-registry.js';
import { getActiveReadContextRouteDescriptors } from '../templates/read-context-template-registry.js';
import type { ReadContextRouteDescriptor } from '../templates/read-context-template-types.js';
import { toolMetas } from '../registry/tool-metas.js';
import { summarizeToolInputShape } from '../manifest/tool-input-shape-metadata.js';
import type { ToolResultProjectionPolicyId } from '../manifest/tool-command-manifest.js';
import { templateIndexCommandForCapabilityKind } from '../cli/cli-subcommand-descriptor.js';
import {
  createToolCapabilityDescriptorRegistry,
  type ToolCapabilityDescriptor,
} from './tool-capability-descriptor-registry.js';
import type { ToolAudience, ToolRisk } from '../types.js';
import type {
  ListToolCapabilitiesOptions,
  ListToolDomainsOptions,
  ToolCapabilityDomain,
  ToolCapabilityItem,
  ToolCapabilityListItem,
  ToolCapabilityKind,
  ToolCapabilityListResult,
  ToolDomainCatalogItem,
  ToolDomainListResult,
} from './tool-capability-types.js';

const toolMetaByName = new Map(toolMetas.map((meta) => [meta.name, meta]));

const DOMAINS: readonly ToolDomainCatalogItem[] = [
  domain('blueprint', 'Blueprint', 'active', ['discover', 'read', 'plan', 'write', 'diagnose'], 'Blueprint graph, function, event, variable, component, and class-setting workflows.', true),
  domain('umg', 'UMG', 'active', ['read', 'plan', 'write'], 'Widget Blueprint tree and property workflows.', true),
  domain('data', 'Data', 'active', ['read', 'plan', 'write'], 'DataAsset, DataTable, and UObject property workflows.', true),
  domain('editor', 'Editor', 'active', ['discover', 'read', 'write', 'diagnose'], 'Runtime profile, diagnostics, screenshot, source-control, and editor evidence workflows.', true),
  domain('project', 'Project', 'active', ['discover', 'read', 'write'], 'AgentGuide, project profile, task-result, and write-session workflows.', true),
  domain('debug', 'Debug', 'active', ['diagnose'], 'Summary-only DebugCase and DebugBundle manifest workflows.', true),
  domain('review', 'Review', 'active', ['diagnose', 'write'], 'ReviewRecord query workflows; write actions require expert access.', true),
  domain('animation', 'Animation', 'reserved', [], 'Reserved for future Animation Blueprint tool catalog entries.', false, 'No Agent-facing tool catalog entry yet.'),
  domain('material', 'Material', 'reserved', [], 'Reserved for future Material graph tool catalog entries.', false, 'No Agent-facing tool catalog entry yet.'),
];

const CAPABILITIES: readonly ToolCapabilityItem[] = [
  capability('blueprint.discover.assets', 'blueprint', 'discover', 'blueprinthelper_find_assets', 'Resolve unknown Unreal asset paths before reads or writes.', 'blueprint-explorer', 'low', true, false, ['blueprinthelper_find_assets']),
  capability('blueprint.read.context.logic_flow', 'blueprint', 'read', 'blueprinthelper_read_context', 'Read compact execution/data flow for a known function, event, or custom event.', 'blueprint-explorer', 'low', true, false, ['read_context_function_logic_flow', 'read_context_event_logic_flow', 'read_context_custom_event_logic_flow']),
  capability('blueprint.read.context.logic_json', 'blueprint', 'read', 'blueprinthelper_read_context', 'Read stable LogicJson anchors for a known graph or block.', 'blueprint-explorer', 'low', true, false, ['read_context_graph_logic_json', 'read_context_block_logic_json']),
  capability('blueprint.read.context.asset', 'blueprint', 'read', 'blueprinthelper_read_context', 'Read Blueprint asset diagnostics context.', 'blueprint-explorer', 'low', true, false, ['read_context_asset']),
  capability('blueprint.read.context.components', 'blueprint', 'read', 'blueprinthelper_read_context', 'Read Blueprint component facts and property metadata.', 'blueprint-explorer', 'low', true, false, ['read_context_components']),
  capability('blueprint.read.context.variables', 'blueprint', 'read', 'blueprinthelper_read_context', 'Read Blueprint variable metadata and defaults.', 'blueprint-explorer', 'low', true, false, ['read_context_variables']),
  capability('blueprint.read.context.properties', 'blueprint', 'read', 'blueprinthelper_read_context', 'Read Blueprint object property context.', 'blueprint-explorer', 'low', true, false, ['read_context_properties']),
  capability('blueprint.read.reference.dependencies', 'blueprint', 'read', 'blueprinthelper_read_reference_context', 'Read dependency ReferenceContextPack before risky edits.', 'blueprint-explorer', 'low', true, false, ['blueprinthelper_read_reference_context_dependencies']),
  capability('blueprint.read.function_chain', 'blueprint', 'read', 'blueprinthelper_read_function_chain_context', 'Trace project-authored function/event/custom-event calls.', 'blueprint-explorer', 'low', true, false, ['blueprinthelper_read_function_chain_context']),
  capability('blueprint.plan.taskspec.preview', 'blueprint', 'plan', 'blueprinthelper_preview_task', 'Validate and preview a BlueprintHelper.TaskSpec.v1 before execute.', 'task-worker', 'low', false, false, ['task_preview_bare_taskspec']),
  capability('blueprint.write.taskspec.execute', 'blueprint', 'write', 'blueprinthelper_execute_task', 'Execute a BlueprintHelper.TaskSpec.v1 after preview and write permission.', 'task-worker', 'high', false, true, ['task_execute_bare_taskspec']),
  capability('blueprint.diagnose.compile', 'blueprint', 'diagnose', 'blueprint_compile_blueprint', 'Compile an explicit Blueprint asset through the running Editor Bridge for validation.', 'task-worker', 'medium', true, true, ['blueprint_compile_blueprint']),
  capability('umg.read.widget_tree', 'umg', 'read', 'blueprinthelper_read_context', 'Read Widget Blueprint tree context.', 'blueprint-explorer', 'low', true, false, ['read_context_widget_tree']),
  capability('umg.read.widget_property', 'umg', 'read', 'blueprinthelper_read_context', 'Read Widget Blueprint property context.', 'blueprint-explorer', 'low', true, false, ['read_context_widget_property']),
  capability('umg.plan.taskspec.preview', 'umg', 'plan', 'blueprinthelper_preview_task', 'Preview UMG TaskSpec changes.', 'task-worker', 'low', false, false, ['task_preview_bare_taskspec']),
  capability('umg.write.taskspec.execute', 'umg', 'write', 'blueprinthelper_execute_task', 'Execute UMG TaskSpec changes after preview.', 'task-worker', 'high', false, true, ['task_execute_bare_taskspec']),
  capability('data.read.data_asset', 'data', 'read', 'blueprinthelper_read_context', 'Read DataAsset object property context.', 'blueprint-explorer', 'low', true, false, ['read_context_data_asset']),
  capability('data.read.data_table', 'data', 'read', 'blueprinthelper_read_context', 'Read DataTable or DataTable row context.', 'blueprint-explorer', 'low', true, false, ['read_context_data_table', 'read_context_data_table_row']),
  capability('data.plan.taskspec.preview', 'data', 'plan', 'blueprinthelper_preview_task', 'Preview DataAsset, DataTable, or object-property TaskSpec changes.', 'task-worker', 'low', false, false, ['task_preview_bare_taskspec']),
  capability('data.write.taskspec.execute', 'data', 'write', 'blueprinthelper_execute_task', 'Execute DataAsset, DataTable, or object-property TaskSpec changes.', 'task-worker', 'high', false, true, ['task_execute_bare_taskspec']),
  capability('editor.read.runtime_profile', 'editor', 'read', 'blueprint_get_runtime_profile', 'Read BlueprintHelper runtime profile from the running Editor Bridge.', 'blueprint-explorer', 'low', true, false, ['blueprint_get_runtime_profile']),
  capability('editor.read.screenshot', 'editor', 'read', 'blueprinthelper_capture_screenshot', 'Capture screenshot evidence for an asset, graph, block, or node.', 'blueprint-explorer', 'low', true, false, ['blueprinthelper_capture_screenshot']),
  capability('editor.read.source_control.status', 'editor', 'read', 'blueprinthelper_source_control_status', 'Read source-control checkout and lock state for assets or files before a write.', 'task-worker', 'low', true, false, ['blueprinthelper_source_control_status']),
  capability('editor.write.source_control.checkout', 'editor', 'write', 'blueprinthelper_source_control_checkout', 'Check out source-controlled assets or files before editing under Perforce/source control.', 'task-worker', 'medium', true, false, ['blueprinthelper_source_control_checkout']),
  capability('editor.write.asset.save', 'editor', 'write', 'blueprint_save_asset', 'Persist an explicit Unreal asset package after write-session and source-control checks.', 'task-worker', 'high', true, true, ['blueprint_save_asset']),
	capability('editor.diagnose.static', 'editor', 'diagnose', 'blueprinthelper_diagnostics', 'Run static installation/configuration diagnostics.', 'main-agent', 'none', false, false, ['blueprinthelper_diagnostics']),
	capability('editor.diagnose.runtime', 'editor', 'diagnose', 'blueprinthelper_diagnostics_runtime', 'Run runtime diagnostics through the running Editor Bridge.', 'blueprint-explorer', 'low', true, false, ['blueprinthelper_diagnostics_runtime']),
	capability('project.discover.agent_guide', 'project', 'discover', 'blueprinthelper_read_agent_guide', 'Read the AgentGuide onboarding entry.', 'main-agent', 'none', false, false, ['blueprinthelper_read_agent_guide']),
	capability('project.discover.read_context_capabilities', 'project', 'discover', 'blueprinthelper_read_context_capabilities', 'Read the static ReadContext route capability matrix before composing a ReadSpec.', 'main-agent', 'none', false, false, ['blueprinthelper_read_context_capabilities']),
	capability('project.write.write_session', 'project', 'write', 'blueprinthelper_request_write_session', 'Request Editor-approved write permission after preview requires it.', 'task-worker', 'medium', false, false, ['blueprinthelper_request_write_session_project', 'blueprinthelper_request_write_session_assets']),
	capability('project.read.task_result', 'project', 'read', 'blueprinthelper_get_task_result', 'Read a TaskRunJournal by task_run_id.', 'task-worker', 'low', false, false, []),
  capability('debug.diagnose.case', 'debug', 'diagnose', 'blueprinthelper_get_debug_case', 'Read one summary-only DebugCase by id.', 'sourcecode-explorer', 'low', true, false, ['blueprinthelper_get_debug_case']),
  capability('debug.diagnose.list', 'debug', 'diagnose', 'blueprinthelper_list_debug_cases', 'List summary-only DebugCases.', 'sourcecode-explorer', 'low', true, false, ['blueprinthelper_list_debug_cases']),
  capability('debug.diagnose.bundle', 'debug', 'diagnose', 'blueprinthelper_export_debug_bundle', 'Export a local DebugBundle manifest for a DebugCase.', 'sourcecode-explorer', 'low', true, false, ['blueprinthelper_export_debug_bundle']),
  capability('review.diagnose.records', 'review', 'diagnose', 'blueprinthelper_query_review_records', 'Query summary ReviewRecords by asset, task run, archive session, or pending state.', 'sourcecode-explorer', 'low', true, false, ['blueprinthelper_query_review_records']),
  capability('review.write.apply_action', 'review', 'write', 'blueprinthelper_apply_review_action', 'Expert/internal Review action tool for accepting or rejecting ReviewRecord targets.', 'task-worker', 'high', true, true, ['blueprinthelper_apply_review_action']),
];

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
const READ_CONTEXT_CAPABILITIES_INVOCATION = ['bh blueprinthelper_read_context_capabilities --json "{}" --format json'] as const;
const READ_CONTEXT_HELP_USAGE = [
  'bh context read --file <read-spec.json> --select status,artifacts.full_result',
  '$json | bh context read --stdin --format full',
] as const;

interface ToolCapabilityDescriptorOptions {
  readonly result_policy_id?: ToolResultProjectionPolicyId;
  readonly route_refs?: readonly string[];
  readonly stop_conditions?: readonly string[];
  readonly recommended_invocations?: readonly string[];
  readonly help_usage?: readonly string[];
  readonly help_notes?: readonly string[];
}

export function listToolDomains(options: ListToolDomainsOptions = {}): ToolDomainListResult {
  const audience = options.audience ?? 'default';
  return {
    schema: 'BlueprintHelper.ToolDomainList.v1',
    audience,
    items: DOMAINS.filter((entry) => entry.status === 'active'),
    reserved: options.includeReserved === true ? DOMAINS.filter((entry) => entry.status === 'reserved') : [],
    next: {
      list_command: 'bh tools list <domain> <kind> --format json',
    },
  };
}

export function listToolCapabilities(options: ListToolCapabilitiesOptions): ToolCapabilityListResult {
  const audience = options.audience ?? 'default';
  const items = listToolCapabilityItems(options);

  return {
    schema: 'BlueprintHelper.ToolCapabilityList.v1',
    query: {
      domain: options.domain,
      kind: options.kind,
      audience,
    },
    items: items.map(toToolCapabilityListItem),
    next: resolveCapabilityListNext(items, options.kind),
  };
}

export function listToolCapabilityItems(options: ListToolCapabilitiesOptions): ToolCapabilityItem[] {
  const audience = options.audience ?? 'default';
  const risks = new Set(options.risks ?? []);
  return CAPABILITIES.filter((capabilityItem) => {
    if (capabilityItem.domain !== options.domain || capabilityItem.kind !== options.kind) return false;
    if (!isAudienceVisible(capabilityItem, audience, options.expert === true)) return false;
    if (options.requiresBridge !== undefined && capabilityItem.requires_bridge !== options.requiresBridge) return false;
    return risks.size === 0 || risks.has(capabilityItem.risk);
  });
}

function resolveTemplateIndexCommand(kind: ToolCapabilityKind): ToolCapabilityListResult['next']['template_index_command'] {
  const command = templateIndexCommandForCapabilityKind(kind);
  if (!command) {
    throw new Error(`No BlueprintHelper CLI template index command registered for capability kind: ${kind}`);
  }
  return command;
}

function resolveCapabilityListNext(
  items: ToolCapabilityItem[],
  kind: ToolCapabilityKind,
): ToolCapabilityListResult['next'] {
  if (items.length === 1 && items[0]?.id === 'project.read.task_result') {
    return { command: 'bh task result --id <task_run_id> --format summary' };
  }
  return {
    template_index_command: resolveTemplateIndexCommand(kind),
  };
}

function toToolCapabilityListItem(capabilityItem: ToolCapabilityItem): ToolCapabilityListItem {
  const cliTemplateIds = agentFacingTemplateIds(capabilityItem);
	return {
		id: capabilityItem.id,
		domain: capabilityItem.domain,
		kind: capabilityItem.kind,
		cli_command: agentFacingCliCommand(capabilityItem),
		purpose: capabilityItem.purpose,
		agent_role: capabilityItem.agent_role,
		audience: capabilityItem.audience,
		risk: capabilityItem.risk,
		requires_bridge: capabilityItem.requires_bridge,
		requires_write_session: capabilityItem.requires_write_session,
		deprecated: capabilityItem.deprecated,
		frozen: capabilityItem.frozen,
		cli_template_ids: cliTemplateIds,
		source: capabilityItem.source,
		...summarizeToolInputShape({
			templateIds: cliTemplateIds,
			requiresBridge: capabilityItem.requires_bridge,
      emptyTemplateInputShape: emptyTemplateInputShapeForCapability(capabilityItem),
		}),
	};
}

function agentFacingCliCommand(capabilityItem: ToolCapabilityItem): string {
  if (capabilityItem.id === 'blueprint.plan.taskspec.preview') return 'bh task preview';
  if (capabilityItem.id === 'blueprint.write.taskspec.execute') return 'bh task execute';
  if (capabilityItem.id === 'umg.plan.taskspec.preview') return 'bh task preview';
  if (capabilityItem.id === 'umg.write.taskspec.execute') return 'bh task execute';
  if (capabilityItem.id === 'data.plan.taskspec.preview') return 'bh task preview';
  if (capabilityItem.id === 'data.write.taskspec.execute') return 'bh task execute';
  if (capabilityItem.id === 'project.read.task_result') return 'bh task result --id <task_run_id>';
  if (capabilityItem.tool_name === 'blueprinthelper_read_context') return 'bh context read';
  return `bh ${capabilityItem.tool_name}`;
}

function agentFacingTemplateIds(capabilityItem: ToolCapabilityItem): string[] {
  if (capabilityItem.kind === 'plan' && capabilityItem.tool_name === 'blueprinthelper_preview_task') {
    return ['task_preview_bare_taskspec'];
  }
  if (capabilityItem.kind === 'write' && capabilityItem.tool_name === 'blueprinthelper_execute_task') {
    return ['task_execute_bare_taskspec'];
  }
  if (capabilityItem.id === 'project.read.task_result') {
    return [];
  }
  return [...capabilityItem.cli_template_ids];
}

function emptyTemplateInputShapeForCapability(capabilityItem: ToolCapabilityItem): 'cli_options' | undefined {
  return capabilityItem.id === 'project.read.task_result' ? 'cli_options' : undefined;
}

export function getToolCapabilityDescriptor(toolId: string): ToolCapabilityDescriptor | undefined {
  return createDescriptorRegistry().get(toolId);
}

export function isToolCapabilityDomain(value: string): value is ToolCapabilityDomain {
  return DOMAINS.some((entry) => entry.id === value);
}

export function isToolCapabilityKind(value: string): value is ToolCapabilityKind {
  return value === 'discover'
    || value === 'read'
    || value === 'plan'
    || value === 'write'
    || value === 'diagnose';
}

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
  const graphWriteRouteRefs = getGraphWriteRoutesForTemplateDiscovery().map((route) => route.route_id);
  const blueprintLogicFlowRouteRefs = readContextRouteRefs((route) =>
    route.domain === 'blueprint' && route.read_cluster === 'logic' && route.view_template === 'logic_flow');
  const blueprintLogicJsonRouteRefs = readContextRouteRefs((route) =>
    route.domain === 'blueprint'
    && route.read_cluster === 'logic'
    && route.view_template === 'logic_json');
  const blueprintAssetRouteRefs = readContextRouteRefs((route) =>
    route.domain === 'blueprint' && route.read_cluster === 'asset');
  const blueprintComponentRouteRefs = readContextRouteRefs((route) =>
    route.domain === 'blueprint' && route.read_cluster === 'components');
  const blueprintVariableRouteRefs = readContextRouteRefs((route) =>
    route.domain === 'blueprint' && route.read_cluster === 'variables');
  const blueprintPropertyRouteRefs = readContextRouteRefs((route) =>
    route.domain === 'blueprint' && route.read_cluster === 'properties');
  const widgetTreeRouteRefs = readContextRouteRefs((route) =>
    route.domain === 'widget_blueprint' && route.target_kind === 'widget_tree');
  const widgetPropertyRouteRefs = readContextRouteRefs((route) =>
    route.domain === 'widget_blueprint' && route.target_kind === 'widget');
  const dataAssetRouteRefs = readContextRouteRefs((route) =>
    route.domain === 'data_asset');
  const dataTableRouteRefs = readContextRouteRefs((route) =>
    route.domain === 'data_table');
  return new Map<string, ToolCapabilityDescriptorOptions>([
    ['blueprint.discover.assets', {
      stop_conditions: FIND_ASSETS_STOP_CONDITIONS,
      help_usage: ['bh blueprinthelper_find_assets --file <find-assets.json> --select status,artifacts.full_result'],
    }],
    ['blueprint.read.context.logic_flow', {
      route_refs: blueprintLogicFlowRouteRefs,
      stop_conditions: READ_CONTEXT_STOP_CONDITIONS,
      help_usage: READ_CONTEXT_HELP_USAGE,
    }],
    ['blueprint.read.context.logic_json', {
      route_refs: blueprintLogicJsonRouteRefs,
      stop_conditions: READ_CONTEXT_STOP_CONDITIONS,
      help_usage: READ_CONTEXT_HELP_USAGE,
    }],
    ['blueprint.read.context.asset', {
      route_refs: blueprintAssetRouteRefs,
      stop_conditions: READ_CONTEXT_STOP_CONDITIONS,
      help_usage: READ_CONTEXT_HELP_USAGE,
    }],
    ['blueprint.read.context.components', {
      route_refs: blueprintComponentRouteRefs,
      stop_conditions: READ_CONTEXT_STOP_CONDITIONS,
      help_usage: READ_CONTEXT_HELP_USAGE,
    }],
    ['blueprint.read.context.variables', {
      route_refs: blueprintVariableRouteRefs,
      stop_conditions: READ_CONTEXT_STOP_CONDITIONS,
      help_usage: READ_CONTEXT_HELP_USAGE,
    }],
    ['blueprint.read.context.properties', {
      route_refs: blueprintPropertyRouteRefs,
      stop_conditions: READ_CONTEXT_STOP_CONDITIONS,
      help_usage: READ_CONTEXT_HELP_USAGE,
    }],
    ['blueprint.read.reference.dependencies', { stop_conditions: READ_REFERENCE_STOP_CONDITIONS }],
    ['blueprint.read.function_chain', { stop_conditions: FUNCTION_CHAIN_STOP_CONDITIONS }],
    ['blueprint.plan.taskspec.preview', {
      result_policy_id: 'task_preview_default',
      route_refs: ['blueprint.create_feature', ...graphWriteRouteRefs],
      stop_conditions: PREVIEW_STOP_CONDITIONS,
      recommended_invocations: TASK_PREVIEW_INVOCATION,
    }],
    ['blueprint.write.taskspec.execute', {
      result_policy_id: 'task_execute_default',
      route_refs: ['blueprint.create_feature', ...graphWriteRouteRefs],
      stop_conditions: EXECUTE_STOP_CONDITIONS,
      recommended_invocations: TASK_EXECUTE_INVOCATION,
    }],
    ['blueprint.diagnose.compile', {
      stop_conditions: COMPILE_STOP_CONDITIONS,
      help_notes: ['Requires an approved write-session gate when the running Editor Bridge enforces write-session policy.'],
    }],
    ['umg.read.widget_tree', {
      route_refs: widgetTreeRouteRefs,
      stop_conditions: READ_CONTEXT_STOP_CONDITIONS,
      help_usage: READ_CONTEXT_HELP_USAGE,
    }],
    ['umg.read.widget_property', {
      route_refs: widgetPropertyRouteRefs,
      stop_conditions: READ_CONTEXT_STOP_CONDITIONS,
      help_usage: READ_CONTEXT_HELP_USAGE,
    }],
    ['umg.plan.taskspec.preview', {
      result_policy_id: 'task_preview_default',
      route_refs: ['umg.widget'],
      stop_conditions: PREVIEW_STOP_CONDITIONS,
      recommended_invocations: TASK_PREVIEW_INVOCATION,
    }],
    ['umg.write.taskspec.execute', {
      result_policy_id: 'task_execute_default',
      route_refs: ['umg.widget'],
      stop_conditions: EXECUTE_STOP_CONDITIONS,
      recommended_invocations: TASK_EXECUTE_INVOCATION,
    }],
    ['data.read.data_asset', {
      route_refs: dataAssetRouteRefs,
      stop_conditions: READ_CONTEXT_STOP_CONDITIONS,
      help_usage: READ_CONTEXT_HELP_USAGE,
    }],
    ['data.read.data_table', {
      route_refs: dataTableRouteRefs,
      stop_conditions: READ_CONTEXT_STOP_CONDITIONS,
      help_usage: READ_CONTEXT_HELP_USAGE,
    }],
    ['data.plan.taskspec.preview', {
      result_policy_id: 'task_preview_default',
      route_refs: ['data.object_properties', 'data.table.rows', 'data.asset.create'],
      stop_conditions: PREVIEW_STOP_CONDITIONS,
      recommended_invocations: TASK_PREVIEW_INVOCATION,
    }],
    ['data.write.taskspec.execute', {
      result_policy_id: 'task_execute_default',
      route_refs: ['data.object_properties', 'data.table.rows', 'data.asset.create'],
      stop_conditions: EXECUTE_STOP_CONDITIONS,
      recommended_invocations: TASK_EXECUTE_INVOCATION,
    }],
    ['editor.read.screenshot', {
      help_usage: ['bh blueprinthelper_capture_screenshot --file <capture-screenshot.json> --select status,artifacts.full_result'],
    }],
    ['editor.write.asset.save', {
      stop_conditions: SAVE_STOP_CONDITIONS,
      help_notes: ['Requires an approved write-session gate before persisting an Unreal asset package.'],
    }],
    ['editor.write.source_control.checkout', { stop_conditions: SOURCE_CONTROL_CHECKOUT_STOP_CONDITIONS }],
    ['editor.read.source_control.status', { stop_conditions: SOURCE_CONTROL_STATUS_STOP_CONDITIONS }],
    ['editor.diagnose.static', { stop_conditions: DIAGNOSTICS_STOP_CONDITIONS }],
    ['editor.diagnose.runtime', { stop_conditions: RUNTIME_DIAGNOSTICS_STOP_CONDITIONS }],
    ['project.discover.read_context_capabilities', {
      recommended_invocations: READ_CONTEXT_CAPABILITIES_INVOCATION,
    }],
    ['project.read.task_result', {
      recommended_invocations: ['bh task result --id <task_run_id> --format summary'],
      help_usage: ['bh task result --id <task_run_id> --format summary'],
    }],
    ['project.write.write_session', { stop_conditions: WRITE_SESSION_STOP_CONDITIONS }],
  ]);
}

function readContextRouteRefs(
  predicate: (route: ReadContextRouteDescriptor) => boolean,
): string[] {
  return getActiveReadContextRouteDescriptors()
    .filter(predicate)
    .map((route) => route.route_id);
}

function toToolCapabilityDescriptor(
  capabilityItem: ToolCapabilityItem,
  options: ToolCapabilityDescriptorOptions = {},
): ToolCapabilityDescriptor {
  return {
    tool_id: capabilityItem.id,
    tool_name: capabilityItem.tool_name,
    result_policy_id: options.result_policy_id ?? defaultResultPolicyId(capabilityItem),
    route_refs: [...(options.route_refs ?? [])],
    stop_conditions: [...(options.stop_conditions ?? defaultStopConditions(capabilityItem))],
    recommended_invocations: [...(options.recommended_invocations ?? defaultRecommendedInvocations(capabilityItem.tool_name))],
    help_usage: [...(options.help_usage ?? options.recommended_invocations ?? defaultRecommendedInvocations(capabilityItem.tool_name))],
    help_notes: [...(options.help_notes ?? defaultHelpNotes(capabilityItem))],
    metrics_identity: {
      capability: `${capabilityItem.domain}.${capabilityItem.kind}`,
      semantic_operation: capabilityItem.id,
    },
    source: 'capability_descriptor_registry',
  };
}

function defaultResultPolicyId(capabilityItem: ToolCapabilityItem): ToolResultProjectionPolicyId {
  if (capabilityItem.tool_name === 'blueprinthelper_get_task_result') {
    return 'task_result_default';
  }
  if (capabilityItem.tool_name === 'blueprinthelper_read_context') {
    return 'read_context_default';
  }
  if (
    capabilityItem.tool_name === 'blueprinthelper_diagnostics'
    || capabilityItem.tool_name === 'blueprinthelper_diagnostics_runtime'
    || capabilityItem.tool_name.startsWith('blueprinthelper_get_debug_')
    || capabilityItem.tool_name === 'blueprinthelper_list_debug_cases'
    || capabilityItem.tool_name === 'blueprinthelper_export_debug_bundle'
  ) {
    return 'diagnostics_default';
  }
  if (capabilityItem.tool_name === 'blueprinthelper_apply_review_action') {
    return 'review_expert_default';
  }
  return capabilityItem.requires_bridge ? 'bridge_default' : 'local_default';
}

function defaultStopConditions(capabilityItem: ToolCapabilityItem): readonly string[] {
  return capabilityItem.requires_bridge ? DEFAULT_BRIDGE_STOP_CONDITIONS : DEFAULT_LOCAL_STOP_CONDITIONS;
}

function defaultRecommendedInvocations(toolName: string): readonly string[] {
  return [`bh ${toolName} --file <filled-template.json> --select status,artifacts.full_result`];
}

function defaultHelpNotes(capabilityItem: ToolCapabilityItem): readonly string[] {
  if (capabilityItem.tool_name === 'blueprinthelper_capture_screenshot') {
    return [
      'graph_name is required when block_ref or node_ref is provided.',
      'Use capture_target:auto for graph_name/block_ref/node_ref requests; graph targets capture Graph-only PNGs.',
    ];
  }
  if (capabilityItem.tool_name === 'blueprinthelper_find_assets') {
    return [
      'Resolve one explicit Unreal asset_path before preview_task or any write request.',
      'Do not infer Unreal asset_path values from filesystem .uasset paths.',
    ];
  }
  return [];
}

function capability(
  id: string,
  domainId: ToolCapabilityDomain,
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
    domain: domainId,
    kind,
    tool_name: toolName,
    purpose,
    agent_role: agentRole,
    audience: meta?.audience ?? 'default',
    risk: meta?.risk ?? fallbackRisk,
    requires_bridge: requiresBridge,
    requires_write_session: requiresWriteSession,
    cli_template_ids: [...cliTemplateIds],
    source: 'capability_catalog',
  };
}

function domain(
  id: ToolCapabilityDomain,
  label: string,
  status: ToolDomainCatalogItem['status'],
  defaultKinds: ToolCapabilityKind[],
  purpose: string,
  availableByDefault: boolean,
  reason?: string,
): ToolDomainCatalogItem {
  return {
    id,
    label,
    status,
    default_kinds: defaultKinds,
    purpose,
    available_by_default: availableByDefault,
    reason,
  };
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
