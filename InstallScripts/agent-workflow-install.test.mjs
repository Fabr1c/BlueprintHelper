import assert from 'node:assert/strict';
import { mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import {
  installProjectWorkflow,
  removeProjectWorkflow,
  upsertManagedSection,
} from './agent-workflow-install.mjs';

test('upsertManagedSection inserts at file head and preserves user content', () => {
  const result = upsertManagedSection('User rules\n', 'BLUEPRINTHELPER CODEX', 'Managed body');
  assert.match(result, /^<!-- BEGIN BLUEPRINTHELPER CODEX -->/);
  assert.match(result, /Managed body/);
  assert.match(result, /User rules/);
});

test('installProjectWorkflow creates profile, workflow markdown, AGENTS and CLAUDE markers', async () => {
  const dir = await mkdtemp(path.join(os.tmpdir(), 'bh-workflow-'));
  try {
    await writeFile(path.join(dir, 'AGENTS.md'), 'User agent rules\n', 'utf8');
    await installProjectWorkflow({
      projectDir: dir,
      engineRoot: 'E:/UE_5.6',
      ueVersion: '5.6',
    });

    const profile = JSON.parse(await readFile(path.join(dir, '.blueprinthelper', 'project-profile.json'), 'utf8'));
    assert.equal(profile.schema, 'BlueprintHelper.ProjectProfile.v1');
    assert.equal(profile.environment.ue_engine_dir, 'E:/UE_5.6');
    assert.equal(profile.environment.ue_version, '5.6');
    assert.equal(profile.workflow_docs.agent_workflow, '.blueprinthelper/AgentWorkFlow.md');

    const workflow = await readFile(path.join(dir, '.blueprinthelper', 'AgentWorkFlow.md'), 'utf8');
    assert.match(workflow, /Editor lifecycle/);
    assert.match(workflow, /Get-Command bh\.cmd/);
    assert.match(workflow, /npm global prefix/);
    assert.match(workflow, /pre-dispatch editor\/Bridge gate/);
    assert.match(workflow, /blueprint_get_runtime_profile/);
    assert.match(workflow, /may open the target Editor once through `mcp__blueprint_helper__blueprint_open_editor`/);
    assert.match(workflow, /Bridge unavailable/);
    assert.match(workflow, /After writing, restoring, or otherwise modifying any file, artifact, or documentation file/);
    assert.match(workflow, /previous reads of that path are stale/);
    assert.match(workflow, /`bh context read --stdin` is supported for generated ReadSpec JSON/);
    assert.match(workflow, /`property_json` `target_name` is a ReadContext route locator\/filter/);
    assert.match(workflow, /TaskSpec Template Composer/);
    assert.match(workflow, /choose the TaskSpec family, write mode, cluster, and operation/);
    assert.match(workflow, /`bh tools templates families --workflow preview_execute --format json`/);
    assert.match(workflow, /`quick-access\.items\[\]\.slot_type`/);
    assert.match(workflow, /`quick-access\.items\[\]\.arg_slots`/);
    assert.match(workflow, /compose with `bh tools templates compose`/);
    assert.match(workflow, /Do not use old tool-id template dispatch or scan `AgentFaceService\/agent-guide\/Templates`/);
    assert.match(workflow, /fill the generated TaskSpec with concrete evidence and intent/);
    assert.match(workflow, /Do not guess fixed enum-like payload fields/);
    assert.match(workflow, /template `\*\.allowed_values`/);
    assert.match(workflow, /evidence_conflict/);
    assert.match(workflow, /binary asset files as fallback evidence/);
    assert.match(workflow, /`\.\\bh\.cmd`/);
    assert.doesNotMatch(workflow, /\u0008/);
    assert.match(await readFile(path.join(dir, 'AGENTS.md'), 'utf8'), /BEGIN BLUEPRINTHELPER CODEX/);
    assert.match(await readFile(path.join(dir, 'AGENTS.md'), 'utf8'), /User agent rules/);
    assert.match(await readFile(path.join(dir, 'CLAUDE.md'), 'utf8'), /BEGIN BLUEPRINTHELPER CLAUDE/);
  } finally {
    await rm(dir, { recursive: true, force: true });
  }
});

test('installProjectWorkflow preserves unknown project profile fields', async () => {
  const dir = await mkdtemp(path.join(os.tmpdir(), 'bh-workflow-preserve-'));
  try {
    const profileDir = path.join(dir, '.blueprinthelper');
    await mkdir(profileDir, { recursive: true });
    await writeFile(
      path.join(profileDir, 'project-profile.json'),
      JSON.stringify(
        {
          custom_root: 'keep',
          environment: {
            custom_environment: 'keep',
          },
          workflow_docs: {
            custom_workflow: 'keep',
          },
        },
        null,
        2,
      ),
      'utf8',
    );

    await installProjectWorkflow({
      projectDir: dir,
      engineRoot: 'E:/UE_5.6',
      ueVersion: '5.6',
    });

    const profile = JSON.parse(await readFile(path.join(profileDir, 'project-profile.json'), 'utf8'));
    assert.equal(profile.custom_root, 'keep');
    assert.equal(profile.environment.custom_environment, 'keep');
    assert.equal(profile.workflow_docs.custom_workflow, 'keep');
    assert.equal(profile.environment.ue_engine_dir, 'E:/UE_5.6');
    assert.equal(profile.workflow_docs.agent_workflow, '.blueprinthelper/AgentWorkFlow.md');
  } finally {
    await rm(dir, { recursive: true, force: true });
  }
});

test('removeProjectWorkflow removes managed files and marker sections only', async () => {
  const dir = await mkdtemp(path.join(os.tmpdir(), 'bh-workflow-remove-'));
  try {
    await installProjectWorkflow({ projectDir: dir, engineRoot: 'E:/UE_5.6', ueVersion: '5.6' });
    const existing = await readFile(path.join(dir, 'AGENTS.md'), 'utf8');
    await writeFile(path.join(dir, 'AGENTS.md'), `${existing}\nUser tail\n`, 'utf8');

    await removeProjectWorkflow({ projectDir: dir, removeLegacyAgentProfile: true });

    assert.equal(await readFile(path.join(dir, 'AGENTS.md'), 'utf8'), 'User tail\n');
  } finally {
    await rm(dir, { recursive: true, force: true });
  }
});
