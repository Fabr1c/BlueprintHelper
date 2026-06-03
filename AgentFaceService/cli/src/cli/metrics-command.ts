import { mkdir, writeFile } from 'node:fs/promises';
import path from 'node:path';

import {
  buildMetricsReport,
  renderMetricsMarkdown,
  type MetricsReportKind,
} from '@blueprinthelper/task-core/metrics/metrics-reporter';
import {
  createMetricsStore,
  type MetricsWindow,
} from '@blueprinthelper/task-core/metrics/metrics-store';
import {
  successRead,
  type ToolResultBase,
} from '@blueprinthelper/task-core/result/tool-result';

export interface MetricsCliCommand {
  kind: 'metrics.report';
  format: 'json' | 'markdown';
  metricsKind: MetricsReportKind;
  metricsRoot: string;
  window: MetricsWindow;
  limit: number;
}

interface RunMetricsCommandInput {
  command: MetricsCliCommand;
}

export async function runMetricsCommand(input: RunMetricsCommandInput): Promise<ToolResultBase> {
  const { command } = input;
  const store = createMetricsStore({ root: command.metricsRoot });
  const report = await buildMetricsReport({
    store,
    window: command.window,
    limit: command.limit,
    kind: command.metricsKind,
  });

  const data: Record<string, unknown> = {
    ...report,
  };

  if (command.format === 'markdown') {
    const markdownReportPath = await writeMetricsMarkdownReport(command, report);
    data['markdown_report_path'] = markdownReportPath;
  }

  return successRead('metrics.report', undefined, data);
}

async function writeMetricsMarkdownReport(
  command: MetricsCliCommand,
  report: Awaited<ReturnType<typeof buildMetricsReport>>,
): Promise<string> {
  const reportsDir = path.join(command.metricsRoot, 'reports');
  await mkdir(reportsDir, { recursive: true });
  const timestamp = report.generated_at.replace(/[:]/g, '-').replace(/\.\d{3}Z$/, 'Z');
  const fileName = `${command.metricsKind}-${command.window}-${timestamp}.md`;
  const reportPath = path.join(reportsDir, fileName);
  await writeFile(reportPath, `${renderMetricsMarkdown(report)}\n`, 'utf8');
  return reportPath;
}
