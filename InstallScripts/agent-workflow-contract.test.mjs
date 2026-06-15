import assert from 'node:assert/strict';
import { mkdtemp, readFile, rm } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { installProjectWorkflow } from './agent-workflow-install.mjs';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(scriptDir, '..');

async function readRepoText(relativePath) {
  return readFile(path.join(repoRoot, relativePath), 'utf8');
}

async function loadGeneratedWorkflow() {
  const dir = await mkdtemp(path.join(os.tmpdir(), 'bh-agent-workflow-contract-'));
  try {
    await installProjectWorkflow({
      projectDir: dir,
      engineRoot: 'E:/UE_5.6',
      ueVersion: '5.6',
    });
    return await readFile(path.join(dir, '.blueprinthelper', 'AgentWorkFlow.md'), 'utf8');
  } finally {
    await rm(dir, { recursive: true, force: true });
  }
}

test('generated AgentWorkFlow stays a MainAgent bootstrap with lifecycle, CLI-first, evidence conflict, and dispatch boundaries', async () => {
  const workflow = await loadGeneratedWorkflow();

  assert.match(workflow, /Editor lifecycle[\s\S]{0,120}global MCP/i);
  assert.match(workflow, /BlueprintHelper CLI[\s\S]{0,120}ordinary UE editor asset reads and writes/i);
  assert.match(workflow, /evidence_conflict/);
  assert.match(workflow, /blueprint-explorer[\s\S]{0,160}UE editor-asset evidence/i);
  assert.match(workflow, /sourcecode-explorer[\s\S]{0,160}source-side grounding/i);
  assert.match(workflow, /source-control[\s\S]{0,80}write-session[\s\S]{0,160}task-worker/i);
  assert.match(workflow, /task-worker[\s\S]{0,160}target asset[\s\S]{0,160}evidence/i);
});

test('generated AgentWorkFlow removes legacy composer, quick-access, hook-ledger, and plugin-dev rule details', async () => {
  const workflow = await loadGeneratedWorkflow();

  assert.doesNotMatch(workflow, /TaskSpec Template Composer/);
  assert.doesNotMatch(workflow, /quick-access\.items/);
  assert.doesNotMatch(workflow, /bh tools templates compose/);
  assert.doesNotMatch(
    workflow,
    /choose the TaskSpec family, write mode, cluster, and operation through CLI tool\/template discovery/i,
  );
  assert.doesNotMatch(workflow, /bh tools templates operations --family/);
  assert.doesNotMatch(workflow, /hook ledger/i);
  assert.doesNotMatch(
    workflow,
    /BlueprintHelper_CurrentPluginArchitecture_Rules_20260606_CN\.md/,
  );
});

test('MainAgent skills stop selecting exact template or composer chains for ordinary writes', async (t) => {
  for (const filePath of [
    'CodexPlugin/skills/blueprint-helper/SKILL.md',
    'ClaudePlugin/skills/blueprint-helper/SKILL.md',
  ]) {
    await t.test(filePath, async () => {
      const skillText = await readRepoText(filePath);
      assert.doesNotMatch(skillText, /four-layer TaskSpec composer/i);
      assert.doesNotMatch(skillText, /clusters -> operations -> quick-access -> compose/i);
      assert.doesNotMatch(skillText, /bh tools templates write-modes --family/);
      assert.doesNotMatch(skillText, /bh tools templates quick-access --family/);
      assert.doesNotMatch(skillText, /tool_id:\s*"<selected tool_id from bh tools list>"/);
      assert.doesNotMatch(skillText, /quick-access item|quick_access item|quick-access choice/i);
      assert.doesNotMatch(skillText, /exact template[_ -]?id|selected template[_ -]?id/i);
      assert.doesNotMatch(skillText, /BlueprintHelper\.BlueprintExplorerPackage\.v1/);
      assert.doesNotMatch(skillText, /BlueprintHelper\.TaskWorkerPackage\.v1/);
      assert.doesNotMatch(skillText, /exploration_package_id|task_package_id|evidence_scope|capability_scope|modification_scope/);
      assert.doesNotMatch(skillText, /prewrite_gates|retry_budget|readback_required/);
      assert.doesNotMatch(skillText, /TaskSpec\/ReadSpec|ReadSpec construction|CLI catalog\/composer discovery/i);
      assert.doesNotMatch(skillText, /BlueprintHelper_CurrentPluginArchitecture_Rules_20260606_CN\.md/);
    });
  }
});

test('TaskWorker skill removes MainAgent-provided source context and template discovery fields', async (t) => {
  for (const filePath of [
    'CodexPlugin/skills/blueprint-helper-task-worker/SKILL.md',
    'CodexPlugin/agents/task-worker.md',
    'CodexPlugin/agents/task-worker.toml',
  ]) {
    await t.test(filePath, async () => {
      const taskWorkerText = await readRepoText(filePath);
      assert.doesNotMatch(taskWorkerText, /source_context_summary/);
      assert.doesNotMatch(taskWorkerText, /cluster_scope/);
      assert.doesNotMatch(taskWorkerText, /template_discovery/);
      assert.doesNotMatch(taskWorkerText, /allowed_cli/);
    });
  }
});

test('BlueprintExplorer skill requires package schema and compact evidence summary instead of raw CLI output', async (t) => {
  for (const filePath of [
    'CodexPlugin/skills/blueprint-helper-blueprint-explorer/SKILL.md',
    'CodexPlugin/agents/blueprint-explorer.md',
    'CodexPlugin/agents/blueprint-explorer.toml',
  ]) {
    await t.test(filePath, async () => {
      const explorerText = await readRepoText(filePath);
      assert.match(explorerText, /BlueprintHelper\.BlueprintExplorerPackage\.v1/);
      assert.match(explorerText, /exploration_package_id/);
      assert.match(explorerText, /target_hint/);
      assert.match(explorerText, /evidence_scope/);
      assert.match(explorerText, /requested_facts/);
      assert.match(explorerText, /family_hint/);
      for (const fact of ['asset_candidates', 'graph_or_scope', 'anchors', 'nodes', 'pins', 'links', 'variables', 'components', 'widget_tree', 'table_rows', 'adapter_boundary']) {
        assert.match(explorerText, new RegExp(fact));
      }
      assert.match(explorerText, /compact .*evidence summary/i);
      assert.match(explorerText, /not raw CLI output|instead of raw CLI output/i);
    });
  }
});

test('SourceExplorer skill keeps broad source evidence scope and does not feed source_context_summary to TaskWorker', async (t) => {
  for (const filePath of [
    'CodexPlugin/skills/blueprint-helper-sourcecode-explorer/SKILL.md',
    'CodexPlugin/agents/sourcecode-explorer.md',
    'CodexPlugin/agents/sourcecode-explorer.toml',
  ]) {
    await t.test(filePath, async () => {
      const sourceExplorerText = await readRepoText(filePath);
      assert.match(sourceExplorerText, /C\+\+/);
      assert.match(sourceExplorerText, /TypeScript/);
      assert.match(sourceExplorerText, /CLI/);
      assert.match(sourceExplorerText, /schema/);
      assert.match(sourceExplorerText, /config/);
      assert.match(sourceExplorerText, /test/);
      assert.match(sourceExplorerText, /template/);
      assert.match(sourceExplorerText, /runtime|adapter|coordinator|service|Review|TaskRuntime/);
      assert.match(sourceExplorerText, /task-core|result-shape|result shape/i);
      assert.doesNotMatch(sourceExplorerText, /source_context_summary/);
    });
  }
});

test('Codex hook packaging references the shared workflow hook wrapper', async () => {
  const hooks = JSON.parse(await readRepoText('CodexPlugin/hooks.json'));
  const commands = JSON.stringify(hooks);

  assert.match(commands, /node \.\/scripts\/global-mcp-lifecycle-notice\.cjs/);
  assert.match(commands, /node \.\/scripts\/workflow-hook\.cjs --event PreToolUse/);
  assert.match(commands, /node \.\/scripts\/workflow-hook\.cjs --event PostToolUse/);
  assert.match(commands, /node \.\/scripts\/workflow-hook\.cjs --event Stop/);
  assert.match(commands, /node \.\/scripts\/workflow-hook\.cjs --event SubagentStop/);
});
