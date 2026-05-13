/**
 * BlueprintHelper MCP 鈥?Tool Result Base 鏍囧噯鍖?
 *
 * 绗?0 绨囷細缁熶竴鎵€鏈?MCP 宸ュ叿杩斿洖浣撶殑鍩虹瀛楁銆?
 * 浠?Bridge 鍘熷鍝嶅簲鏄犲皠鍒?Agent 鍙鐨勭簿绠€ Tool Result Base銆?
 *
 * 绾︽潫锛?
 * - 涓嶈繑鍥?tool / command / request_id / diagnostics / next / ownership 椤跺眰瀛楁 / safety_profile銆?
 * - content[0].text 鍙斁鐭憳瑕併€?
 * - structuredContent 鏀惧畬鏁?ToolResultBase JSON銆?
 */

import { BridgeResponse } from '../bridge/bridge-client.js';

// 鈹€鈹€鈹€ 鏋氫妇鍊煎父閲?鈹€鈹€鈹€

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

// 鈹€鈹€鈹€ 瀛愮粨鏋勭被鍨?鈹€鈹€鈹€

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

// 鈹€鈹€鈹€ 椤跺眰 ToolResultBase 鈹€鈹€鈹€

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

// 鈹€鈹€鈹€ Schema 鐗堟湰 鈹€鈹€鈹€

export const TOOL_RESULT_SCHEMA = 'BlueprintHelper.McpToolResult.v1';

// 鈹€鈹€鈹€ 鐢熸垚鍞竴 ID 鈹€鈹€鈹€

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

// 鈹€鈹€鈹€ 鐭憳瑕佺敓鎴?鈹€鈹€鈹€

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

// 鈹€鈹€鈹€ 浠?Bridge 鍘熷鍝嶅簲鏍囧噯鍖?鈹€鈹€鈹€

/**
 * 灏嗗師濮?BridgeResponse 鏍囧噯鍖栦负 ToolResultBase銆?
 * 鐢ㄤ簬 MCP 宸ュ叿 handler 涓浛鎹?toToolResult()銆?
 *
 * @param resp - Bridge 鍘熷鍝嶅簲
 * @param operation - 鍏叡鎿嶄綔鍚嶏紙濡?append_blueprint_graph锛?
 * @param overrides - 鍙€夎鐩栧瓧娈碉紙濡?target, transaction, validation锛?
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
      message: resp.message ?? '鏈煡 Bridge 閿欒',
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
 * 灏?ToolResultBase 杞崲涓?MCP tool result 鏍煎紡銆?
 * - content[0].text: 鐭憳瑕?
 * - structuredContent: 瀹屾暣 ToolResultBase
 */
/**
 * 渚挎嵎鏋勯€狅細鎴愬姛璇绘搷浣溿€?
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
 * 渚挎嵎鏋勯€狅細鎴愬姛鍐欐搷浣溿€?
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
 * 渚挎嵎鏋勯€狅細dry_run銆?
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
 * 渚挎嵎鏋勯€狅細澶辫触銆?
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

// 鈹€鈹€鈹€ Diagnostics 涓撶敤 鈹€鈹€鈹€

/**
 * 鍗曟潯璇婃柇浠ｇ爜琛屻€?
 */
export interface DiagnosticsCodeLine {
  code: string;
  extra?: string;
}

/**
 * Markdown 璇婃柇鎶ュ憡銆?
 */
export interface DiagnosticsMarkdownReport {
  blocking: DiagnosticsCodeLine[];
  warnings: DiagnosticsCodeLine[];
  info: DiagnosticsCodeLine[];
}

/**
 * 鏋勫缓 Markdown 璇婃柇鎶ュ憡瀛楃涓层€?
 * Blocking 鍜?Warning 鍥哄畾鍑虹幇锛孖nfo 鍙€夈€?
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
 * 鏋勫缓 Diagnostics data payload銆?
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
