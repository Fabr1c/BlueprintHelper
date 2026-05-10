/**
 * BlueprintHelper MCP — Tool Result Base 标准化
 *
 * 第 0 簇：统一所有 MCP 工具返回体的基础字段。
 * 从 Bridge 原始响应映射到 Agent 可见的精简 Tool Result Base。
 *
 * 约束：
 * - 不返回 tool / command / request_id / diagnostics / next / ownership 顶层字段 / safety_profile。
 * - content[0].text 只放短摘要。
 * - structuredContent 放完整 ToolResultBase JSON。
 */

import { BridgeResponse } from './bridge-client.js';

// ─── 枚举值常量 ───

export type ToolStatus =
  | 'completed'
  | 'applied'
  | 'dry_run'
  | 'failed'
  | 'no_op'
  | 'skipped'
  | 'rolled_back';

export type TargetType =
  | 'asset'
  | 'blueprint'
  | 'graph'
  | 'function'
  | 'event'
  | 'custom_event'
  | 'block'
  | 'node'
  | 'pin'
  | 'link'
  | 'component'
  | 'property'
  | 'mapping_context'
  | 'data_table'
  | 'data_table_row'
  | 'widget';

export type RiskLevel =
  | 'none'
  | 'low'
  | 'medium'
  | 'high'
  | 'destructive';

export type TransactionStatus =
  | 'applied'
  | 'failed'
  | 'rolled_back'
  | 'no_op';

export type ReviewStatus =
  | 'none'
  | 'pending'
  | 'accepted'
  | 'rejected'
  | 'rolled_back'
  | 'archived';

export type ValidationSeverity =
  | 'info'
  | 'warning'
  | 'error';

export type ToolStage =
  | 'parse_input'
  | 'auth'
  | 'runtime_profile'
  | 'resolve_target'
  | 'preflight'
  | 'dry_run'
  | 'execute'
  | 'validate'
  | 'review'
  | 'rollback'
  | 'bridge'
  | 'mcp_wrap';

export type RollbackResult =
  | 'not_needed'
  | 'rolled_back'
  | 'rollback_failed'
  | 'unavailable';

// ─── 子结构类型 ───

export interface ToolResultTarget {
  asset_path?: string;
  asset_class?: string;
  blueprint_path?: string;
  target_type: TargetType;
  graph?: string;
  function?: string;
  event?: string;
  block_id?: string;
  node_path?: string;
  pin_path?: string;
  link_path?: string;
  component_name?: string;
  property_path?: string;
  widget_path?: string;
  row_name?: string;
}

export interface ToolResultSafety {
  risk_level: RiskLevel;
  dry_run_required: boolean;
  dry_run_performed: boolean;
  can_execute: boolean;
  blocked_by: string[];
}

export interface ToolResultOwnershipSummary {
  owned_nodes_count: number;
  owned_links_count: number;
  affected_owned_nodes_count: number;
  affected_owned_links_count: number;
  deleted_owned_nodes_count: number;
  deleted_owned_links_count: number;
  metadata_written: boolean;
  node_comments_written: boolean;
  metadata_preserved: boolean;
  node_comments_preserved: boolean;
  metadata_removed: boolean;
  node_comments_removed: boolean;
}

export interface ToolResultTransaction {
  transaction_id: string;
  status: TransactionStatus;
  affected_assets: string[];
  block_ids: string[];
  rollback_available: boolean;
  ownership_summary?: ToolResultOwnershipSummary;
}

export interface ToolResultValidationMessage {
  code: string;
  message: string;
  severity: ValidationSeverity;
  node_path?: string;
  pin_path?: string;
}

export interface ToolResultValidation {
  should_compile: boolean;
  should_save: boolean;
  compiled: boolean;
  saved: boolean;
  compile_success: boolean;
  errors: ToolResultValidationMessage[];
  warnings: ToolResultValidationMessage[];
}

export interface ToolResultError {
  code: string;
  stage: ToolStage;
  message: string;
  retryable: boolean;
  rollback_result: RollbackResult;
  field?: string;
  expected?: string;
  actual?: string;
}

export interface ToolResultReview {
  review_required: boolean;
  review_status: ReviewStatus;
  review_reason: string;
  review_grouping: string[];
}

export interface DryRunData {
  can_execute: boolean;
  warnings: ToolResultValidationMessage[];
  conflicts: ToolResultValidationMessage[];
  errors: ToolResultValidationMessage[];
}

// ─── 顶层 ToolResultBase ───

export interface ToolResultBase {
  ok: boolean;
  schema: string;
  operation: string;
  trace_id: string;
  status: ToolStatus;
  modified: boolean;
  debug_case_ids?: string[];
  target?: ToolResultTarget;
  data?: Record<string, unknown>;
  validation?: ToolResultValidation;
  error?: ToolResultError;
}

// ─── Schema 版本 ───

export const TOOL_RESULT_SCHEMA = 'BlueprintHelper.McpToolResult.v1';

// ─── 生成唯一 ID ───

let traceCounter = 0;
let transactionCounter = 0;

export function generateTraceId(): string {
  const timePart = Math.floor(Date.now() / 1000);
  const counterPart = String(++traceCounter).padStart(4, '0');
  return `trace_${timePart}_${counterPart}`;
}

export function generateTransactionId(prefix = 'tx'): string {
  const timePart = Math.floor(Date.now() / 1000);
  const counterPart = String(++transactionCounter).padStart(4, '0');
  return `${prefix}_${timePart}_${counterPart}`;
}

// ─── 短摘要生成 ───

export function makeSummary(result: ToolResultBase): string {
  const targetInfo = result.target
    ? `${result.target.asset_path ?? ''}${result.target.graph ? '.' + result.target.graph : ''}`
    : '';
  const baseSummary = `${result.operation} ${result.status}: ${targetInfo}, modified=${result.modified}.`;
  const errorMessage = !result.ok ? result.error?.message.trim() : undefined;
  const debugCases =
    !result.ok && result.debug_case_ids?.length
      ? ` debug_case_ids=${result.debug_case_ids.join(',')}`
      : '';

  return errorMessage ? `${baseSummary} error=${errorMessage}${debugCases}` : `${baseSummary}${debugCases}`;
}

// ─── 从 Bridge 原始响应标准化 ───

/**
 * 将原始 BridgeResponse 标准化为 ToolResultBase。
 * 用于 MCP 工具 handler 中替换 toToolResult()。
 *
 * @param resp - Bridge 原始响应
 * @param operation - 公共操作名（如 append_blueprint_graph）
 * @param overrides - 可选覆盖字段（如 target, transaction, validation）
 */
export function normalizeToolResult(
  resp: BridgeResponse,
  operation: string,
  overrides?: {
    target?: Partial<ToolResultTarget>;
    data?: Record<string, unknown>;
    validation?: Partial<ToolResultValidation>;
    error?: Partial<ToolResultError>;
    modified?: boolean;
  },
): ToolResultBase {
  const traceId = generateTraceId();

  if (!resp.success) {
    const error: ToolResultError = {
      code: resp.error_code ?? 'bridge_error',
      stage: 'bridge',
      message: resp.message ?? '未知 Bridge 错误',
      retryable: false,
      rollback_result: 'not_needed',
      ...overrides?.error,
    };

    return {
      ok: false,
      schema: TOOL_RESULT_SCHEMA,
      operation,
      trace_id: traceId,
      status: 'failed',
      modified: false,
      target: overrides?.target as ToolResultTarget | undefined,
      error,
    };
  }

  const result = (resp.result ?? {}) as Record<string, unknown>;
  const status = (result['status'] as ToolStatus) ?? 'completed';
  const modified = overrides?.modified ?? (status === 'applied');

  const base: ToolResultBase = {
    ok: true,
    schema: TOOL_RESULT_SCHEMA,
    operation,
    trace_id: traceId,
    status,
    modified,
    target: overrides?.target as ToolResultTarget | undefined,
    data: overrides?.data ?? (result as Record<string, unknown>),
  };

  if (overrides?.validation) {
    base.validation = overrides.validation as ToolResultValidation;
  }

  return base;
}

/**
 * 将 ToolResultBase 转换为 MCP tool result 格式。
 * - content[0].text: 短摘要
 * - structuredContent: 完整 ToolResultBase
 */
export function toMcpResult(toolResult: ToolResultBase) {
  return {
    content: [
      {
        type: 'text' as const,
        text: makeSummary(toolResult),
      },
    ],
    isError: !toolResult.ok,
    structuredContent: toolResult as unknown as Record<string, unknown>,
  };
}

/**
 * 便捷构造：成功读操作。
 */
export function successRead(
  operation: string,
  target?: ToolResultTarget,
  data?: Record<string, unknown>,
) {
  return {
    ok: true,
    schema: TOOL_RESULT_SCHEMA,
    operation,
    trace_id: generateTraceId(),
    status: 'completed' as const,
    modified: false,
    target,
    data,
  };
}

/**
 * 便捷构造：成功写操作。
 */
export function successWrite(
  operation: string,
  target: ToolResultTarget,
  data?: Record<string, unknown>,
  validation?: ToolResultValidation,
): ToolResultBase {
  return {
    ok: true,
    schema: TOOL_RESULT_SCHEMA,
    operation,
    trace_id: generateTraceId(),
    status: 'applied',
    modified: true,
    target,
    data,
    validation,
  };
}

/**
 * 便捷构造：dry_run。
 */
export function successDryRun(
  operation: string,
  target: ToolResultTarget,
  dryRun: DryRunData,
): ToolResultBase {
  return {
    ok: true,
    schema: TOOL_RESULT_SCHEMA,
    operation,
    trace_id: generateTraceId(),
    status: 'dry_run',
    modified: false,
    target,
    data: { dry_run: dryRun },
  };
}

/**
 * 便捷构造：失败。
 */
export function failureResult(
  operation: string,
  error: ToolResultError,
  target?: ToolResultTarget,
): ToolResultBase {
  return {
    ok: false,
    schema: TOOL_RESULT_SCHEMA,
    operation,
    trace_id: generateTraceId(),
    status: 'failed',
    modified: false,
    target,
    error,
  };
}

// ─── Diagnostics 专用 ───

/**
 * 单条诊断代码行。
 */
export interface DiagnosticsCodeLine {
  code: string;
  extra?: string;
}

/**
 * Markdown 诊断报告。
 */
export interface DiagnosticsMarkdownReport {
  blocking: DiagnosticsCodeLine[];
  warnings: DiagnosticsCodeLine[];
  info: DiagnosticsCodeLine[];
}

/**
 * 构建 Markdown 诊断报告字符串。
 * Blocking 和 Warning 固定出现，Info 可选。
 */
export function buildDiagnosticsMarkdown(report: DiagnosticsMarkdownReport): string {
  const lines: string[] = [];

  // Blocking
  lines.push('## Blocking');
  if (report.blocking.length === 0) {
    lines.push('None');
  } else {
    for (const item of report.blocking) {
      lines.push(`- ${item.code}`);
      if (item.extra) {
        for (const extraLine of item.extra.split('\n')) {
          lines.push(`  - ${extraLine}`);
        }
      }
    }
  }

  lines.push('');
  lines.push('## Warning');
  if (report.warnings.length === 0) {
    lines.push('None');
  } else {
    for (const item of report.warnings) {
      lines.push(`- ${item.code}`);
      if (item.extra) {
        for (const extraLine of item.extra.split('\n')) {
          lines.push(`  - ${extraLine}`);
        }
      }
    }
  }

  // Info (optional section only when there are items)
  if (report.info.length > 0) {
    lines.push('');
    lines.push('## Info');
    for (const item of report.info) {
      lines.push(`- ${item.code}`);
      if (item.extra) {
        for (const extraLine of item.extra.split('\n')) {
          lines.push(`  - ${extraLine}`);
        }
      }
    }
  }

  return lines.join('\n');
}

/**
 * 构建 Diagnostics data payload。
 */
export function buildDiagnosticsData(
  mode: 'static' | 'runtime',
  markdown: string,
): Record<string, unknown> {
  return {
    schema: 'Diagnostics.v1',
    mode,
    format: 'markdown',
    markdown,
  };
}
