import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import type { BridgeClient } from '@blueprinthelper/task-core/bridge/bridge-client';
import {
  registerEditorLifecycleTools,
  type EditorLifecycleConfig,
} from './editor-lifecycle-tools.js';

export type EditorConfig = EditorLifecycleConfig;

export function registerTools(
  server: McpServer,
  bridge: BridgeClient,
  config: EditorConfig,
): void {
  registerEditorLifecycleTools(server, bridge, config);
}
