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

test('Codex plugin registers all BlueprintHelper subagents', () => {
  const agents = [
    {
      name: 'blueprint-explorer',
      skill: 'blueprint-helper-blueprint-explorer',
    },
    {
      name: 'sourcecode-explorer',
      skill: 'blueprint-helper-sourcecode-explorer',
    },
    {
      name: 'task-worker',
      skill: 'blueprint-helper-task-worker',
    },
  ] as const;

  for (const agent of agents) {
    const tomlPath = path.join(PLUGIN_ROOT, 'CodexPlugin', 'agents', `${agent.name}.toml`);
    const skillPath = path.join(PLUGIN_ROOT, 'CodexPlugin', 'skills', agent.skill, 'SKILL.md');
    const openAiAgentPath = path.join(PLUGIN_ROOT, 'CodexPlugin', 'skills', agent.skill, 'agents', 'openai.yaml');

    assert.equal(fs.existsSync(tomlPath), true, `${agent.name} TOML is missing`);
    assert.equal(fs.existsSync(skillPath), true, `${agent.name} fork skill is missing`);
    assert.equal(fs.existsSync(openAiAgentPath), true, `${agent.name} OpenAI agent manifest is missing`);

    const toml = readText(tomlPath);
    assert.match(toml, new RegExp(`^name\\s*=\\s*"${agent.name}"`, 'm'), `${agent.name} TOML has wrong name`);
    assert.match(toml, /^model\s*=\s*"/m, `${agent.name} TOML must pin a model`);
    assert.match(toml, /^model_reasoning_effort\s*=\s*"/m, `${agent.name} TOML must use Codex reasoning field`);
    assert.doesNotMatch(toml, /^reasoning_effort\s*=/m, `${agent.name} TOML uses the unsupported reasoning_effort field`);

    const skill = readText(skillPath);
    assert.match(skill, new RegExp(`^agent:\\s*${agent.name}\\s*$`, 'm'), `${agent.skill} must fork ${agent.name}`);

    const openAiAgent = readText(openAiAgentPath);
    assert.match(openAiAgent, /^interface:\s*$/m, `${agent.skill} OpenAI agent manifest is missing interface`);
    assert.match(openAiAgent, /^\s{2}display_name:\s*"/m, `${agent.skill} OpenAI agent manifest is missing display_name`);
    assert.match(openAiAgent, /^\s{2}short_description:\s*"/m, `${agent.skill} OpenAI agent manifest is missing short_description`);
  }
});

function readText(filePath: string): string {
  return fs.readFileSync(filePath, 'utf8').replace(/^\uFEFF/, '');
}
