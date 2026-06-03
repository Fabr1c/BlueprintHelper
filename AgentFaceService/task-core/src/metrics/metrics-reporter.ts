import type { MetricsErrorCategory, MetricsEvent, MetricsStatus, MetricsTaskKey } from './metrics-types.js';
import type { MetricsEpisode, MetricsStore, MetricsWindow } from './metrics-store.js';

export type MetricsReportKind = 'report' | 'top-errors' | 'tool-usage' | 'task-health';

export interface BuildMetricsReportOptions {
  store: Pick<MetricsStore, 'readEvents' | 'readOpenEpisodes' | 'readClosedEpisodes'>;
  window: MetricsWindow;
  limit?: number;
  kind: MetricsReportKind;
}

export interface MetricsReport {
  schema: 'BlueprintHelper.MetricsReport.v1';
  generated_at: string;
  window: MetricsWindow;
  kind: MetricsReportKind;
  summary: MetricsReportSummary;
  tool_usage: MetricsUsageRow[];
  io_usage: MetricsIoUsageRow[];
  top_errors: MetricsTopErrorRow[];
  task_health: MetricsTaskHealthRow[];
  operation_usage: MetricsOperationUsageRow[];
  error_category_distribution: MetricsErrorCategoryDistributionRow[];
  unknown_errors: MetricsTopErrorRow[];
  stale_open: MetricsStaleOpenSummary;
}

export interface MetricsReportSummary {
  total_events: number;
  total_failures: number;
  unknown_errors: number;
  open_episodes: number;
  stale_open_episodes: number;
}

export interface MetricsUsageRow {
  tool_name: string;
  total: number;
  success: number;
  failed: number;
  success_rate: number;
}

export interface MetricsIoUsageRow {
  tool_name: string;
  total: number;
  input_chars_total: number;
  input_utf8_bytes_total: number;
  output_chars_total: number;
  output_utf8_bytes_total: number;
  estimated_input_tokens_total: number;
  estimated_output_tokens_total: number;
  input_chars_avg: number;
  output_chars_avg: number;
}

export interface MetricsTopErrorRow {
  error_category: MetricsErrorCategory;
  error_code: string;
  issue_code?: string;
  count: number;
}

export interface MetricsTaskHealthRow {
  task_key: MetricsTaskKey;
  preview_attempts: number;
  execute_attempts: number;
  failed_attempts: number;
  success_attempts: number;
  attempts_to_success?: number;
  first_attempt_at: string;
  last_attempt_at: string;
  closed_at?: string;
  close_reason?: 'success' | 'stale_open';
}

export interface MetricsOperationUsageRow {
  capability: string;
  semantic_operation: string;
  total: number;
  success: number;
  failed: number;
  success_rate: number;
}

export interface MetricsErrorCategoryDistributionRow {
  error_category: MetricsErrorCategory;
  count: number;
}

export interface MetricsStaleOpenSummary {
  count: number;
  episodes: MetricsTaskHealthRow[];
}

export async function buildMetricsReport(options: BuildMetricsReportOptions): Promise<MetricsReport> {
  const limit = options.limit ?? 20;
  const [events, openEpisodes, closedEpisodes] = await Promise.all([
    options.store.readEvents(options.window),
    options.store.readOpenEpisodes(),
    options.store.readClosedEpisodes(options.window),
  ]);
  const failures = events.filter((event) => event.status !== 'success');
  const allTopErrors = buildTopErrors(failures);
  const allTaskHealth = buildTaskHealth([...closedEpisodes, ...openEpisodes]);
  const allStaleOpenEpisodes = allTaskHealth.filter((entry) => entry.close_reason === 'stale_open');
  const includeErrors = includesErrorSections(options.kind);
  const includeUsage = includesUsageSections(options.kind);
  const includeTaskHealth = includesTaskHealthSections(options.kind);

  return {
    schema: 'BlueprintHelper.MetricsReport.v1',
    generated_at: new Date().toISOString(),
    window: options.window,
    kind: options.kind,
    summary: {
      total_events: events.length,
      total_failures: failures.length,
      unknown_errors: failures.filter((event) => normalizeCategory(event.error_category) === 'unknown').length,
      open_episodes: openEpisodes.length,
      stale_open_episodes: allStaleOpenEpisodes.length,
    },
    tool_usage: includeUsage ? limitRows(buildToolUsage(events), limit) : [],
    io_usage: includeUsage ? limitRows(buildIoUsage(events), limit) : [],
    top_errors: includeErrors ? limitRows(allTopErrors, limit) : [],
    task_health: includeTaskHealth ? limitRows(allTaskHealth, limit) : [],
    operation_usage: includeUsage ? limitRows(buildOperationUsage(events), limit) : [],
    error_category_distribution: includeErrors ? buildErrorCategoryDistribution(failures) : [],
    unknown_errors: includeErrors
      ? limitRows(allTopErrors.filter((entry) => entry.error_category === 'unknown'), limit)
      : [],
    stale_open: {
      count: allStaleOpenEpisodes.length,
      episodes: includeTaskHealth ? limitRows(allStaleOpenEpisodes, limit) : [],
    },
  };
}

export function renderMetricsMarkdown(report: MetricsReport): string {
  const lines = [
    '# BlueprintHelper Metrics Report',
    '',
    `- schema: \`${report.schema}\``,
    `- generated_at: \`${report.generated_at}\``,
    `- window: \`${report.window}\``,
    `- kind: \`${report.kind}\``,
    `- total_events: ${report.summary.total_events}`,
    `- total_failures: ${report.summary.total_failures}`,
    `- unknown_errors: ${report.summary.unknown_errors}`,
    `- open_episodes: ${report.summary.open_episodes}`,
    `- stale_open_episodes: ${report.summary.stale_open_episodes}`,
    '',
  ];

  if (includesUsageSections(report.kind)) {
    lines.push(renderTable('Tool Usage', ['tool_name', 'total', 'success', 'failed', 'success_rate'], report.tool_usage.map((row) => [
      row.tool_name,
      row.total,
      row.success,
      row.failed,
      formatRate(row.success_rate),
    ])));
    lines.push(renderTable('IO Usage', [
      'tool_name',
      'total',
      'input_chars_total',
      'input_utf8_bytes_total',
      'output_chars_total',
      'output_utf8_bytes_total',
      'estimated_input_tokens_total',
      'estimated_output_tokens_total',
      'input_chars_avg',
      'output_chars_avg',
    ], report.io_usage.map((row) => [
      row.tool_name,
      row.total,
      row.input_chars_total,
      row.input_utf8_bytes_total,
      row.output_chars_total,
      row.output_utf8_bytes_total,
      row.estimated_input_tokens_total,
      row.estimated_output_tokens_total,
      formatNumber(row.input_chars_avg),
      formatNumber(row.output_chars_avg),
    ])));
  }

  if (includesErrorSections(report.kind)) {
    lines.push(renderTable('Top Errors', ['error_category', 'error_code', 'issue_code', 'count'], report.top_errors.map((row) => [
      row.error_category,
      row.error_code,
      row.issue_code ?? '',
      row.count,
    ])));
  }

  if (includesTaskHealthSections(report.kind)) {
    lines.push(renderTable('Task Health', [
      'task_type',
      'feature_name',
      'target_type',
      'preview_attempts',
      'execute_attempts',
      'failed_attempts',
      'success_attempts',
      'attempts_to_success',
      'close_reason',
    ], report.task_health.map((row) => [
      row.task_key.task_type,
      row.task_key.feature_name ?? '',
      row.task_key.target_type,
      row.preview_attempts,
      row.execute_attempts,
      row.failed_attempts,
      row.success_attempts,
      row.attempts_to_success ?? '',
      row.close_reason ?? '',
    ])));
  }

  if (includesUsageSections(report.kind)) {
    lines.push(renderTable('Operation Usage', ['capability', 'semantic_operation', 'total', 'success', 'failed', 'success_rate'], report.operation_usage.map((row) => [
      row.capability,
      row.semantic_operation,
      row.total,
      row.success,
      row.failed,
      formatRate(row.success_rate),
    ])));
  }

  if (includesErrorSections(report.kind)) {
    lines.push(renderTable('Error Category Distribution', ['error_category', 'count'], report.error_category_distribution.map((row) => [
      row.error_category,
      row.count,
    ])));
    lines.push(renderTable('Unknown Errors', ['error_category', 'error_code', 'issue_code', 'count'], report.unknown_errors.map((row) => [
      row.error_category,
      row.error_code,
      row.issue_code ?? '',
      row.count,
    ])));
  }

  if (includesTaskHealthSections(report.kind)) {
    lines.push(renderTable('Stale Open', [
      'task_type',
      'feature_name',
      'target_type',
      'last_attempt_at',
      'failed_attempts',
      'close_reason',
    ], report.stale_open.episodes.map((row) => [
      row.task_key.task_type,
      row.task_key.feature_name ?? '',
      row.task_key.target_type,
      row.last_attempt_at,
      row.failed_attempts,
      row.close_reason ?? '',
    ])));
  }

  return lines.join('\n');
}

function buildToolUsage(events: MetricsEvent[]): MetricsUsageRow[] {
  const groups = new Map<string, MetricsUsageRow>();

  for (const event of events) {
    if (isIoEvent(event)) {
      continue;
    }
    if (event.tool_name === undefined) {
      continue;
    }
    const row = groups.get(event.tool_name) ?? {
      tool_name: event.tool_name,
      total: 0,
      success: 0,
      failed: 0,
      success_rate: 0,
    };
    applyStatus(row, event.status);
    groups.set(event.tool_name, row);
  }

  return sortUsageRows([...groups.values()].map(finalizeUsageRow));
}

function buildOperationUsage(events: MetricsEvent[]): MetricsOperationUsageRow[] {
  const groups = new Map<string, MetricsOperationUsageRow>();

  for (const event of events) {
    if (isIoEvent(event)) {
      continue;
    }
    if (event.capability === undefined && event.semantic_operation === undefined) {
      continue;
    }
    const capability = event.capability ?? 'unknown_capability';
    const semanticOperation = event.semantic_operation ?? 'unknown_operation';
    const key = `${capability}\u0000${semanticOperation}`;
    const row = groups.get(key) ?? {
      capability,
      semantic_operation: semanticOperation,
      total: 0,
      success: 0,
      failed: 0,
      success_rate: 0,
    };
    applyStatus(row, event.status);
    groups.set(key, row);
  }

  return sortUsageRows([...groups.values()].map(finalizeUsageRow));
}

function buildIoUsage(events: MetricsEvent[]): MetricsIoUsageRow[] {
  const groups = new Map<string, MetricsIoUsageRow>();

  for (const event of events) {
    if (!isIoEvent(event) || event.io === undefined) {
      continue;
    }
    const toolName = event.tool_name ?? 'unknown_tool';
    const row = groups.get(toolName) ?? {
      tool_name: toolName,
      total: 0,
      input_chars_total: 0,
      input_utf8_bytes_total: 0,
      output_chars_total: 0,
      output_utf8_bytes_total: 0,
      estimated_input_tokens_total: 0,
      estimated_output_tokens_total: 0,
      input_chars_avg: 0,
      output_chars_avg: 0,
    };
    row.total += 1;
    row.input_chars_total += readMetricNumber(event.io.input_chars);
    row.input_utf8_bytes_total += readMetricNumber(event.io.input_utf8_bytes);
    row.output_chars_total += readMetricNumber(event.io.output_chars);
    row.output_utf8_bytes_total += readMetricNumber(event.io.output_utf8_bytes);
    row.estimated_input_tokens_total += readMetricNumber(event.io.estimated_input_tokens);
    row.estimated_output_tokens_total += readMetricNumber(event.io.estimated_output_tokens);
    groups.set(toolName, row);
  }

  return [...groups.values()]
    .map(finalizeIoUsageRow)
    .sort((left, right) => {
      const totalBytesDiff =
        (right.input_utf8_bytes_total + right.output_utf8_bytes_total)
        - (left.input_utf8_bytes_total + left.output_utf8_bytes_total);
      if (totalBytesDiff !== 0) {
        return totalBytesDiff;
      }
      return compareText(left.tool_name, right.tool_name);
    });
}

function buildTopErrors(events: MetricsEvent[]): MetricsTopErrorRow[] {
  const groups = new Map<string, MetricsTopErrorRow>();

  for (const event of events) {
    const errorCategory = normalizeCategory(event.error_category);
    const errorCode = event.error_code ?? event.issue?.code ?? 'unknown_error';
    const issueCode = event.issue?.code;
    const key = `${errorCategory}\u0000${errorCode}\u0000${issueCode ?? ''}`;
    const row = groups.get(key) ?? {
      error_category: errorCategory,
      error_code: errorCode,
      ...(issueCode ? { issue_code: issueCode } : {}),
      count: 0,
    };
    row.count += 1;
    groups.set(key, row);
  }

  return [...groups.values()].sort((left, right) => {
    const countDiff = right.count - left.count;
    if (countDiff !== 0) {
      return countDiff;
    }
    const categoryDiff = compareText(left.error_category, right.error_category);
    return categoryDiff !== 0 ? categoryDiff : compareText(left.error_code, right.error_code);
  });
}

function buildErrorCategoryDistribution(events: MetricsEvent[]): MetricsErrorCategoryDistributionRow[] {
  const groups = new Map<MetricsErrorCategory, number>();

  for (const event of events) {
    const category = normalizeCategory(event.error_category);
    groups.set(category, (groups.get(category) ?? 0) + 1);
  }

  return [...groups.entries()]
    .map(([error_category, count]) => ({ error_category, count }))
    .sort((left, right) => {
      const countDiff = right.count - left.count;
      return countDiff !== 0 ? countDiff : compareText(left.error_category, right.error_category);
    });
}

function buildTaskHealth(episodes: MetricsEpisode[]): MetricsTaskHealthRow[] {
  return episodes
    .map((episode) => ({
      task_key: episode.task_key,
      preview_attempts: episode.preview_attempts,
      execute_attempts: episode.execute_attempts,
      failed_attempts: episode.failed_attempts,
      success_attempts: episode.success_attempts,
      attempts_to_success: episode.attempts_to_success,
      first_attempt_at: episode.first_attempt_at,
      last_attempt_at: episode.last_attempt_at,
      closed_at: episode.closed_at,
      close_reason: episode.close_reason,
    }))
    .sort((left, right) => {
      const leftAttempts = left.attempts_to_success ?? Number.MAX_SAFE_INTEGER;
      const rightAttempts = right.attempts_to_success ?? Number.MAX_SAFE_INTEGER;
      const attemptDiff = rightAttempts - leftAttempts;
      if (attemptDiff !== 0) {
        return attemptDiff;
      }
      return compareText(right.last_attempt_at, left.last_attempt_at);
    });
}

function applyStatus(row: { total: number; success: number; failed: number }, status: MetricsStatus): void {
  row.total += 1;
  if (status === 'success') {
    row.success += 1;
  } else {
    row.failed += 1;
  }
}

function finalizeUsageRow<T extends { total: number; success: number; success_rate: number }>(row: T): T {
  row.success_rate = row.total === 0 ? 0 : row.success / row.total;
  return row;
}

function sortUsageRows<T extends { total: number; success_rate: number }>(rows: T[]): T[] {
  return rows.sort((left, right) => {
    const totalDiff = right.total - left.total;
    if (totalDiff !== 0) {
      return totalDiff;
    }
    return right.success_rate - left.success_rate;
  });
}

function finalizeIoUsageRow(row: MetricsIoUsageRow): MetricsIoUsageRow {
  row.input_chars_avg = row.total === 0 ? 0 : row.input_chars_total / row.total;
  row.output_chars_avg = row.total === 0 ? 0 : row.output_chars_total / row.total;
  return row;
}

function readMetricNumber(value: number | undefined): number {
  return typeof value === 'number' && Number.isFinite(value) ? value : 0;
}

function isIoEvent(event: MetricsEvent): boolean {
  return event.event_type === 'cli_io_completed';
}

function normalizeCategory(value: MetricsErrorCategory | undefined): MetricsErrorCategory {
  return value ?? 'unknown';
}

function includesErrorSections(kind: MetricsReportKind): boolean {
  return kind === 'report' || kind === 'top-errors';
}

function includesUsageSections(kind: MetricsReportKind): boolean {
  return kind === 'report' || kind === 'tool-usage';
}

function includesTaskHealthSections(kind: MetricsReportKind): boolean {
  return kind === 'report' || kind === 'task-health';
}

function limitRows<T>(rows: T[], limit: number): T[] {
  return rows.slice(0, Math.max(0, limit));
}

function formatRate(value: number): string {
  return value.toFixed(3);
}

function formatNumber(value: number): string {
  return Number.isInteger(value) ? String(value) : value.toFixed(1);
}

function renderTable(title: string, headers: string[], rows: Array<Array<string | number>>): string {
  const lines = [`## ${title}`, ''];
  if (rows.length === 0) {
    lines.push('No data.', '');
    return lines.join('\n');
  }

  lines.push(`| ${headers.join(' |')} |`);
  lines.push(`| ${headers.map(() => '---').join(' |')} |`);
  for (const row of rows) {
    lines.push(`| ${row.map((entry) => escapeMarkdownCell(String(entry))).join(' |')} |`);
  }
  lines.push('');
  return lines.join('\n');
}

function escapeMarkdownCell(value: string): string {
  return value.replaceAll('|', '\\|');
}

function compareText(left: string, right: string): number {
  return left < right ? -1 : left > right ? 1 : 0;
}
