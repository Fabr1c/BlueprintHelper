import { strict as assert } from 'node:assert';
import test from 'node:test';
import { runCli } from '../../cli/run.js';
import type { BridgeResponse } from '@blueprinthelper/task-core/bridge/bridge-client';

test('direct blueprint_get_runtime_profile calls matching Bridge command', async () => {
  const writes: string[] = [];
  const calls: Array<{ command: string; payload?: Record<string, unknown> }> = [];
  const bridge = {
    sendCommand: async (command: string, payload?: Record<string, unknown>): Promise<BridgeResponse> => {
      calls.push({ command, payload });
      return {
        request_id: 'runtime_profile',
        success: true,
        result: {
          ok: true,
          schema: 'BlueprintHelper.ToolResult.v1',
          operation: 'get_runtime_profile',
          status: 'completed',
          modified: false,
          data: { version: '0.4.1' },
        },
      };
    },
  };

  const exitCode = await runCli({
    argv: ['blueprint_get_runtime_profile', '--json', '{}', '--select', 'status,artifacts.full_result'],
    cwd: process.cwd(),
    bridge,
    runner: {} as never,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  assert.deepEqual(calls, [{ command: 'get_runtime_profile', payload: {} }]);
  assert.equal(JSON.parse(writes.join('')).status, 'completed');
});

test('frozen direct Bridge tools are not exposed through CLI tool invocation', async () => {
  const writes: string[] = [];
  const errors: string[] = [];
  const exitCode = await runCli({
    argv: ['blueprint_exec_console_command', '--json', '{ "command": "stat fps" }', '--expert'],
    cwd: process.cwd(),
    bridge: {
      sendCommand: async () => { throw new Error('must not call bridge'); },
    },
    runner: {} as never,
    stdout: (line) => writes.push(line),
    stderr: (line) => errors.push(line),
  });

  assert.equal(exitCode, 64);
  assert.equal(writes.join(''), '');
  assert.match(errors.join(''), /Unsupported BlueprintHelper CLI command/);
});

