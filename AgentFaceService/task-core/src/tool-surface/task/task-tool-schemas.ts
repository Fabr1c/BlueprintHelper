import { createDefaultTaskToolHandlerRegistry, type TaskToolName } from './task-tool-handler-registry.js';

const descriptorRegistry = createDefaultTaskToolHandlerRegistry();

export const taskToolSchemas = Object.fromEntries(
  descriptorRegistry.list().map((descriptor) => [descriptor.toolName, descriptor.inputSchema]),
) as Record<TaskToolName, ReturnType<typeof descriptorRegistry.require>['inputSchema']>;

export type { TaskToolName };
