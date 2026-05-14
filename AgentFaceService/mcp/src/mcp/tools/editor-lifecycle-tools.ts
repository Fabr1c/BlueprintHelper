import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { BridgeClient, BridgeResponse } from '@blueprinthelper/task-core/bridge/bridge-client';
import { spawn } from 'node:child_process';
import * as path from 'node:path';
import { resolveProjectEngineDir } from '../../project-profile/agent-profile.js';
import { resolveExplicitProjectFile } from '../../project-profile/editor-paths.js';
import type { TaskToolsConfig } from './task-tools.js';

export interface EditorLifecycleConfig {
  ueEngineDir: string;
  taskCompiler?: TaskToolsConfig['taskCompiler'];
}

function toToolResult(resp: BridgeResponse, isError = false) {
  return {
    content: [{ type: 'text' as const, text: JSON.stringify(resp, null, 2) }],
    isError: isError || !resp.success,
  };
}

function toErrorResult(err: unknown) {
  const message = err instanceof Error ? err.message : String(err);
  return {
    content: [{ type: 'text' as const, text: `Bridge error: ${message}` }],
    isError: true,
  };
}

function editorLifecycleDescription(description: string): string {
  return `Editor lifecycle only. ${description} Ordinary BlueprintHelper reads and writes must use the CLI surface, not MCP.`;
}

export function registerEditorLifecycleTools(
  server: McpServer,
  bridge: BridgeClient,
  config: EditorLifecycleConfig,
): void {
  server.registerTool(
    'blueprint_close_editor',
    {
      description: editorLifecycleDescription(
        'Save dirty assets when requested and close the running Unreal Editor through the BlueprintHelper Bridge.',
      ),
      inputSchema: z.object({
        save_all: z
          .boolean()
          .optional()
          .describe('Whether to save all dirty packages before closing (default true)'),
      }),
    },
    async ({ save_all }) => {
      try {
        const payload: Record<string, unknown> = {};
        if (save_all !== undefined) payload.save_all = save_all;
        const resp = await bridge.sendCommand('close_editor', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  server.registerTool(
    'blueprint_open_editor',
    {
      description: editorLifecycleDescription(
        'Launch Unreal Editor with an explicit .uproject file, then wait for the BlueprintHelper Bridge server to become available.',
      ),
      inputSchema: z.object({
        project_file: z
          .string()
          .min(1)
          .describe('Absolute path to the target .uproject file.'),
        wait_timeout_ms: z
          .number()
          .optional()
          .describe('Max time in ms to wait for the editor Bridge to become available (default 120000)'),
      }),
    },
    async ({ project_file, wait_timeout_ms }) => {
      let uprojectFile: string;
      try {
        uprojectFile = resolveExplicitProjectFile(project_file);
      } catch (err) {
        return toErrorResult(err);
      }

      let ueEngineDir: string;
      try {
        ueEngineDir = resolveProjectEngineDir(uprojectFile, config);
      } catch (err) {
        return toErrorResult(err);
      }

      const editorExe = path.join(
        ueEngineDir,
        'Engine',
        'Binaries',
        'Win64',
        'UnrealEditor.exe',
      );
      const timeoutMs = wait_timeout_ms ?? 120_000;
      const launchCommand = `"${editorExe}" "${uprojectFile}"`;

      console.error(`[BlueprintHelper MCP] Launching editor with project file: ${launchCommand}`);

      try {
        const child = spawn(editorExe, [uprojectFile], {
          detached: true,
          stdio: 'ignore',
        });
        child.unref();
      } catch (err) {
        return {
          content: [
            {
              type: 'text' as const,
              text: JSON.stringify(
                {
                  success: false,
                  code: 'EDITOR_LAUNCH_FAILED',
                  message: 'Failed to launch Unreal Editor with the requested .uproject.',
                  editor_exe: editorExe,
                  uproject_path: uprojectFile,
                  launch_command: launchCommand,
                  error: err instanceof Error ? err.message : String(err),
                },
                null,
                2,
              ),
            },
          ],
          isError: true,
        };
      }

      const startTime = Date.now();
      const pollIntervalMs = 3000;

      while (Date.now() - startTime < timeoutMs) {
        await new Promise((resolve) => setTimeout(resolve, pollIntervalMs));

        const alive = await bridge.ping();
        if (alive) {
          return {
            content: [
              {
                type: 'text' as const,
                text: JSON.stringify(
                  {
                    success: true,
                    code: 'EDITOR_BRIDGE_AVAILABLE',
                    message: 'Unreal Editor was launched and BlueprintHelper Bridge is available.',
                    editor_exe: editorExe,
                    uproject_path: uprojectFile,
                    launch_command: launchCommand,
                    elapsed_ms: Date.now() - startTime,
                  },
                  null,
                  2,
                ),
              },
            ],
            isError: false,
          };
        }
      }

      return {
        content: [
          {
            type: 'text' as const,
            text: JSON.stringify(
              {
                success: false,
                code: 'EDITOR_STARTED_BRIDGE_TIMEOUT',
                message: `Unreal Editor was started, but BlueprintHelper Bridge did not become available within ${timeoutMs}ms.`,
                editor_exe: editorExe,
                uproject_path: uprojectFile,
                launch_command: launchCommand,
                elapsed_ms: Date.now() - startTime,
              },
              null,
              2,
            ),
          },
        ],
        isError: true,
      };
    },
  );
}
