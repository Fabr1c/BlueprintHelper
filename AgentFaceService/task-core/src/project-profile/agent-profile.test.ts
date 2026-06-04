import assert from 'node:assert/strict';
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import { resolveProjectEngineDir } from './agent-profile.js';

function createProjectDir(): string {
  const dir = mkdtempSync(path.join(os.tmpdir(), 'bh-profile-'));
  writeFileSync(path.join(dir, 'Demo.uproject'), '{}', 'utf8');
  mkdirSync(path.join(dir, '.blueprinthelper'), { recursive: true });
  return dir;
}

function parseError(error: unknown): Record<string, unknown> {
  assert.ok(error instanceof Error);
  return JSON.parse(error.message) as Record<string, unknown>;
}

function captureError(fn: () => unknown): unknown {
  try {
    fn();
  } catch (error) {
    return error;
  }
  assert.fail('Expected function to throw');
}

test('resolveProjectEngineDir reads project-profile.json first', () => {
  const dir = createProjectDir();
  try {
    writeFileSync(
      path.join(dir, '.blueprinthelper', 'project-profile.json'),
      JSON.stringify({ environment: { ue_engine_dir: 'E:/UE_5.6' } }),
      'utf8',
    );
    writeFileSync(
      path.join(dir, '.blueprinthelper', 'agent-profile.json'),
      JSON.stringify({ environment: { ue_engine_dir: 'E:/UE_5.5' } }),
      'utf8',
    );

    assert.equal(resolveProjectEngineDir(path.join(dir, 'Demo.uproject'), {}), path.normalize('E:/UE_5.6'));
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test('resolveProjectEngineDir falls back to legacy agent-profile.json', () => {
  const dir = createProjectDir();
  try {
    writeFileSync(
      path.join(dir, '.blueprinthelper', 'agent-profile.json'),
      JSON.stringify({ environment: { ue_engine_dir: 'E:/UE_5.6' } }),
      'utf8',
    );

    assert.equal(resolveProjectEngineDir(path.join(dir, 'Demo.uproject'), {}), path.normalize('E:/UE_5.6'));
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test('resolveProjectEngineDir reports project-profile.json when both profiles are missing', () => {
  const dir = createProjectDir();
  try {
    const error = captureError(() => resolveProjectEngineDir(path.join(dir, 'Demo.uproject'), {}));
    const payload = parseError(error);
    assert.equal(payload.code, 'PROJECT_PROFILE_ENGINE_DIR_MISSING');
    assert.equal(payload.project_profile_path, path.join(dir, '.blueprinthelper', 'project-profile.json'));
    assert.equal(payload.legacy_agent_profile_path, path.join(dir, '.blueprinthelper', 'agent-profile.json'));
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test('resolveProjectEngineDir reports invalid JSON path for project-profile.json', () => {
  const dir = createProjectDir();
  try {
    writeFileSync(path.join(dir, '.blueprinthelper', 'project-profile.json'), '{', 'utf8');
    const error = captureError(() => resolveProjectEngineDir(path.join(dir, 'Demo.uproject'), {}));
    const payload = parseError(error);
    assert.equal(payload.code, 'PROJECT_PROFILE_INVALID_JSON');
    assert.equal(payload.profile_path, path.join(dir, '.blueprinthelper', 'project-profile.json'));
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});
