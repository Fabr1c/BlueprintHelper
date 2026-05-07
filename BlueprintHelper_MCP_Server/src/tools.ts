/**
 * MCP Tools 注册
 *
 * 将 42 个 Bridge 命令映射为 MCP 工具，另提供 2 个本地生命周期工具。
 * Phase 1-3: 8 个蓝图操作/逻辑读取工具
 * Phase 4:   5 个资产浏览工具
 * Phase 5:   9 个蓝图结构操作工具
 * Phase 6:   6 个 UMG Widget 操作工具
 * Phase 7:   6 个 DataAsset & DataTable 操作工具
 * Phase 8:   6 个编辑器命令工具
 */

import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { BridgeClient, BridgeResponse } from './bridge-client.js';
import {
  McpResponseMode,
  buildBlueprintToolResult,
  getRecordField,
  getStringField,
  isRecord,
  makeBlueprintResourceUri,
  normalizeBlueprintPayload,
  resolveResponseMode,
} from './mcp-response.js';
import {
  normalizeToolResult,
  toMcpResult,
  buildDiagnosticsMarkdown,
  buildDiagnosticsData,
  type ToolResultBase,
  type DiagnosticsMarkdownReport,
} from './tool-result.js';
import { registerTaskTools, type TaskToolsConfig } from './task-tools.js';
import { execFile, spawn } from 'node:child_process';
import * as path from 'node:path';
import * as fs from 'node:fs';

/** 编辑器/引擎路径配置 */
export interface EditorConfig {
  ueEngineDir: string;
  ueProjectFile: string;
  taskCompiler?: TaskToolsConfig['taskCompiler'];
}

/** RawJson input: accepts structured object or legacy JSON string */
const rawJsonInputSchema = z.union([
  z.string(),
  z.record(z.unknown()),
]).describe('The BlueprintHelper RawJson object (nodes, links, version, schema) or legacy JSON string to import');

const LEGACY_TOOL_GUIDANCE =
  'Normal Agents should prefer blueprinthelper_read_agent_guide, blueprinthelper_read_context, blueprinthelper_preview_task, and blueprinthelper_execute_task.';
const FROZEN_TOOL_PREFIX =
  'FROZEN / Expert-only / Normal agents must not call directly.';

const AGENT_GUIDE_INDEX_RELATIVE_PATH = path.join(
  'Resources',
  'AgentGuide',
  '00_Agent_Onboarding_Index_20260504.md',
);

function legacyDebugExpertDescription(description: string): string {
  return `${FROZEN_TOOL_PREFIX} [Legacy/internal/debug/expert] ${description} ${LEGACY_TOOL_GUIDANCE}`;
}

function legacyWriteExpertDescription(description: string): string {
  return `${FROZEN_TOOL_PREFIX} [Legacy/internal/debug/expert write] ${description} ${LEGACY_TOOL_GUIDANCE}`;
}

function preflightOnlyDescription(description: string): string {
  return `Preflight only. ${description} Use this only to start the explicit target editor before TaskSpec-first work. ${LEGACY_TOOL_GUIDANCE}`;
}

/** 将 Bridge 响应转换为 MCP tool result */
function toToolResult(resp: BridgeResponse, isError = false) {
  return {
    content: [{ type: 'text' as const, text: JSON.stringify(resp, null, 2) }],
    isError: isError || !resp.success,
  };
}

const responseModeSchema = z.enum([
  'summary_text',
  'structured_json',
  'resource_ref',
  'legacy_text_json',
]).optional();

const readContextInputSchema = z.object({
  schema: z.literal('BlueprintHelper.ReadSpec.v1'),
  read_type: z.enum([
    'asset_context',
    'blueprint_logic',
    'component_context',
    'variable_context',
    'graph_context',
    'widget_context',
    'data_table_context',
    'object_property_context',
  ]),
  target: z.object({
    asset_path: z.string(),
    asset_type: z.string().optional(),
    target_type: z.enum([
      'asset',
      'blueprint',
      'graph',
      'function',
      'event',
      'custom_event',
      'component',
      'member_variable',
      'event_dispatcher',
      'widget',
      'data_table_row',
      'block',
    ]).optional().default('blueprint'),
    target_name: z.string().optional(),
    block_id: z.string().optional(),
  }),
  view: z.object({
    format: z.enum(['logic_md', 'logic_json', 'summary', 'schema']).optional().default('logic_md'),
    max_items: z.number().int().positive().optional(),
    detail: z.enum(['brief', 'normal', 'full', 'debug']).optional(),
  }).optional().default({ format: 'logic_md' }),
  context: z.object({
    context_id: z.string().optional(),
    task_run_id: z.string().optional(),
  }).optional(),
});

const BlueprintLogicMdOutputSchema = z.object({
  format: z.literal('logic_md'),
  schema: z.literal('BlueprintHelper.LogicMd.v1'),
  assetPath: z.string(),
  graph: z.string().optional(),
  importable: z.boolean(),
  stats: z.record(z.unknown()).optional(),
  diagnostics: z.array(z.record(z.unknown())).optional(),
});

const BlueprintLogicJsonOutputSchema = z.object({
  format: z.literal('logic_json'),
  schema: z.literal('BlueprintHelper.LogicJson.v1'),
  assetPath: z.string(),
  graph: z.string().optional(),
  importable: z.boolean(),
  logic: z.unknown(),
  stats: z.record(z.unknown()).optional(),
  diagnostics: z.array(z.record(z.unknown())).optional(),
});

const BlueprintRawJsonRefOutputSchema = z.object({
  format: z.literal('raw_json_ref'),
  schema: z.literal('BlueprintHelper.RawJsonRef.v1'),
  assetPath: z.string(),
  graph: z.string().optional(),
  importable: z.boolean(),
  rawUri: z.string(),
  stats: z.record(z.unknown()).optional(),
});

const BlueprintRawJsonExportOutputSchema = BlueprintRawJsonRefOutputSchema
  .partial()
  .passthrough();

function makeLogicMdStructured(
  payload: Record<string, unknown>,
  targetBlueprint?: string,
  targetGraph?: string,
) {
  const assetPath = getStringField(payload, 'assetPath') ?? targetBlueprint;
  if (!assetPath) {
    return undefined;
  }

  return omitUndefined({
    format: 'logic_md',
    schema: 'BlueprintHelper.LogicMd.v1',
    assetPath,
    graph: getStringField(payload, 'graph') ?? targetGraph,
    importable: false,
    stats: getRecordField(payload, 'stats'),
    diagnostics: Array.isArray(payload['diagnostics']) ? payload['diagnostics'] : undefined,
  });
}

function makeLogicJsonStructured(
  payload: Record<string, unknown>,
  targetBlueprint?: string,
  targetGraph?: string,
) {
  const assetPath = getStringField(payload, 'assetPath') ?? targetBlueprint;
  if (!assetPath || payload['logic'] === undefined) {
    return undefined;
  }

  return omitUndefined({
    format: 'logic_json',
    schema: 'BlueprintHelper.LogicJson.v1',
    assetPath,
    graph: getStringField(payload, 'graph') ?? targetGraph,
    importable: false,
    logic: payload['logic'],
    stats: getRecordField(payload, 'stats'),
    diagnostics: Array.isArray(payload['diagnostics']) ? payload['diagnostics'] : undefined,
  });
}

function getRawJsonPayload(normalizedPayload: unknown) {
  if (!isRecord(normalizedPayload)) return normalizedPayload;
  // payload 是主要字段（object-first），json 是兼容回退
  if (normalizedPayload['payload'] !== undefined) return normalizedPayload['payload'];
  if (normalizedPayload['json'] !== undefined) return normalizedPayload['json'];
  return normalizedPayload;
}

function omitUndefined(input: Record<string, unknown>) {
  return Object.fromEntries(
    Object.entries(input).filter(([, value]) => value !== undefined),
  );
}

function toMarkdownToolResult(
  resp: BridgeResponse,
  markdown: string,
  structured?: Record<string, unknown>,
  mode: McpResponseMode = 'summary_text',
) {
  return buildBlueprintToolResult({
    mode,
    markdown,
    structured,
  });
}

/** 将 Bridge 错误转换为 MCP tool error result */
function toErrorResult(err: unknown) {
  const message = err instanceof Error ? err.message : String(err);
  return {
    content: [{ type: 'text' as const, text: `Bridge error: ${message}` }],
    isError: true,
  };
}

function resolvePluginResourcePath(relativePath: string): string {
  const cwd = process.cwd();
  const candidates = [
    cwd,
    path.resolve(cwd, '..'),
    path.resolve(cwd, 'Plugins', 'BlueprintHelper'),
    path.resolve(cwd, '..', 'Plugins', 'BlueprintHelper'),
  ];

  for (const root of candidates) {
    const candidate = path.resolve(root, relativePath);
    if (fs.existsSync(candidate)) {
      return candidate;
    }
  }

  throw new Error(`Unable to find ${relativePath} from ${cwd}`);
}

function readAgentGuideIndexMarkdown(): string {
  return fs.readFileSync(resolvePluginResourcePath(AGENT_GUIDE_INDEX_RELATIVE_PATH), 'utf8');
}

function makeReadTraceId(): string {
  return `trace_read_context_${Date.now()}`;
}

function stripBlueprintHelperPrefix(schema: unknown): unknown {
  if (typeof schema !== 'string') return schema;
  return schema.startsWith('BlueprintHelper.') ? schema.slice('BlueprintHelper.'.length) : schema;
}

function normalizeReadPayloadSchema(payload: Record<string, unknown>): Record<string, unknown> {
  return {
    ...payload,
    schema: stripBlueprintHelperPrefix(payload['schema']),
  };
}

function buildReadContextTarget(target: z.infer<typeof readContextInputSchema>['target']) {
  return omitUndefined({
    asset_path: target.asset_path,
    asset_type: target.asset_type,
    target_type: target.target_type,
    target_name: target.target_name,
    block_id: target.block_id,
  });
}

function inferBlueprintLogicScope(targetType: string): string {
  if (targetType === 'blueprint' || targetType === 'asset') return 'blueprint';
  if (targetType === 'function') return 'target_function';
  if (targetType === 'event') return 'target_event';
  if (targetType === 'custom_event') return 'target_custom_event';
  return 'target_graph';
}

function buildBlueprintLogicReadPayload(input: z.infer<typeof readContextInputSchema>) {
  const payload: Record<string, unknown> = {
    asset_path: input.target.asset_path,
  };

  const targetName = input.target.target_name;
  switch (input.target.target_type) {
    case 'graph':
      if (targetName) payload['graph'] = targetName;
      break;
    case 'function':
      if (targetName) payload['function'] = targetName;
      break;
    case 'event':
    case 'custom_event':
      if (targetName) payload['event'] = targetName;
      break;
    case 'block':
      if (input.target.block_id ?? targetName) payload['block_id'] = input.target.block_id ?? targetName;
      break;
  }

  payload['scope'] = inferBlueprintLogicScope(input.target.target_type);
  return payload;
}

function makeBlueprintLogicSchemaPayload() {
  return {
    schema: 'BlueprintLogicReadSchema.v1',
    read_type: 'blueprint_logic',
    target_types: ['blueprint', 'graph', 'function', 'event', 'custom_event', 'block'],
    formats: ['logic_md', 'logic_json', 'summary', 'schema'],
    target_name_semantics: {
      graph: 'Graph name',
      function: 'Function name',
      event: 'Event name',
      custom_event: 'Custom Event name',
    },
  };
}

function toReadContextMcpResult(input: {
  target: z.infer<typeof readContextInputSchema>['target'];
  readType: string;
  format: string;
  payload: Record<string, unknown>;
  status?: string;
  ok?: boolean;
}) {
  const ok = input.ok ?? true;
  const payload = normalizeReadPayloadSchema(input.payload);
  const scope = typeof payload['scope'] === 'string'
    ? payload['scope']
    : inferBlueprintLogicScope(input.target.target_type);
  const structured = {
    ok,
    schema: 'BlueprintHelper.McpToolResult.v1',
    operation: 'read_context',
    trace_id: makeReadTraceId(),
    status: input.status ?? (ok ? 'completed' : 'failed'),
    modified: false,
    target: buildReadContextTarget(input.target),
    data: {
      schema: 'ReadContextPack.v1',
      read_type: input.readType,
      format: input.format,
      scope,
      payload,
      stats: isRecord(payload['stats']) ? payload['stats'] : {},
      truncated: false,
      large_payload_ref: null,
    },
  };

  return {
    content: [
      {
        type: 'text' as const,
        text: `read_context ${structured.status}: ${input.target.asset_path}, read_type=${input.readType}, format=${input.format}.`,
      },
    ],
    isError: !ok,
    structuredContent: structured,
  };
}

function extractReadPayloadFromBridgeResult(result: unknown): {
  ok: boolean;
  status?: string;
  payload: Record<string, unknown>;
} {
  const normalized = normalizeBlueprintPayload(result);
  const raw = isRecord(normalized) ? normalized : {};
  const ok = typeof raw['ok'] === 'boolean' ? raw['ok'] : true;
  const data = isRecord(raw['data']) ? raw['data'] : raw;
  return {
    ok,
    status: typeof raw['status'] === 'string' ? raw['status'] : undefined,
    payload: data,
  };
}

export function registerTools(server: McpServer, bridge: BridgeClient, config: EditorConfig): void {
  registerTaskTools(server, bridge, config);

  // ─── 1. read_agent_guide ───
  server.registerTool(
    'blueprinthelper_read_agent_guide',
    {
      description: 'Read the BlueprintHelper AgentGuide onboarding index Markdown. Use this as the Agent-facing entry for supported capability surface and schema guide paths.',
      inputSchema: z.object({}),
    },
    async () => {
      try {
        return {
          content: [{ type: 'text' as const, text: readAgentGuideIndexMarkdown() }],
        };
      } catch (err) {
        const message = err instanceof Error ? err.message : String(err);
        return {
          content: [{ type: 'text' as const, text: `AgentGuide error: ${message}` }],
          isError: true,
        };
      }
    },
  );

  server.registerTool(
    'blueprinthelper_read_context',
    {
      description: 'Read UE asset context through BlueprintHelper.ReadSpec.v1. First slice supports blueprint_logic with logic_md, logic_json, summary, and schema formats.',
      inputSchema: readContextInputSchema,
    },
    async (input) => {
      try {
        if (input.read_type !== 'blueprint_logic') {
          return toMcpResult(normalizeToolResult(
            {
              request_id: 'read_context',
              success: false,
              error_code: 'unsupported_read_type',
              message: `read_context currently supports blueprint_logic only, got ${input.read_type}.`,
            },
            'read_context',
          ));
        }

        const format = input.view?.format ?? 'logic_md';
        if (format === 'schema') {
          return toReadContextMcpResult({
            target: input.target,
            readType: input.read_type,
            format,
            payload: makeBlueprintLogicSchemaPayload(),
          });
        }

        const bridgeFormat = format === 'logic_json' ? 'logic_json' : 'logic_md';
        const command = bridgeFormat === 'logic_json'
          ? 'read_blueprint_logic_json'
          : 'read_blueprint_logic_md';
        const payload = buildBlueprintLogicReadPayload(input);

        const resp = await bridge.sendCommand(command, payload);
        if (!resp.success) {
          return toMcpResult(normalizeToolResult(resp, 'read_context', {
            target: buildReadContextTarget(input.target) as never,
            modified: false,
          }));
        }

        const readPayload = extractReadPayloadFromBridgeResult(resp.result);
        return toReadContextMcpResult({
          target: input.target,
          readType: input.read_type,
          format,
          payload: readPayload.payload,
          status: readPayload.status,
          ok: readPayload.ok,
        });
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 2. get_editor_context ───
  server.registerTool(
    'blueprint_get_editor_context',
    {
      description: legacyDebugExpertDescription('Get the current Unreal Editor context: active blueprint, graph, node count, compile status.'),
      inputSchema: z.object({}),
    },
    async () => {
      try {
        const resp = await bridge.sendCommand('get_editor_context');
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 2.5. get_runtime_profile ───
  server.registerTool(
    'blueprint_get_runtime_profile',
    {
      description:
        '获取当前 BlueprintHelper 运行时事实：版本、Bridge 连接状态、配置状态、写权限、风险命令状态、当前安全档位、不可用能力列表。Agent 应在每次会话开始时调用此工具以了解当前环境能力。',
      inputSchema: z.object({}),
    },
    async () => {
      try {
        const resp = await bridge.sendCommand('get_runtime_profile');
        if (!resp.success) {
          return toToolResult(resp);
        }

        // 尝试解析 UE 侧已序列化的 ToolResultBase
        const raw = resp.result as Record<string, unknown> | undefined;
        if (raw && typeof raw['ok'] === 'boolean') {
          const data = raw['data'] as Record<string, unknown> | undefined;
          const version = typeof data?.['version'] === 'string' ? data['version'] : 'unknown';
          // UE 侧已经返回了 ToolResultBase JSON，直接使用
          return {
            content: [{ type: 'text' as const, text: `get_runtime_profile ${raw['status'] ?? 'completed'}: version=${version}, modified=${raw['modified'] ?? false}.` }],
            isError: !raw['ok'],
            structuredContent: raw as Record<string, unknown>,
          };
        }

        // 回退：用 normalizeToolResult 标准化
        const result = normalizeToolResult(resp, 'get_runtime_profile');
        return toMcpResult(result);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 2.6. diagnostics (Static) ───
  server.registerTool(
    'blueprinthelper_diagnostics',
    {
      description:
        '执行静态诊断检查（不要求 UE Editor 运行）。检查 settings.json、CLAUDE.md managed block、Skill 入口、项目目录结构等安装/配置状态。返回 Markdown 诊断报告。',
      inputSchema: z.object({}),
    },
    async () => {
      try {
        const report: DiagnosticsMarkdownReport = {
          blocking: [],
          warnings: [],
          info: [],
        };

        // ═══════ Static 检查 ═══════

        // 检查 settings.json
        // settings.json 应该由 setup profile 生成，这里只检查是否存在
        const settingsPath = path.join(
          process.cwd(),
          '.blueprinthelper',
          'settings.json',
        );
        if (fs.existsSync(settingsPath)) {
          report.info.push({ code: 'settings.valid' });
        } else {
          report.blocking.push({
            code: 'settings.unavailable',
            extra: 'reason: .blueprinthelper/settings.json not found',
          });
        }

        // 检查 CLAUDE.md（Global guidance）
        const globalClaudePath = path.join(
          process.env['USERPROFILE'] ?? process.env['HOME'] ?? '',
          '.claude',
          'CLAUDE.md',
        );
        if (fs.existsSync(globalClaudePath)) {
          report.info.push({ code: 'global_guidance.present' });
        } else {
          report.blocking.push({ code: 'global_guidance.missing' });
        }

        // 检查 Skill 入口
        const skillPath = path.join(
          process.env['USERPROFILE'] ?? process.env['HOME'] ?? '',
          '.claude',
          'skills',
          'blueprinthelper',
          'SKILL.md',
        );
        if (fs.existsSync(skillPath)) {
          report.info.push({ code: 'skill_entry.valid' });
        } else {
          report.warnings.push({ code: 'skill_entry.invalid' });
        }

        // 检查项目目录结构（是否像 UE 项目）
        const projectDir = process.cwd();
        const uprojectFiles = fs
          .readdirSync(projectDir)
          .filter((f) => f.endsWith('.uproject'));
        if (uprojectFiles.length > 0) {
          report.info.push({ code: 'project_structure.valid' });
          // 检查 Project Marker
          const projectClaudePath = path.join(projectDir, '.claude', 'CLAUDE.md');
          const projectAgentsPath = path.join(projectDir, 'AGENTS.md');
          const projectProfilePath = path.join(
            projectDir,
            '.blueprinthelper',
            'agent-profile.json',
          );
          if (
            fs.existsSync(projectClaudePath) ||
            fs.existsSync(projectAgentsPath) ||
            fs.existsSync(projectProfilePath)
          ) {
            report.info.push({ code: 'project_marker.present' });
          } else {
            report.warnings.push({ code: 'project_marker.missing' });
          }
        } else {
          report.warnings.push({
            code: 'project_structure.invalid',
            extra: 'reason: no .uproject found in current directory',
          });
        }

        // 检查版本
        const packageJsonPath = path.join(
          process.cwd(),
          'plugins',
          'BlueprintHelper_MCP_Server',
          'package.json',
        );
        let mcpVersion = 'unknown';
        if (fs.existsSync(packageJsonPath)) {
          try {
            const pkg = JSON.parse(fs.readFileSync(packageJsonPath, 'utf-8'));
            mcpVersion = pkg.version ?? 'unknown';
            report.info.push({ code: 'version.match' });
          } catch {
            report.warnings.push({ code: 'version.invalid' });
          }
        } else {
          report.warnings.push({ code: 'version.invalid' });
        }

        const markdown = buildDiagnosticsMarkdown(report);
        const data = buildDiagnosticsData('static', markdown);

        return {
          content: [
            {
              type: 'text' as const,
              text: `diagnostics static: blocking=${report.blocking.length}, warnings=${report.warnings.length}, info=${report.info.length}.`,
            },
          ],
          isError: false,
          structuredContent: {
            ok: true,
            schema: 'BlueprintHelper.McpToolResult.v1',
            operation: 'diagnostics',
            trace_id: `trace_diag_static_${Date.now()}`,
            status: 'completed',
            modified: false,
            data,
          },
        };
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 2.7. diagnostics_runtime ───
  server.registerTool(
    'blueprinthelper_diagnostics_runtime',
    {
      description:
        'Run BlueprintHelper runtime diagnostics when the UE Editor Bridge is reachable. Checks editor status, Bridge connectivity, runtime profile, write permission, risk-command state, and project markers. Read-only Agent-facing diagnostic tool.',
      inputSchema: z.object({}),
    },
    async () => {
      try {
        const resp = await bridge.sendCommand('diagnostics_runtime');
        if (!resp.success) {
          return toToolResult(resp);
        }

        // 尝试解析 UE 侧已序列化的 ToolResultBase
        const raw = resp.result as Record<string, unknown> | undefined;
        if (raw && typeof raw['ok'] === 'boolean') {
          const data = raw['data'] as Record<string, unknown> | undefined;
          const mode = data?.['mode'] ?? 'runtime';
          return {
            content: [
              {
                type: 'text' as const,
                text: `diagnostics ${mode}: ok=${raw['ok']}, status=${raw['status'] ?? 'completed'}.`,
              },
            ],
            isError: !raw['ok'],
            structuredContent: raw as Record<string, unknown>,
          };
        }

        // 回退：用 normalizeToolResult 标准化
        const result = normalizeToolResult(resp, 'diagnostics_runtime');
        return toMcpResult(result);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 2.8. get_logic_md ───
  server.registerTool(
    'blueprint_get_logic_md',
    {
      description:
        legacyDebugExpertDescription('以低 Token 成本的 Markdown 格式阅读蓝图逻辑。旧版直接蓝图理解工具。指定 asset_path 和目标（graph/function/event/block_id）即可以人类可读格式获取蓝图执行流和数据依赖。不可用于导入。'),
      inputSchema: z.object({
        asset_path: z.string()
          .describe('蓝图资产路径，例如 /Game/BP/BP_Player.BP_Player。'),
        graph: z.string().optional()
          .describe('目标图表名，例如 EventGraph、EG_PhysicsDoor。默认使用 EventGraph。'),
        function: z.string().optional()
          .describe('目标函数名。与 graph 互斥，指定此参数表示读取特定函数。'),
        event: z.string().optional()
          .describe('目标事件名。与 graph/function 互斥，指定此参数表示读取特定事件。'),
        block_id: z.string().optional()
          .describe('BlueprintHelper-owned block ID。与上述互斥，指定此参数表示读取特定 block。'),
        scope: z.enum(['blueprint', 'target_graph', 'target_function', 'target_event']).optional()
          .describe('覆盖自动推断的作用域。blueprint=全蓝图、target_graph=单图表、target_function=单函数、target_event=单事件。'),
      }),
    },
    async ({ asset_path, graph, function: func, event, block_id, scope }) => {
      try {
        const payload: Record<string, unknown> = { asset_path };
        if (graph) payload['graph'] = graph;
        if (func) payload['function'] = func;
        if (event) payload['event'] = event;
        if (block_id) payload['block_id'] = block_id;
        if (scope) payload['scope'] = scope;

        const resp = await bridge.sendCommand('read_blueprint_logic_md', payload);
        if (!resp.success) {
          return toToolResult(resp);
        }

        // 尝试解析 UE 侧已序列化的 ToolResultBase
        const raw = resp.result as Record<string, unknown> | undefined;
        if (raw && typeof raw['ok'] === 'boolean') {
          const data = raw['data'] as Record<string, unknown> | undefined;
          const targetInfo = raw['target'] as Record<string, unknown> | undefined;
          const assetInfo = targetInfo?.['asset_path'] ?? asset_path;
          const graphInfo = targetInfo?.['graph'] ?? graph ?? '';
          return {
            content: [
              {
                type: 'text' as const,
                text: `read_blueprint_logic_md ${raw['status'] ?? 'completed'}: ${assetInfo}${graphInfo ? '.' + graphInfo : ''}, scope=${data?.['scope'] ?? 'target_graph'}.`,
              },
            ],
            isError: !raw['ok'],
            structuredContent: raw as Record<string, unknown>,
          };
        }

        // 回退
        const result = normalizeToolResult(resp, 'read_blueprint_logic_md_by_target');
        return toMcpResult(result);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 2.10. create_asset ───
  server.registerTool(
    'blueprint_create_asset',
    {
      description:
        legacyWriteExpertDescription('创建 UE 资产（Blueprint Class、Interface、Structure、Input Action、Input Mapping Context、DataAsset、DataTable）。支持冲突策略 fail_if_exists（默认）和 reuse_if_exists。返回标准 ToolResultBase。'),
      inputSchema: z.object({
        asset_path: z.string()
          .describe('资产路径，例如 /Game/Blueprints/BP_MyActor。'),
        asset_type: z.enum([
          'blueprint_class',
          'blueprint_interface',
          'structure',
          'input_action',
          'input_mapping_context',
          'data_asset',
        ]).describe('要创建的资产类型。'),
        parent_class: z.string().optional()
          .describe('父类名称（仅 blueprint_class 需要），例如 Actor、Pawn、Character。默认 Actor。'),
        value_type: z.string().optional()
          .describe('Input Action 值类型（仅 input_action 需要）：bool（默认）、axis1d、axis2d、axis3d。'),
        collision: z.enum(['fail_if_exists', 'reuse_if_exists']).optional()
          .describe('冲突策略。fail_if_exists=已存在则失败（默认），reuse_if_exists=同类型则复用已有资产。'),
      }),
    },
    async ({ asset_path, asset_type, parent_class, value_type, collision }) => {
      try {
        const payload: Record<string, unknown> = { asset_path, asset_type };
        if (parent_class) payload['parent_class'] = parent_class;
        if (value_type) payload['value_type'] = value_type;
        if (collision) payload['collision'] = collision;

        const resp = await bridge.sendCommand('create_asset', payload);

        // 尝试解析 UE 侧已序列化的 ToolResultBase
        const raw = resp.result as Record<string, unknown> | undefined;
        if (raw && typeof raw['ok'] === 'boolean') {
          const targetInfo = raw['target'] as Record<string, unknown> | undefined;
          const data = raw['data'] as Record<string, unknown> | undefined;
          const asset = data?.['asset'] as Record<string, unknown> | undefined;
          const created = asset?.['created'] ?? false;
          const existed = asset?.['already_existed'] ?? false;
          return {
            content: [
              {
                type: 'text' as const,
                text: `create_asset ${raw['status'] ?? 'applied'}: ${targetInfo?.['asset_path'] ?? asset_path}, type=${asset_type}, created=${created}, already_existed=${existed}.`,
              },
            ],
            isError: !raw['ok'],
            structuredContent: raw as Record<string, unknown>,
          };
        }

        if (!resp.success) {
          return toToolResult(resp);
        }

        const result = normalizeToolResult(resp, 'create_asset');
        return toMcpResult(result);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 2.11. read_components ───
  server.registerTool(
    'blueprint_read_components',
    {
      description: legacyDebugExpertDescription('读取蓝图组件树（SCS），返回组件名、类名、父组件、子组件列表和统计。不修改资产。'),
      inputSchema: z.object({
        asset_path: z.string().describe('蓝图资产路径，例如 /Game/BP/BP_Player.BP_Player。'),
      }),
    },
    async ({ asset_path }) => {
      try {
        const resp = await bridge.sendCommand('read_components', { asset_path });
        const raw = resp.result as Record<string, unknown> | undefined;
        if (raw && typeof raw['ok'] === 'boolean') {
          return { content: [{ type: 'text' as const, text: `read_components ${raw['status'] ?? 'completed'}: ${asset_path}, components=${((raw['data'] as Record<string, unknown>|undefined)?.['stats'] as Record<string, unknown>|undefined)?.['components'] ?? '?'}.` }], isError: !raw['ok'], structuredContent: raw as Record<string, unknown> };
        }
        if (!resp.success) return toToolResult(resp);
        const result = normalizeToolResult(resp, 'read_components');
        return toMcpResult(result);
      } catch (err) { return toErrorResult(err); }
    },
  );

  // ─── 2.12. add_component ───
  server.registerTool(
    'blueprint_add_component',
    {
      description: legacyWriteExpertDescription('向蓝图添加组件。只负责创建+挂接，不设置属性。Transform/Collision/Physics 通过 set_component_property 设置。'),
      inputSchema: z.object({
        asset_path: z.string().describe('蓝图资产路径。'),
        component_name: z.string().describe('组件名。'),
        component_class: z.string().describe('组件类，例如 StaticMeshComponent、/Script/Engine.SkeletalMeshComponent。'),
        parent_component: z.string().optional().describe('父组件名。省略则挂到根组件。'),
        socket_name: z.string().optional().describe('挂接的 Socket 名。'),
        attach_rule: z.enum(['keep_relative', 'snap_to_target']).optional().describe('挂接规则。默认 keep_relative。'),
        name_collision_policy: z.enum(['fail_if_exists', 'reuse_if_exists']).optional().describe('名称冲突策略。默认 fail_if_exists。'),
      }),
    },
    async ({ asset_path, component_name, component_class, parent_component, socket_name, attach_rule, name_collision_policy }) => {
      try {
        const payload: Record<string, unknown> = { asset_path, component_name, component_class };
        if (parent_component) payload['parent_component'] = parent_component;
        if (socket_name) payload['socket_name'] = socket_name;
        if (attach_rule) payload['attach_rule'] = attach_rule;
        if (name_collision_policy) payload['name_collision_policy'] = name_collision_policy;
        const resp = await bridge.sendCommand('add_component', payload);
        const raw = resp.result as Record<string, unknown> | undefined;
        if (raw && typeof raw['ok'] === 'boolean') {
          const comp = (raw['data'] as Record<string, unknown>|undefined)?.['component'] as Record<string, unknown>|undefined;
          return { content: [{ type: 'text' as const, text: `add_component ${raw['status'] ?? 'applied'}: ${component_name}(${component_class}), parent=${parent_component ?? 'root'}, created=${comp?.['created'] ?? false}.` }], isError: !raw['ok'], structuredContent: raw as Record<string, unknown> };
        }
        if (!resp.success) return toToolResult(resp);
        const result = normalizeToolResult(resp, 'add_component');
        return toMcpResult(result);
      } catch (err) { return toErrorResult(err); }
    },
  );

  // ─── 2.13. set_component_property ───
  server.registerTool(
    'blueprint_set_component_property',
    {
      description: legacyWriteExpertDescription('设置单个组件属性。支持 string/bool/number 值，支持点路径如 BodyInstance.bSimulatePhysics。'),
      inputSchema: z.object({
        asset_path: z.string().describe('蓝图资产路径。'),
        component_name: z.string().describe('组件名。'),
        property_path: z.string().describe('属性路径，例如 Mobility、RelativeLocation.X、BodyInstance.bSimulatePhysics。'),
        value: z.union([z.string(), z.boolean(), z.number()]).describe('属性值。'),
      }),
    },
    async ({ asset_path, component_name, property_path, value }) => {
      try {
        const resp = await bridge.sendCommand('set_component_property', { asset_path, component_name, property_path, value });
        const raw = resp.result as Record<string, unknown> | undefined;
        if (raw && typeof raw['ok'] === 'boolean') {
          const pr = (raw['data'] as Record<string, unknown>|undefined)?.['property_result'] as Record<string, unknown>|undefined;
          return { content: [{ type: 'text' as const, text: `set_component_property ${raw['status'] ?? 'applied'}: ${component_name}.${property_path}, applied=${pr?.['applied_count'] ?? 0}, changed=${pr?.['changed_count'] ?? 0}.` }], isError: !raw['ok'], structuredContent: raw as Record<string, unknown> };
        }
        if (!resp.success) return toToolResult(resp);
        const result = normalizeToolResult(resp, 'set_component_property');
        return toMcpResult(result);
      } catch (err) { return toErrorResult(err); }
    },
  );

  // ─── 2.14. set_component_properties ───
  server.registerTool(
    'blueprint_set_component_properties',
    {
      description: legacyWriteExpertDescription('批量设置组件属性。事务式：任一属性验证失败则不应用任何属性。'),
      inputSchema: z.object({
        asset_path: z.string().describe('蓝图资产路径。'),
        component_name: z.string().describe('组件名。'),
        settings: z.array(z.object({
          property_path: z.string().describe('属性路径。'),
          value: z.union([z.string(), z.boolean(), z.number()]).describe('属性值。'),
        })).describe('属性设置数组。'),
      }),
    },
    async ({ asset_path, component_name, settings }) => {
      try {
        const resp = await bridge.sendCommand('set_component_properties', { asset_path, component_name, settings });
        const raw = resp.result as Record<string, unknown> | undefined;
        if (raw && typeof raw['ok'] === 'boolean') {
          const pr = (raw['data'] as Record<string, unknown>|undefined)?.['property_result'] as Record<string, unknown>|undefined;
          return { content: [{ type: 'text' as const, text: `set_component_properties ${raw['status'] ?? 'applied'}: ${component_name}, requested=${pr?.['requested_count'] ?? 0}, applied=${pr?.['applied_count'] ?? 0}, changed=${pr?.['changed_count'] ?? 0}, invalid=${(pr?.['invalid_settings'] as unknown[])?.length ?? 0}.` }], isError: !raw['ok'], structuredContent: raw as Record<string, unknown> };
        }
        if (!resp.success) return toToolResult(resp);
        const result = normalizeToolResult(resp, 'set_component_properties');
        return toMcpResult(result);
      } catch (err) { return toErrorResult(err); }
    },
  );

  // ─── 2.15. remove_component ───
  server.registerTool(
    'blueprint_remove_component',
    {
      description: legacyWriteExpertDescription('删除蓝图中的组件。要求明确的 component_name。第一版不删除 DefaultSceneRoot 或带子组件的组件。'),
      inputSchema: z.object({
        asset_path: z.string().describe('蓝图资产路径。'),
        component_name: z.string().describe('要删除的组件名。'),
      }),
    },
    async ({ asset_path, component_name }) => {
      try {
        const resp = await bridge.sendCommand('remove_component', { asset_path, component_name });
        const raw = resp.result as Record<string, unknown> | undefined;
        if (raw && typeof raw['ok'] === 'boolean') {
          const comp = (raw['data'] as Record<string, unknown>|undefined)?.['component'] as Record<string, unknown>|undefined;
          return { content: [{ type: 'text' as const, text: `remove_component ${raw['status'] ?? 'applied'}: ${component_name}, removed=${comp?.['removed'] ?? false}.` }], isError: !raw['ok'], structuredContent: raw as Record<string, unknown> };
        }
        if (!resp.success) return toToolResult(resp);
        const result = normalizeToolResult(resp, 'remove_component');
        return toMcpResult(result);
      } catch (err) { return toErrorResult(err); }
    },
  );

  // ─── 3. validate_json ───
  server.registerTool(
    'blueprint_validate_json',
    {
      description: legacyDebugExpertDescription('Pre-validate a JSON string against BlueprintHelper import rules before importing.'),
      inputSchema: z.object({
        json: z.string().describe('The JSON string to validate'),
      }),
    },
    async ({ json }) => {
      try {
        const resp = await bridge.sendCommand('validate_json', { json });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 4. export_to_json ───
  server.registerTool(
    'blueprint_export_to_json',
    {
      description: legacyDebugExpertDescription('Export a blueprint graph to raw BlueprintHelper JSON that can be written back or replayed with blueprint_import_json_to_graph. Use blueprint_get_logic for read-only logic summaries.'),
      inputSchema: z.object({
        target_blueprint: z.string().optional()
          .describe('Blueprint asset path, e.g. /Game/BP/BP_Test.BP_Test. Omit to use the active blueprint.'),
        target_graph: z.string().optional()
          .describe('Graph name to export. Omit to use the active graph.'),
        scope: z.enum(['graph', 'blueprint', 'selection', 'full_graph', 'full_blueprint']).optional().default('graph')
          .describe('Export scope: graph, blueprint, or selection. Legacy full_graph/full_blueprint are accepted.'),
        response_mode: responseModeSchema
          .describe('MCP response mode. Default returns a RawJson resource link; legacy_text_json returns inline JSON text.'),
      }),
      outputSchema: BlueprintRawJsonExportOutputSchema,
    },
    async ({ target_blueprint, target_graph, scope, response_mode }) => {
      try {
        const payload: Record<string, unknown> = {};
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        if (target_graph) payload.target_graph = target_graph;
        if (scope) payload.scope = scope;
        const resp = await bridge.sendCommand('export_to_json', payload);
        if (!resp.success) {
          return toToolResult(resp);
        }

        const normalizedPayload = normalizeBlueprintPayload(resp.result);
        const rawJsonPayload = getRawJsonPayload(normalizedPayload);
        const normalizedRecord = isRecord(normalizedPayload) ? normalizedPayload : undefined;
        const rawRecord = isRecord(rawJsonPayload) ? rawJsonPayload : undefined;
        const assetPath =
          target_blueprint ??
          getStringField(normalizedRecord, 'assetPath') ??
          getStringField(normalizedRecord, 'target_blueprint');
        const graph = target_graph ?? getStringField(normalizedRecord, 'graph');
        const mode = resolveResponseMode(response_mode, assetPath ? 'resource_ref' : 'legacy_text_json');

        if (mode === 'legacy_text_json' || !assetPath) {
          const jsonPayload = isRecord(rawJsonPayload) ? rawJsonPayload : rawJsonPayload;
          const structured: Record<string, unknown> = {
            format: 'raw_json',
            schema: 'BlueprintHelper.RawJsonRef.v1',
            importable: true,
            json: jsonPayload,
            ...(assetPath ? { assetPath } : {}),
            ...(graph ? { graph } : {}),
          };
          return buildBlueprintToolResult({
            mode: 'legacy_text_json',
            structured,
          });
        }

        const rawUri = makeBlueprintResourceUri({
          assetPath,
          graph,
          view: 'raw-json',
        });
        return buildBlueprintToolResult({
          mode,
          summary: 'RawJson is available as a resource. Use only for debugging, compatibility, or replay.',
          structured: omitUndefined({
            format: 'raw_json_ref',
            schema: 'BlueprintHelper.RawJsonRef.v1',
            assetPath,
            graph,
            importable: true,
            rawUri,
            stats: getRecordField(rawRecord, 'stats') ?? getRecordField(normalizedRecord, 'stats'),
            json: isRecord(rawJsonPayload) ? rawJsonPayload : rawJsonPayload,
          }),
          resourceLinks: [
            {
              uri: rawUri,
              name: `${assetPath} RawJson`,
              description: 'Full raw BlueprintHelper JSON export.',
              mimeType: 'application/json',
            },
          ],
        });
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 5. get_logic ───
  server.registerTool(
    'blueprint_get_logic',
    {
      description: legacyDebugExpertDescription('Read the current Blueprint logic as Markdown. This is for understanding and review only; it is not importable raw BlueprintHelper JSON.'),
      inputSchema: z.object({
        target_blueprint: z.string().optional()
          .describe('Blueprint asset path, e.g. /Game/BP/BP_Test.BP_Test. Omit to use the active blueprint.'),
        target_graph: z.string().optional()
          .describe('Graph name to inspect. Omit to use the active graph.'),
        scope: z.enum(['single_graph', 'full_blueprint']).optional().default('single_graph')
          .describe('Logic read scope: single_graph or full_blueprint'),
        detail: z.enum(['brief', 'normal', 'full', 'debug']).optional().default('normal')
          .describe('Detail level for the logic summary'),
        include_data_dependencies: z.boolean().optional().default(true)
          .describe('Whether to include data dependencies in the logic summary'),
        include_orphans: z.boolean().optional().default(true)
          .describe('Whether to include orphan nodes in the logic summary'),
        response_mode: responseModeSchema
          .describe('MCP response mode. Default returns Markdown text and structured metadata.'),
      }),
      outputSchema: BlueprintLogicMdOutputSchema,
    },
    async ({
      target_blueprint,
      target_graph,
      scope,
      detail,
      include_data_dependencies,
      include_orphans,
      response_mode,
    }) => {
      try {
        const payload: Record<string, unknown> = {
          format: 'logic_md',
          scope: scope ?? 'single_graph',
          detail: detail ?? 'normal',
          include_data_dependencies: include_data_dependencies ?? true,
          include_orphans: include_orphans ?? true,
        };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        if (target_graph) payload.target_graph = target_graph;

        const resp = await bridge.sendCommand('export_logic', payload);
        const normalizedPayload = normalizeBlueprintPayload(resp.result);
        const payloadRecord = isRecord(normalizedPayload) ? normalizedPayload : undefined;
        const markdown = getStringField(payloadRecord, 'markdown');
        if (resp.success && typeof markdown === 'string' && payloadRecord) {
          const mode = resolveResponseMode(response_mode, 'summary_text');
          return toMarkdownToolResult(
            resp,
            markdown,
            makeLogicMdStructured(payloadRecord, target_blueprint, target_graph),
            mode,
          );
        }
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 6. get_logic_json ───
  server.registerTool(
    'blueprint_get_logic_json',
    {
      description: legacyDebugExpertDescription('Read the current Blueprint logic as structured logic JSON for analysis. This is not raw BlueprintHelper JSON and must not be passed directly to blueprint_import_json_to_graph.'),
      inputSchema: z.object({
        target_blueprint: z.string().optional()
          .describe('Blueprint asset path, e.g. /Game/BP/BP_Test.BP_Test. Omit to use the active blueprint.'),
        target_graph: z.string().optional()
          .describe('Graph name to inspect. Omit to use the active graph.'),
        scope: z.enum(['single_graph', 'full_blueprint']).optional().default('single_graph')
          .describe('Logic read scope: single_graph or full_blueprint'),
        detail: z.enum(['brief', 'normal', 'full', 'debug']).optional().default('normal')
          .describe('Detail level for the logic JSON'),
        include_data_dependencies: z.boolean().optional().default(true)
          .describe('Whether to include data dependencies in the logic JSON'),
        include_orphans: z.boolean().optional().default(true)
          .describe('Whether to include orphan nodes in the logic JSON'),
        include_node_ids: z.boolean().optional().default(false)
          .describe('Whether to include stable node identifiers where available'),
        include_positions: z.boolean().optional().default(false)
          .describe('Whether to include graph node positions'),
        include_raw_node_types: z.boolean().optional().default(false)
          .describe('Whether to include raw Unreal node type names'),
        response_mode: responseModeSchema
          .describe('MCP response mode. Default returns LogicJson in structuredContent with text summary.'),
      }),
      outputSchema: BlueprintLogicJsonOutputSchema,
    },
    async ({
      target_blueprint,
      target_graph,
      scope,
      detail,
      include_data_dependencies,
      include_orphans,
      include_node_ids,
      include_positions,
      include_raw_node_types,
      response_mode,
    }) => {
      try {
        const payload: Record<string, unknown> = {
          format: 'logic_json',
          scope: scope ?? 'single_graph',
          detail: detail ?? 'normal',
          include_data_dependencies: include_data_dependencies ?? true,
          include_orphans: include_orphans ?? true,
          include_node_ids: include_node_ids ?? false,
          include_positions: include_positions ?? false,
          include_raw_node_types: include_raw_node_types ?? false,
        };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        if (target_graph) payload.target_graph = target_graph;

        const resp = await bridge.sendCommand('export_logic', payload);
        if (!resp.success) {
          return toToolResult(resp);
        }

        const normalizedPayload = normalizeBlueprintPayload(resp.result);
        const payloadRecord = isRecord(normalizedPayload) ? normalizedPayload : undefined;
        if (!payloadRecord) {
          return toToolResult(resp);
        }

        const structured = makeLogicJsonStructured(payloadRecord, target_blueprint, target_graph);
        if (!structured) {
          return toToolResult(resp);
        }

        const stats = getRecordField(structured, 'stats');
        const nodes = stats?.['nodes'] ?? 'unknown';
        const assetPath = getStringField(structured, 'assetPath') ?? target_blueprint ?? 'unknown asset';
        const graphName = getStringField(structured, 'graph') ?? target_graph;
        const suffix = graphName ? `.${graphName}` : '';
        const mode = resolveResponseMode(response_mode, 'structured_json');

        return buildBlueprintToolResult({
          mode,
          summary: `Exported LogicJson: ${assetPath}${suffix}, nodes=${nodes}.`,
          structured,
        });
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 7. import_json_to_graph ───
  server.registerTool(
    'blueprint_import_json_to_graph',
    {
      description: legacyWriteExpertDescription('Import JSON into a blueprint graph, creating nodes and connections. Accepts structured RawJson objects or legacy JSON strings. Rejects LogicJson/LogicMD read-only views.'),
      inputSchema: z.object({
        json: rawJsonInputSchema,
        target_blueprint: z.string().optional()
          .describe('Target blueprint asset path. Omit to use the active blueprint.'),
        target_graph: z.string().optional()
          .describe('Target graph name. Omit to use the active graph.'),
        compile_after_import: z.boolean().optional().default(true)
          .describe('Whether to compile the blueprint after import'),
        strict: z.boolean().optional().default(true)
          .describe('When true, any import error, no-op, partial node generation, or link failure rolls back the transaction'),
        allow_partial: z.boolean().optional().default(false)
          .describe('Only meaningful with strict=false; explicitly allows partial success to remain applied'),
      }),
    },
    async ({ json, target_blueprint, target_graph, compile_after_import, strict, allow_partial }) => {
      try {
        // Pre-check: reject read-only views before Bridge call
        if (isRecord(json)) {
          const schema = typeof json['schema'] === 'string' ? json['schema'] : '';
          if (schema.startsWith('BlueprintHelper.Logic')) {
            return {
              content: [{ type: 'text' as const, text: 'LogicJson/LogicMD are read-only views and cannot be imported as RawJson.' }],
              isError: true,
            };
          }
          if (json['importable'] === false) {
            return {
              content: [{ type: 'text' as const, text: 'This JSON view is marked importable=false and cannot be imported.' }],
              isError: true,
            };
          }
        }

        const payload: Record<string, unknown> = {
          json,
          compile_after_import,
          strict: strict ?? true,
          allow_partial: allow_partial ?? false,
        };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        if (target_graph) payload.target_graph = target_graph;
        const resp = await bridge.sendCommand('import_json', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 8. import_agent_graph ───
  const agentImportNodeSchema = z.object({
    id: z.string().describe('Local semantic node id'),
    kind: z.enum([
      'event',
      'custom_event',
      'call',
      'get',
      'set',
      'branch',
      'sequence',
      'comment',
    ]).describe('Semantic node kind supported by AgentImportGraph v1'),
  }).passthrough();

  const agentImportLinkSchema = z.object({
    kind: z.enum(['exec', 'data']).describe('Link kind: exec or data'),
    from: z.string().optional().describe('Shorthand source endpoint, e.g. begin_play.then'),
    to: z.string().optional().describe('Shorthand target endpoint, e.g. print.execute'),
    from_node: z.string().optional().describe('Structured source node id'),
    from_pin: z.string().optional().describe('Structured source pin name'),
    to_node: z.string().optional().describe('Structured target node id'),
    to_pin: z.string().optional().describe('Structured target pin name'),
  }).passthrough();

  server.registerTool(
    'blueprint_import_agent_graph',
    {
      description:
        legacyWriteExpertDescription('Import a legacy semantic BlueprintHelper.AgentImportGraph object. This creates Blueprint nodes from intent-level event/call/get/set/branch/sequence/comment nodes and auto-generates layout. It does not replace blueprint_import_json_to_graph for raw JSON replay.'),
      inputSchema: z.object({
        schema: z.literal('BlueprintHelper.AgentImportGraph').default('BlueprintHelper.AgentImportGraph'),
        version: z.literal('1.0').default('1.0'),
        target_blueprint: z.string()
          .describe('Required Blueprint asset path, e.g. /Game/BP/BP_Player.BP_Player'),
        target_graph: z.string()
          .describe('Required graph name, e.g. EventGraph'),
        mode: z.literal('append').default('append'),
        layout: z.enum(['auto', 'append_right']).optional().default('auto'),
        declarations: z.object({
          variables: z.array(z.object({
            name: z.string(),
            type: z.string().describe('Pin type category, e.g. bool, int, float, string'),
            default: z.union([z.string(), z.number(), z.boolean()]).optional(),
            default_value: z.string().optional(),
            editable: z.boolean().optional(),
            category: z.string().optional(),
          }).passthrough()).optional(),
        }).passthrough().optional(),
        nodes: z.array(agentImportNodeSchema),
        links: z.array(agentImportLinkSchema).optional().default([]),
        options: z.object({
          compile: z.boolean().optional().default(true),
          save: z.boolean().optional().default(false),
          strict: z.boolean().optional().default(true),
          dry_run: z.boolean().optional().default(false),
          create_missing_variables: z.boolean().optional().default(true),
          reconstruct_existing_nodes: z.boolean().optional().default(false),
        }).passthrough().optional(),
      }).passthrough(),
    },
    async (payload) => {
      try {
        const resp = await bridge.sendCommand('import_agent_graph', payload);
        return toToolResult(resp, resp.result?.['success'] === false);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 9. compile_blueprint ───
  server.registerTool(
    'blueprint_compile_blueprint',
    {
      description: legacyWriteExpertDescription('Trigger compilation of a blueprint asset and return diagnostics.'),
      inputSchema: z.object({
        target_blueprint: z.string().optional()
          .describe('Blueprint asset path to compile. Omit to use the active blueprint.'),
      }),
    },
    async ({ target_blueprint }) => {
      try {
        const payload: Record<string, unknown> = {};
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        const resp = await bridge.sendCommand('compile_blueprint', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ═══════════════════════════════════════════════════════════
  // Phase 4 — 资产浏览工具
  // ═══════════════════════════════════════════════════════════

  // ─── 9. open_asset ───
  server.registerTool(
    'blueprint_open_asset',
    {
      description: legacyWriteExpertDescription('Open any Unreal asset in its default editor by asset path.'),
      inputSchema: z.object({
        asset_path: z.string().describe('Full asset path, e.g. /Game/Blueprints/BP_Player.BP_Player'),
      }),
    },
    async ({ asset_path }) => {
      try {
        const resp = await bridge.sendCommand('open_asset', { asset_path });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 10. list_assets ───
  server.registerTool(
    'blueprint_list_assets',
    {
      description: legacyDebugExpertDescription('List assets in a Content Browser directory with optional class and name filters.'),
      inputSchema: z.object({
        path: z.string().optional().default('/Game')
          .describe('Content directory path, e.g. /Game/Blueprints'),
        class_filter: z.string().optional()
          .describe('Filter by asset class name, e.g. Blueprint, DataTable, WidgetBlueprint'),
        name_filter: z.string().optional()
          .describe('Filter by asset name substring'),
        recursive: z.boolean().optional().default(true)
          .describe('Whether to search subdirectories recursively'),
        max_results: z.number().optional().default(200)
          .describe('Maximum number of results to return (0 = unlimited)'),
      }),
    },
    async ({ path, class_filter, name_filter, recursive, max_results }) => {
      try {
        const payload: Record<string, unknown> = {};
        if (path) payload.path = path;
        if (class_filter) payload.class_filter = class_filter;
        if (name_filter) payload.name_filter = name_filter;
        if (recursive !== undefined) payload.recursive = recursive;
        if (max_results !== undefined) payload.max_results = max_results;
        const resp = await bridge.sendCommand('list_assets', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 11. search_assets ───
  server.registerTool(
    'blueprint_search_assets',
    {
      description: legacyDebugExpertDescription('Search for assets by keyword across the project content. Always searches recursively.'),
      inputSchema: z.object({
        query: z.string().describe('Search keyword (matches asset name substring)'),
        path: z.string().optional()
          .describe('Limit search to this content path, e.g. /Game/Blueprints'),
        class_filter: z.string().optional()
          .describe('Filter by asset class name'),
        max_results: z.number().optional().default(50)
          .describe('Maximum number of results to return'),
      }),
    },
    async ({ query, path, class_filter, max_results }) => {
      try {
        const payload: Record<string, unknown> = { query };
        if (path) payload.path = path;
        if (class_filter) payload.class_filter = class_filter;
        if (max_results !== undefined) payload.max_results = max_results;
        const resp = await bridge.sendCommand('search_assets', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 12. save_asset ───
  server.registerTool(
    'blueprint_save_asset',
    {
      description: legacyWriteExpertDescription('Save a specific asset to disk.'),
      inputSchema: z.object({
        asset_path: z.string().describe('Full asset path to save, e.g. /Game/Blueprints/BP_Player.BP_Player'),
      }),
    },
    async ({ asset_path }) => {
      try {
        const resp = await bridge.sendCommand('save_asset', { asset_path });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 13. get_asset_info ───
  server.registerTool(
    'blueprint_get_asset_info',
    {
      description: legacyDebugExpertDescription('Get detailed information about an asset: class, parent class, disk size.'),
      inputSchema: z.object({
        asset_path: z.string().describe('Full asset path, e.g. /Game/Blueprints/BP_Player.BP_Player'),
      }),
    },
    async ({ asset_path }) => {
      try {
        const resp = await bridge.sendCommand('get_asset_info', { asset_path });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ═══════════════════════════════════════════════════════════
  // Phase 5 — 蓝图结构查询与操作工具
  // ═══════════════════════════════════════════════════════════

  const targetSchemaFields = {
    target_blueprint: z.string().optional()
      .describe('Blueprint asset path. Omit to use the active blueprint.'),
    target_graph: z.string().optional()
      .describe('Graph name. Omit to use the active/default graph.'),
  };

  // ─── 14. list_graphs ───
  server.registerTool(
    'blueprint_list_graphs',
    {
      description: legacyDebugExpertDescription('List all graphs (EventGraph, functions, macros) in a blueprint with their types and node counts.'),
      inputSchema: z.object({
        target_blueprint: targetSchemaFields.target_blueprint,
      }),
    },
    async ({ target_blueprint }) => {
      try {
        const payload: Record<string, unknown> = {};
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        const resp = await bridge.sendCommand('list_graphs', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 15. list_variables ───
  server.registerTool(
    'blueprint_list_variables',
    {
      description: legacyDebugExpertDescription('List all member variables in a blueprint with type info, defaults, and categories.'),
      inputSchema: z.object({
        target_blueprint: targetSchemaFields.target_blueprint,
      }),
    },
    async ({ target_blueprint }) => {
      try {
        const payload: Record<string, unknown> = {};
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        const resp = await bridge.sendCommand('list_variables', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 16. list_event_dispatchers ───
  server.registerTool(
    'blueprint_list_event_dispatchers',
    {
      description: legacyDebugExpertDescription('List all event dispatchers in a blueprint with their parameter signatures.'),
      inputSchema: z.object({
        target_blueprint: targetSchemaFields.target_blueprint,
      }),
    },
    async ({ target_blueprint }) => {
      try {
        const payload: Record<string, unknown> = {};
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        const resp = await bridge.sendCommand('list_event_dispatchers', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 17. add_variable ───
  server.registerTool(
    'blueprint_add_variable',
    {
      description: legacyWriteExpertDescription('Add a member variable to a blueprint. Supports type, default value, category, and flags.'),
      inputSchema: z.object({
        target_blueprint: targetSchemaFields.target_blueprint,
        name: z.string().describe('Variable name'),
        pin_type: z.object({
          category: z.string().describe('Pin category: bool, int, float, real, byte, name, string, text, object, class, struct, interface, softobject, softclass, enum'),
          sub_category: z.string().optional().describe('Pin sub-category'),
          object_path: z.string().optional().describe('Object path for object/struct/enum types'),
          container_type: z.string().optional().describe('Container type: None, Array, Set, Map'),
        }).optional().describe('Variable type. Defaults to bool if omitted.'),
        default_value: z.string().optional().describe('Default value as string'),
        category: z.string().optional().describe('Variable category for organization'),
        flags: z.object({
          blueprint_read_only: z.boolean().optional(),
          expose_on_spawn: z.boolean().optional(),
        }).optional().describe('Variable flags'),
      }),
    },
    async ({ target_blueprint, name, pin_type, default_value, category, flags }) => {
      try {
        const payload: Record<string, unknown> = { name };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        if (pin_type) payload.pin_type = pin_type;
        if (default_value !== undefined) payload.default_value = default_value;
        if (category) payload.category = category;
        if (flags) payload.flags = flags;
        const resp = await bridge.sendCommand('add_variable', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 18. remove_variable ───
  server.registerTool(
    'blueprint_remove_variable',
    {
      description: legacyWriteExpertDescription('Remove a member variable from a blueprint by name. Idempotent — succeeds if variable does not exist.'),
      inputSchema: z.object({
        target_blueprint: targetSchemaFields.target_blueprint,
        name: z.string().describe('Variable name to remove'),
      }),
    },
    async ({ target_blueprint, name }) => {
      try {
        const payload: Record<string, unknown> = { name };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        const resp = await bridge.sendCommand('remove_variable', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 19. add_graph ───
  server.registerTool(
    'blueprint_add_graph',
    {
      description: legacyWriteExpertDescription('Add a new function or macro graph to a blueprint. Supports inputs, outputs, and pure flag.'),
      inputSchema: z.object({
        target_blueprint: targetSchemaFields.target_blueprint,
        name: z.string().describe('Graph name'),
        graph_type: z.enum(['Function', 'Macro']).optional().default('Function')
          .describe('Graph type: Function or Macro'),
        inputs: z.array(z.object({
          name: z.string(),
          pin_type: z.object({
            category: z.string(),
            sub_category: z.string().optional(),
            object_path: z.string().optional(),
            container_type: z.string().optional(),
          }),
        })).optional().describe('Function input parameters'),
        outputs: z.array(z.object({
          name: z.string(),
          pin_type: z.object({
            category: z.string(),
            sub_category: z.string().optional(),
            object_path: z.string().optional(),
            container_type: z.string().optional(),
          }),
        })).optional().describe('Function output/return parameters'),
        is_pure: z.boolean().optional().default(false)
          .describe('Whether the function is pure (no execution pins)'),
      }),
    },
    async ({ target_blueprint, name, graph_type, inputs, outputs, is_pure }) => {
      try {
        const payload: Record<string, unknown> = { name };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        if (graph_type) payload.graph_type = graph_type;
        if (inputs) payload.inputs = inputs;
        if (outputs) payload.outputs = outputs;
        if (is_pure !== undefined) payload.is_pure = is_pure;
        const resp = await bridge.sendCommand('add_graph', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 20. remove_graph ───
  server.registerTool(
    'blueprint_remove_graph',
    {
      description: legacyWriteExpertDescription('Remove a function or macro graph from a blueprint. Cannot remove EventGraph. Idempotent.'),
      inputSchema: z.object({
        target_blueprint: targetSchemaFields.target_blueprint,
        name: z.string().describe('Graph name to remove'),
      }),
    },
    async ({ target_blueprint, name }) => {
      try {
        const payload: Record<string, unknown> = { name };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        const resp = await bridge.sendCommand('remove_graph', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 21. add_event_dispatcher ───
  server.registerTool(
    'blueprint_add_event_dispatcher',
    {
      description: legacyWriteExpertDescription('Add an event dispatcher to a blueprint with optional typed parameters.'),
      inputSchema: z.object({
        target_blueprint: targetSchemaFields.target_blueprint,
        name: z.string().describe('Event dispatcher name'),
        params: z.array(z.object({
          name: z.string().describe('Parameter name'),
          pin_type: z.object({
            category: z.string(),
            sub_category: z.string().optional(),
            object_path: z.string().optional(),
            container_type: z.string().optional(),
          }).optional().describe('Parameter type. Defaults to bool.'),
        })).optional().describe('Dispatcher parameters'),
      }),
    },
    async ({ target_blueprint, name, params }) => {
      try {
        const payload: Record<string, unknown> = { name };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        if (params) payload.params = params;
        const resp = await bridge.sendCommand('add_event_dispatcher', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 22. delete_nodes ───
  server.registerTool(
    'blueprint_delete_nodes',
    {
      description: legacyWriteExpertDescription('Delete specific nodes from a blueprint graph by their IDs (Node_0, Node_1, etc.). Cannot delete FunctionEntry/FunctionResult nodes.'),
      inputSchema: z.object({
        ...targetSchemaFields,
        node_ids: z.array(z.string()).describe('Array of node IDs to delete, e.g. ["Node_0", "Node_3"]'),
      }),
    },
    async ({ target_blueprint, target_graph, node_ids }) => {
      try {
        const payload: Record<string, unknown> = { node_ids };
        if (target_blueprint) payload.target_blueprint = target_blueprint;
        if (target_graph) payload.target_graph = target_graph;
        const resp = await bridge.sendCommand('delete_nodes', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ═══════════════════════════════════════════════════════════
  // Phase 6 — UMG Widget 操作工具
  // ═══════════════════════════════════════════════════════════

  const widgetAssetPath = z.string()
    .describe('WidgetBlueprint asset path, e.g. /Game/UI/WBP_Main.WBP_Main');

  // ─── 23. get_widget_tree ───
  server.registerTool(
    'blueprint_get_widget_tree',
    {
      description: legacyDebugExpertDescription('Get the full widget tree of a WidgetBlueprint, showing hierarchy, types, and slot info.'),
      inputSchema: z.object({
        asset_path: widgetAssetPath,
      }),
    },
    async ({ asset_path }) => {
      try {
        const resp = await bridge.sendCommand('get_widget_tree', { asset_path });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 24. add_widget ───
  server.registerTool(
    'blueprint_add_widget',
    {
      description: legacyWriteExpertDescription('Add a new widget to a WidgetBlueprint. Specify parent panel and widget class (e.g. TextBlock, Button, CanvasPanel).'),
      inputSchema: z.object({
        asset_path: widgetAssetPath,
        widget_class: z.string()
          .describe('Widget class name without U prefix, e.g. TextBlock, Button, CanvasPanel, VerticalBox, Image'),
        parent_name: z.string().optional()
          .describe('Parent panel widget name. Omit to add to root panel.'),
        widget_name: z.string().optional()
          .describe('Name for the new widget. Auto-generated if omitted.'),
      }),
    },
    async ({ asset_path, widget_class, parent_name, widget_name }) => {
      try {
        const payload: Record<string, unknown> = { asset_path, widget_class };
        if (parent_name) payload.parent_name = parent_name;
        if (widget_name) payload.widget_name = widget_name;
        const resp = await bridge.sendCommand('add_widget', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 25. remove_widget ───
  server.registerTool(
    'blueprint_remove_widget',
    {
      description: legacyWriteExpertDescription('Remove a widget (and its subtree) from a WidgetBlueprint by name.'),
      inputSchema: z.object({
        asset_path: widgetAssetPath,
        widget_name: z.string().describe('Name of the widget to remove'),
      }),
    },
    async ({ asset_path, widget_name }) => {
      try {
        const resp = await bridge.sendCommand('remove_widget', { asset_path, widget_name });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 26. move_widget ───
  server.registerTool(
    'blueprint_move_widget',
    {
      description: legacyWriteExpertDescription('Move a widget to a different parent panel within the same WidgetBlueprint.'),
      inputSchema: z.object({
        asset_path: widgetAssetPath,
        widget_name: z.string().describe('Name of the widget to move'),
        new_parent: z.string().describe('Name of the new parent panel widget'),
        insert_index: z.number().optional()
          .describe('Position to insert at (-1 or omit for end)'),
      }),
    },
    async ({ asset_path, widget_name, new_parent, insert_index }) => {
      try {
        const payload: Record<string, unknown> = { asset_path, widget_name, new_parent };
        if (insert_index !== undefined) payload.insert_index = insert_index;
        const resp = await bridge.sendCommand('move_widget', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 27. get_widget_properties ───
  server.registerTool(
    'blueprint_get_widget_properties',
    {
      description: legacyDebugExpertDescription('Get all editable properties of a widget with their current values and types.'),
      inputSchema: z.object({
        asset_path: widgetAssetPath,
        widget_name: z.string().describe('Name of the widget to inspect'),
      }),
    },
    async ({ asset_path, widget_name }) => {
      try {
        const resp = await bridge.sendCommand('get_widget_properties', { asset_path, widget_name });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 28. set_widget_property ───
  server.registerTool(
    'blueprint_set_widget_property',
    {
      description: legacyWriteExpertDescription('Set a property value on a widget using text import format. Use get_widget_properties to discover available properties first.'),
      inputSchema: z.object({
        asset_path: widgetAssetPath,
        widget_name: z.string().describe('Name of the widget to modify'),
        property_name: z.string().describe('Property name, e.g. Text, ColorAndOpacity, Visibility'),
        value: z.string().describe('Property value in UE text import format'),
      }),
    },
    async ({ asset_path, widget_name, property_name, value }) => {
      try {
        const resp = await bridge.sendCommand('set_widget_property', {
          asset_path, widget_name, property_name, value,
        });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ═══════════════════════════════════════════════════════════
  // Phase 7 — DataAsset & DataTable 操作 (Tools 29–34)
  // ═══════════════════════════════════════════════════════════

  const objectAssetPath = z.string()
    .describe('Asset path to any UObject (DataAsset, DataTable, etc.), e.g. /Game/Data/DA_Example.DA_Example');

  const dataTableAssetPath = z.string()
    .describe('DataTable asset path, e.g. /Game/Data/DT_Items.DT_Items');

  // ─── 29. get_object_properties ───
  server.registerTool(
    'blueprint_get_object_properties',
    {
      description: legacyDebugExpertDescription('Get all editable UPROPERTY fields of any asset (DataAsset, etc.) with their current values, types, and categories via FProperty reflection.'),
      inputSchema: z.object({
        asset_path: objectAssetPath,
      }),
    },
    async ({ asset_path }) => {
      try {
        const resp = await bridge.sendCommand('get_object_properties', { asset_path });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 30. set_object_property ───
  server.registerTool(
    'blueprint_set_object_property',
    {
      description: legacyWriteExpertDescription('Set a single UPROPERTY value on any asset using UE text import format. Use get_object_properties to discover available properties first.'),
      inputSchema: z.object({
        asset_path: objectAssetPath,
        property_name: z.string().describe('Property name to set'),
        value: z.string().describe('New value in UE text import format'),
      }),
    },
    async ({ asset_path, property_name, value }) => {
      try {
        const resp = await bridge.sendCommand('set_object_property', {
          asset_path, property_name, value,
        });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 31. get_datatable_rows ───
  server.registerTool(
    'blueprint_get_datatable_rows',
    {
      description: legacyDebugExpertDescription('Get all rows (or specific rows) from a DataTable with column schema and field values.'),
      inputSchema: z.object({
        asset_path: dataTableAssetPath,
        row_names: z.array(z.string()).optional()
          .describe('Optional list of row names to filter. Omit to get all rows.'),
      }),
    },
    async ({ asset_path, row_names }) => {
      try {
        const payload: Record<string, unknown> = { asset_path };
        if (row_names && row_names.length > 0) payload.row_names = row_names;
        const resp = await bridge.sendCommand('get_datatable_rows', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 32. add_datatable_row ───
  server.registerTool(
    'blueprint_add_datatable_row',
    {
      description: legacyWriteExpertDescription('Add a new row to a DataTable. Fields are key-value pairs matching the row struct properties.'),
      inputSchema: z.object({
        asset_path: dataTableAssetPath,
        row_name: z.string().describe('Name for the new row'),
        fields: z.record(z.string()).optional()
          .describe('Object of field_name: value_string pairs. Values use UE text import format.'),
      }),
    },
    async ({ asset_path, row_name, fields }) => {
      try {
        const payload: Record<string, unknown> = { asset_path, row_name };
        if (fields) payload.fields = fields;
        const resp = await bridge.sendCommand('add_datatable_row', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 33. update_datatable_row ───
  server.registerTool(
    'blueprint_update_datatable_row',
    {
      description: legacyWriteExpertDescription('Update fields on an existing DataTable row. Only specified fields are modified.'),
      inputSchema: z.object({
        asset_path: dataTableAssetPath,
        row_name: z.string().describe('Name of the row to update'),
        fields: z.record(z.string())
          .describe('Object of field_name: new_value_string pairs. Values use UE text import format.'),
      }),
    },
    async ({ asset_path, row_name, fields }) => {
      try {
        const resp = await bridge.sendCommand('update_datatable_row', {
          asset_path, row_name, fields,
        });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 34. delete_datatable_row ───
  server.registerTool(
    'blueprint_delete_datatable_row',
    {
      description: legacyWriteExpertDescription('Delete a row from a DataTable by name.'),
      inputSchema: z.object({
        asset_path: dataTableAssetPath,
        row_name: z.string().describe('Name of the row to delete'),
      }),
    },
    async ({ asset_path, row_name }) => {
      try {
        const resp = await bridge.sendCommand('delete_datatable_row', { asset_path, row_name });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ═══════════════════════════════════════════════
  // Phase 8: 编辑器命令 (6 tools)
  // ═══════════════════════════════════════════════

  // ─── 35. undo ───
  server.registerTool(
    'blueprint_undo',
    {
      description: legacyWriteExpertDescription('Undo the last editor action.'),
      inputSchema: z.object({}),
    },
    async () => {
      try {
        const resp = await bridge.sendCommand('undo', {});
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 36. redo ───
  server.registerTool(
    'blueprint_redo',
    {
      description: legacyWriteExpertDescription('Redo the last undone editor action.'),
      inputSchema: z.object({}),
    },
    async () => {
      try {
        const resp = await bridge.sendCommand('redo', {});
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 37. play_in_editor ───
  server.registerTool(
    'blueprint_play_in_editor',
    {
      description: legacyWriteExpertDescription('Start a Play In Editor (PIE) session.'),
      inputSchema: z.object({}),
    },
    async () => {
      try {
        const resp = await bridge.sendCommand('play_in_editor', {});
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 38. stop_pie ───
  server.registerTool(
    'blueprint_stop_pie',
    {
      description: legacyWriteExpertDescription('Stop the currently running PIE session.'),
      inputSchema: z.object({}),
    },
    async () => {
      try {
        const resp = await bridge.sendCommand('stop_pie', {});
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 39. create_blueprint ───
  server.registerTool(
    'blueprint_create_blueprint',
    {
      description: legacyWriteExpertDescription('Create a new Blueprint asset in the project.'),
      inputSchema: z.object({
        asset_path: z.string().describe('Full asset path, e.g. /Game/Blueprints/BP_MyActor'),
        parent_class: z.string().optional().default('Actor').describe('Parent class name (Actor, Pawn, Character, etc.)'),
      }),
    },
    async ({ asset_path, parent_class }) => {
      try {
        const resp = await bridge.sendCommand('create_blueprint', { asset_path, parent_class });
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 40. exec_console_command ───
  server.registerTool(
    'blueprint_exec_console_command',
    {
      description: legacyWriteExpertDescription('Execute an Unreal Editor console command and return its output.'),
      inputSchema: z.object({
        command: z.string().describe('The console command to execute'),
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

  // ═══════════════════════════════════════════════════════════
  // Editor Lifecycle Tools (41-43)
  // ═══════════════════════════════════════════════════════════

  // ─── 41. close_editor ───
  server.registerTool(
    'blueprint_close_editor',
    {
      description:
        legacyWriteExpertDescription('Save all dirty assets and close the Unreal Editor. The editor will exit on the next frame after the response is sent. Use this before building to avoid LiveCoding duplicate-class issues.'),
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
        if (save_all !== undefined) payload['save_all'] = save_all;
        const resp = await bridge.sendCommand('close_editor', payload);
        return toToolResult(resp);
      } catch (err) {
        return toErrorResult(err);
      }
    },
  );

  // ─── 42. build_project ───
  server.registerTool(
    'blueprint_build_project',
    {
      description:
        legacyWriteExpertDescription('Build the Unreal project using UnrealBuildTool. The editor must be closed first. Returns build output. Requires UE_ENGINE_DIR and UE_PROJECT_FILE env vars.'),
      inputSchema: z.object({
        target: z
          .string()
          .optional()
          .describe('Build target name (default: derived from project filename + "Editor", e.g. MrStoneEditor)'),
        configuration: z
          .enum(['Development', 'DebugGame', 'Debug', 'Shipping', 'Test'])
          .optional()
          .describe('Build configuration (default: Development)'),
        platform: z.string().optional().describe('Target platform (default: Win64)'),
      }),
    },
    async ({ target, configuration, platform }) => {
      if (!config.ueEngineDir || !config.ueProjectFile) {
        return toErrorResult(
          new Error(
            'UE_ENGINE_DIR and UE_PROJECT_FILE environment variables must be set for build_project.',
          ),
        );
      }

      const buildBat = path.join(config.ueEngineDir, 'Engine', 'Build', 'BatchFiles', 'Build.bat');
      const projectName = path.basename(config.ueProjectFile, '.uproject');
      const buildTarget = target ?? `${projectName}Editor`;
      const buildConfig = configuration ?? 'Development';
      const buildPlatform = platform ?? 'Win64';

      return new Promise((resolve) => {
        console.error(
          `[BlueprintHelper MCP] Building: ${buildBat} ${buildTarget} ${buildPlatform} ${buildConfig} "${config.ueProjectFile}" -WaitMutex`,
        );

        execFile(
          buildBat,
          [buildTarget, buildPlatform, buildConfig, config.ueProjectFile, '-WaitMutex'],
          { maxBuffer: 10 * 1024 * 1024, timeout: 600_000 },
          (error, stdout, stderr) => {
            const output = (stdout || '') + (stderr ? `\n--- stderr ---\n${stderr}` : '');
            if (error) {
              resolve({
                content: [
                  {
                    type: 'text' as const,
                    text: JSON.stringify(
                      {
                        success: false,
                        exit_code: error.code ?? -1,
                        message: `Build failed: ${error.message}`,
                        output: output.slice(-8000),
                      },
                      null,
                      2,
                    ),
                  },
                ],
                isError: true,
              });
            } else {
              resolve({
                content: [
                  {
                    type: 'text' as const,
                    text: JSON.stringify(
                      {
                        success: true,
                        message: 'Build succeeded.',
                        output: output.slice(-4000),
                      },
                      null,
                      2,
                    ),
                  },
                ],
                isError: false,
              });
            }
          },
        );
      });
    },
  );

  // ─── 43. open_editor ───
  server.registerTool(
  'blueprint_open_editor',
  {
    description:
      preflightOnlyDescription('Launch Unreal Editor for the current project by opening its .uproject file, then wait for the BlueprintHelper Bridge server to become available. Requires UE_ENGINE_DIR and UE_PROJECT_FILE env vars. UE_PROJECT_FILE must be an absolute path to the .uproject file, for example G:/UnrealPractise/MrStone/MrStone.uproject. Template variables like ${workspaceFolder} are automatically expanded at startup.'),
    inputSchema: z.object({
      wait_timeout_ms: z
        .number()
        .optional()
        .describe('Max time in ms to wait for the editor Bridge to become available (default 120000)'),
    }),
  },
  async ({ wait_timeout_ms }) => {
    if (!config.ueEngineDir || !config.ueProjectFile) {
      return toErrorResult(
        new Error(
          [
            'UE_ENGINE_DIR and UE_PROJECT_FILE environment variables must be set for blueprint_open_editor.',
            'Both must be absolute paths (e.g. F:/UE_5.6 and G:/UnrealPractise/MrStone/MrStone.uproject).',
            'UE_PROJECT_FILE must point to the current project .uproject file, not only a project directory.',
            'Template variables (${workspaceFolder}) are expanded automatically — set them literally in the MCP env config.',
            `Current values: UE_ENGINE_DIR=${config.ueEngineDir || '(empty)'}, UE_PROJECT_FILE=${config.ueProjectFile || '(empty)'}`,
            'Agent instruction: verify the MCP server env configuration uses absolute paths. If the paths look correct but the editor still fails to open, check that the .uproject file exists.',
          ].join('\n'),
        ),
      );
    }

    const uprojectFile = path.resolve(config.ueProjectFile);

    if (path.extname(uprojectFile).toLowerCase() !== '.uproject') {
      return {
        content: [
          {
            type: 'text' as const,
            text: JSON.stringify(
              {
                success: false,
                code: 'UE_PROJECT_FILE_NOT_UPROJECT',
                message:
                  'UE_PROJECT_FILE must point to a .uproject file for the current Unreal project.',
                received_ue_project_file: config.ueProjectFile,
                resolved_project_file: uprojectFile,
                agent_instruction:
                  'Resolve the current project path, find the matching .uproject file, set UE_PROJECT_FILE to that absolute .uproject path, then call blueprint_open_editor again.',
              },
              null,
              2,
            ),
          },
        ],
        isError: true,
      };
    }

    const editorExe = path.join(
      config.ueEngineDir,
      'Engine',
      'Binaries',
      'Win64',
      'UnrealEditor.exe',
    );

    const timeoutMs = wait_timeout_ms ?? 120_000;

    const launchCommand = `"${editorExe}" "${uprojectFile}"`;

    console.error(`[BlueprintHelper MCP] Launching editor with project file: ${launchCommand}`);

    let child;
    try {
      // 启动当前项目的 .uproject，而不是打开裸 Unreal Editor。
      child = spawn(editorExe, [uprojectFile], {
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
                message: 'Failed to launch Unreal Editor with the current project .uproject.',
                editor_exe: editorExe,
                uproject_path: uprojectFile,
                launch_command: launchCommand,
                agent_instruction:
                  'Verify UE_ENGINE_DIR and UE_PROJECT_FILE. UE_PROJECT_FILE must be the absolute path to the current project .uproject.',
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

    // 轮询 Bridge 直到可用。
    const startTime = Date.now();
    const pollIntervalMs = 3000;

    while (Date.now() - startTime < timeoutMs) {
      await new Promise((r) => setTimeout(r, pollIntervalMs));

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
                  message:
                    'Unreal Editor was launched with the current project .uproject and BlueprintHelper Bridge is available.',
                  editor_exe: editorExe,
                  uproject_path: uprojectFile,
                  launch_command: launchCommand,
                  elapsed_ms: Date.now() - startTime,
                  agent_instruction:
                    'The editor and Bridge are ready. Editor-bound BlueprintHelper MCP tools may now be used. Prefer explicit asset_path and target_graph arguments for write operations.',
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
              message:
                `Unreal Editor was started with the current project .uproject, but BlueprintHelper Bridge did not become available within ${timeoutMs}ms. The editor may still be loading.`,
              editor_exe: editorExe,
              uproject_path: uprojectFile,
              launch_command: launchCommand,
              elapsed_ms: Date.now() - startTime,
              agent_instruction:
                'Do not use editor-bound BlueprintHelper MCP tools yet. Wait for Unreal Editor to finish loading, then check Bridge/status or call blueprint_open_editor again with a longer wait_timeout_ms.',
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
