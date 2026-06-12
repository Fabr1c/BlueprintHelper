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

test('Agent-facing docs do not expose internal policy fields or legacy task envelopes', () => {
  const docs = [
    'README.md',
    'ClaudePlugin/README.md',
    'CodexPlugin/agents/blueprint-explorer.toml',
    'CodexPlugin/agents/task-worker.toml',
    'CodexPlugin/skills/blueprint-helper/SKILL.md',
    'ClaudePlugin/skills/blueprint-helper/SKILL.md',
    'CodexPlugin/skills/blueprint-helper/references/04_TaskSpec_Edit_Blueprint_Workflow.md',
    'ClaudePlugin/skills/blueprint-helper/references/04_TaskSpec_Edit_Blueprint_Workflow.md',
    'CodexPlugin/skills/blueprint-helper/references/05_Edit_Blueprint_Workflow.md',
    'ClaudePlugin/skills/blueprint-helper/references/05_Edit_Blueprint_Workflow.md',
    'CodexPlugin/skills/blueprint-helper/references/06_UMG_Data_Workflows.md',
    'ClaudePlugin/skills/blueprint-helper/references/06_UMG_Data_Workflows.md',
    'CodexPlugin/skills/blueprint-helper/references/07_Safety_Validation_And_Recovery.md',
    'ClaudePlugin/skills/blueprint-helper/references/07_Safety_Validation_And_Recovery.md',
    'CodexPlugin/skills/blueprint-helper/references/08_User_Preferences.md',
    'ClaudePlugin/skills/blueprint-helper/references/08_User_Preferences.md',
    'CodexPlugin/skills/blueprint-helper/references/09_SideAgent_Tool_Execution.md',
    'ClaudePlugin/skills/blueprint-helper/references/09_SideAgent_Tool_Execution.md',
    'AgentFaceService/agent-guide/Workflows/04_TaskSpec_Edit_Blueprint_Workflow.md',
    'AgentFaceService/agent-guide/Workflows/05_Edit_Blueprint_Workflow.md',
  ];
  const forbidden = [
    /bh blueprinthelper_preview_task/,
    /bh blueprinthelper_execute_task/,
    /bh blueprinthelper_get_task_result/,
    /bh blueprinthelper_read_context(?!_capabilities)/,
    /validation\.should_compile/,
    /validation\.should_save/,
    /execution_policy\.should_compile/,
    /execution_policy\.should_save/,
    /scope_policy/,
    /allow_modify_user_nodes/,
    /direct-tool wrapper/i,
    /`task_spec`\s+wrapper/i,
    /task_spec wrapper/i,
    /wrapper envelope/i,
    /wrapped payload envelopes/i,
    /extra\s+`args`\s+envelope/i,
  ];

  for (const relativePath of docs) {
    const text = fs.readFileSync(path.join(PLUGIN_ROOT, relativePath), 'utf8');
    for (const pattern of forbidden) {
      assert.doesNotMatch(text, pattern, `${relativePath} still exposes ${pattern}`);
    }
  }
});

test('MCP fallback source does not advertise retired Agent-facing TaskSpec or ReadContext surfaces', () => {
  const text = fs.readFileSync(path.join(PLUGIN_ROOT, 'AgentFaceService/mcp/src/mcp/tools/register-tools.ts'), 'utf8');
  assert.doesNotMatch(text, /Normal Agents should prefer blueprinthelper_read_context/);
  assert.doesNotMatch(text, /blueprinthelper_preview_task,\s*and blueprinthelper_execute_task/);
  assert.doesNotMatch(text, /'graph_context'/);
  assert.doesNotMatch(text, /format:\s*z\.enum\(\[[^\]]*'summary'/);
  assert.doesNotMatch(text, /formats:\s*\[[^\]]*'summary'/);
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
