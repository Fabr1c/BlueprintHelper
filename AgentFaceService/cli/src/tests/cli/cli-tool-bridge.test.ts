import { strict as assert } from 'node:assert';
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
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
          data: { version: '0.4.4' },
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

test('direct read context capabilities stays local and returns compact matrix artifact', async () => {
  const writes: string[] = [];
  const artifactDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-cli-read-capabilities-'));
  const exitCode = await runCli({
    argv: [
      'blueprinthelper_read_context_capabilities',
      '--json',
      '{}',
      '--artifact-dir',
      artifactDir,
      '--select',
      'status,artifacts.full_result',
    ],
    cwd: process.cwd(),
    bridge: {
      sendCommand: async () => { throw new Error('read context capabilities must not call Bridge'); },
    },
    runner: {} as never,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  assert.equal(output.status, 'completed');
  const fullResultPath = String((output.artifacts as Record<string, unknown>).full_result);
  const fullResult = JSON.parse(fs.readFileSync(fullResultPath, 'utf8')) as Record<string, unknown>;
  const toolResult = fullResult.toolResult as Record<string, unknown>;
  const data = toolResult.data as Record<string, unknown>;
  assert.equal(data.schema, 'ReadContextCapabilities.v1');
  assert.deepEqual(data.formats, ['logic_json', 'logic_md']);
  assert.equal(Array.isArray(data.read_types), true);
  assert.equal(JSON.stringify(data).includes('bridge_command'), false);
});

test('direct function chain context read calls matching Bridge command', async () => {
  const writes: string[] = [];
  const calls: Array<{ command: string; payload?: Record<string, unknown> }> = [];
  const payload = {
    asset_path: '/Game/BP_PlayerController',
    target_type: 'custom_event',
    target_name: 'Input_Fire',
    graph_name: 'EventGraph',
    max_depth: 3,
  };
  const bridge = {
    sendCommand: async (command: string, commandPayload?: Record<string, unknown>): Promise<BridgeResponse> => {
      calls.push({ command, payload: commandPayload });
      return {
        request_id: 'function_chain_context',
        success: true,
        result: {
          ok: true,
          schema: 'BlueprintHelper.ToolResult.v1',
          operation: 'read_function_chain_context',
          status: 'completed',
          modified: false,
          data: {
            schema: 'BlueprintHelper.FunctionChainContext.v1',
            custom_logic_refs: [],
            summary: { returned_custom_refs: 0 },
            unresolved: [],
            ambiguous: [],
          },
        },
      };
    },
  };

  const exitCode = await runCli({
    argv: [
      'blueprinthelper_read_function_chain_context',
      '--json',
      JSON.stringify(payload),
      '--select',
      'status,artifacts.full_result',
    ],
    cwd: process.cwd(),
    bridge,
    runner: {} as never,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  assert.deepEqual(calls, [{
    command: 'read_function_chain_context',
    payload: {
      ...payload,
      include_data_dependencies: true,
      expand_cross_asset: true,
    },
  }]);
  const output = JSON.parse(writes.join(''));
  assert.equal(output.status, 'completed');
  const fullResultPath = String(output.artifacts.full_result);
  const fullResult = JSON.parse(fs.readFileSync(fullResultPath, 'utf8'));
  assert.equal(fullResult.toolResult.data.schema, 'FunctionChainContext.v1');
});

test('delayed Bridge calls emit Agent wait hints to stderr without contaminating stdout JSON', async () => {
  const writes: string[] = [];
  const errors: string[] = [];
  const previousInitial = process.env['BPH_CLI_WAIT_HINT_INITIAL_MS'];
  const previousInterval = process.env['BPH_CLI_WAIT_HINT_INTERVAL_MS'];
  process.env['BPH_CLI_WAIT_HINT_INITIAL_MS'] = '1';
  process.env['BPH_CLI_WAIT_HINT_INTERVAL_MS'] = '50';
  try {
    const bridge = {
      sendCommand: async (command: string, payload?: Record<string, unknown>): Promise<BridgeResponse> => {
        assert.equal(command, 'get_runtime_profile');
        assert.deepEqual(payload, {});
        await new Promise((resolve) => setTimeout(resolve, 25));
        return {
          request_id: 'runtime_profile',
          success: true,
          result: {
            ok: true,
            schema: 'BlueprintHelper.ToolResult.v1',
            operation: 'get_runtime_profile',
            status: 'completed',
            modified: false,
            data: { version: '0.4.4' },
          },
        };
      },
    };

    const exitCode = await runCli({
      argv: ['blueprint_get_runtime_profile', '--json', '{}', '--select', 'status'],
      cwd: process.cwd(),
      bridge,
      runner: {} as never,
      stdout: (line) => writes.push(line),
      stderr: (line) => errors.push(line),
    });

    assert.equal(exitCode, 0);
    assert.equal(JSON.parse(writes.join('')).status, 'completed');
    assert.doesNotMatch(writes.join(''), /waiting for UE Bridge response/);
    assert.match(errors.join(''), /waiting for UE Bridge response: command=get_runtime_profile/);
    assert.match(errors.join(''), /keep waiting unless the CLI exits/);
  } finally {
    restoreEnv('BPH_CLI_WAIT_HINT_INITIAL_MS', previousInitial);
    restoreEnv('BPH_CLI_WAIT_HINT_INTERVAL_MS', previousInterval);
  }
});

test('direct blueprint_close_editor calls matching Bridge command when expert flag is present', async () => {
  const writes: string[] = [];
  const calls: Array<{ command: string; payload?: Record<string, unknown> }> = [];
  const bridge = {
    sendCommand: async (command: string, payload?: Record<string, unknown>): Promise<BridgeResponse> => {
      calls.push({ command, payload });
      return {
        request_id: 'close_editor',
        success: true,
        result: {
          ok: true,
          schema: 'BlueprintHelper.ToolResult.v1',
          operation: 'close_editor',
          status: 'completed',
          modified: false,
          data: { schema: 'CloseEditor.v1', close_scheduled: true },
        },
      };
    },
  };

  const exitCode = await runCli({
    argv: ['blueprint_close_editor', '--json', '{ "save_all": false }', '--expert', '--select', 'status,artifacts.full_result'],
    cwd: process.cwd(),
    bridge,
    runner: {} as never,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  assert.deepEqual(calls, [{ command: 'close_editor', payload: { save_all: false } }]);
  assert.equal(JSON.parse(writes.join('')).status, 'completed');
});

test('short close_editor does not require expert flag', async () => {
  const writes: string[] = [];
  const calls: Array<{ command: string; payload?: Record<string, unknown> }> = [];
  const exitCode = await runCli({
    argv: ['close_editor', '--select', 'status,artifacts.full_result'],
    cwd: process.cwd(),
    bridge: {
      sendCommand: async (command: string, payload?: Record<string, unknown>): Promise<BridgeResponse> => {
        calls.push({ command, payload });
        return {
          request_id: 'close_editor',
          success: true,
          result: {
            ok: true,
            schema: 'BlueprintHelper.ToolResult.v1',
            operation: 'close_editor',
            status: 'completed',
            modified: false,
            data: { schema: 'CloseEditor.v1', close_scheduled: true },
          },
        };
      },
    },
    runner: {} as never,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  assert.deepEqual(calls, [{ command: 'close_editor', payload: { save_all: true } }]);
  const output = JSON.parse(writes.join('')) as Record<string, unknown>;
  assert.equal(output.status, 'completed');
});


test('short open_editor discovers uproject from cwd and uses robust editor args', async () => {
  const tmpRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-cli-open-editor-'));
  const projectDir = path.join(tmpRoot, 'Template');
  const childDir = path.join(projectDir, 'Plugins', 'BlueprintHelper');
  fs.mkdirSync(path.join(projectDir, '.blueprinthelper'), { recursive: true });
  fs.mkdirSync(childDir, { recursive: true });
  fs.writeFileSync(path.join(projectDir, 'Template.uproject'), '{}', 'utf8');
  fs.writeFileSync(path.join(projectDir, '.blueprinthelper', 'agent-profile.json'), JSON.stringify({
    environment: { ue_engine_dir: 'E:\\UE_5.6' },
  }), 'utf8');

  const writes: string[] = [];
  const launches: Array<{ command: string; args: string[] }> = [];
  let pingCount = 0;
  const exitCode = await runCli({
    argv: ['open_editor', '--select', 'status,artifacts.full_result'],
    cwd: childDir,
    bridge: {
      sendCommand: async () => { throw new Error('must not call sendCommand'); },
      ping: async () => {
        pingCount += 1;
        return pingCount >= 2;
      },
    } as never,
    runner: {} as never,
    runLocalProcess: async (command, args) => {
      launches.push({ command, args });
      return { exitCode: 0, stdout: '', stderr: '' };
    },
    sleep: async () => {},
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  assert.equal(launches.length, 1);
  assert.equal(path.basename(launches[0].command), 'UnrealEditor.exe');
  assert.equal(launches[0].args[0], path.join(projectDir, 'Template.uproject'));
  assert.ok(launches[0].args.includes('-DDC-ForceMemoryCache'));
  assert.ok(launches[0].args.some((arg) => arg.startsWith('-ShaderWorkingDir=')));
  assert.ok(launches[0].args.includes('-NoSplash'));
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

function restoreEnv(name: string, value: string | undefined): void {
  if (value === undefined) {
    delete process.env[name];
    return;
  }
  process.env[name] = value;
}

