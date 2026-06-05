import assert from 'node:assert/strict';
import test from 'node:test';

import {
  getToolTemplateDispatch,
  listToolCapabilities,
  listToolDomains,
} from './tool-capability-catalog.js';

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
  assert.equal(dispatch.taskspec_semantic_templates.length, 0);
  assert.deepEqual(dispatch.allowed_tools, ['blueprinthelper_read_context']);
  assert.deepEqual(dispatch.stop_conditions, [
    'tool_unavailable',
    'bridge_unavailable',
    'target function not found',
  ]);
  assert.match(dispatch.recommended_invocation, /blueprinthelper_read_context/);
  assert.equal('show_command' in dispatch, false);
});

test('getToolTemplateDispatch returns both CLI and TaskSpec templates for preview tool', () => {
  const dispatch = getToolTemplateDispatch('blueprint.plan.taskspec.preview');

  assert.equal(dispatch.tool_name, 'blueprinthelper_preview_task');
  assert.equal(dispatch.cli_invocation_templates.some((template) => template.cli_template_id === 'task_preview_bare_taskspec'), true);
  assert.equal(dispatch.taskspec_semantic_templates.some((template) => template.taskspec_template_id === 'taskspec_graph_merge_external_flow'), true);
  assert.deepEqual(dispatch.allowed_tools, ['blueprinthelper_preview_task']);
  assert.deepEqual(dispatch.stop_conditions, [
    'tool_unavailable',
    'write_session_required',
    'taskspec_template_unavailable',
    'preview_blocked',
  ]);
});

test('getToolTemplateDispatch rejects unknown tool ids', () => {
  assert.throws(
    () => getToolTemplateDispatch('blueprint.read.unknown'),
    /Unknown BlueprintHelper tool capability id: blueprint\.read\.unknown/,
  );
});
