import { strict as assert } from 'node:assert';
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
import test from 'node:test';
import { runCli } from '../../cli/run.js';
import type { BridgeResponse } from '@blueprinthelper/task-core/bridge/bridge-client';
import { getActiveReadContextRouteDescriptors } from '@blueprinthelper/task-core/tool-surface/templates/read-context-template-registry';

const legacyTemplateIndexName = 'SEMANTIC' + '_INDEX';

test('global help points Agents to CLI catalog and per-tool help', async () => {
  const writes: string[] = [];
  const exitCode = await runCli({
    argv: ['--help'],
    cwd: process.cwd(),
    bridge: {} as never,
    runner: {} as never,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  const output = writes.join('');
  assert.equal(exitCode, 0);
  assert.doesNotMatch(output, /bh <tool_name> --help/);
  assert.match(output, /bh tools domains --format json/);
  assert.match(output, /bh tools list <domain> <kind> --format json/);
  assert.match(output, /bh tools templates families --workflow preview_execute --format json/);
  assert.match(output, /bh tools templates compose --family <family>/);
  assert.match(output, /bh tools read-templates domains --format json/);
  assert.match(
    output,
    /Compose a temporary TaskSpec or ReadSpec, then run bh task preview, bh task execute, or bh context read/,
  );
  assert.doesNotMatch(output, new RegExp(['bh tools templates', '<tool_id>'].join(' ')));
  assert.doesNotMatch(output, new RegExp(legacyTemplateIndexName));
});

test('help command accepts text format alias for shell smoke checks', async () => {
  const writes: string[] = [];
  const exitCode = await runCli({
    argv: ['help', '--format', 'text'],
    cwd: process.cwd(),
    bridge: {} as never,
    runner: {} as never,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  const output = writes.join('');
  assert.equal(exitCode, 0);
  assert.match(output, /BlueprintHelper CLI/);
  assert.match(output, /bh tools domains --format json/);
});

test('tool help is specific for ReadContext without concrete template dispatch paths', async () => {
  const writes: string[] = [];
  const exitCode = await runCli({
    argv: ['context', 'read', '--help'],
    cwd: process.cwd(),
    bridge: {} as never,
    runner: {} as never,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  const output = writes.join('');
  assert.equal(exitCode, 0);
  assert.match(output, /BlueprintHelper CLI help: context read/);
  assert.match(output, /Root JSON: bare BlueprintHelper\.ReadSpec\.v1/);
  assert.match(output, /bh context read --file <read-spec\.json> --select status,artifacts\.full_result/);
  assert.doesNotMatch(output, /read\/routes\/blueprint_logic_function_logic_flow_template\.json/);
  assert.doesNotMatch(output, /read\/routes\/blueprint_logic_graph_logic_json_template\.json/);
  assert.doesNotMatch(output, /Templates\/read\/read_context_/);
  assert.doesNotMatch(output, new RegExp(String.raw`read/${legacyTemplateIndexName}\.md`));
  assert.doesNotMatch(output, /Default tool names:/);
});

test('removed direct preview help reports grouped command replacement', async () => {
  const writes: string[] = [];
  const exitCode = await runCli({
    argv: ['blueprinthelper_preview_task', '--help'],
    cwd: process.cwd(),
    bridge: {} as never,
    runner: {} as never,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  const output = writes.join('');
  assert.equal(exitCode, 0);
  assert.match(output, /BlueprintHelper CLI help: blueprinthelper_preview_task/);
  assert.match(output, /Direct tool-name CLI entry was removed/);
  assert.match(output, /bh task preview --file <task-spec\.json>/);
  assert.doesNotMatch(output, /bh tools templates compose --family <family>/);
  assert.doesNotMatch(output, /blueprinthelper_preview_task_wrapper_template\.json/);
  assert.doesNotMatch(output, /task_preview_bare_taskspec_template\.json/);
});

test('tool help is specific for find assets and points to the request template', async () => {
  const writes: string[] = [];
  const exitCode = await runCli({
    argv: ['blueprinthelper_find_assets', '--help'],
    cwd: process.cwd(),
    bridge: {} as never,
    runner: {} as never,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  const output = writes.join('');
  assert.equal(exitCode, 0);
  assert.match(output, /BlueprintHelper CLI help: blueprinthelper_find_assets/);
  assert.match(output, /bh blueprinthelper_find_assets --file <find-assets\.json> --select status,artifacts\.full_result/);
  assert.match(output, /Root JSON: Bridge tool payload object/);
  assert.doesNotMatch(output, /AgentFaceService\/agent-guide\/Templates\/blueprinthelper_find_assets_template\.json/);
  assert.doesNotMatch(output, /Default tool names:/);
});

test('tool help is specific for capture screenshot and points to the request template', async () => {
  const writes: string[] = [];
  const exitCode = await runCli({
    argv: ['blueprinthelper_capture_screenshot', '--help'],
    cwd: process.cwd(),
    bridge: {} as never,
    runner: {} as never,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  const output = writes.join('');
  assert.equal(exitCode, 0);
  assert.match(output, /BlueprintHelper CLI help: blueprinthelper_capture_screenshot/);
  assert.match(output, /graph_name is required when block_ref or node_ref is provided/);
  assert.match(output, /Root JSON: Bridge tool payload object/);
  assert.doesNotMatch(output, /AgentFaceService\/agent-guide\/Templates\/blueprinthelper_capture_screenshot_template\.json/);
  assert.doesNotMatch(output, /Default tool names:/);
});

test('direct find assets calls matching Bridge command and returns compact FindAssets payload', async () => {
  const writes: string[] = [];
  const calls: Array<{ command: string; payload?: Record<string, unknown> }> = [];
  const artifactDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-cli-find-assets-'));
  const payload = {
    schema: 'BlueprintHelper.FindAssetsRequest.v1',
    query: 'Player',
    path_prefixes: ['/Game'],
    asset_types: ['blueprint'],
    limit: 25,
  };
  const bridge = {
    sendCommand: async (command: string, commandPayload?: Record<string, unknown>): Promise<BridgeResponse> => {
      calls.push({ command, payload: commandPayload });
      return {
        request_id: 'find_assets',
        success: true,
        result: {
          schema: 'FindAssets.v1',
          assets: [
            {
              asset_path: '/Game/Blueprints/BP_Player.BP_Player',
              asset_name: 'BP_Player',
              asset_type: 'blueprint',
              asset_class: '/Script/Engine.Blueprint',
              package_path: '/Game/Blueprints',
            },
          ],
          page: {
            limit: 25,
            has_more: false,
          },
        },
      };
    },
  };

  const exitCode = await runCli({
    argv: [
      'blueprinthelper_find_assets',
      '--json',
      JSON.stringify(payload),
      '--format',
      'full',
      '--artifact-dir',
      artifactDir,
    ],
    cwd: process.cwd(),
    bridge,
    runner: {} as never,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  assert.deepEqual(calls, [{ command: 'find_assets', payload }]);
  const output = JSON.parse(writes.join(''));
  assert.equal(output.status, 'completed');
  assert.equal(output.tool_name, 'blueprinthelper_find_assets');
  assert.equal(output.tool_result.data.schema, 'FindAssets.v1');
  assert.equal(output.tool_result.data.assets[0].asset_path, '/Game/Blueprints/BP_Player.BP_Player');
  assert.equal(output.tool_result.data.page.limit, 25);
  assert.deepEqual(
    collectForbiddenKeys(output.tool_result.data, new Set(['cursor', 'next_cursor', 'total_count'])),
    [],
  );
});

test('direct capture screenshot orchestrates open, focus, and graph-only screenshot capture', async () => {
  const writes: string[] = [];
  const calls: Array<{ command: string; payload?: Record<string, unknown> }> = [];
  const sleeps: number[] = [];
  const artifactDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bh-cli-capture-screenshot-'));
  const payload = {
    schema: 'BlueprintHelper.CaptureScreenshotRequest.v1',
    asset_path: '/Game/Blueprints/BP_Player.BP_Player',
    graph_name: 'EventGraph',
    node_ref: 'nodes[0]',
    label: 'bp_player_eventgraph',
    settle_delay_ms: 25,
  };
  const bridge = {
    sendCommand: async (command: string, commandPayload?: Record<string, unknown>): Promise<BridgeResponse> => {
      calls.push({ command, payload: commandPayload });
      return {
        request_id: `capture_${command}`,
        success: true,
        result: command === 'capture_focused_graph_screenshot'
          ? {
            schema: 'BlueprintHelper.GraphScreenshotResult.v1',
            screenshots: [{
              schema: 'BlueprintHelper.EditorScreenshotResult.v1',
              screenshot_path: 'D:/UEProjects/Template/Saved/BlueprintHelper/Debug/Screenshots/bp_player_eventgraph.png',
              relative_path: 'Screenshots/bp_player_eventgraph.png',
              width: 1600,
              height: 900,
            }],
          }
          : { status: 'completed' },
      };
    },
  };

  const exitCode = await runCli({
    argv: [
      'blueprinthelper_capture_screenshot',
      '--json',
      JSON.stringify(payload),
      '--format',
      'full',
      '--artifact-dir',
      artifactDir,
    ],
    cwd: process.cwd(),
    bridge,
    runner: {} as never,
    sleep: async (ms: number) => {
      sleeps.push(ms);
    },
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  assert.equal(exitCode, 0);
  assert.deepEqual(calls, [
    { command: 'open_asset', payload: { asset_path: '/Game/Blueprints/BP_Player.BP_Player' } },
    {
      command: 'focus_blueprint_editor_target',
      payload: {
        asset_path: '/Game/Blueprints/BP_Player.BP_Player',
        graph_name: 'EventGraph',
        node_ref: 'nodes[0]',
      },
    },
    { command: 'capture_focused_graph_screenshot', payload: { label: 'bp_player_eventgraph' } },
  ]);
  assert.deepEqual(sleeps, [25]);

  const output = JSON.parse(writes.join(''));
  assert.equal(output.status, 'completed');
  assert.equal(output.tool_name, 'blueprinthelper_capture_screenshot');
  assert.equal(output.tool_result.data.asset_path, '/Game/Blueprints/BP_Player.BP_Player');
  assert.equal(output.tool_result.data.graph_name, 'EventGraph');
  assert.equal(output.tool_result.data.capture_scope, 'graph');
  assert.equal(
    output.tool_result.data.screenshot.screenshot_path,
    'D:/UEProjects/Template/Saved/BlueprintHelper/Debug/Screenshots/bp_player_eventgraph.png',
  );
});

test('grouped command help points TaskSpec preview to composer navigation', async () => {
  const writes: string[] = [];
  const exitCode = await runCli({
    argv: ['task', 'preview', '--help'],
    cwd: process.cwd(),
    bridge: {} as never,
    runner: {} as never,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  const output = writes.join('');
  assert.equal(exitCode, 0);
  assert.match(output, /BlueprintHelper CLI help: task preview/);
  assert.match(output, /Input file root: bare BlueprintHelper\.TaskSpec\.v1/);
  assert.match(output, /bh tools templates families --workflow preview_execute --format json/);
  assert.match(output, /bh tools templates compose --family <family>/);
  assert.doesNotMatch(output, /task_preview_bare_taskspec_template\.json/);
});

test('lifecycle help directs Agents to global MCP instead of CLI aliases', async () => {
  const writes: string[] = [];
  const exitCode = await runCli({
    argv: ['open_editor', '--help'],
    cwd: process.cwd(),
    bridge: {} as never,
    runner: {} as never,
    stdout: (line) => writes.push(line),
    stderr: () => {},
  });

  const output = writes.join('');
  assert.equal(exitCode, 0);
  assert.match(output, /BlueprintHelper CLI help: open_editor/);
  assert.match(output, /mcp__blueprint_helper__blueprint_open_editor/);
  assert.match(output, /direct tool-name command is intentionally not an Agent-facing compatibility path/i);
});

test('lifecycle CLI invocation is blocked and points Agents to MCP', async () => {
  const writes: string[] = [];
  const errors: string[] = [];
  const exitCode = await runCli({
    argv: ['open_editor', '--json', '{}'],
    cwd: process.cwd(),
    bridge: {} as never,
    runner: {} as never,
    stdout: (line) => writes.push(line),
    stderr: (line) => errors.push(line),
  });

  assert.equal(exitCode, 64);
  assert.equal(writes.join(''), '');
  assert.match(errors.join(''), /open_editor direct CLI command was removed/);
  assert.match(errors.join(''), /mcp__blueprint_helper__blueprint_open_editor/);
});

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
  const activeRoutes = getActiveReadContextRouteDescriptors();
  assert.deepEqual(data.formats, uniqueSorted(activeRoutes.flatMap((route) => route.supported_formats)));
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

test('direct blueprint_close_editor is blocked even when expert flag is present', async () => {
  const writes: string[] = [];
  const errors: string[] = [];
  const exitCode = await runCli({
    argv: ['blueprint_close_editor', '--json', '{ "save_all": false }', '--expert'],
    cwd: process.cwd(),
    bridge: {
      sendCommand: async () => { throw new Error('must not call bridge'); },
    } as never,
    runner: {} as never,
    stdout: (line) => writes.push(line),
    stderr: (line) => errors.push(line),
  });

  assert.equal(exitCode, 64);
  assert.equal(writes.join(''), '');
  assert.match(errors.join(''), /blueprint_close_editor direct CLI command was removed/);
  assert.match(errors.join(''), /mcp__blueprint_helper__blueprint_close_editor/);
});

test('short close_editor is blocked and does not call Bridge', async () => {
  const writes: string[] = [];
  const errors: string[] = [];
  const exitCode = await runCli({
    argv: ['close_editor'],
    cwd: process.cwd(),
    bridge: {
      sendCommand: async () => { throw new Error('must not call bridge'); },
    } as never,
    runner: {} as never,
    stdout: (line) => writes.push(line),
    stderr: (line) => errors.push(line),
  });

  assert.equal(exitCode, 64);
  assert.equal(writes.join(''), '');
  assert.match(errors.join(''), /close_editor direct CLI command was removed/);
  assert.match(errors.join(''), /mcp__blueprint_helper__blueprint_close_editor/);
});


test('short open_editor is blocked and does not launch a process', async () => {
  const writes: string[] = [];
  const errors: string[] = [];
  const exitCode = await runCli({
    argv: ['open_editor'],
    cwd: process.cwd(),
    bridge: {
      sendCommand: async () => { throw new Error('must not call sendCommand'); },
      ping: async () => { throw new Error('must not call ping'); },
    } as never,
    runner: {} as never,
    runLocalProcess: async () => { throw new Error('must not launch process'); },
    sleep: async () => {},
    stdout: (line) => writes.push(line),
    stderr: (line) => errors.push(line),
  });

  assert.equal(exitCode, 64);
  assert.equal(writes.join(''), '');
  assert.match(errors.join(''), /open_editor direct CLI command was removed/);
  assert.match(errors.join(''), /mcp__blueprint_helper__blueprint_open_editor/);
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

function uniqueSorted(values: readonly string[]): string[] {
  return [...new Set(values)].sort((left, right) => left.localeCompare(right));
}

function restoreEnv(name: string, value: string | undefined): void {
  if (value === undefined) {
    delete process.env[name];
    return;
  }
  process.env[name] = value;
}

function collectForbiddenKeys(value: unknown, forbidden: ReadonlySet<string>, path = '$'): string[] {
  if (Array.isArray(value)) {
    return value.flatMap((item, index) => collectForbiddenKeys(item, forbidden, `${path}[${index}]`));
  }
  if (value === null || typeof value !== 'object') {
    return [];
  }

  const matches: string[] = [];
  for (const [key, entry] of Object.entries(value as Record<string, unknown>)) {
    const childPath = `${path}.${key}`;
    if (forbidden.has(key)) {
      matches.push(childPath);
    }
    matches.push(...collectForbiddenKeys(entry, forbidden, childPath));
  }
  return matches;
}

