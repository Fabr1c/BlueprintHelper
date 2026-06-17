import assert from 'node:assert/strict';
import test from 'node:test';

import { listCliCommandDescriptors } from './cli-command-descriptor.js';
import { routeCliCommand } from './cli-command-router.js';
import {
  listCliSubcommandDescriptors,
  resolveCliSubcommandGroupFromPositionals,
  templateIndexCommandForCapabilityKind,
} from './cli-subcommand-descriptor.js';

const base = {
  format: 'summary',
  artifactDir: undefined,
  maxBytes: undefined,
  fields: undefined,
  omitFields: undefined,
  develop: undefined,
  expert: undefined,
};

test('routes task preview through descriptor required-option mapping', () => {
  const missingFile = routeCliCommand({
    positionals: ['task', 'preview'],
    options: {},
    base,
  });
  assert.equal(missingFile.ok, false);
  assert.equal(missingFile.message, 'Missing --file for bh task preview.');

  const routed = routeCliCommand({
    positionals: ['task', 'preview'],
    options: { file: 'task.json', compileOnly: true },
    base,
  });
  assert.equal(routed.ok, true);
  assert.equal(routed.command.kind, 'task.preview');
  assert.equal(routed.command.resultPolicyId, 'task.preview.default');
  assert.equal(routed.command.statusPolicyId, 'task.preview_status');
  assert.equal(routed.command.runIdPolicyId, 'task.preview_run_id');
  assert.equal(routed.command.metricsToolName, 'blueprinthelper_preview_task');
  assert.equal(routed.command.metricsLookupId, 'blueprint.plan.taskspec.preview');
  const capabilityDescriptorIds = routed.command.capabilityDescriptorIds;
  assert.ok(Array.isArray(capabilityDescriptorIds));
  assert.equal(capabilityDescriptorIds.includes('graphwrite.execute'), true);
  assert.equal(capabilityDescriptorIds.includes('asset_factory.create'), true);
  assert.equal(capabilityDescriptorIds.includes('data_table.rows.edit'), true);
  assert.equal(capabilityDescriptorIds.includes('material_graph.edit'), true);
  assert.equal(capabilityDescriptorIds.includes('struct.fields.edit'), false);
  assert.equal(capabilityDescriptorIds.includes('material_instance.edit'), false);
  assert.equal(routed.command.inputIoKind, 'task_file');
  assert.equal(routed.command.file, 'task.json');
  assert.equal(routed.command.compileOnly, true);
});

test('routes grouped context read with one-of input source mapping', () => {
  const missingInput = routeCliCommand({
    positionals: ['context', 'read'],
    options: {},
    base,
  });
  assert.equal(missingInput.ok, false);
  assert.equal(missingInput.message, 'Missing input for bh context read: choose --file, --json, or --stdin.');

  const routed = routeCliCommand({
    positionals: ['context', 'read'],
    options: { stdin: true },
    base,
  });
  assert.equal(routed.ok, true);
  assert.equal(routed.command.kind, 'context.read');
  assert.equal(routed.command.toolName, 'blueprinthelper_read_context');
  assert.equal(routed.command.manifestLookupId, 'blueprint.read.context.logic_flow');
  assert.equal(routed.command.metricsLookupId, 'blueprint.read.context.logic_flow');
  assert.equal(routed.command.stdin, true);
});

test('routes tools list with descriptor positional captures and option defaults', () => {
  const routed = routeCliCommand({
    positionals: ['tools', 'list', 'write', 'safe'],
    options: { requiresBridge: true, risks: ['write'], expert: true },
    base,
  });

  assert.equal(routed.ok, true);
  assert.equal(routed.command.kind, 'tools.list');
  assert.equal(routed.command.toolDomain, 'write');
  assert.equal(routed.command.toolCatalogKind, 'safe');
  assert.equal(routed.command.audience, 'default');
  assert.equal(routed.command.requiresBridge, true);
  assert.deepEqual(routed.command.risks, ['write']);
});

test('routes template compose subcommands through the top-level CLI command descriptor', () => {
  const routed = routeCliCommand({
    positionals: ['tools', 'templates', 'compose'],
    options: {
      family: 'graphwrite',
      writeMode: 'preview_execute',
      templates: ['entry@exec', 'body(args)'],
      out: 'task.json',
    },
    base,
  });

  assert.equal(routed.ok, true);
  assert.equal(routed.command.kind, 'tools.templates.compose');
  assert.equal(routed.command.family, 'graphwrite');
  assert.equal(routed.command.writeMode, 'preview_execute');
  assert.equal(routed.command.outputPath, 'task.json');
  assert.deepEqual(routed.command.templateIds, ['entry@exec', 'body(args)']);
});

test('subcommand CLI positionals and template index commands come from group descriptors', () => {
  const subcommands = listCliSubcommandDescriptors();
  for (const subcommand of subcommands) {
    const command = listCliCommandDescriptors().find((entry) => entry.id === subcommand.kind);
    assert.ok(command, `missing CLI command descriptor for ${subcommand.kind}`);
    assert.deepEqual(command.positionals, subcommand.positionals);
  }

  const readGroup = resolveCliSubcommandGroupFromPositionals(['tools', 'read-templates', 'families']);
  assert.equal(readGroup?.group, 'tools.read_templates');
  assert.deepEqual(readGroup?.command_prefix, ['tools', 'read-templates']);
  assert.equal(
    templateIndexCommandForCapabilityKind('read'),
    'bh tools read-templates families --format json',
  );
  assert.equal(
    templateIndexCommandForCapabilityKind('write'),
    'bh tools templates families --workflow preview_execute --format json',
  );
});

test('CLI command descriptors do not expose editor lifecycle aliases', () => {
  const descriptors = listCliCommandDescriptors();
  const serialized = JSON.stringify(descriptors);

  assert.equal(descriptors.some((entry) => entry.positionals.join(' ') === 'open_editor'), false);
  assert.equal(descriptors.some((entry) => entry.positionals.join(' ') === 'close_editor'), false);
  assert.equal(serialized.includes('blueprint_open_editor'), false);
  assert.equal(serialized.includes('blueprint_close_editor'), false);
});

test('routes metrics commands with descriptor defaults and format guard', () => {
  const defaults = routeCliCommand({
    positionals: ['metrics', 'top-errors'],
    options: {},
    base,
  });
  assert.equal(defaults.ok, true);
  assert.equal(defaults.command.kind, 'metrics.report');
  assert.equal(defaults.command.resultPolicyId, 'metrics.report.default');
  assert.equal(defaults.command.statusPolicyId, 'metrics.report_status');
  assert.equal(defaults.command.runIdPolicyId, 'metrics.report_run_id');
  assert.equal(defaults.command.outputDataPolicyId, 'metrics.report_data');
  assert.equal(defaults.command.metricsKind, 'top-errors');
  assert.equal(defaults.command.format, 'json');
  assert.equal(defaults.command.window, '7d');
  assert.equal(defaults.command.limit, 20);

  const markdown = routeCliCommand({
    positionals: ['metrics', 'task-health'],
    options: { format: 'markdown' },
    base,
  });
  assert.equal(markdown.ok, true);
  assert.equal(markdown.command.format, 'markdown');

  const summary = routeCliCommand({
    positionals: ['metrics', 'report'],
    options: { format: 'summary' },
    base,
  });
  assert.equal(summary.ok, false);
  assert.equal(summary.message, 'Unsupported --format value for bh metrics report: summary');
});
