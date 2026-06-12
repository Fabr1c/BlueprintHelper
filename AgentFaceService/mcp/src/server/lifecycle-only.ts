import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { BridgeClient, DEFAULT_BRIDGE_HOST, DEFAULT_BRIDGE_PORT } from '@blueprinthelper/task-core/bridge/bridge-client';
import { registerEditorLifecycleTools } from '../mcp/tools/editor-lifecycle-tools.js';

const BRIDGE_HOST = process.env['BRIDGE_HOST'] ?? DEFAULT_BRIDGE_HOST;
const BRIDGE_PORT = parseInt(process.env['BRIDGE_PORT'] ?? String(DEFAULT_BRIDGE_PORT), 10);

async function main() {
  console.error(`[BlueprintHelper Lifecycle MCP] Starting... Bridge target: ${BRIDGE_HOST}:${BRIDGE_PORT}`);

  const bridge = new BridgeClient({ host: BRIDGE_HOST, port: BRIDGE_PORT });

  const server = new McpServer({
    name: 'blueprint-helper',
    version: '0.6.0',
  });

  registerEditorLifecycleTools(server, bridge, { ueEngineDir: '' });

  const transport = new StdioServerTransport();
  await server.connect(transport);

  console.error('[BlueprintHelper Lifecycle MCP] Server running (stdio mode, lifecycle tools plus developer-only exec)');
}

process.on('SIGINT', () => {
  console.error('[BlueprintHelper Lifecycle MCP] Received SIGINT, shutting down...');
  process.exit(0);
});

process.on('SIGTERM', () => {
  console.error('[BlueprintHelper Lifecycle MCP] Received SIGTERM, shutting down...');
  process.exit(0);
});

main().catch((err) => {
  console.error('[BlueprintHelper Lifecycle MCP] Fatal error:', err);
  process.exit(1);
});
