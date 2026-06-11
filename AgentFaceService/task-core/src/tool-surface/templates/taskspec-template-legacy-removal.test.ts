import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
const PLUGIN_ROOT = path.resolve(ROOT, '../..');
const PRODUCTION_FILES = [
  'src/tool-surface/tool-registry.ts',
  'src/tool-surface/catalog/tool-capability-types.ts',
  'src/tool-surface/catalog/tool-capability-catalog.ts',
  'src/tool-surface/manifest/tool-command-manifest-builder.ts',
];

test('old ToolTemplateSelection dispatch surface is removed from production code', () => {
  const forbidden = [
    'ToolTemplateSelection.v1',
    'getToolTemplateDispatch',
    'GetToolTemplateDispatchOptions',
    'selected_route',
    'slot_templates',
    'ToolsTemplateBuilder',
    'bh tools templates <tool_id>',
  ];

  for (const relativePath of PRODUCTION_FILES) {
    const text = fs.readFileSync(path.join(ROOT, relativePath), 'utf8');
    for (const token of forbidden) {
      assert.equal(text.includes(token), false, `${relativePath} still contains ${token}`);
    }
  }
});

test('TaskSpec workflow docs require four-layer composer and grouped task commands', () => {
  const docs = [
    'CodexPlugin/skills/blueprint-helper-task-worker/SKILL.md',
    'CodexPlugin/agents/task-worker.md',
    'CodexPlugin/agents/task-worker.toml',
    'CodexPlugin/skills/blueprint-helper/references/04_TaskSpec_Edit_Blueprint_Workflow.md',
    'AgentFaceService/agent-guide/Workflows/04_TaskSpec_Edit_Blueprint_Workflow.md',
  ];

  for (const relativePath of docs) {
    const text = fs.readFileSync(path.join(PLUGIN_ROOT, relativePath), 'utf8');
    assert.match(text, /tools templates families|tools templates compose/, relativePath);
    assert.match(text, /task preview --file|task execute --file/, relativePath);
    assert.doesNotMatch(text, /bh tools templates <tool_id>/, relativePath);
    assert.doesNotMatch(text, /ToolTemplateSelection\.v1|selected_route|slot_templates|ToolsTemplateBuilder/, relativePath);
  }
});
