/**
 * BlueprintHelper MCP Server 鈥?鍏ュ彛
 *
 * stdio 妯″紡杩愯锛岄€氳繃 TCP 杩炴帴 UE5 Bridge (127.0.0.1:54321)銆?
 * 杈撳嚭鏃ュ織鐢?console.error锛坰tdout 淇濈暀缁?JSON-RPC锛夈€?
 */

import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { BridgeClient } from './bridge-client.js';
import { registerTools } from './tools.js';
import { registerResources } from './resources.js';

// 鈹€鈹€鈹€ 閰嶇疆 鈹€鈹€鈹€

const BRIDGE_HOST = process.env['BRIDGE_HOST'] ?? '127.0.0.1';
const BRIDGE_PORT = parseInt(process.env['BRIDGE_PORT'] ?? '54321', 10);

// 灞曞紑甯歌鐨勬ā鏉垮彉閲忥紙濡?${workspaceFolder}锛夛紝纭繚璺緞鍙敤
function expandTemplateVars(raw: string): string {
  return raw
    .replace(/\$\{workspaceFolder\}/gi, process.cwd())
    .replace(/\$\{workspaceRoot\}/gi, process.cwd())
    .replace(/\$\{userHome\}/gi, process.env['USERPROFILE'] ?? process.env['HOME'] ?? '');
}

const UE_ENGINE_DIR = expandTemplateVars(process.env['UE_ENGINE_DIR'] ?? '');
const UE_PROJECT_FILE = expandTemplateVars(process.env['UE_PROJECT_FILE'] ?? '');

// 鈹€鈹€鈹€ 鍚姩 鈹€鈹€鈹€

async function main() {
  console.error(`[BlueprintHelper MCP] Starting... Bridge target: ${BRIDGE_HOST}:${BRIDGE_PORT}`);

  const bridge = new BridgeClient({ host: BRIDGE_HOST, port: BRIDGE_PORT });

  const server = new McpServer({
    name: 'blueprint-helper',
    version: '0.3.8',
  });

  // 娉ㄥ唽宸ュ叿涓庤祫婧?
  registerTools(server, bridge, { ueEngineDir: UE_ENGINE_DIR, ueProjectFile: UE_PROJECT_FILE });
  registerResources(server, bridge);

  // stdio 浼犺緭
  const transport = new StdioServerTransport();
  await server.connect(transport);

  console.error('[BlueprintHelper MCP] Server running (stdio mode)');
}

// 鈹€鈹€鈹€ 浼橀泤閫€鍑?鈹€鈹€鈹€

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
