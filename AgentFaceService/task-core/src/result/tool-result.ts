/**
 * BlueprintHelper MCP 閳?Tool Result Base 閺嶅洤鍣崠?
 *
 * 缁?0 缁ㄥ浄绱扮紒鐔剁閹碘偓閺?MCP 瀹搞儱鍙挎潻鏂挎礀娴ｆ挾娈戦崺铏诡攨鐎涙顔岄妴?
 * 娴?Bridge 閸樼喎顫愰崫宥呯安閺勭姴鐨犻崚?Agent 閸欘垵顫嗛惃鍕翱缁犫偓 Tool Result Base閵?
 *
 * 缁撅附娼敍?
 * - 娑撳秷绻戦崶?tool / command / request_id / diagnostics / next / ownership 妞よ泛鐪扮€涙顔?/ safety_profile閵?
 * - content[0].text 閸欘亝鏂侀惌顓熸喅鐟曚降鈧?
 * - structuredContent 閺€鎯х暚閺?ToolResultBase JSON閵?
 */

import { BridgeResponse } from '../bridge/bridge-client.js';

// 閳光偓閳光偓閳光偓 閺嬫矮濡囬崐鐓庣埗闁?閳光偓閳光偓閳光偓

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

// 閳光偓閳光偓閳光偓 鐎涙劗绮ㄩ弸鍕閸?閳光偓閳光偓閳光偓

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

// 閳光偓閳光偓閳光偓 妞よ泛鐪?ToolResultBase 閳光偓閳光偓閳光偓

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

// 閳光偓閳光偓閳光偓 Schema 閻楀牊婀?閳光偓閳光偓閳光偓

export const TOOL_RESULT_SCHEMA = 'BlueprintHelper.ToolResult.v1';

const AGENT_FACING_REDIRECTED_KEYS = new Set([
  'atomic_targets',
  'before_snapshot_json',
  'after_snapshot_json',
  'rollback_data',
  'target_key',
  'target_keys',
  'visual_group_key',
]);

function isAgentFacingInternalKey(key: string): boolean {
  const normalized = key.toLowerCase();
  const tokenized = normalized.replace(/[^a-z0-9]+/g, '_');
  const compact = normalized.replace(/[^a-z0-9]+/g, '');
  const isGuidKey =
    tokenized === 'guid' ||
    tokenized === 'guids' ||
    tokenized.startsWith('guid_') ||
    tokenized.startsWith('guids_') ||
    tokenized.endsWith('_guid') ||
    tokenized.endsWith('_guids') ||
    tokenized.includes('_guid_') ||
    tokenized.includes('_guids_') ||
    compact.endsWith('guid') ||
    compact.endsWith('guids');
  return isGuidKey || AGENT_FACING_REDIRECTED_KEYS.has(normalized);
}

export function sanitizeAgentFacingValue<T>(value: T): T {
  return sanitizeAgentFacingUnknown(value, new WeakSet<object>()) as T;
}

export function sanitizeAgentFacingToolResult(result: ToolResultBase): ToolResultBase {
  return sanitizeAgentFacingValue(result);
}

function sanitizeAgentFacingUnknown(value: unknown, seen: WeakSet<object>): unknown {
  if (Array.isArray(value)) {
    return value
      .map((item) => sanitizeAgentFacingUnknown(item, seen))
      .filter((item) => item !== undefined);
  }

  if (value === null || typeof value !== 'object') {
    return value;
  }

  if (seen.has(value)) {
    return undefined;
  }
  seen.add(value);

  const sanitized: Record<string, unknown> = {};
  for (const [key, entryValue] of Object.entries(value as Record<string, unknown>)) {
    if (isAgentFacingInternalKey(key)) {
      continue;
    }
    const nextValue = sanitizeAgentFacingUnknown(entryValue, seen);
    if (nextValue !== undefined) {
      sanitized[key] = nextValue;
    }
  }

  seen.delete(value);
  return sanitized;
}

// 閳光偓閳光偓閳光偓 閻㈢喐鍨氶崬顖欑 ID 閳光偓閳光偓閳光偓

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

// 閳光偓閳光偓閳光偓 閻厽鎲崇憰浣烘晸閹?閳光偓閳光偓閳光偓

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

// 閳光偓閳光偓閳光偓 娴?Bridge 閸樼喎顫愰崫宥呯安閺嶅洤鍣崠?閳光偓閳光偓閳光偓

/**
 * 鐏忓棗甯慨?BridgeResponse 閺嶅洤鍣崠鏍﹁礋 ToolResultBase閵?
 * 閻劋绨?MCP 瀹搞儱鍙?handler 娑擃厽娴涢幑?toToolResult()閵?
 *
 * @param resp - Bridge 閸樼喎顫愰崫宥呯安
 * @param operation - 閸忣剙鍙￠幙宥勭稊閸氬稄绱欐俊?append_blueprint_graph閿?
 * @param overrides - 閸欘垶鈧顩惄鏍х摟濞堢绱欐俊?target, transaction, validation閿?
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
      message: resp.message ?? 'Bridge request failed.',
      retryable: false,
      rollback_result: 'not_needed',
      ...overrides?.error,
    };

    return sanitizeAgentFacingToolResult({
      ok: false,
      schema: TOOL_RESULT_SCHEMA,
      operation,
      trace_id: traceId,
      status: 'failed',
      modified: false,
      target: overrides?.target as ToolResultTarget | undefined,
      error,
    });
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

  return sanitizeAgentFacingToolResult(base);
}

/**
 * 鐏?ToolResultBase 鏉烆剚宕叉稉?MCP tool result 閺嶇厧绱￠妴?
 * - content[0].text: 閻厽鎲崇憰?
 * - structuredContent: 鐎瑰本鏆?ToolResultBase
 */
/**
 * 娓氭寧宓庨弸鍕偓鐙呯窗閹存劕濮涚拠缁樻惙娴ｆ嚎鈧?
 */
export function successRead(
  operation: string,
  target?: ToolResultTarget,
  data?: Record<string, unknown>,
): ToolResultBase {
  return sanitizeAgentFacingToolResult({
    ok: true,
    schema: TOOL_RESULT_SCHEMA,
    operation,
    trace_id: generateTraceId(),
    status: 'completed' as const,
    modified: false,
    target,
    data,
  });
}

/**
 * 娓氭寧宓庨弸鍕偓鐙呯窗閹存劕濮涢崘娆愭惙娴ｆ嚎鈧?
 */
export function successWrite(
  operation: string,
  target: ToolResultTarget,
  data?: Record<string, unknown>,
  validation?: ToolResultValidation,
): ToolResultBase {
  return sanitizeAgentFacingToolResult({
    ok: true,
    schema: TOOL_RESULT_SCHEMA,
    operation,
    trace_id: generateTraceId(),
    status: 'applied',
    modified: true,
    target,
    data,
    validation,
  });
}

/**
 * 娓氭寧宓庨弸鍕偓鐙呯窗dry_run閵?
 */
export function successDryRun(
  operation: string,
  target: ToolResultTarget,
  dryRun: DryRunData,
): ToolResultBase {
  return sanitizeAgentFacingToolResult({
    ok: true,
    schema: TOOL_RESULT_SCHEMA,
    operation,
    trace_id: generateTraceId(),
    status: 'dry_run',
    modified: false,
    target,
    data: { dry_run: dryRun },
  });
}

/**
 * 娓氭寧宓庨弸鍕偓鐙呯窗婢惰精瑙﹂妴?
 */
export function failureResult(
  operation: string,
  error: ToolResultError,
  target?: ToolResultTarget,
): ToolResultBase {
  return sanitizeAgentFacingToolResult({
    ok: false,
    schema: TOOL_RESULT_SCHEMA,
    operation,
    trace_id: generateTraceId(),
    status: 'failed',
    modified: false,
    target,
    error,
  });
}

// 閳光偓閳光偓閳光偓 Diagnostics 娑撴挾鏁?閳光偓閳光偓閳光偓

/**
 * 閸楁洘娼拠濠冩焽娴狅絿鐖滅悰灞烩偓?
 */
export interface DiagnosticsCodeLine {
  code: string;
  extra?: string;
}

/**
 * Markdown 鐠囧﹥鏌囬幎銉ユ啞閵?
 */
export interface DiagnosticsMarkdownReport {
  blocking: DiagnosticsCodeLine[];
  warnings: DiagnosticsCodeLine[];
  info: DiagnosticsCodeLine[];
}

/**
 * 閺嬪嫬缂?Markdown 鐠囧﹥鏌囬幎銉ユ啞鐎涙顑佹稉灞傗偓?
 * Blocking 閸?Warning 閸ュ搫鐣鹃崙铏瑰箛閿涘瓥nfo 閸欘垶鈧鈧?
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
 * 閺嬪嫬缂?Diagnostics data payload閵?
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

