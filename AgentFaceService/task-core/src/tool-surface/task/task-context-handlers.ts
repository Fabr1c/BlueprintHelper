import { ReadTaskContextInputSchema } from '../../task/schema/task-schemas.js';
import type { BlueprintHelperToolContext } from '../types.js';
import { ReadReferenceContextInputSchema } from './read-reference-context-schema.js';

export async function readTaskContext(input: Record<string, unknown>, context: BlueprintHelperToolContext) {
  return await context.taskRunner.readTaskContext(ReadTaskContextInputSchema.parse(input));
}

export async function readReferenceContext(input: Record<string, unknown>, context: BlueprintHelperToolContext) {
  return await context.taskRunner.readReferenceContext(ReadReferenceContextInputSchema.parse(input));
}
