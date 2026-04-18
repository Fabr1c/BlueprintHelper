/**
 * MCP Resources 注册
 *
 * 暴露 2 个 MCP 资源：
 * - blueprint://rules/json-to-blueprint  — 转换规则文档
 * - blueprint://context/active-graph     — 当前编辑器上下文
 */

import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { BridgeClient } from './bridge-client.js';

export function registerResources(server: McpServer, bridge: BridgeClient): void {

  // ─── 1. 规则文档 ───
  server.registerResource(
    'json-to-blueprint-rules',
    'blueprint://rules/json-to-blueprint',
    {
      description: 'The JSON-to-Blueprint conversion rule document (Markdown).',
      mimeType: 'text/markdown',
    },
    async (uri) => {
      try {
        const resp = await bridge.sendCommand('get_rule_markdown');
        const markdown = resp.success && resp.result?.['markdown']
          ? resp.result['markdown'] as string
          : `Error: ${resp.message ?? 'Failed to retrieve rules'}`;
        return {
          contents: [{ uri: uri.href, mimeType: 'text/markdown', text: markdown }],
        };
      } catch (err) {
        const msg = err instanceof Error ? err.message : String(err);
        return {
          contents: [{ uri: uri.href, mimeType: 'text/plain', text: `Bridge error: ${msg}` }],
        };
      }
    },
  );

  // ─── 2. 编辑器上下文 ───
  server.registerResource(
    'active-graph-context',
    'blueprint://context/active-graph',
    {
      description: 'Current active blueprint and graph context in the Unreal Editor.',
      mimeType: 'application/json',
    },
    async (uri) => {
      try {
        const resp = await bridge.sendCommand('get_editor_context');
        const text = JSON.stringify(resp.success ? resp.result : resp, null, 2);
        return {
          contents: [{ uri: uri.href, mimeType: 'application/json', text }],
        };
      } catch (err) {
        const msg = err instanceof Error ? err.message : String(err);
        return {
          contents: [{
            uri: uri.href,
            mimeType: 'application/json',
            text: JSON.stringify({ error: msg }),
          }],
        };
      }
    },
  );
}
