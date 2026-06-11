import { z } from 'zod';

import { ReadContextInputSchema } from '../bridge/read-context/read-context-schemas.js';
import { ReadReferenceContextInputSchema } from '../task/read-reference-context-schema.js';
import {
  InputShapeAdapterRegistry,
  type InputShapeAdapter,
} from './input-shape-adapter.js';

const readSpecAdapter: InputShapeAdapter<Record<string, unknown>> = {
  id: 'readspec',
  inputSchema: ReadContextInputSchema,
  adapt(input) {
    return ReadContextInputSchema.parse(input) as Record<string, unknown>;
  },
};

const readReferenceContextAdapter: InputShapeAdapter<Record<string, unknown>> = {
  id: 'read_reference_context',
  inputSchema: ReadReferenceContextInputSchema,
  adapt(input) {
    return ReadReferenceContextInputSchema.parse(input) as Record<string, unknown>;
  },
};

const genericPayloadSchema = z.record(z.unknown());

const bridgePayloadAdapter: InputShapeAdapter<Record<string, unknown>> = {
  id: 'bridge_payload',
  inputSchema: genericPayloadSchema,
  adapt(input) {
    return genericPayloadSchema.parse(input);
  },
};

const bridgeLogicJsonPayloadAdapter: InputShapeAdapter<Record<string, unknown>> = {
  id: 'bridge_logic_json_payload',
  inputSchema: genericPayloadSchema,
  adapt(input) {
    return { format: 'logic_json', ...genericPayloadSchema.parse(input) };
  },
};

const toolPayloadAdapter: InputShapeAdapter<Record<string, unknown>> = {
  id: 'tool_payload',
  inputSchema: genericPayloadSchema,
  adapt(input) {
    return genericPayloadSchema.parse(input);
  },
};

const emptyObjectAdapter: InputShapeAdapter<Record<string, unknown>> = {
  id: 'empty_object',
  inputSchema: z.object({}).strict(),
  adapt(input) {
    return z.object({}).strict().parse(input);
  },
};

export function registerReadSpecInputShapeAdapters(
  registry: InputShapeAdapterRegistry,
): InputShapeAdapterRegistry {
  return registry
    .register(readSpecAdapter)
    .register(readReferenceContextAdapter)
    .register(bridgePayloadAdapter)
    .register(bridgeLogicJsonPayloadAdapter)
    .register(toolPayloadAdapter)
    .register(emptyObjectAdapter);
}

export function createReadSpecInputShapeAdapterRegistry(): InputShapeAdapterRegistry {
  return registerReadSpecInputShapeAdapters(new InputShapeAdapterRegistry());
}
