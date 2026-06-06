import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import {
  getToolTemplateDispatch,
  listToolCapabilities,
  listToolDomains,
} from './tool-capability-catalog.js';
import { createToolsTemplateBuilder } from '../manifest/tools-template-builder.js';
import { buildReadonlyToolCommandManifestRegistry } from '../manifest/tool-command-manifest-builder.js';
import { getGraphWriteRoutesForTemplateDiscovery } from '../../task/compiler/graphwrite/graphwrite-route-registry.js';
import { getAllGraphWriteSlotDescriptors } from '../../task/compiler/graphwrite/graphwrite-slot-registry.js';

const HIDDEN_TASKSPEC_POLICY_FIELDS = new Set(['execution_policy', 'scope_policy', 'validation']);

test('listToolDomains returns active domains by default and reserved domains only when requested', () => {
  const activeDomains = listToolDomains({ audience: 'default' });

  assert.equal(activeDomains.schema, 'BlueprintHelper.ToolDomainList.v1');
  assert.equal(activeDomains.items.some((item) => item.id === 'blueprint' && item.status === 'active'), true);
  assert.equal(activeDomains.items.some((item) => item.id === 'animation'), false);
  assert.equal(activeDomains.reserved.length, 0);

  const withReserved = listToolDomains({ audience: 'default', includeReserved: true });

  assert.equal(withReserved.items.some((item) => item.id === 'blueprint' && item.status === 'active'), true);
  assert.equal(withReserved.reserved.some((item) => item.id === 'animation' && item.status === 'reserved'), true);
  assert.equal(withReserved.reserved.some((item) => item.id === 'material' && item.status === 'reserved'), true);
});

test('listToolCapabilities filters by domain kind audience risk and bridge requirement', () => {
  const readList = listToolCapabilities({
    domain: 'blueprint',
    kind: 'read',
    audience: 'default',
  });

  assert.equal(readList.schema, 'BlueprintHelper.ToolCapabilityList.v1');
  assert.equal(readList.query.domain, 'blueprint');
  assert.equal(readList.query.kind, 'read');
  assert.equal(readList.items.some((item) => item.id === 'blueprint.read.context.logic_flow'), true);
  assert.equal(readList.items.some((item) => item.tool_name === 'blueprinthelper_apply_review_action'), false);
  assert.deepEqual(readList.next, {
    templates_command: 'bh tools templates <tool_id> --format json',
  });

  const noBridgeList = listToolCapabilities({
    domain: 'project',
    kind: 'discover',
    audience: 'default',
    requiresBridge: false,
  });

  assert.equal(noBridgeList.items.every((item) => item.requires_bridge === false), true);

  const lowRiskReadList = listToolCapabilities({
    domain: 'blueprint',
    kind: 'read',
    audience: 'default',
    risks: ['low'],
  });

  assert.equal(lowRiskReadList.items.every((item) => item.risk === 'low'), true);
});

test('listToolCapabilities keeps expert entries hidden unless expert is requested', () => {
  const defaultReview = listToolCapabilities({
    domain: 'review',
    kind: 'write',
    audience: 'default',
  });

  assert.equal(defaultReview.items.some((item) => item.tool_name === 'blueprinthelper_apply_review_action'), false);

  const expertReview = listToolCapabilities({
    domain: 'review',
    kind: 'write',
    audience: 'expert',
    expert: true,
  });

  assert.equal(expertReview.items.some((item) => item.tool_name === 'blueprinthelper_apply_review_action'), true);
});

test('getToolTemplateDispatch returns template dispatch package without tool detail command', () => {
  const dispatch = getToolTemplateDispatch('blueprint.read.context.logic_flow');

  assert.equal(dispatch.schema, 'BlueprintHelper.ToolTemplateSelection.v1');
  assert.equal(dispatch.tool_id, 'blueprint.read.context.logic_flow');
  assert.equal(dispatch.tool_name, 'blueprinthelper_read_context');
  assert.equal(dispatch.cli_invocation_templates[0]?.cli_template_id, 'read_context_function_logic_flow');
  assert.equal(
    dispatch.cli_invocation_templates.every((template) => template.path.includes('/read/routes/')),
    true,
  );
  assert.doesNotMatch(dispatch.recommended_invocation, /Templates\/read\/read_context_/);
  assert.equal('taskspec_semantic_templates' in dispatch, false);
  assert.deepEqual(dispatch.allowed_tools, ['blueprinthelper_read_context']);
  assert.deepEqual(dispatch.stop_conditions, [
    'tool_unavailable',
    'bridge_unavailable',
    'target not found',
    'target asset not found',
    'target graph not found',
  ]);
  assert.match(dispatch.recommended_invocation, /blueprinthelper_read_context/);
  assert.equal('show_command' in dispatch, false);
});

test('getToolTemplateDispatch returns route-owned templates for preview tool', () => {
  const dispatch = getToolTemplateDispatch('blueprint.plan.taskspec.preview');

  assert.equal(dispatch.tool_name, 'blueprinthelper_preview_task');
  assert.equal(dispatch.cli_invocation_templates.some((template) => template.cli_template_id === 'task_preview_bare_taskspec'), true);
  assert.equal('taskspec_semantic_templates' in dispatch, false);
  assert.equal(
    dispatch.routes.some((route) =>
      route.route_id === 'graph.merge_external_flow.append_after'
      && route.template_paths.some((path) => path.endsWith('write/routes/graph_merge_external_flow_template.json'))),
    true,
  );
  assert.deepEqual(dispatch.allowed_tools, ['blueprinthelper_preview_task']);
  assert.deepEqual(dispatch.stop_conditions, [
    'tool_unavailable',
    'write_session_required',
    'taskspec_template_unavailable',
    'preview_blocked',
  ]);
});

test('ToolsTemplateBuilder matches current preview template dispatch', () => {
  const builder = createToolsTemplateBuilder(buildReadonlyToolCommandManifestRegistry());

  assert.deepEqual(
    builder.getTemplateDispatch('blueprint.plan.taskspec.preview'),
    getToolTemplateDispatch('blueprint.plan.taskspec.preview'),
  );
});

test('ToolsTemplateBuilder filters route slots like catalog dispatch', () => {
  const builder = createToolsTemplateBuilder(buildReadonlyToolCommandManifestRegistry());
  const dispatch = builder.getTemplateDispatch('blueprint.write.taskspec.execute', {
    route: 'graph.replace.function_body',
    slot: true,
    slotKind: 'statement',
  });

  assert.equal(dispatch.selected_route?.route_id, 'graph.replace.function_body');
  assert.equal(
    dispatch.slot_templates.every((slot) => slot.slot_type === 'statement'),
    true,
  );
  assert.deepEqual(
    dispatch,
    getToolTemplateDispatch('blueprint.write.taskspec.execute', {
      route: 'graph.replace.function_body',
      slot: true,
      slotKind: 'statement',
    }),
  );
});

test('getToolTemplateDispatch exposes route-first GraphWrite template navigation', () => {
  const dispatch = getToolTemplateDispatch('blueprint.write.taskspec.execute');

  assert.equal(
    dispatch.routes.some((route) =>
      route.route_id === 'graph.replace.function_body'
      && route.template_paths.some((path) => path.endsWith('write/routes/graph_replace_owned_template.json'))),
    true,
  );
  assert.equal(dispatch.slot_templates.length, 0);
  assert.ok(dispatch.next.slot_command);
  assert.match(dispatch.next.slot_command, /--route <route_id> --slot/);
});

test('active GraphWrite routes are descriptor-backed and planned routes stay hidden from template discovery', () => {
  const dispatch = getToolTemplateDispatch('blueprint.plan.taskspec.preview');
  const routeIds = dispatch.routes.map((route) => route.route_id);

  assert.equal(routeIds.includes('graph.replace.function_body'), true);
  assert.equal(routeIds.includes('graph.replace.macro_body'), false);
  assert.throws(
    () => getToolTemplateDispatch('blueprint.plan.taskspec.preview', { route: 'graph.replace.macro_body' }),
    /Unknown BlueprintHelper template route/,
  );
});

test('agent-facing write templates do not expose hidden TaskSpec policy fields', () => {
  const writeTemplates = listJsonFiles(path.join(agentGuideTemplatesRoot(), 'write'));

  assert.notEqual(writeTemplates.length, 0);
  for (const templatePath of writeTemplates) {
    const template = JSON.parse(fs.readFileSync(templatePath, 'utf8')) as unknown;
    assertNoHiddenTaskSpecPolicyFields(template, templatePath);
  }
});

test('write template routes do not advertise hidden TaskSpec policy fields', () => {
  for (const toolId of ['blueprint.write.taskspec.execute', 'umg.write.taskspec.execute', 'data.write.taskspec.execute']) {
    const dispatch = getToolTemplateDispatch(toolId);

    for (const route of dispatch.routes) {
      for (const fieldPath of [...route.required_fields, ...route.optional_fields, ...route.insert_paths]) {
        assert.equal(
          isHiddenTaskSpecPolicyPath(fieldPath),
          false,
          `${toolId} route ${route.route_id} advertises hidden policy field ${fieldPath}`,
        );
      }
    }
  }
});

test('write template root does not keep legacy TaskSpec semantic templates', () => {
  const writeRoot = path.join(agentGuideTemplatesRoot(), 'write');
  const legacyTemplates = fs
    .readdirSync(writeRoot, { withFileTypes: true })
    .filter((entry) => entry.isFile() && /^taskspec_.*\.json$/.test(entry.name))
    .map((entry) => entry.name);

  assert.deepEqual(legacyTemplates, []);
});

test('read template root does not keep legacy ReadContext semantic templates', () => {
  const readRoot = path.join(agentGuideTemplatesRoot(), 'read');
  const legacyTemplates = fs
    .readdirSync(readRoot, { withFileTypes: true })
    .filter((entry) => entry.isFile() && /^read_context_.*\.json$/.test(entry.name))
    .map((entry) => entry.name);

  assert.deepEqual(legacyTemplates, []);
});

test('getToolTemplateDispatch routes every registered GraphWrite TaskSpec template', () => {
  const dispatch = getToolTemplateDispatch('blueprint.write.taskspec.execute');
  const routedPaths = new Set(dispatch.routes.flatMap((route) => route.template_paths));
  const expectedTemplateNames = [
    'write/routes/graph_append_container_action_template.json',
    'write/routes/graph_append_event_delegate_template.json',
    'write/routes/graph_append_generic_ops_template.json',
    'write/routes/graph_append_generic_schedule_template.json',
    'write/routes/graph_append_owned_template.json',
    'write/routes/graph_merge_append_after_template.json',
    'write/routes/graph_merge_branch_fork_template.json',
    'write/routes/graph_merge_external_flow_template.json',
    'write/routes/graph_merge_insert_between_template.json',
    'write/routes/graph_patch_connect_pins_template.json',
    'write/routes/graph_patch_delete_owned_node_template.json',
    'write/routes/graph_patch_disconnect_link_template.json',
    'write/routes/graph_patch_node_comment_template.json',
    'write/routes/graph_patch_pin_default_template.json',
    'write/routes/graph_patch_replace_link_template.json',
    'write/routes/graph_replace_owned_template.json',
  ];

  for (const templateName of expectedTemplateNames) {
    assert.equal(
      [...routedPaths].some((path) => path.endsWith(templateName)),
      true,
      `Missing route for ${templateName}`,
    );
  }
});

test('getToolTemplateDispatch filters GraphWrite slots by route and slot kind', () => {
  const dispatch = getToolTemplateDispatch('blueprint.write.taskspec.execute', {
    route: 'graph.replace.function_body',
    slot: true,
    slotKind: 'statement',
  });

  assert.equal(dispatch.selected_route?.route_id, 'graph.replace.function_body');
  assert.equal(dispatch.slot_templates.every((slot) => slot.slot_type === 'statement'), true);
  assert.equal(
    dispatch.slot_templates.some((slot) =>
      slot.slot_id === 'graph.statement.call.direct'
      && slot.path.endsWith('graph_statement_call_direct_template.json')
      && slot.applies_to_routes.includes('graph.replace.function_body')),
    true,
  );
});

test('getToolTemplateDispatch reflects descriptor-owned GraphWrite slot routing', () => {
  const visibleGraphWriteRoutes = getGraphWriteRoutesForTemplateDiscovery();
  const visibleRouteIds = new Set(visibleGraphWriteRoutes.map((route) => route.route_id));
  const allGraphWriteSlots = getToolTemplateDispatch('blueprint.write.taskspec.execute', { slot: true }).slot_templates
    .filter((slot) => slot.slot_id.startsWith('graph.'));

  for (const slot of allGraphWriteSlots) {
    for (const routeId of slot.applies_to_routes) {
      assert.equal(visibleRouteIds.has(routeId), true, `${slot.slot_id} points to unknown GraphWrite route ${routeId}`);
    }
  }

  for (const route of visibleGraphWriteRoutes) {
    const dispatch = getToolTemplateDispatch('blueprint.write.taskspec.execute', {
      route: route.route_id,
      slot: true,
    });
    const slotIds = new Set(dispatch.slot_templates.map((slot) => slot.slot_id));

    for (const allowedSlotId of route.allowed_slot_ids) {
      if (allowedSlotId === 'graph.body.*') {
        assert.equal(
          slotIds.has('graph.statement.call.direct'),
          true,
          `${route.route_id} should receive graph body common slots from descriptor wildcard`,
        );
        continue;
      }
      assert.equal(slotIds.has(allowedSlotId), true, `${route.route_id} should expose descriptor slot ${allowedSlotId}`);
    }
  }
});

test('GraphWrite slot descriptors own slot template discovery', () => {
  const slotIds = new Set(getAllGraphWriteSlotDescriptors().map((slot) => slot.slot_id));
  const dispatch = getToolTemplateDispatch('blueprint.plan.taskspec.preview', {
    route: 'graph.replace.function_body',
    slot: true,
  });

  assert.ok(dispatch.slot_templates.length > 0);
  for (const slot of dispatch.slot_templates) {
    if (!slot.slot_id.startsWith('graph.')) {
      continue;
    }
    assert.equal(slotIds.has(slot.slot_id), true, `${slot.slot_id} must come from generated slot descriptors`);
  }
});

test('getToolTemplateDispatch exposes function parameter expression slot for function bodies', () => {
  const dispatch = getToolTemplateDispatch('blueprint.write.taskspec.execute', {
    route: 'graph.replace.function_body',
    slot: true,
    slotKind: 'expression',
  });

  assert.equal(dispatch.selected_route?.route_id, 'graph.replace.function_body');
  assert.equal(
    dispatch.slot_templates.some((slot) =>
      slot.slot_id === 'graph.expression.get.function_param'
      && slot.path.endsWith('graph_expression_get_function_param_template.json')
      && slot.applies_to_routes.includes('graph.replace.function_body')),
    true,
  );
});

test('getToolTemplateDispatch scopes function parameter expression slot to function bodies', () => {
  const functionDispatch = getToolTemplateDispatch('blueprint.plan.taskspec.preview', {
    route: 'graph.replace.function_body',
    slot: true,
    slotKind: 'expression',
  });
  const functionSlotIds = new Set(functionDispatch.slot_templates.map((slot) => slot.slot_id));

  assert.equal(functionSlotIds.has('graph.expression.get.function_param'), true);

  for (const routeId of [
    'graph.append.custom_event',
    'graph.replace.event_body',
    'graph.patch.pin_default',
    'graph.merge_external_flow.append_after',
  ]) {
    const dispatch = getToolTemplateDispatch('blueprint.plan.taskspec.preview', {
      route: routeId,
      slot: true,
      slotKind: 'expression',
    });
    const slotIds = new Set(dispatch.slot_templates.map((slot) => slot.slot_id));

    assert.equal(slotIds.has('graph.expression.get.function_param'), false, `${routeId} must not expose function parameter reads`);
  }
});

test('getToolTemplateDispatch scopes return statement slot to function bodies', () => {
  const functionDispatch = getToolTemplateDispatch('blueprint.plan.taskspec.preview', {
    route: 'graph.replace.function_body',
    slot: true,
    slotKind: 'statement',
  });
  const functionSlotIds = new Set(functionDispatch.slot_templates.map((slot) => slot.slot_id));

  assert.equal(functionSlotIds.has('graph.statement.control.return'), true);

  for (const routeId of [
    'graph.append.custom_event',
    'graph.replace.event_body',
    'graph.patch.pin_default',
    'graph.merge_external_flow.append_after',
  ]) {
    const dispatch = getToolTemplateDispatch('blueprint.plan.taskspec.preview', {
      route: routeId,
      slot: true,
      slotKind: 'statement',
    });
    const slotIds = new Set(dispatch.slot_templates.map((slot) => slot.slot_id));

    assert.equal(slotIds.has('graph.statement.control.return'), false, `${routeId} must not expose function return statements`);
  }
});

test('getToolTemplateDispatch rejects unknown template routes', () => {
  assert.throws(
    () => getToolTemplateDispatch('blueprint.write.taskspec.execute', { route: 'graph.unknown.route' }),
    /Unknown BlueprintHelper template route/,
  );
});

test('getToolTemplateDispatch exposes route-first ReadContext template navigation', () => {
  const dispatch = getToolTemplateDispatch('blueprint.read.context.logic_flow', {
    route: 'read.blueprint.logic.function.logic_flow',
    slot: true,
  });

  assert.equal(dispatch.selected_route?.route_id, 'read.blueprint.logic.function.logic_flow');
  assert.equal(
    dispatch.selected_route?.template_paths.some((path) => path.endsWith('read/routes/blueprint_logic_function_logic_flow_template.json')),
    true,
  );
  assert.equal(
    dispatch.slot_templates.some((slot) =>
      slot.slot_id === 'read.target.function'
      && slot.path.endsWith('read_target_function_template.json')),
    true,
  );
});

test('getToolTemplateDispatch accepts compact ReadContext route aliases', () => {
  const dispatch = getToolTemplateDispatch('blueprint.read.context.logic_flow', {
    route: 'read_context.function.logic_flow',
    slot: true,
  });

  assert.equal(dispatch.selected_route?.route_id, 'read.blueprint.logic.function.logic_flow');
  assert.equal(
    dispatch.slot_templates.some((slot) => slot.slot_id === 'read.target.function'),
    true,
  );
});

test('getToolTemplateDispatch exposes route-first ReadContext navigation for widget properties and data assets', () => {
  const widgetDispatch = getToolTemplateDispatch('umg.read.widget_property', {
    route: 'read.widget.property',
    slot: true,
  });
  const dataAssetDispatch = getToolTemplateDispatch('data.read.data_asset', {
    route: 'read.data_asset.object',
    slot: true,
  });

  assert.equal(widgetDispatch.selected_route?.route_id, 'read.widget.property');
  assert.equal(
    widgetDispatch.slot_templates.some((slot) => slot.slot_id === 'read.target.widget'),
    true,
  );
  assert.equal(dataAssetDispatch.selected_route?.route_id, 'read.data_asset.object');
  assert.equal(
    dataAssetDispatch.slot_templates.some((slot) => slot.slot_id === 'read.target.asset'),
    true,
  );
});

test('source control checkout is advertised as an editor write capability', () => {
  const writeList = listToolCapabilities({ domain: 'editor', kind: 'write' });
  assert.equal(writeList.items.some((item) => item.id === 'editor.write.source_control.checkout'), true);

  const dispatch = getToolTemplateDispatch('editor.write.source_control.checkout');
  assert.equal(dispatch.tool_name, 'blueprinthelper_source_control_checkout');
  assert.deepEqual(dispatch.allowed_tools, ['blueprinthelper_source_control_checkout']);
  assert.equal(
    dispatch.cli_invocation_templates.some((template) =>
      template.path.endsWith('blueprinthelper_source_control_checkout_template.json')),
    true,
  );
  assert.equal(dispatch.stop_conditions.includes('checked_out_by_other'), true);
  assert.equal(dispatch.stop_conditions.includes('source_control_conflicted'), true);
  assert.equal(dispatch.stop_conditions.includes('checkout_failed'), true);
  assert.equal(dispatch.stop_conditions.includes('not_editable'), true);
});

test('compile blueprint is advertised as a blueprint diagnose capability', () => {
  const diagnoseList = listToolCapabilities({ domain: 'blueprint', kind: 'diagnose' });
  const compileItem = diagnoseList.items.find((item) => item.id === 'blueprint.diagnose.compile');
  assert.ok(compileItem);
  assert.equal(compileItem.risk, 'medium');
  assert.equal(compileItem.requires_bridge, true);
  assert.equal(compileItem.requires_write_session, true);

  const dispatch = getToolTemplateDispatch('blueprint.diagnose.compile');
  assert.equal(dispatch.tool_name, 'blueprint_compile_blueprint');
  assert.deepEqual(dispatch.allowed_tools, ['blueprint_compile_blueprint']);
  assert.equal(dispatch.routes.length, 0);
  assert.equal(dispatch.slot_templates.length, 0);
  assert.equal(
    dispatch.cli_invocation_templates.some((template) =>
      template.path.endsWith('blueprint_compile_blueprint_template.json')),
    true,
  );
  assert.equal(dispatch.stop_conditions.includes('write_session_required'), true);
  assert.equal(dispatch.stop_conditions.includes('compile_failed'), true);
  assert.equal(dispatch.stop_conditions.includes('target_asset_not_found'), true);
});

test('save asset is advertised as an editor write capability', () => {
  const writeList = listToolCapabilities({ domain: 'editor', kind: 'write' });
  const saveItem = writeList.items.find((item) => item.id === 'editor.write.asset.save');
  assert.ok(saveItem);
  assert.equal(saveItem.risk, 'high');
  assert.equal(saveItem.requires_bridge, true);
  assert.equal(saveItem.requires_write_session, true);

  const dispatch = getToolTemplateDispatch('editor.write.asset.save');
  assert.equal(dispatch.tool_name, 'blueprint_save_asset');
  assert.deepEqual(dispatch.allowed_tools, ['blueprint_save_asset']);
  assert.equal(dispatch.routes.length, 0);
  assert.equal(dispatch.slot_templates.length, 0);
  assert.equal(
    dispatch.cli_invocation_templates.some((template) =>
      template.path.endsWith('blueprint_save_asset_template.json')),
    true,
  );
  assert.equal(dispatch.stop_conditions.includes('source_control_unavailable'), true);
  assert.equal(dispatch.stop_conditions.includes('checked_out_by_other'), true);
  assert.equal(dispatch.stop_conditions.includes('source_control_conflicted'), true);
  assert.equal(dispatch.stop_conditions.includes('checkout_required'), true);
  assert.equal(dispatch.stop_conditions.includes('not_editable'), true);
  assert.equal(dispatch.stop_conditions.includes('save_failed'), true);
});

test('source control status is advertised as an editor read capability with conflict stops', () => {
  const readList = listToolCapabilities({ domain: 'editor', kind: 'read' });
  assert.equal(readList.items.some((item) => item.id === 'editor.read.source_control.status'), true);

  const dispatch = getToolTemplateDispatch('editor.read.source_control.status');
  assert.equal(dispatch.tool_name, 'blueprinthelper_source_control_status');
  assert.deepEqual(dispatch.allowed_tools, ['blueprinthelper_source_control_status']);
  assert.equal(
    dispatch.cli_invocation_templates.some((template) =>
      template.path.endsWith('blueprinthelper_source_control_status_template.json')),
    true,
  );
  assert.equal(dispatch.stop_conditions.includes('checked_out_by_other'), true);
  assert.equal(dispatch.stop_conditions.includes('source_control_conflicted'), true);
  assert.equal(dispatch.stop_conditions.includes('not_editable'), true);
});

test('getToolTemplateDispatch rejects unknown tool ids', () => {
  assert.throws(
    () => getToolTemplateDispatch('blueprint.read.unknown'),
    /Unknown BlueprintHelper tool capability id: blueprint\.read\.unknown/,
  );
});

function agentGuideTemplatesRoot(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../../agent-guide/Templates');
}

function listJsonFiles(root: string): string[] {
  const entries = fs.readdirSync(root, { withFileTypes: true });
  return entries.flatMap((entry) => {
    const entryPath = path.join(root, entry.name);
    if (entry.isDirectory()) {
      return listJsonFiles(entryPath);
    }
    return entry.isFile() && entry.name.endsWith('.json') ? [entryPath] : [];
  });
}

function assertNoHiddenTaskSpecPolicyFields(value: unknown, location: string, pathParts: string[] = []): void {
  if (Array.isArray(value)) {
    value.forEach((item, index) => assertNoHiddenTaskSpecPolicyFields(item, location, [...pathParts, String(index)]));
    return;
  }
  if (value === null || typeof value !== 'object') {
    return;
  }

  for (const [key, nestedValue] of Object.entries(value)) {
    const jsonPath = [...pathParts, key].join('.');
    assert.equal(
      HIDDEN_TASKSPEC_POLICY_FIELDS.has(key),
      false,
      `${location} exposes hidden policy field ${jsonPath}`,
    );
    assertNoHiddenTaskSpecPolicyFields(nestedValue, location, [...pathParts, key]);
  }
}

function isHiddenTaskSpecPolicyPath(fieldPath: string): boolean {
  return [...HIDDEN_TASKSPEC_POLICY_FIELDS].some((field) => fieldPath === field || fieldPath.startsWith(`${field}.`));
}
