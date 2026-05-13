import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import type { BridgeClient } from '@blueprinthelper/task-core/bridge/bridge-client';
import { compileTaskSpecWithPython } from '@blueprinthelper/task-core/task/compiler/task-python-orchestrator';
import {
  createTaskSpecRunner,
  type TaskCompiler,
} from '@blueprinthelper/task-core/task/service/task-spec-runner';
import { getBlueprintHelperToolRegistry } from '@blueprinthelper/task-core/tool-surface/tool-registry';
import { toMcpResult } from '../result/tool-result.js';

export interface SharedRegistryMcpConfig {
  cwd: string;
  ueEngineDir: string;
  taskCompiler?: TaskCompiler;
  toolNames?: Set<string>;
}

export function registerSharedRegistryTools(
  server: McpServer,
  bridge: BridgeClient,
  config: SharedRegistryMcpConfig,
): Set<string> {
  const taskRunner = createTaskSpecRunner({
    bridge,
    taskCompiler: config.taskCompiler ?? compileTaskSpecWithPython,
  });
  const registered = new Set<string>();

  for (const tool of getBlueprintHelperToolRegistry()) {
    if (tool.audience !== 'default') {
      continue;
    }
    if (config.toolNames && !config.toolNames.has(tool.name)) {
      continue;
    }

    server.registerTool(
      tool.name,
      {
        description: tool.description,
        inputSchema: tool.inputSchema,
      },
      async (input) => toMcpResult(await tool.execute(input as Record<string, unknown>, {
        cwd: config.cwd,
        bridge,
        taskRunner,
        ueEngineDir: config.ueEngineDir,
      })),
    );
    registered.add(tool.name);
  }

  return registered;
}
