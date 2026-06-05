import { InputShapeAdapterRegistry } from './input-shape-adapter.js';

export function createReadSpecInputShapeAdapterRegistry(): InputShapeAdapterRegistry {
  return new InputShapeAdapterRegistry();
}
