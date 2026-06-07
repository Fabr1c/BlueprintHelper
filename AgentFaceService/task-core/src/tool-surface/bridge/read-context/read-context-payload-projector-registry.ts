import type {
  ReadContextPayloadProjectorId,
  ReadContextRouteDescriptor,
} from '../../templates/read-context-template-types.js';
import type { ReadContextInput } from './read-context-schemas.js';

export type ReadContextPostProcessResult = {
  payload: Record<string, unknown>;
  debug?: Record<string, unknown>;
};

export type ReadContextPayloadProjectorInput = {
  input: ReadContextInput;
  route: ReadContextRouteDescriptor;
  payloadSchema: string;
  payload: Record<string, unknown>;
};

export type ReadContextPayloadProjector = (
  input: ReadContextPayloadProjectorInput,
) => ReadContextPostProcessResult;

const PROJECTORS = new Map<ReadContextPayloadProjectorId, ReadContextPayloadProjector>();

export function registerReadContextPayloadProjector(
  id: ReadContextPayloadProjectorId,
  projector: ReadContextPayloadProjector,
): void {
  if (PROJECTORS.has(id)) {
    throw new Error(`Duplicate ReadContext payload projector: ${id}`);
  }
  PROJECTORS.set(id, projector);
}

export function getReadContextPayloadProjector(id: ReadContextPayloadProjectorId): ReadContextPayloadProjector {
  const projector = PROJECTORS.get(id);
  if (!projector) {
    throw new Error(`ReadContext payload projector is not registered: ${id}`);
  }
  return projector;
}
