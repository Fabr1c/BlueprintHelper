import {
  makeSummary,
  type ToolResultBase,
} from '@blueprinthelper/task-core/result/tool-result';

export * from '@blueprinthelper/task-core/result/tool-result';

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
