import type {
  MetricsIoInputSource,
  MetricsIoSummary,
} from '@blueprinthelper/task-core/metrics/metrics-types';

export function createInputIoSummary(inputSource: MetricsIoInputSource, text: string): MetricsIoSummary {
  const chars = countChars(text);
  return {
    input_source: inputSource,
    input_chars: chars,
    input_utf8_bytes: Buffer.byteLength(text, 'utf8'),
    estimated_input_tokens: estimateTokens(chars),
  };
}

export function createOutputIoSummary(text: string): MetricsIoSummary {
  const chars = countChars(text);
  return {
    output_chars: chars,
    output_utf8_bytes: Buffer.byteLength(text, 'utf8'),
    estimated_output_tokens: estimateTokens(chars),
  };
}

function countChars(text: string): number {
  return Array.from(text).length;
}

function estimateTokens(chars: number): number {
  return Math.ceil(chars / 4);
}
