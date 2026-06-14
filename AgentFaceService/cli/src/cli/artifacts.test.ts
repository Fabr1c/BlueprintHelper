import assert from 'node:assert/strict';
import { mkdir, mkdtemp, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import { resolveArtifactRoot } from './artifacts.js';

test('resolveArtifactRoot accepts a BOM-prefixed project setting file', async () => {
  const workspace = await makeTempWorkspace('bph-artifact-setting-bom-');
  try {
    const settingDir = path.join(workspace, '.blueprinthelper');
    await mkdir(settingDir, { recursive: true });
    await writeFile(
      path.join(settingDir, 'setting.json'),
      `\uFEFF${JSON.stringify({
        cli: {
          artifacts: {
            default_output_dir: 'Saved/CustomArtifacts',
          },
        },
      })}`,
      'utf8',
    );

    assert.equal(resolveArtifactRoot({ cwd: workspace }), path.join(workspace, 'Saved', 'CustomArtifacts'));
  } finally {
    await rm(workspace, { recursive: true, force: true });
  }
});

async function makeTempWorkspace(prefix: string): Promise<string> {
  await mkdir(os.tmpdir(), { recursive: true });
  return await mkdtemp(path.join(os.tmpdir(), prefix));
}
