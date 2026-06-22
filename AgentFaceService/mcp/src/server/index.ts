/**
 * BlueprintHelper MCP server.
 *
 * Stdio server forwarding MCP requests to the UE Bridge. The default target is
 * 127.0.0.1:32147, with BRIDGE_HOST / BRIDGE_PORT available as overrides.
 * Transport logs go to stderr; stdout stays reserved for JSON-RPC.
 */

import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { BridgeClient, DEFAULT_BRIDGE_HOST, DEFAULT_BRIDGE_PORT } from '@blueprinthelper/task-core/bridge/bridge-client';
import { registerTools } from '../mcp/tools/register-tools.js';

const BRIDGE_HOST = process.env['BRIDGE_HOST'] ?? DEFAULT_BRIDGE_HOST;
const BRIDGE_PORT = parseInt(process.env['BRIDGE_PORT'] ?? String(DEFAULT_BRIDGE_PORT), 10);

async function main() {
  console.error(`[BlueprintHelper MCP] Starting... Bridge target: ${BRIDGE_HOST}:${BRIDGE_PORT}`);

  const bridge = new BridgeClient({ host: BRIDGE_HOST, port: BRIDGE_PORT });

  const server = new McpServer({
    name: 'blueprint-helper',
    version: '0.6.6',
  });

  registerTools(server, bridge, { ueEngineDir: '' });

  const transport = new StdioServerTransport();
  await server.connect(transport);

  console.error('[BlueprintHelper MCP] Server running (stdio mode)');
}

process.on('SIGINT', () => {
  console.error('[BlueprintHelper MCP] Received SIGINT, shutting down...');
  process.exit(0);
});

process.on('SIGTERM', () => {
  console.error('[BlueprintHelper MCP] Received SIGTERM, shutting down...');
  process.exit(0);
});

main().catch((err) => {
  console.error('[BlueprintHelper MCP] Fatal error:', err);
  process.exit(1);
});
