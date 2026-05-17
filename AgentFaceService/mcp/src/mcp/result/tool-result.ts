import {
  makeSummary,
  sanitizeAgentFacingToolResult,
  type ToolResultBase,
} from '@blueprinthelper/task-core/result/tool-result';

export * from '@blueprinthelper/task-core/result/tool-result';

export function toMcpResult(toolResult: ToolResultBase) {
  const safeToolResult = sanitizeAgentFacingToolResult(toolResult);
  return {
    content: [
      {
        type: 'text' as const,
        text: makeSummary(safeToolResult),
      },
    ],
    isError: !safeToolResult.ok,
    structuredContent: safeToolResult as unknown as Record<string, unknown>,
  };
}
