import { successRead, type ToolResultBase } from '../../result/tool-result.js';
import type { BridgeResponse } from '../../bridge/bridge-client.js';
import type { BlueprintHelperToolContext } from '../types.js';
import { isRecord, normalizeBridgeToolResult } from './bridge-tool-result-utils.js';

export async function executeWriteSessionRequest(
  input: Record<string, unknown>,
  context: BlueprintHelperToolContext,
): Promise<ToolResultBase> {
  const payload = input;
  const response = await context.bridge.sendCommand('request_write_session', payload);
  if (response.success) {
    const sessionId = extractWriteSessionId(response);
    if (sessionId) {
      context.bridge.setWriteSessionId(sessionId);
    }
    return sanitizedWriteSessionResult(response, payload);
  }
  return normalizeBridgeToolResult('blueprinthelper_request_write_session', response);
}

function extractWriteSessionId(response: BridgeResponse): string | undefined {
  const result = isRecord(response.result) ? response.result : undefined;
  const writeSession = isRecord(result?.['write_session']) ? result['write_session'] : undefined;
  const sessionId = writeSession?.['session_id'];
  return typeof sessionId === 'string' && sessionId.length > 0 ? sessionId : undefined;
}

function sanitizedWriteSessionResult(
  response: BridgeResponse,
  payload: Record<string, unknown>,
): ToolResultBase {
  if (!response.success) {
    return normalizeBridgeToolResult('blueprinthelper_request_write_session', response);
  }
  const result = isRecord(response.result) ? response.result : {};
  const writeSession = isRecord(result['write_session']) ? result['write_session'] : {};
  const sanitizedSession: Record<string, unknown> = {
    scope: writeSession['scope'] ?? payload['scope'] ?? 'project',
    expires_at_utc: writeSession['expires_at_utc'],
  };
  if (Array.isArray(writeSession['asset_paths'])) {
    sanitizedSession['asset_paths'] = writeSession['asset_paths'];
  }
  return successRead('blueprinthelper_request_write_session', { target_type: 'asset' }, {
    schema: 'WriteSession.v1',
    write_session: sanitizedSession,
  }) as ToolResultBase;
}
