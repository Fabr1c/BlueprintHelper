import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { BridgeClient, BridgeResponse } from '@blueprinthelper/task-core/bridge/bridge-client';
import { spawn } from 'node:child_process';
import * as path from 'node:path';
import { resolveProjectEngineDir } from '../../project-profile/agent-profile.js';
import { resolveExplicitProjectFile } from '../../project-profile/editor-paths.js';
import {
  dismissUnrealEditorModalDialogsByOsWindow,
  type EditorModalDialogDismissFallbackResult,
} from './editor-modal-dialogs.js';

export interface EditorLifecycleConfig {
  ueEngineDir: string;
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

function bridgeErrorMessage(err: unknown): string {
  return err instanceof Error ? err.message : String(err);
}

function lifecycleJsonResult(payload: Record<string, unknown>, isError: boolean) {
  return {
    content: [{ type: 'text' as const, text: JSON.stringify(payload, null, 2) }],
    isError,
  };
}

function editorLifecycleDescription(description: string): string {
  return `Editor lifecycle only. ${description} Ordinary BlueprintHelper reads and writes must use the CLI surface, not MCP.`;
}

function buildModalDialogFallbackPayload(
  fallback: EditorModalDialogDismissFallbackResult,
  options: {
    code: string;
    bridgeError?: string;
    noModalIsSuccess: boolean;
  },
): Record<string, unknown> {
  const dismissedCount = fallback.dismissed_modal_windows;
  const hasFailure = fallback.error !== undefined || fallback.has_remaining_modal;
  const noModalFound = dismissedCount === 0 && !fallback.has_remaining_modal && !fallback.error;
  const success = options.noModalIsSuccess
    ? !hasFailure
    : !hasFailure && dismissedCount > 0;
  const lifecycleStatus = fallback.error
    ? 'modal_dismiss_failed'
    : fallback.has_remaining_modal
      ? 'modal_dialogs_remaining'
      : dismissedCount > 0
        ? 'modal_dialogs_dismissed'
        : 'no_modal_dialog_found';

  return {
    success,
    code: options.code,
    lifecycle_status: lifecycleStatus,
    bridge_status: options.bridgeError ? 'unavailable' : 'not_used',
    ...(options.bridgeError ? { bridge_error: options.bridgeError } : {}),
    modal_dialogs: fallback,
    recommended_action: fallback.has_remaining_modal
      ? 'Call blueprint_close_editor_dialogs to close remaining Unreal Editor modal dialogs, or close the remaining modal dialog manually, then retry the lifecycle command.'
      : dismissedCount > 0
        ? 'Retry blueprint_close_editor after the modal dialog was dismissed.'
        : noModalFound
          ? 'No Unreal Editor modal dialog was found.'
          : 'Inspect modal_dialogs.error and retry after the OS-window fallback is available.',
  };
}

function shouldBlockCloseForRemainingModal(fallback: EditorModalDialogDismissFallbackResult): boolean {
  return fallback.has_remaining_modal;
}

function attachModalDialogPreflight(
  resp: BridgeResponse,
  preflight: EditorModalDialogDismissFallbackResult,
): BridgeResponse {
  return {
    ...resp,
    result: {
      ...(resp.result ?? {}),
      modal_dialogs_preflight: preflight,
    },
  };
}

function developerOnlyDescription(description: string): string {
  return `Developer-only MCP tool. ${description} Ordinary agents must not call this tool; use BlueprintHelper CLI/TaskSpec workflows or editor lifecycle tools instead.`;
}

export function registerEditorLifecycleTools(
  server: McpServer,
  bridge: BridgeClient,
  config: EditorLifecycleConfig,
): void {
  server.registerTool(
    'blueprint_developer_exec_console_command',
    {
      title: 'BlueprintHelper Developer Exec Console Command',
      description: developerOnlyDescription(
        'Execute an Unreal Editor console command through the BlueprintHelper Bridge for local BlueprintHelper development and test orchestration only. The running Editor Bridge still enforces high-risk command configuration and write-session authorization.',
      ),
      annotations: {
        readOnlyHint: false,
        destructiveHint: true,
        idempotentHint: false,
        openWorldHint: false,
      },
      _meta: {
        'blueprinthelper/audience': 'developer',
        'blueprinthelper/ordinaryAgentCallable': false,
        'blueprinthelper/bridgeCommand': 'exec_console_command',
      },
      inputSchema: z.object({
        command: z.string().min(1).describe('Unreal Editor console command to execute.'),
        reason: z
          .string()
          .min(1)
          .describe('Developer-facing reason for executing this high-risk console command.'),
      }),
    },
    async ({ command }) => {
      try {
        const resp = await bridge.sendCommand('exec_console_command', { command });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  server.registerTool(
    'blueprint_dismiss_editor_dialogs',
    {
      description: editorLifecycleDescription(
        'Dismiss active Unreal Editor modal dialogs that can block lifecycle commands, then return the remaining modal-dialog state.',
      ),
      annotations: {
        readOnlyHint: false,
        destructiveHint: false,
        idempotentHint: true,
        openWorldHint: false,
      },
      _meta: {
        'blueprinthelper/audience': 'lifecycle',
        'blueprinthelper/ordinaryAgentCallable': true,
        'blueprinthelper/bridgeCommand': 'dismiss_editor_dialogs',
      },
      inputSchema: z.object({}),
    },
    async () => {
      try {
        const resp = await bridge.sendCommand('dismiss_editor_dialogs', {});
        return toToolResult(resp);
      } catch (err) {
        const fallback = await dismissUnrealEditorModalDialogsByOsWindow();
        const bridgeError = bridgeErrorMessage(err);
        const payload = buildModalDialogFallbackPayload(fallback, {
          code: 'EDITOR_MODAL_DISMISS_BRIDGE_UNAVAILABLE',
          bridgeError,
          noModalIsSuccess: true,
        });
        return lifecycleJsonResult(
          payload,
          payload.success !== true,
        );
      }
    },
  );

  server.registerTool(
    'blueprint_close_editor_dialogs',
    {
      description: editorLifecycleDescription(
        'Close visible Unreal Editor modal dialogs through an OS-window fallback without requiring the BlueprintHelper Bridge. Use this when close_editor is blocked by a modal popup.',
      ),
      annotations: {
        readOnlyHint: false,
        destructiveHint: false,
        idempotentHint: true,
        openWorldHint: false,
      },
      _meta: {
        'blueprinthelper/audience': 'lifecycle',
        'blueprinthelper/ordinaryAgentCallable': true,
        'blueprinthelper/bridgeCommand': 'os_window_close_editor_dialogs',
      },
      inputSchema: z.object({}),
    },
    async () => {
      const fallback = await dismissUnrealEditorModalDialogsByOsWindow();
      const payload = buildModalDialogFallbackPayload(fallback, {
        code: 'EDITOR_MODAL_DIALOG_OS_CLOSE',
        noModalIsSuccess: true,
      });
      return lifecycleJsonResult(payload, payload.success !== true);
    },
  );

  server.registerTool(
    'blueprint_close_editor',
    {
      description: editorLifecycleDescription(
        'Save dirty assets when requested and close the running Unreal Editor through the BlueprintHelper Bridge. If modal dialogs block shutdown, the response includes lifecycle_status and modal_dialogs details; call blueprint_dismiss_editor_dialogs when Bridge is responsive, or blueprint_close_editor_dialogs when the modal blocks Bridge, then retry close.',
      ),
      inputSchema: z.object({
        save_all: z
          .boolean()
          .optional()
          .describe('Whether to save all dirty packages before closing (default true)'),
      }),
    },
    async ({ save_all }) => {
      const preflight = await dismissUnrealEditorModalDialogsByOsWindow();
      if (shouldBlockCloseForRemainingModal(preflight)) {
        return lifecycleJsonResult(
          {
            success: false,
            code: 'EDITOR_CLOSE_BLOCKED_BY_MODAL_DIALOG',
            lifecycle_status: 'close_blocked_by_modal_dialog',
            bridge_status: 'not_used',
            close_requested: false,
            modal_dialogs_preflight: preflight,
            recommended_action: 'Call blueprint_close_editor_dialogs to close remaining Unreal Editor modal dialogs, or close the remaining modal dialog manually, then retry blueprint_close_editor.',
          },
          true,
        );
      }

      try {
        const payload: Record<string, unknown> = {};
        if (save_all !== undefined) payload.save_all = save_all;
        const resp = await bridge.sendCommand('close_editor', payload);
        return toToolResult(attachModalDialogPreflight(resp, preflight));
      } catch (err) {
        const fallback = await dismissUnrealEditorModalDialogsByOsWindow();
        const bridgeError = bridgeErrorMessage(err);
        const payload = buildModalDialogFallbackPayload(fallback, {
          code: 'EDITOR_CLOSE_BRIDGE_UNAVAILABLE',
          bridgeError,
          noModalIsSuccess: false,
        });
        return lifecycleJsonResult(
          {
            ...payload,
            success: false,
            close_requested: false,
            modal_dialogs_preflight: preflight,
          },
          true,
        );
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
