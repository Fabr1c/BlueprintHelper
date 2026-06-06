import type { BlueprintHelperToolContext } from '../types.js';

export async function readReferenceContext(input: Record<string, unknown>, context: BlueprintHelperToolContext) {
  return await context.taskRunner.readReferenceContext(input);
}
