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

test('Agent-facing docs use explicit composer commands instead of slash-joined pseudo commands', () => {
  const docs = [
    'CodexPlugin/skills/blueprint-helper/references/00_Agent_Onboarding_Index_20260504.md',
    'ClaudePlugin/skills/blueprint-helper/references/00_Agent_Onboarding_Index_20260504.md',
    'CodexPlugin/skills/blueprint-helper/references/09_SideAgent_Tool_Execution.md',
    'ClaudePlugin/skills/blueprint-helper/references/09_SideAgent_Tool_Execution.md',
    'CodexPlugin/skills/blueprint-helper/SKILL.md',
    'ClaudePlugin/skills/blueprint-helper/SKILL.md',
  ];
  const forbidden = [
    new RegExp(['bh tools read-templates', 'domains'].join(' ')),
    new RegExp(['bh tools read-templates', 'targets'].join(' ')),
    new RegExp(['bh tools read-templates', 'views'].join(' ')),
    new RegExp(['bh tools read-templates', 'quick-access'].join(' ')),
    new RegExp(['bh tools read-templates compose', '--domain'].join(' ')),
    /bh tools domains\/list\/templates/,
    /bh tools templates quick-access\/compose/,
  ];

  for (const relativePath of docs) {
    const text = fs.readFileSync(path.join(PLUGIN_ROOT, relativePath), 'utf8');
    for (const pattern of forbidden) {
      assert.doesNotMatch(text, pattern, `${relativePath} still uses slash-joined composer shorthand`);
    }
  }
});

test('Agent-facing lifecycle guidance does not mention retired lifecycle_mcp_required code', () => {
  const docs = [
    'CodexPlugin/skills/blueprint-helper/references/00_Agent_Onboarding_Index_20260504.md',
    'ClaudePlugin/skills/blueprint-helper/references/00_Agent_Onboarding_Index_20260504.md',
    'CodexPlugin/skills/blueprint-helper/references/09_SideAgent_Tool_Execution.md',
    'ClaudePlugin/skills/blueprint-helper/references/09_SideAgent_Tool_Execution.md',
    'CodexPlugin/skills/blueprint-helper/SKILL.md',
    'ClaudePlugin/skills/blueprint-helper/SKILL.md',
    'AgentFaceService/docs/TaskSpec_CLI_QuickStart.md',
    'AgentFaceService/docs/Install_CLI_QuickStart.md',
  ];

  for (const relativePath of docs) {
    const text = fs.readFileSync(path.join(PLUGIN_ROOT, relativePath), 'utf8');
    assert.doesNotMatch(text, /lifecycle_mcp_required/, `${relativePath} still mentions lifecycle_mcp_required`);
  }
});

test('Agent-facing write template tree does not keep orphaned bare preview or execute samples', () => {
  const removedSamples = [
    'AgentFaceService/agent-guide/Templates/write/task_preview_bare_taskspec_template.json',
    'AgentFaceService/agent-guide/Templates/write/task_execute_bare_taskspec_template.json',
  ];

  for (const relativePath of removedSamples) {
    assert.equal(
      fs.existsSync(path.join(PLUGIN_ROOT, relativePath)),
      false,
      `${relativePath} should be removed; TaskSpec files are produced through bh tools templates compose`,
    );
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

test('TaskSpec workflow docs require compose-first scaffold edits and explicit fallback records', () => {
  const docs = [
    'AgentFaceService/agent-guide/Workflows/04_TaskSpec_Edit_Blueprint_Workflow.md',
    'AgentFaceService/agent-guide/Workflows/07_Safety_Validation_And_Recovery.md',
    'CodexPlugin/skills/blueprint-helper/references/04_TaskSpec_Edit_Blueprint_Workflow.md',
    'CodexPlugin/skills/blueprint-helper/references/07_Safety_Validation_And_Recovery.md',
    'ClaudePlugin/skills/blueprint-helper/references/04_TaskSpec_Edit_Blueprint_Workflow.md',
    'ClaudePlugin/skills/blueprint-helper/references/07_Safety_Validation_And_Recovery.md',
  ];

  for (const relativePath of docs) {
    const text = fs.readFileSync(path.join(PLUGIN_ROOT, relativePath), 'utf8');
    assert.match(text, /TaskSpec structure must be composed before placeholder edits/u, relativePath);
    assert.match(text, /Only replace `__REQUIRED_\*__` placeholders/u, relativePath);
    assert.match(text, /Full handwritten TaskSpec JSON is a fallback only when discovery or compose fails/u, relativePath);
    assert.match(text, /ReadSpec JSON may remain handwritten when the stable ReadSpec schema is enough/u, relativePath);
    assert.match(text, /source-control status and checkout payloads are direct editor tool payloads/u, relativePath);
  }
});

test('TaskSpec workflow docs include high-frequency composer recipes', () => {
  const docs = [
    'AgentFaceService/agent-guide/Workflows/04_TaskSpec_Edit_Blueprint_Workflow.md',
    'CodexPlugin/skills/blueprint-helper/references/04_TaskSpec_Edit_Blueprint_Workflow.md',
    'ClaudePlugin/skills/blueprint-helper/references/04_TaskSpec_Edit_Blueprint_Workflow.md',
  ];
  const requiredTokens = [
    'blueprint_variables.variables.ensure_member_variable',
    'blueprint_variables.variables.configure_member_variable',
    'external_body.replace_body.body',
    'generic_ops.set_variable.default',
    'generic_ops.branch.default',
    'blueprint_signature.signature.remove_signature',
    'blueprint_signature.signature.ensure_override_event',
    'material_graph.material_graph.append_block',
    'material_graph.material_graph.replace_block',
  ];

  for (const relativePath of docs) {
    const text = readText(path.join(PLUGIN_ROOT, relativePath));
    for (const token of requiredTokens) {
      assert.match(text, new RegExp(token.replaceAll('.', '\\.'), 'u'), `${relativePath} missing ${token}`);
    }
  }
});

test('SideAgent guidance requires template indexes before capability-missing reports', () => {
  const docs = [
    'CodexPlugin/agents/blueprint-explorer.md',
    'CodexPlugin/skills/blueprint-helper-blueprint-explorer/SKILL.md',
    'CodexPlugin/agents/task-worker.md',
    'CodexPlugin/skills/blueprint-helper-task-worker/SKILL.md',
  ];
  const requiredTokens = [
    'bh tools read-templates families --format json',
    'bh tools read-templates clusters --family <family> --format json',
    'bh tools read-templates list --family <family> --cluster <cluster> --format json',
    'output.format',
    'view.format',
    'bridge_unavailable',
    'route_missing',
    'graph_body_target_unresolved',
    'adapter_boundary.body_entry',
    'body_fingerprint',
  ];

  for (const relativePath of docs) {
    const text = readText(path.join(PLUGIN_ROOT, relativePath));
    for (const token of requiredTokens) {
      assert.match(text, new RegExp(escapeRegExp(token), 'u'), `${relativePath} missing ${token}`);
    }
    assert.match(text, /read_capability_missing|capability_missing/u, `${relativePath} must report capability-missing only after indexed discovery`);
  }
});

test('GraphWrite workflow docs distinguish owned function bodies from external body replacement', () => {
  const docs = [
    'AgentFaceService/agent-guide/Workflows/04_TaskSpec_Edit_Blueprint_Workflow.md',
    'CodexPlugin/skills/blueprint-helper/references/04_TaskSpec_Edit_Blueprint_Workflow.md',
    'ClaudePlugin/skills/blueprint-helper/references/04_TaskSpec_Edit_Blueprint_Workflow.md',
  ];

  for (const relativePath of docs) {
    const text = readText(path.join(PLUGIN_ROOT, relativePath));
    assert.match(text, /function_body.*replace_owned_graph.*BlueprintHelper-owned|BlueprintHelper-owned.*replace_owned_graph.*function_body/us, relativePath);
    assert.match(text, /external_body.*adapter_boundary\.body_entry.*body_fingerprint|adapter_boundary\.body_entry.*body_fingerprint.*external_body/us, relativePath);
    assert.doesNotMatch(text, /Non-BlueprintHelper-owned graph content is read-only in the normal GraphWrite flow/u, relativePath);
  }
});

function escapeRegExp(value: string): string {
  return value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}
