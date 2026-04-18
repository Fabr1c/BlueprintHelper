/**
 * BlueprintHelper MCP Server — 入口
 *
 * stdio 模式运行，通过 TCP 连接 UE5 Bridge (127.0.0.1:54321)。
 * 输出日志用 console.error（stdout 保留给 JSON-RPC）。
 */

import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { BridgeClient } from './bridge-client.js';
import { registerTools } from './tools.js';
import { registerResources } from './resources.js';

// ─── 配置 ───

const BRIDGE_HOST = process.env['BRIDGE_HOST'] ?? '127.0.0.1';
const BRIDGE_PORT = parseInt(process.env['BRIDGE_PORT'] ?? '54321', 10);

// ─── 启动 ───

async function main() {
  console.error(`[BlueprintHelper MCP] Starting... Bridge target: ${BRIDGE_HOST}:${BRIDGE_PORT}`);

  const bridge = new BridgeClient({ host: BRIDGE_HOST, port: BRIDGE_PORT });

  const server = new McpServer({
    name: 'blueprint-helper',
    version: '0.1.0',
  });

  // 注册工具与资源
  registerTools(server, bridge);
  registerResources(server, bridge);

  // stdio 传输
  const transport = new StdioServerTransport();
  await server.connect(transport);

  console.error('[BlueprintHelper MCP] Server running (stdio mode)');
}

// ─── 优雅退出 ───

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
