/**
 * MCP Resources 注册
 *
 * 暴露 2 �?MCP 资源�? * - blueprint://rules/json-to-blueprint  �?转换规则文档
 * - blueprint://context/active-graph     �?当前编辑器上下文
 */

import { McpServer, ResourceTemplate } from '@modelcontextprotocol/sdk/server/mcp.js';
import { BridgeClient } from '../../bridge/bridge-client.js';
import {
  getBlueprintPayloadBody,
  getStringField,
  isRecord,
  normalizeBlueprintPayload,
  parseBlueprintResourceUri,
} from '../result/mcp-response.js';

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

  // ─── 3. Blueprint asset views ───
  server.registerResource(
    'blueprint-asset-view',
    new ResourceTemplate('blueprint://asset/{assetPath}', { list: undefined }),
    {
      description: 'On-demand Blueprint asset views such as LogicMd, LogicJson, and RawJson.',
      mimeType: 'application/json',
    },
    async (uri) => {
      try {
        const request = parseBlueprintResourceUri(uri.href);

        if (request.view === 'logic-md') {
          const resp = await bridge.sendCommand('export_logic', {
            target_blueprint: request.assetPath,
            ...(request.graph ? { target_graph: request.graph } : {}),
            format: 'logic_md',
          });
          const payload = normalizeBlueprintPayload(resp.result);
          const payloadRecord = isRecord(payload) ? payload : undefined;
          const markdown = getStringField(payloadRecord, 'markdown')
            ?? (typeof payload === 'string' ? payload : JSON.stringify(payload, null, 2));

          return {
            contents: [{ uri: uri.href, mimeType: 'text/markdown', text: markdown }],
          };
        }

        if (request.view === 'logic-json') {
          const resp = await bridge.sendCommand('export_logic', {
            target_blueprint: request.assetPath,
            ...(request.graph ? { target_graph: request.graph } : {}),
            format: 'logic_json',
          });
          const payload = normalizeBlueprintPayload(resp.result);

          return {
            contents: [{
              uri: uri.href,
              mimeType: 'application/json',
              text: JSON.stringify(payload),
            }],
          };
        }

        if (request.view === 'raw-json') {
          const resp = await bridge.sendCommand('export_to_json', {
            target_blueprint: request.assetPath,
            ...(request.graph ? { target_graph: request.graph } : {}),
          });
          const body = getBlueprintPayloadBody(resp.result);

          return {
            contents: [{
              uri: uri.href,
              mimeType: 'application/json',
              text: JSON.stringify(body),
            }],
          };
        }

        throw new Error(`Unsupported resource view: ${request.view}`);
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
