import {
  failureResult,
  normalizeToolResult,
  successRead,
  type ToolResultBase,
} from '../../../result/tool-result.js';
import type { BridgeResponse } from '../../../bridge/bridge-client.js';
import type { BlueprintHelperToolContext } from '../../types.js';
import { omitUndefined } from '../bridge-tool-result-utils.js';
import { CaptureScreenshotInputSchema, type CaptureScreenshotInput } from './capture-screenshot-schema.js';

const operation = 'blueprinthelper_capture_screenshot';

export async function executeCaptureScreenshot(
  input: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Promise<ToolResultBase> {
  const parsed = CaptureScreenshotInputSchema.safeParse(input);
  if (!parsed.success) {
    return failureResult(operation, {
      code: 'invalid_capture_screenshot_input',
      stage: 'parse_input',
      message: parsed.error.issues.map((issue) => `${issue.path.join('.')}: ${issue.message}`).join('; '),
      retryable: false,
      rollback_result: 'not_needed',
    });
  }

  const request = parsed.data;
  const target = omitUndefined({
    target_type: 'asset',
    asset_path: request.asset_path,
    graph: request.graph_name,
  }) as { target_type: 'asset'; asset_path: string; graph?: string };

  const openResponse = await context.bridge.sendCommand('open_asset', {
    asset_path: request.asset_path,
  });
  if (!openResponse.success) {
    return failedBridgeStep(openResponse, 'open_asset', target);
  }

  let focusResponse: BridgeResponse | undefined;
  if (needsFocus(request)) {
    focusResponse = await context.bridge.sendCommand('focus_blueprint_editor_target', omitUndefined({
      asset_path: request.asset_path,
      graph_name: request.graph_name,
      block_ref: request.block_ref,
      node_ref: request.node_ref,
    }));
    if (!focusResponse.success) {
      return failedBridgeStep(focusResponse, 'focus_blueprint_editor_target', target);
    }
  }

  if (request.settle_delay_ms > 0) {
    if (context.sleep) {
      await context.sleep(request.settle_delay_ms);
    } else {
      await new Promise((resolve) => setTimeout(resolve, request.settle_delay_ms));
    }
  }

  const screenshotCommand = needsFocus(request)
    ? 'capture_focused_graph_screenshot'
    : 'capture_editor_screenshot';
  const screenshotResponse = await context.bridge.sendCommand(screenshotCommand, omitUndefined({
    target: request.capture_target === 'auto' ? undefined : request.capture_target,
    label: request.label,
  }));
  if (!screenshotResponse.success) {
    return failedBridgeStep(screenshotResponse, screenshotCommand, target);
  }

  const normalizedScreenshots = normalizeScreenshots(screenshotResponse.result);

  return successRead(operation, target, omitUndefined({
    schema: 'BlueprintHelper.ScreenshotEvidence.v1',
    asset_path: request.asset_path,
    graph_name: request.graph_name,
    block_ref: request.block_ref,
    node_ref: request.node_ref,
    steps: omitUndefined({
      open_asset: openResponse.result ?? {},
      focus_blueprint_editor_target: focusResponse?.result,
    }),
    capture_command: screenshotCommand,
    screenshots: normalizedScreenshots,
    screenshot: normalizedScreenshots[0] ?? screenshotResponse.result ?? {},
    graph_capture: needsFocus(request) ? screenshotResponse.result ?? {} : undefined,
    window_capture: needsFocus(request) ? undefined : screenshotResponse.result ?? {},
    screenshot_count: normalizedScreenshots.length,
    capture_scope: needsFocus(request) ? 'graph' : 'asset_window',
  }));
}

function needsFocus(request: CaptureScreenshotInput): boolean {
  return Boolean(request.graph_name || request.block_ref || request.node_ref);
}

function normalizeScreenshots(result: Record<string, unknown> | undefined): Record<string, unknown>[] {
  if (!result) {
    return [];
  }
  const screenshots = result['screenshots'];
  if (Array.isArray(screenshots)) {
    return screenshots.filter((item): item is Record<string, unknown> => isRecord(item));
  }
  return [result];
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return Boolean(value) && typeof value === 'object' && !Array.isArray(value);
}

function failedBridgeStep(
  response: BridgeResponse,
  step: string,
  target: { target_type: 'asset'; asset_path: string; graph?: string },
): ToolResultBase {
  const normalized = normalizeToolResult(response, operation, {
    target,
    error: {
      stage: 'bridge',
      code: response.error_code ?? `${step}_failed`,
      message: response.message ?? `${step} failed.`,
      retryable: false,
      rollback_result: 'not_needed',
    },
  });
  normalized.data = { failed_step: step };
  return normalized;
}
