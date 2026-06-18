import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import {
  createDescriptorFixtureRuntimeCapabilityState,
  getToolCapabilityDescriptor,
  listToolCapabilities,
  listToolDomains,
} from '../tool-registry.js';
import { getReadContextRouteDescriptor } from '../templates/read-context-template-registry.js';

const PLUGIN_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../../..');
const RUNTIME_FIXTURE = createDescriptorFixtureRuntimeCapabilityState();

test('tool catalog lists active domains and points TaskSpec write discovery to family index', () => {
  const domains = listToolDomains();
  assert.equal(domains.schema, 'BlueprintHelper.ToolDomainList.v1');
  assert.equal(domains.items.some((item) => item.id === 'blueprint'), true);
  assert.equal(domains.reserved.length, 0);

  const blueprintWrite = listToolCapabilities({ domain: 'blueprint', kind: 'write', runtime: RUNTIME_FIXTURE });
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
  const blueprintWrite = listToolCapabilities({ domain: 'blueprint', kind: 'write', runtime: RUNTIME_FIXTURE });
  assert.equal(
    blueprintWrite.next.template_index_command,
    'bh tools templates families --workflow preview_execute --format json',
  );

  const blueprintRead = listToolCapabilities({ domain: 'blueprint', kind: 'read' });
  assert.equal(
    blueprintRead.next.template_index_command,
    'bh tools read-templates families --format json',
  );

  const projectDomain = listToolDomains().items.find((item) => item.id === 'project');
  assert.ok(projectDomain);
  assert.equal(projectDomain.default_kinds.includes('read'), true);

  const projectRead = listToolCapabilities({ domain: 'project', kind: 'read' });
  const projectReadNext = projectRead.next as Record<string, unknown>;
  assert.equal(projectReadNext['template_index_command'], undefined);
  assert.equal(projectReadNext['command'], 'bh task result --id <task_run_id> --format summary');
  const taskResult = projectRead.items.find((item) => item.id === 'project.read.task_result');
  assert.ok(taskResult);
  assert.equal(taskResult.cli_command, 'bh task result --id <task_run_id>');
  assert.deepEqual(taskResult.input_shapes, ['cli_options']);
});

test('tool catalog filters by bridge and risk without old template dispatch', () => {
  const reads = listToolCapabilities({
    domain: 'blueprint',
    kind: 'read',
    requiresBridge: true,
    risks: ['low'],
  });

  assert.equal(reads.items.some((item) => item.id === 'blueprint.read.context.logic_flow'), true);
  const removedMarkdownFormat = ['logic', 'md'].join('_');
  const removedMarkdownCapabilityId = `blueprint.read.context.${removedMarkdownFormat}`;
  assert.equal(reads.items.some((item) => item.id === removedMarkdownCapabilityId), false);
  assert.equal(reads.items.some((item) => JSON.stringify(item).includes(removedMarkdownFormat)), false);
  assert.equal(reads.items.every((item) => item.requires_bridge === true), true);
  assert.equal(reads.items.every((item) => item.risk === 'low'), true);
});

test('tool catalog does not expose removed markdown read format in any read capability', () => {
  const removedMarkdownFormat = ['logic', 'md'].join('_');
  for (const domain of ['blueprint', 'material'] as const) {
    const reads = listToolCapabilities({
      domain,
      kind: 'read',
      requiresBridge: true,
      risks: ['low'],
    });
    assert.equal(reads.items.some((item) => item.id.includes(removedMarkdownFormat)), false);
    assert.equal(JSON.stringify(reads.items).includes(removedMarkdownFormat), false);
    assert.equal(JSON.stringify(reads.items).includes(['Logic', 'Md'].join('')), false);
  }

  assert.equal(getToolCapabilityDescriptor(`material.read.context.${removedMarkdownFormat}`), undefined);
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
    'widget.structure.tree_json',
    'widget.structure.tree_flow',
  ]);
	assert.equal(widgetTree.stop_conditions.includes('bridge_unavailable'), true);
});

test('tool catalog exposes only composeable flat ReadContext template ids', () => {
  for (const domain of ['blueprint', 'umg', 'data', 'material'] as const) {
    const reads = listToolCapabilities({
      domain,
      kind: 'read',
      requiresBridge: true,
      risks: ['low'],
    });

    for (const item of reads.items.filter((entry) => entry.cli_command === 'bh context read')) {
      const descriptor = getToolCapabilityDescriptor(item.id);
      assert.ok(descriptor, `${item.id} has capability descriptor`);
      assert.deepEqual(
        [...item.cli_template_ids].sort(),
        [...descriptor.route_refs].sort(),
        `${item.id} cli_template_ids should mirror ReadContext route_refs`,
      );
      assert.deepEqual(item.input_shapes, ['readspec'], `${item.id} should still accept ReadSpec input`);
      for (const templateId of item.cli_template_ids) {
        assert.ok(
          getReadContextRouteDescriptor(templateId),
          `${item.id} exposes unknown ReadContext template id ${templateId}`,
        );
      }
    }
  }
});

test('tool catalog exposes ReadContext capabilities matrix as local discovery', () => {
	const projectDiscover = listToolCapabilities({
		domain: 'project',
		kind: 'discover',
	});

	const capability = projectDiscover.items.find((item) => item.id === 'project.discover.read_context_capabilities');
	assert.ok(capability);
	assert.equal(capability.cli_command, 'bh blueprinthelper_read_context_capabilities');
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
	const agentGuide = projectDiscover.items.find((item) => item.cli_command === 'bh blueprinthelper_read_agent_guide');
	assert.ok(agentGuide);
	assert.equal(agentGuide.input_shape, 'empty_object');
	assert.equal(agentGuide.no_input, true);
	assert.match(agentGuide.input_note ?? '', /No parameters/);
	assert.match(agentGuide.input_note ?? '', /\{\}/);

	const editorRead = listToolCapabilities({
		domain: 'editor',
		kind: 'read',
	});
	const runtimeProfile = editorRead.items.find((item) => item.cli_command === 'bh blueprint_get_runtime_profile');
	assert.ok(runtimeProfile);
	assert.equal(runtimeProfile.input_shape, 'empty_object');
	assert.equal(runtimeProfile.no_input, true);
	assert.match(runtimeProfile.input_note ?? '', /Use the empty-object template as-is/);
});

test('expert review action exposes a developer-mode template file', () => {
  const reviewWrite = listToolCapabilities({
    domain: 'review',
    kind: 'write',
    audience: 'expert',
    expert: true,
  });
  const applyAction = reviewWrite.items.find((item) => item.cli_command === 'bh blueprinthelper_apply_review_action');
  assert.ok(applyAction);
  assert.deepEqual(applyAction.cli_template_ids, ['blueprinthelper_apply_review_action']);

  const templatePath = path.join(
    PLUGIN_ROOT,
    'AgentFaceService/agent-guide/Templates/blueprinthelper_apply_review_action_template.json',
  );
  assert.equal(fs.existsSync(templatePath), true);
  const template = JSON.parse(fs.readFileSync(templatePath, 'utf8')) as Record<string, unknown>;
  assert.match(String(template['$comment'] ?? ''), /Developer mode only/i);
  assert.equal(template.review_record_id, '__REQUIRED_REVIEW_RECORD_ID__');
  assert.equal(template.action, 'accept');
});

test('tool catalog does not expose editor lifecycle as CLI compat entries', () => {
  const editorWrite = listToolCapabilities({
    domain: 'editor',
    kind: 'write',
    audience: 'compat',
  });

  assert.equal(editorWrite.items.some((item) => item.cli_command.includes('blueprint_open_editor')), false);
  assert.equal(editorWrite.items.some((item) => item.cli_command.includes('blueprint_close_editor')), false);
  assert.equal(JSON.stringify(editorWrite.items).includes('lifecycle_mcp_only'), false);
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

test('tool catalog exposes grouped CLI commands and hides direct TaskSpec handler names', () => {
  const blueprintRead = listToolCapabilities({ domain: 'blueprint', kind: 'read' });
  const logicFlow = blueprintRead.items.find((item) => item.id === 'blueprint.read.context.logic_flow');
  assert.ok(logicFlow);
  assert.equal(logicFlow.cli_command, 'bh context read');

  const blueprintPlan = listToolCapabilities({ domain: 'blueprint', kind: 'plan', runtime: RUNTIME_FIXTURE });
  const preview = blueprintPlan.items.find((item) => item.id === 'blueprint.plan.taskspec.preview');
  assert.ok(preview);
  assert.equal(preview.cli_command, 'bh task preview');
  assert.deepEqual(preview.cli_template_ids, ['task_preview_bare_taskspec']);
  assert.equal(preview.input_shape, 'bare_taskspec');

  const blueprintWrite = listToolCapabilities({ domain: 'blueprint', kind: 'write', runtime: RUNTIME_FIXTURE });
  const execute = blueprintWrite.items.find((item) => item.id === 'blueprint.write.taskspec.execute');
  assert.ok(execute);
  assert.equal(execute.cli_command, 'bh task execute');
  assert.deepEqual(execute.cli_template_ids, ['task_execute_bare_taskspec']);
  assert.equal(execute.input_shape, 'bare_taskspec');

  const projectRead = listToolCapabilities({ domain: 'project', kind: 'read' });
  const taskResult = projectRead.items.find((item) => item.id === 'project.read.task_result');
  assert.ok(taskResult);
  assert.equal(taskResult.cli_command, 'bh task result --id <task_run_id>');
  assert.deepEqual(taskResult.cli_template_ids, []);
  assert.deepEqual(taskResult.input_shapes, ['cli_options']);

  const serialized = JSON.stringify([
    ...blueprintRead.items,
    ...blueprintPlan.items,
    ...blueprintWrite.items,
    ...projectRead.items,
  ]);
  assert.equal(serialized.includes('blueprinthelper_read_context'), false);
  assert.equal(serialized.includes('blueprinthelper_preview_task'), false);
  assert.equal(serialized.includes('blueprinthelper_execute_task'), false);
  assert.equal(serialized.includes('blueprinthelper_get_task_result'), false);
});

test('tool catalog gates descriptor-driven material write discovery by runtime adapters', () => {
  const hidden = listToolCapabilities({
    domain: 'material',
    kind: 'write',
    runtime: {
      registered_runtime_adapter_ids: [],
      allow_write_capabilities: true,
      allow_high_risk_capabilities: true,
    },
  });
  assert.equal(hidden.items.some((item) => item.id === 'material.write.taskspec.execute'), false);

  const visible = listToolCapabilities({
    domain: 'material',
    kind: 'write',
    runtime: {
      registered_runtime_adapter_ids: ['material_graph_runtime_adapter'],
      allow_write_capabilities: true,
      allow_high_risk_capabilities: true,
    },
  });
  assert.equal(visible.items.some((item) => item.id === 'material.write.taskspec.execute'), true);
  assert.equal(JSON.stringify(visible.items).includes('material_instance'), false);

  const materialInstanceVisible = listToolCapabilities({
    domain: 'material',
    kind: 'write',
    runtime: {
      registered_runtime_adapter_ids: ['material_instance_runtime_adapter'],
      allow_write_capabilities: true,
      allow_high_risk_capabilities: true,
    },
  });
  assert.equal(materialInstanceVisible.items.some((item) => item.id === 'material.write.taskspec.execute'), true);
});

test('tool catalog derives task runtime capability truth from active non-reserved descriptors', () => {
  const dataWrite = listToolCapabilities({
    domain: 'data',
    kind: 'write',
    runtime: {
      registered_runtime_adapter_ids: [
        'data_table_runtime_adapter',
        'struct_runtime_adapter',
      ],
      allow_write_capabilities: true,
      allow_high_risk_capabilities: true,
    },
  });
  assert.equal(dataWrite.items.some((item) => item.id === 'data.write.taskspec.execute'), true);
  assert.equal(JSON.stringify(dataWrite.items).includes('struct.fields.edit'), false);

  const materialWrite = listToolCapabilities({
    domain: 'material',
    kind: 'write',
    runtime: {
      registered_runtime_adapter_ids: [
        'material_graph_runtime_adapter',
        'material_instance_runtime_adapter',
      ],
      allow_write_capabilities: true,
      allow_high_risk_capabilities: true,
    },
  });
  assert.equal(materialWrite.items.some((item) => item.id === 'material.write.taskspec.execute'), true);
  assert.equal(materialWrite.items[0]?.purpose.includes('MaterialInstance'), true);
});

test('tool catalog hides write capabilities when descriptor safety disallows writes', () => {
  const hidden = listToolCapabilities({
    domain: 'blueprint',
    kind: 'write',
    runtime: {
      registered_runtime_adapter_ids: ['graphwrite_runtime_adapter'],
      allow_write_capabilities: false,
      allow_high_risk_capabilities: true,
    },
  });
  assert.equal(hidden.items.some((item) => item.id === 'blueprint.write.taskspec.execute'), false);
});
