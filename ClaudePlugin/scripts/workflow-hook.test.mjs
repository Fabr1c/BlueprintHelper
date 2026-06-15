import assert from 'node:assert/strict';
import { existsSync } from 'node:fs';
import { createRequire } from 'node:module';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const wrapperPath = path.join(scriptDir, 'workflow-hook.cjs');
const require = createRequire(import.meta.url);

test('Claude workflow hook wrapper resolves the shared adapter from the plugin directory', () => {
  const wrapper = require(wrapperPath);
  assert.equal(
    wrapper.adapterPath,
    path.resolve(scriptDir, '..', '..', 'AgentFaceService', 'hooks', 'claude-hook-adapter.cjs'),
  );
  assert.ok(existsSync(wrapper.adapterPath));
});
