import { strict as assert } from 'node:assert';
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
import test from 'node:test';
import { readCliInputObject } from '../../cli/input.js';

test('reads a root-object params file', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'bph-cli-input-'));
  const file = path.join(dir, 'params.json');
  fs.writeFileSync(file, JSON.stringify({ path: '/Game', recursive: true }));

  const input = await readCliInputObject({ cwd: dir, file: 'params.json' });

  assert.deepEqual(input, { path: '/Game', recursive: true });
});

test('reads root-object JSON params', async () => {
  const input = await readCliInputObject({ cwd: process.cwd(), json: '{ "task_run_id": "task_001" }' });

  assert.deepEqual(input, { task_run_id: 'task_001' });
});

test('reads root-object params from stdin provider', async () => {
  const input = await readCliInputObject({
    cwd: process.cwd(),
    stdin: true,
    readStdin: async () => '{ "debug_case_id": "dbg_001" }',
  });

  assert.deepEqual(input, { debug_case_id: 'dbg_001' });
});

test('rejects multiple params input sources', async () => {
  await assert.rejects(
    () => readCliInputObject({ cwd: process.cwd(), file: 'a.json', json: '{}' }),
    /Choose exactly one params input source/,
  );
});

test('rejects non-object params', async () => {
  await assert.rejects(
    () => readCliInputObject({ cwd: process.cwd(), json: '[]' }),
    /CLI params must be a JSON object/,
  );
});
