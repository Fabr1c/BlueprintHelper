import {
  ExecuteTaskInputSchema,
  PreviewTaskInputSchema,
  TaskSpecSchema,
} from '../../task/schema/task-schemas.js';
import {
  InputShapeAdapterError,
  InputShapeAdapterRegistry,
  type InputShapeAdapter,
} from './input-shape-adapter.js';

const bareTaskSpecAdapter: InputShapeAdapter<{ task_spec: unknown }> = {
  id: 'bare_taskspec',
  inputSchema: TaskSpecSchema,
  adapt(input) {
    if (input && typeof input === 'object' && !Array.isArray(input) && 'preview_token' in input) {
      throw new InputShapeAdapterError(
        'preview_token_requires_task_spec_wrapper',
        'execute_task preview_token is only accepted on the wrapped input shape: { task_spec, preview_token }.',
        'preview_token',
      );
    }
    return { task_spec: TaskSpecSchema.parse(input) };
  },
};

const wrappedPreviewTaskSpecAdapter: InputShapeAdapter<{ task_spec: unknown }> = {
  id: 'wrapped_taskspec_preview',
  inputSchema: PreviewTaskInputSchema,
  adapt(input) {
    return PreviewTaskInputSchema.parse(input) as { task_spec: unknown };
  },
};

const wrappedExecuteTaskSpecAdapter: InputShapeAdapter<{ task_spec: unknown; preview_token?: string }> = {
  id: 'wrapped_taskspec_execute',
  inputSchema: ExecuteTaskInputSchema,
  adapt(input) {
    return ExecuteTaskInputSchema.parse(input) as { task_spec: unknown; preview_token?: string };
  },
};

export function createTaskSpecInputShapeAdapterRegistry(): InputShapeAdapterRegistry {
  return new InputShapeAdapterRegistry()
    .register(bareTaskSpecAdapter)
    .register(wrappedPreviewTaskSpecAdapter)
    .register(wrappedExecuteTaskSpecAdapter);
}
