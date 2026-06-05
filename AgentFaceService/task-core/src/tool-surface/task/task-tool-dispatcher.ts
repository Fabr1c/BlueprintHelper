import type { BlueprintHelperToolContext } from '../types.js';
import {
  createDefaultTaskToolHandlerRegistry,
  type TaskToolName,
} from './task-tool-handler-registry.js';

const defaultTaskToolHandlerRegistry = createDefaultTaskToolHandlerRegistry();

export async function executeTaskTool(
  name: TaskToolName,
  input: Record<string, unknown>,
  context: BlueprintHelperToolContext,
) {
  return await defaultTaskToolHandlerRegistry.require(name).execute(input, context);
}

export function getDefaultTaskToolHandlerRegistry() {
  return defaultTaskToolHandlerRegistry;
}
