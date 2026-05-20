/**
 * BlueprintHelper MCP Server 閳?閸忋儱褰?
 *
 * stdio 濡€崇础鏉╂劘顢戦敍宀勨偓姘崇箖 TCP 鏉╃偞甯?UE5 Bridge (127.0.0.1:54321)閵?
 * 鏉堟挸鍤弮銉ョ箶閻?console.error閿涘澃tdout 娣囨繄鏆€缂?JSON-RPC閿涘鈧?
 */

import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { BridgeClient } from '@blueprinthelper/task-core/bridge/bridge-client';
import { registerTools } from '../mcp/tools/register-tools.js';

// 閳光偓閳光偓閳光偓 闁板秶鐤?閳光偓閳光偓閳光偓

const BRIDGE_HOST = process.env['BRIDGE_HOST'] ?? '127.0.0.1';
const BRIDGE_PORT = parseInt(process.env['BRIDGE_PORT'] ?? '54321', 10);

// 鐏炴洖绱戠敮姝岊潌閻ㄥ嫭膩閺夊灝褰夐柌蹇ョ礄婵?${workspaceFolder}閿涘绱濈涵顔荤箽鐠侯垰绶為崣顖滄暏
// 閳光偓閳光偓閳光偓 閸氼垰濮?閳光偓閳光偓閳光偓

async function main() {
  console.error(`[BlueprintHelper MCP] Starting... Bridge target: ${BRIDGE_HOST}:${BRIDGE_PORT}`);

  const bridge = new BridgeClient({ host: BRIDGE_HOST, port: BRIDGE_PORT });

  const server = new McpServer({
    name: 'blueprint-helper',
    version: '0.5.3',
  });

  // 濞夈劌鍞藉銉ュ徔娑撳氦绁┃?
  registerTools(server, bridge, { ueEngineDir: '' });

  // stdio 娴肩姾绶?
  const transport = new StdioServerTransport();
  await server.connect(transport);

  console.error('[BlueprintHelper MCP] Server running (stdio mode)');
}

// 閳光偓閳光偓閳光偓 娴兼﹢娉ら柅鈧崙?閳光偓閳光偓閳光偓

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
