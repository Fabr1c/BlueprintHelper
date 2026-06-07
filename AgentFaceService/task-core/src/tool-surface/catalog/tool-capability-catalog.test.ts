import assert from 'node:assert/strict';
import test from 'node:test';

import {
  getToolCapabilityDescriptor,
  listToolCapabilities,
  listToolDomains,
} from '../tool-registry.js';

test('tool catalog lists active domains and points TaskSpec write discovery to family index', () => {
  const domains = listToolDomains();
  assert.equal(domains.schema, 'BlueprintHelper.ToolDomainList.v1');
  assert.equal(domains.items.some((item) => item.id === 'blueprint'), true);
  assert.equal(domains.reserved.length, 0);

  const blueprintWrite = listToolCapabilities({ domain: 'blueprint', kind: 'write' });
  assert.equal(blueprintWrite.schema, 'BlueprintHelper.ToolCapabilityList.v1');
  assert.equal(
    blueprintWrite.next.template_index_command,
    'bh tools templates families --workflow preview_execute --format json',
  );
  assert.equal(
    blueprintWrite.items.some((item) => item.id === 'blueprint.write.taskspec.execute'),
    true,
  );
});

test('tool catalog points read and write capabilities to their own template indexes', () => {
  const blueprintWrite = listToolCapabilities({ domain: 'blueprint', kind: 'write' });
  assert.equal(
    blueprintWrite.next.template_index_command,
    'bh tools templates families --workflow preview_execute --format json',
  );

  const blueprintRead = listToolCapabilities({ domain: 'blueprint', kind: 'read' });
  assert.equal(
    blueprintRead.next.template_index_command,
    'bh tools read-templates domains --format json',
  );
});

test('tool catalog filters by bridge and risk without old template dispatch', () => {
  const reads = listToolCapabilities({
    domain: 'blueprint',
    kind: 'read',
    requiresBridge: true,
    risks: ['low'],
  });

  assert.equal(reads.items.some((item) => item.id === 'blueprint.read.context.logic_flow'), true);
  assert.equal(reads.items.every((item) => item.requires_bridge === true), true);
  assert.equal(reads.items.every((item) => item.risk === 'low'), true);
});

test('tool catalog exposes UMG widget tree read capability through ReadContext route refs', () => {
	const reads = listToolCapabilities({
		domain: 'umg',
		kind: 'read',
    requiresBridge: true,
    risks: ['low'],
  });

  assert.equal(reads.items.some((item) => item.id === 'umg.read.widget_tree'), true);
  assert.equal(reads.items.some((item) => item.id === 'umg.read.widget_property'), true);

  const widgetTree = getToolCapabilityDescriptor('umg.read.widget_tree');
  assert.ok(widgetTree);
  assert.equal(widgetTree.tool_name, 'blueprinthelper_read_context');
  assert.deepEqual(widgetTree.route_refs, [
    'read.widget_blueprint.structure_tree.widget_tree.tree_json',
    'read.widget_blueprint.structure_tree.widget_tree.logic_flow',
  ]);
	assert.equal(widgetTree.stop_conditions.includes('bridge_unavailable'), true);
});

test('tool catalog exposes ReadContext capabilities matrix as local discovery', () => {
	const projectDiscover = listToolCapabilities({
		domain: 'project',
		kind: 'discover',
	});

	const capability = projectDiscover.items.find((item) => item.id === 'project.discover.read_context_capabilities');
	assert.ok(capability);
	assert.equal(capability.tool_name, 'blueprinthelper_read_context_capabilities');
	assert.equal(capability.requires_bridge, false);
	assert.equal(capability.risk, 'none');

	const descriptor = getToolCapabilityDescriptor('project.discover.read_context_capabilities');
	assert.ok(descriptor);
	assert.deepEqual(
		descriptor.recommended_invocations,
		['bh blueprinthelper_read_context_capabilities --json "{}" --format json'],
	);
});

test('tool catalog marks empty-object templates as no-input requests', () => {
	const projectDiscover = listToolCapabilities({
		domain: 'project',
		kind: 'discover',
	});
	const agentGuide = projectDiscover.items.find((item) => item.tool_name === 'blueprinthelper_read_agent_guide');
	assert.ok(agentGuide);
	assert.equal(agentGuide.input_shape, 'empty_object');
	assert.equal(agentGuide.no_input, true);
	assert.match(agentGuide.input_note ?? '', /No parameters/);
	assert.match(agentGuide.input_note ?? '', /\{\}/);

	const editorRead = listToolCapabilities({
		domain: 'editor',
		kind: 'read',
	});
	const runtimeProfile = editorRead.items.find((item) => item.tool_name === 'blueprint_get_runtime_profile');
	assert.ok(runtimeProfile);
	assert.equal(runtimeProfile.input_shape, 'empty_object');
	assert.equal(runtimeProfile.no_input, true);
	assert.match(runtimeProfile.input_note ?? '', /Use the empty-object template as-is/);
});

test('tool capability descriptors keep manifest facts without exposing old template selection schema', () => {
	const descriptor = getToolCapabilityDescriptor('blueprint.write.taskspec.execute');
	assert.ok(descriptor);
  assert.equal(descriptor.tool_name, 'blueprinthelper_execute_task');
  assert.equal(descriptor.route_refs.includes('graph.replace.function_body'), true);
  assert.equal(descriptor.route_refs.includes('blueprint.create_feature'), true);
  assert.equal(descriptor.stop_conditions.includes('execute_failed'), true);
  assert.equal(
    descriptor.recommended_invocations.includes('bh task execute --file <filled_taskspec.json> --preview-token <preview_token> --format summary'),
    true,
  );
});
