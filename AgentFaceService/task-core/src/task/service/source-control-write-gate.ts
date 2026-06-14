import type { TaskRunnerBridge } from './task-spec-runner.js';

export interface SourceControlWriteGateOptions {
  readonly autoCheckout: boolean;
}

export interface SourceControlWriteGateResult {
  readonly ok: boolean;
  readonly code?: string;
  readonly message: string;
  readonly data?: Record<string, unknown>;
}

export async function runSourceControlWriteGate(
  bridge: TaskRunnerBridge,
  assetPaths: readonly string[],
  options: SourceControlWriteGateOptions,
): Promise<SourceControlWriteGateResult> {
  const uniqueAssetPaths = [...new Set(assetPaths.map((path) => path.trim()).filter((path) => path.length > 0))];
  if (uniqueAssetPaths.length === 0) {
    return {
      ok: false,
      code: 'source_control_target_missing',
      message: 'execute_task requires non-empty target_assets before source-control write gate can run.',
      data: { target_assets: [] },
    };
  }

  const status = await bridge.sendCommand('source_control_status', { asset_paths: uniqueAssetPaths });
  if (!status.success) {
    return bridgeFailureGateResult(status, 'source_control_status_failed', 'Source-control status check failed.');
  }
  const sourceControl = extractSourceControlResult(status.result);
  const statusCode = sourceControlString(sourceControl, 'status');

  if (isEditableSourceControlStatus(statusCode, sourceControl)) {
    return { ok: true, message: 'Source-control write gate passed.', data: sourceControl };
  }

  if (statusCode === 'checkout_required' && options.autoCheckout) {
    const checkout = await bridge.sendCommand('source_control_checkout', { asset_paths: uniqueAssetPaths });
    if (!checkout.success) {
      return bridgeFailureGateResult(checkout, 'source_control_checkout_failed', 'Source-control checkout failed.');
    }
    const checkoutSourceControl = extractSourceControlResult(checkout.result);
    const checkoutCode = sourceControlString(checkoutSourceControl, 'status');
    if (isEditableSourceControlStatus(checkoutCode, checkoutSourceControl)) {
      return { ok: true, message: 'Source-control checkout completed.', data: checkoutSourceControl };
    }
    return {
      ok: false,
      code: checkoutCode ?? 'checkout_failed',
      message: 'Source-control checkout did not make all target assets editable.',
      data: checkoutSourceControl,
    };
  }

  return {
    ok: false,
    code: statusCode ?? 'source_control_gate_failed',
    message: 'Run blueprinthelper_source_control_checkout for the same asset_paths before execute_task.',
    data: sourceControl,
  };
}

function bridgeFailureGateResult(
  response: Awaited<ReturnType<TaskRunnerBridge['sendCommand']>>,
  fallbackCode: string,
  fallbackMessage: string,
): SourceControlWriteGateResult {
  return {
    ok: false,
    code: response.error_code ?? fallbackCode,
    message: response.message ?? fallbackMessage,
    data: extractSourceControlResult(response.result) ?? {
      request_id: response.request_id,
      success: response.success,
      ...(response.error_code ? { error_code: response.error_code } : {}),
      ...(response.message ? { message: response.message } : {}),
    },
  };
}

function extractSourceControlResult(value: unknown): Record<string, unknown> | undefined {
  const record = asRecord(value);
  return asRecord(record?.['source_control']) ?? asRecord(record?.['data']) ?? record;
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return value !== null && typeof value === 'object' && !Array.isArray(value)
    ? value as Record<string, unknown>
    : undefined;
}

function sourceControlString(record: Record<string, unknown> | undefined, field: string): string | undefined {
  const value = record?.[field];
  return typeof value === 'string' ? value : undefined;
}

function isEditableSourceControlStatus(
  status: string | undefined,
  sourceControl: Record<string, unknown> | undefined,
): boolean {
  if (status === 'editable' || status === 'not_source_controlled' || status === 'source_control_disabled') {
    return true;
  }

  const files = sourceControl?.['files'];
  if (!Array.isArray(files) || files.length === 0) {
    return false;
  }

  return files.every((file) => {
    const record = asRecord(file);
    const fileStatus = sourceControlString(record, 'status');
    return record?.['editable'] === true ||
      fileStatus === 'editable' ||
      fileStatus === 'not_source_controlled' ||
      fileStatus === 'source_control_disabled';
  });
}
