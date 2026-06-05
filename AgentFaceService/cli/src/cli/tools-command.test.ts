import assert from 'node:assert/strict';
import path from 'node:path';
import test from 'node:test';

import { buildHelpText } from './help.js';
import { runCli } from './run.js';

test('runCli returns active tool domains as catalog JSON without bridge access', async () => {
  const { output, stderr } = await runCliJson(['tools', 'domains', '--format', 'json']);

  assert.deepEqual(stderr, []);
  assert.equal(output.schema, 'BlueprintHelper.ToolDomainList.v1');
  assert.equal(output.items.some((item: Record<string, unknown>) => item.id === 'blueprint'), true);
  assert.equal(output.items.some((item: Record<string, unknown>) => item.id === 'animation'), false);
  assert.deepEqual(output.reserved, []);
  assert.deepEqual(output.next, {
    list_command: 'bh tools list <domain> <kind> --format json',
  });
});

test('runCli returns reserved tool domains only when requested', async () => {
  const { output } = await runCliJson(['tools', 'domains', '--include-reserved', '--format', 'json']);

  assert.equal(output.reserved.some((item: Record<string, unknown>) => item.id === 'animation'), true);
  assert.equal(output.reserved.some((item: Record<string, unknown>) => item.id === 'material'), true);
});

test('runCli filters tool capability catalog by domain kind and options', async () => {
  const { output } = await runCliJson([
    'tools',
    'list',
    'blueprint',
    'read',
    '--requires-bridge',
    'true',
    '--risk',
    'low',
    '--format',
    'json',
  ]);

  assert.equal(output.schema, 'BlueprintHelper.ToolCapabilityList.v1');
  assert.equal(output.query.domain, 'blueprint');
  assert.equal(output.query.kind, 'read');
  assert.equal(output.items.some((item: Record<string, unknown>) => item.id === 'blueprint.read.context.logic_flow'), true);
  assert.equal(output.items.every((item: Record<string, unknown>) => item.requires_bridge === true), true);
  assert.equal(output.items.every((item: Record<string, unknown>) => item.risk === 'low'), true);
});

test('runCli returns template dispatch package for a selected tool id', async () => {
  const { output } = await runCliJson(['tools', 'templates', 'blueprint.read.context.logic_flow', '--format', 'json']);

  assert.equal(output.schema, 'BlueprintHelper.ToolTemplateSelection.v1');
  assert.equal(output.tool_id, 'blueprint.read.context.logic_flow');
  assert.equal(output.tool_name, 'blueprinthelper_read_context');
  assert.deepEqual(output.allowed_tools, ['blueprinthelper_read_context']);
  assert.match(output.recommended_invocation, /blueprinthelper_read_context/);
  assert.doesNotMatch(output.recommended_invocation, /Templates\/read\/read_context_/);
  assert.equal('taskspec_semantic_templates' in output, false);
  assert.equal(
    output.cli_invocation_templates.every((template: Record<string, unknown>) => {
      return String(template.path).includes('/read/routes/');
    }),
    true,
  );
  assert.equal('show_command' in output, false);
});

test('ReadContext help points to route-owned templates only', () => {
  const readContextHelp = buildHelpText(['blueprinthelper_read_context']);
  const groupedReadHelp = buildHelpText(['context', 'read']);

  assert.doesNotMatch(readContextHelp, /Templates\/read\/read_context_/);
  assert.doesNotMatch(groupedReadHelp, /Templates\/read\/read_context_/);
  assert.match(readContextHelp, /Templates\/read\/routes\/blueprint_logic_function_logic_flow_template\.json/);
  assert.match(groupedReadHelp, /Templates\/read\/routes\/blueprint_logic_function_logic_flow_template\.json/);
});

test('runCli returns route-filtered slot templates for a selected tool id', async () => {
  const { output } = await runCliJson([
    'tools',
    'templates',
    'blueprint.write.taskspec.execute',
    '--route',
    'graph.replace.function_body',
    '--slot',
    '--kind',
    'statement',
    '--format',
    'json',
  ]);

  assert.equal(output.schema, 'BlueprintHelper.ToolTemplateSelection.v1');
  assert.equal(output.selected_route.route_id, 'graph.replace.function_body');
  assert.equal(output.slot_templates.every((slot: Record<string, unknown>) => slot.slot_type === 'statement'), true);
  assert.equal(
    output.slot_templates.some((slot: Record<string, unknown>) =>
      slot.slot_id === 'graph.statement.call.direct'
      && String(slot.path).endsWith('graph_statement_call_direct_template.json')),
    true,
  );
});

test('runCli rejects template slot kind without slot output', async () => {
  const stdout: string[] = [];
  const stderr: string[] = [];
  const exitCode = await runCli({
    argv: [
      'tools',
      'templates',
      'blueprint.write.taskspec.execute',
      '--route',
      'graph.replace.function_body',
      '--kind',
      'statement',
      '--format',
      'json',
    ],
    cwd: workspaceRoot(),
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  });

  assert.equal(exitCode, 64);
  assert.equal(stdout.join(''), '');
  assert.match(stderr.join(''), /--kind requires --slot/);
});

test('runCli does not register an independent tool detail command', async () => {
  const stdout: string[] = [];
  const stderr: string[] = [];
  const exitCode = await runCli({
    argv: ['tools', 'show', 'blueprint.read.context.logic_flow', '--format', 'json'],
    cwd: workspaceRoot(),
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  });

  assert.equal(exitCode, 64);
  assert.equal(stdout.join(''), '');
  assert.match(stderr.join(''), /Unsupported BlueprintHelper CLI tools command: show/);
});

test('global help points tool selection to tools catalog and not semantic indexes', () => {
  const help = buildHelpText();

  assert.match(help, /bh tools domains --format json/);
  assert.match(help, /bh tools list <domain> <kind> --format json/);
  assert.match(help, /bh tools templates <tool_id> --format json/);
  assert.doesNotMatch(help, new RegExp(['SEMANTIC', 'INDEX'].join('_')));
});

async function runCliJson(argv: string[]): Promise<{ output: Record<string, any>; stderr: string[] }> {
  const stdout: string[] = [];
  const stderr: string[] = [];
  const exitCode = await runCli({
    argv,
    cwd: workspaceRoot(),
    stdout: (text) => stdout.push(text),
    stderr: (text) => stderr.push(text),
  });

  assert.equal(exitCode, 0);
  assert.deepEqual(stderr, []);
  return {
    output: JSON.parse(stdout.join('')) as Record<string, any>,
    stderr,
  };
}

function workspaceRoot(): string {
  return path.join('D:', 'UEProjects', 'Template', 'Plugins', 'BlueprintHelper');
}
