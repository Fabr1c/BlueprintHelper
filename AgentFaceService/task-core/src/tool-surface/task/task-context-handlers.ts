import type { BlueprintHelperToolContext } from '../types.js';
import { ReadReferenceContextInputSchema } from './read-reference-context-schema.js';

export async function readReferenceContext(input: Record<string, unknown>, context: BlueprintHelperToolContext) {
  return await context.taskRunner.readReferenceContext(ReadReferenceContextInputSchema.parse(input));
}
