import { ZodError, type z } from 'zod';

export type InputShapeId =
  | 'bare_taskspec'
  | 'wrapped_taskspec_preview'
  | 'wrapped_taskspec_execute'
  | 'readspec'
  | 'read_reference_context'
  | 'bridge_payload'
  | 'bridge_logic_md_payload'
  | 'bridge_logic_json_payload'
  | 'tool_payload'
  | 'empty_object';

export interface InputShapeAdapter<TOutput = Record<string, unknown>> {
  readonly id: InputShapeId;
  readonly inputSchema: z.ZodTypeAny;
  adapt(input: unknown): TOutput;
}

export class InputShapeAdapterError extends Error {
  constructor(
    readonly code: string,
    message: string,
    readonly field?: string,
  ) {
    super(message);
    this.name = 'InputShapeAdapterError';
  }
}

export class InputShapeAdapterRegistry {
  private readonly adapters = new Map<InputShapeId, InputShapeAdapter>();

  register(adapter: InputShapeAdapter): this {
    if (this.adapters.has(adapter.id)) {
      throw new Error(`Input shape adapter is already registered: ${adapter.id}`);
    }
    this.adapters.set(adapter.id, adapter);
    return this;
  }

  get(id: InputShapeId): InputShapeAdapter | undefined {
    return this.adapters.get(id);
  }

  require(id: InputShapeId): InputShapeAdapter {
    const adapter = this.get(id);
    if (!adapter) {
      throw new Error(`Input shape adapter is not registered: ${id}`);
    }
    return adapter;
  }
}

export function adaptToolInput(
  registry: InputShapeAdapterRegistry,
  inputShapeIds: readonly InputShapeId[],
  input: unknown,
): Record<string, unknown> {
  const failures: string[] = [];
  const structuredFailures: InputShapeAdapterError[] = [];
  for (const inputShapeId of inputShapeIds) {
    try {
      return registry.require(inputShapeId).adapt(input) as Record<string, unknown>;
    } catch (err) {
      if (err instanceof InputShapeAdapterError) {
        structuredFailures.push(err);
      } else if (!(err instanceof ZodError)) {
        throw err;
      }
      failures.push(err instanceof Error ? err.message : String(err));
    }
  }
  if (structuredFailures.length > 0) {
    throw structuredFailures[0];
  }
  throw new Error(`Input did not match supported shapes ${inputShapeIds.join(', ')}: ${failures.join(' | ')}`);
}
